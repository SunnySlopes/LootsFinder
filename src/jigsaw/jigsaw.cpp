#include "loots/jigsaw.h"

#include <cstring>
#include <string>
#include <vector>

#include "loots/jigsaw_core.h"
#include "loots/transform.h"
#include "pieces_c.h"

// Generated template data (gitignored; see .gitignore). Without it the module is
// inert so a fresh checkout still builds.
#if defined(__has_include)
#if __has_include("jigsaw_templates.hpp")
#define LOOTS_HAS_JIGSAW_DATA 1
#include "jigsaw_templates.hpp"
#endif
#endif

namespace loots {
namespace {

using jigsaw::Box;
using jigsaw::Connector;
using jigsaw::Facing;
using jigsaw::Random;
using jigsaw::Top;
using jigsaw::Vec3;
using jigsaw::Piece;

#ifdef LOOTS_HAS_JIGSAW_DATA

int sizeX(int templateIndex) { return data::kTemplateInfos[templateIndex].sx; }
int sizeZ(int templateIndex) { return data::kTemplateInfos[templateIndex].sz; }
int sizeY(int templateIndex) { return data::kTemplateInfos[templateIndex].sy; }

void rotateLocal(int x, int z, int rot, int &ox, int &oz)
{
    loots::templateTransform(x, z, loots::Mirror::none, loots::rotationByIndex(rot), 0, 0, ox, oz);
}

std::vector<Connector> connectorsOf(int index)
{
    std::vector<Connector> out;
    const data::TemplateInfo &info = data::kTemplateInfos[index];
    for (int i = 0; i < info.jigsawCount; i++) {
        const data::JigsawSpec &j = data::kJigsawSpecs[info.firstJigsaw + i];
        Connector c;
        c.pos = {j.x, j.y, j.z};
        c.facing = static_cast<Facing>(j.facing);
        c.top = static_cast<Top>(j.top);
        c.name = j.name;
        c.target = j.target;
        c.pool = j.pool;
        c.rollable = !j.aligned;
        out.push_back(c);
    }
    return out;
}

/** One candidate: template index + rotation + world origin. */
struct Candidate {
    int templateIndex = -1;
    int rot = 0;
    Vec3 origin{};
};

struct Builder {
    std::vector<Piece> pieces;
    std::vector<Box> boxes;
    Random &rng;
    int mc = 0;
    uint64_t seed = 0;

    Builder(Random &r, int mc_, uint64_t seed_) : rng(r), mc(mc_), seed(seed_) {}

    /** Index of the piece whose connectors are being expanded (its box is skipped). */
    size_t ignoreBox = size_t(-1);

    bool collides(const Box &b) const
    {
        for (size_t i = 0; i < boxes.size(); i++) {
            if (i == ignoreBox) continue;
            if (b.intersects(boxes[i])) return true;
        }
        return false;
    }
};

/** Namespace-insensitive compare ("minecraft:x/y" == "x/y"). */
bool sameName(const char *a, const char *b)
{
    if (!a || !b) return false;
    if (std::strncmp(a, "minecraft:", 10) == 0) a += 10;
    if (std::strncmp(b, "minecraft:", 10) == 0) b += 10;
    return std::strcmp(a, b) == 0;
}

/** Weighted pick from a pool; entries are (templateIndex, weight). */
int pickPool(const char *poolName, Random &rng, bool *rigidOut)
{
    int total = 0;
    for (const auto &e : data::kPoolEntries)
        if (sameName(e.pool, poolName)) total += e.weight;
    if (total <= 0) return -1;
    int pick = rng.integer(total);
    for (const auto &e : data::kPoolEntries) {
        if (!sameName(e.pool, poolName)) continue;
        pick -= e.weight;
        if (pick < 0) {
            if (rigidOut) *rigidOut = e.rigid != 0;
            return e.templateIndex;
        }
    }
    return -1;
}

/**
 * `legacy(...)` pool elements use TERRAIN_MATCHING projection: their Y comes from
 * the heightmap, so they are allowed to overlap the source piece.
 */
bool isTerrainMatching(int templateIndex)
{
    const char *name = data::kTemplateInfos[templateIndex].name;
    static const char *const kPrefixes[] = {
        "pillager_outpost/", "bastion/", "ancient_city/", "woodland_mansion/",
    };
    for (const char *p : kPrefixes) {
        if (std::strncmp(name, p, std::strlen(p)) == 0) return true;
    }
    return false;
}

/**
 * Place one template whose connector `connIndex` must line up with the parent's
 * connector. Rotation candidates are tried in the game's shuffled order.
 */
bool tryAttach(Builder &b, const std::vector<Connector> &parentConnectors, int parentConnIndex,
               Vec3 parentOrigin, int parentRot, int parentTemplate, const Box &parentBox,
               bool parentRigid, bool terrainMatching, const char *poolName, int depth)
{
    const Connector &src = parentConnectors[size_t(parentConnIndex)];
    if (!poolName || !poolName[0]) return false;

    bool candidateRigid = true;
    const int candidateIndex = pickPool(poolName, b.rng, &candidateRigid);
    if (candidateIndex < 0) return false;
    const auto childConnectors = connectorsOf(candidateIndex);

    // Rotation candidates: shuffled 0..3 (consumes the same draws as the game).
    std::vector<int> rotations{0, 1, 2, 3};
    b.rng.shuffle(rotations);

    // Source connector in world space.
    int sx, sz;
    rotateLocal(src.pos.x, src.pos.z, parentRot, sx, sz);
    const Vec3 srcWorld{parentOrigin.x + sx, parentOrigin.y + src.pos.y, parentOrigin.z + sz};

    for (int rot : rotations) {
        for (const Connector &dst : childConnectors) {
            // vanilla matches the parent's target against the child's name
            if (!sameName(dst.name, src.target)) continue;

            int dx, dz;
            rotateLocal(dst.pos.x, dst.pos.z, rot, dx, dz);
            // Terrain-matching pieces anchor to the surface at the source jigsaw;
            // rigid pieces stay in the parent's plane (vanilla deltaY path).
            int childY = srcWorld.y - dst.pos.y;
            if (!(parentRigid && candidateRigid)) {
                // Terrain anchor: Y comes from the surface at the SOURCE jigsaw.
                childY = loots_surface_height(b.mc, b.seed, srcWorld.x, srcWorld.z) - dst.pos.y;
            }
            const Vec3 origin{srcWorld.x - dx, childY, srcWorld.z - dz};

            const int fsx = sizeX(candidateIndex);
            const int fsz = sizeZ(candidateIndex);
            const loots::Box2i tb = loots::templateBox(origin.x, origin.z,
                                                       loots::rotationByIndex(rot), 0, 0, fsx, fsz);
            const Box box{{tb.minX, origin.y, tb.minZ},
                          {tb.maxX, origin.y + sizeY(candidateIndex) - 1, tb.maxZ}};

            // ---- attachInsideSource (JigsawPlacement.Placer) ------------------
            // The candidate is tested against the source piece alone when its
            // jigsaw block sits inside the source bounding box, otherwise
            // against every already placed piece.
            const bool attachInsideSource =
                srcWorld.x >= parentBox.lo.x && srcWorld.x <= parentBox.hi.x &&
                srcWorld.y >= parentBox.lo.y && srcWorld.y <= parentBox.hi.y &&
                srcWorld.z >= parentBox.lo.z && srcWorld.z <= parentBox.hi.z;
            if (attachInsideSource) {
                // Terrain-matching children legitimately sit inside the source box
                // (the outpost tower on its base plate), so the source is not an
                // obstacle. Modelling vanilla's per-source free shape is still a
                // TODO; without it the base plate can grow two towers.
                if (!terrainMatching && box.intersects(parentBox)) continue;
            } else if (b.collides(box)) {
                continue;
            }

            Piece piece;
            piece.templateIndex = candidateIndex;
            piece.box = box;
            piece.rotation = rot;
            piece.rigid = candidateRigid;
            b.pieces.push_back(piece);
            b.boxes.push_back(box);
            (void)depth;
            return true;
        }
    }
    return false;
}

#endif

} // namespace

bool hasJigsawData()
{
#ifdef LOOTS_HAS_JIGSAW_DATA
    return true;
#else
    return false;
#endif
}

void jigsawStats(int &templates, int &connectors, int &poolEntries)
{
    templates = connectors = poolEntries = 0;
#ifdef LOOTS_HAS_JIGSAW_DATA
    templates = int(sizeof(data::kTemplateInfos) / sizeof(data::kTemplateInfos[0]));
    connectors = int(sizeof(data::kJigsawSpecs) / sizeof(data::kJigsawSpecs[0]));
    poolEntries = int(sizeof(data::kPoolEntries) / sizeof(data::kPoolEntries[0]));
#endif
}

JigsawAssembly assembleStructure(const char *startPool, int mc, uint64_t seed, int x, int z,
                                 int maxDepth)
{
    JigsawAssembly out;
    out.startPool = startPool ? startPool : "";
    out.dataAvailable = hasJigsawData();
    if (!out.dataAvailable || !startPool) return out;

#ifdef LOOTS_HAS_JIGSAW_DATA
    Random rng(loots_structure_rng_seed(mc, seed, x >> 4, z >> 4));

    // 1) start template from the start pool
    bool startRigid = true;
    const int startIndex = pickPool(startPool, rng, &startRigid);
    if (startIndex < 0) return out;
    const int startRot = rng.integer(4);   // Rotation.getRandom
    Piece start;
    start.templateIndex = startIndex;
    start.rotation = startRot;
    start.rigid = startRigid;
    const loots::Box2i sb = loots::templateBox(x, z, loots::rotationByIndex(startRot), 0, 0,
                                               sizeX(startIndex), sizeZ(startIndex));
    start.box = Box{{sb.minX, 0, sb.minZ}, {sb.maxX, sizeY(startIndex) - 1, sb.maxZ}};

    Builder builder(rng, mc, seed);
    builder.pieces.push_back(start);
    builder.boxes.push_back(start.box);

    // 2) breadth-first expansion, bounded by maxDepth
    size_t cursor = 0;
    int depth = 0;
    while (cursor < builder.pieces.size() && depth < maxDepth) {
        const size_t frontier = builder.pieces.size();
        for (; cursor < frontier; cursor++) {
            const Piece parent = builder.pieces[cursor]; // Child placement can reallocate the vector.
            builder.ignoreBox = cursor;
            const auto connectors = connectorsOf(parent.templateIndex);
            if (connectors.empty()) continue;
            std::vector<int> selection(connectors.size(), 0);
            std::vector<int> order;
            jigsaw::connector_order(rng, connectors.size(), selection, true, order);
            for (int ci : order) {
                tryAttach(builder, connectors, ci, parent.box.lo, parent.rotation,
                          parent.templateIndex, parent.box, parent.rigid,
                          isTerrainMatching(parent.templateIndex), connectors[size_t(ci)].pool,
                          depth);
            }
        }
        depth++;
    }

    for (const auto &p : builder.pieces) {
        PlacedTemplate pt;
        pt.name = data::kTemplateInfos[p.templateIndex].name;
        pt.x = p.box.lo.x;
        pt.y = p.box.lo.y;
        pt.z = p.box.lo.z;
        pt.maxX = p.box.hi.x;
        pt.maxY = p.box.hi.y;
        pt.maxZ = p.box.hi.z;
        pt.rotation = p.rotation;
        out.pieces.push_back(pt);
    }
    // The placement loop is a best-effort port; positions are not yet verified
    // against the game, so callers must keep `complete` false.
    out.complete = false;
#endif
    return out;
}

} // namespace loots
