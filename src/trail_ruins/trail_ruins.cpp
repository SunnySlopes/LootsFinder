#include "loots/trail_ruins.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <numeric>
#include <utility>
#include <vector>

#include "data.hpp"
#include "loots/rng.h"
#include "loots/transform.h"
#include "pieces_c.h"
#include "placer.hpp"
#include "processor.hpp"
#include "trail_ruins.hpp"
#include "trail_ruins_templates.hpp"

// Trail ruins adapter:
//   * the piece graph is produced by the shared JigsawPlacement port
//   * the archaeology processor chain (RuleProcessor / CappedProcessor /
//     AppendLoot) is replayed here because it is structure-specific
// See the public header for the terrain-height assumptions.

namespace loots {
namespace {

struct Vec3i {
    int x = 0, y = 0, z = 0;
};

Vec3i operator+(const Vec3i &a, const Vec3i &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

template <class Container>
void shuffle(LegacyRandom &rng, Container &v)
{
    for (int n = static_cast<int>(v.size()); n > 1; --n)
        std::swap(v[static_cast<size_t>(n - 1)], v[static_cast<size_t>(rng.nextInt(n))]);
}

Vec3i rotateLocal(int x, int y, int z, int rot)
{
    int ox = 0;
    int oz = 0;
    templateTransform(x, z, Mirror::none, rotationByIndex(rot), 0, 0, ox, oz);
    return {ox, y, oz};
}

int trailTemplateIndex(const std::string &name)
{
    for (size_t i = 0; i < sizeof(data::kTrailTemplates) / sizeof(data::kTrailTemplates[0]); i++)
        if (name == data::kTrailTemplates[i].name) return static_cast<int>(i);
    return -1;
}

struct Piece {
    int tmpl = -1;
    Vec3i pos;
    int rot = 0;
};

bool firstRuleLeavesGravel(int processor, const Vec3i &w)
{
    // trail_ruins_tower_top_archaeology has no initial RuleProcessor.
    if (processor == 2) return true;
    LegacyRandom r(static_cast<uint64_t>(jigsaw::mthGetSeed(w.x, w.y, w.z)));
    if (r.nextFloat() < 0.2f) return false; // gravel -> dirt
    if (r.nextFloat() < 0.1f) return false; // gravel -> coarse_dirt
    return true;
}

void collectSuspicious(const Piece &p, uint64_t seed, StructureLoot &out)
{
    const data::TrailTemplate &t = data::kTrailTemplates[p.tmpl];
    if (t.gravelCount == 0) return;

    std::vector<Vec3i> world;
    world.reserve(t.gravelCount);

    // Map surviving block index -> gravel position.  The two capped processors
    // in the houses list share the same seed, hence the same permutation; the
    // first one consumes its matches, the second sees the rest.
    std::vector<int> byIndex(static_cast<size_t>(t.blockCount), -1);
    for (int i = 0; i < t.gravelCount; i++) {
        const data::TrailGravel &g = data::kTrailGravel[t.firstGravel + i];
        const Vec3i local = rotateLocal(g.x, g.y, g.z, p.rot);
        const Vec3i w = p.pos + local;
        if (!firstRuleLeavesGravel(t.processor, w)) continue;
        byIndex[static_cast<size_t>(g.index)] = static_cast<int>(world.size());
        world.push_back(w);
    }
    if (world.empty()) return;

    // CappedProcessor: createThreadLocalInstance(worldSeed).forkPositional().at(position)
    LegacyRandom seedRng(seed);
    const uint64_t forkSeed = seedRng.nextLong();
    LegacyRandom capRng(static_cast<uint64_t>(jigsaw::mthGetSeed(p.pos.x, p.pos.y, p.pos.z)) ^
                        forkSeed);
    std::vector<uint16_t> perm(static_cast<size_t>(t.blockCount));
    std::iota(perm.begin(), perm.end(), 0);
    shuffle(capRng, perm);

    struct Cap {
        int limit;
        const char *table;
    };
    Cap caps[2];
    int capCount = 0;
    if (t.processor == 0) { // houses: capped(common,6) + capped(rare,3)
        caps[0] = {6, "trail_ruins_common"};
        caps[1] = {3, "trail_ruins_rare"};
        capCount = 2;
    } else { // roads / tower_top: capped(common,2)
        caps[0] = {2, "trail_ruins_common"};
        capCount = 1;
    }

    for (int c = 0; c < capCount; c++) {
        int replaced = 0;
        for (uint16_t index : perm) {
            const int ri = byIndex[static_cast<size_t>(index)];
            if (ri < 0) continue;
            const Vec3i w = world[static_cast<size_t>(ri)];
            StructureChest chest;
            chest.x = w.x;
            chest.y = w.y;
            chest.z = w.z;
            chest.templateName = t.name;
            chest.lootTable = caps[c].table;
            chest.lootSeed =
                jigsaw::legacySeedAt({w.x, w.y, w.z});
            out.suspicious.push_back(std::move(chest));
            byIndex[static_cast<size_t>(index)] = -1;
            if (++replaced >= caps[c].limit) break;
        }
    }
}

} // namespace

bool trailRuinsLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.kind = "trail_ruins";
    if (mc < 25 || mc > 35) return false;

    const auto &data = jigsaw::dataFor(mc);
    if (!data.available()) return false;

    jigsaw::JigsawConfig config;
    config.startPool = "trail_ruins/tower";
    config.maxDepth = 7;                   // trail_ruins.json "size"
    config.maxDistanceHorizontal = 80;
    config.maxDistanceVertical = 80;
    config.startHeight = -15;              // start_height.absolute
    config.projectStartToHeightmap = true; // project_start_to_heightmap
    config.useExpansionHack = false;

    const auto placement = jigsaw::placeJigsaw(config, mc, seed, x, z);
    if (!placement.exists || placement.pieces.empty()) return false;

    out.exists = true;
    out.pieces = int(placement.pieces.size());
    out.minX = out.minY = out.minZ = 0x7fffffff;
    out.maxX = out.maxY = out.maxZ = -0x7fffffff;
    for (const auto &placed : placement.pieces) {
        out.minX = std::min(out.minX, placed.box.lo.x);
        out.minY = std::min(out.minY, placed.box.lo.y);
        out.minZ = std::min(out.minZ, placed.box.lo.z);
        out.maxX = std::max(out.maxX, placed.box.hi.x);
        out.maxY = std::max(out.maxY, placed.box.hi.y);
        out.maxZ = std::max(out.maxZ, placed.box.hi.z);

        std::vector<int> templates;
        data.concreteTemplates(placed.element, templates);
        for (int templateIndex : templates) {
            const jigsaw::TemplateData &tmpl = data.templateAt(templateIndex);
            const int trailIndex = trailTemplateIndex(tmpl.name);
            if (trailIndex < 0) continue;
            Piece piece;
            piece.tmpl = trailIndex;
            piece.pos = {placed.origin.x, placed.origin.y, placed.origin.z};
            piece.rot = placed.rotation;
            collectSuspicious(piece, seed, out);
        }
    }
    return true;
}

} // namespace loots
