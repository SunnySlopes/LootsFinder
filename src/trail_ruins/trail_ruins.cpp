#include "loots/trail_ruins.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#ifndef NDEBUG
#include <cstdlib>
#include <cstdio>
#endif
#include <deque>
#include <functional>
#include <map>
#include <numeric>
#include <utility>
#include <vector>

#include "loots/rng.h"
#include "loots/transform.h"
#include "pieces_c.h"
#include "trail_ruins.hpp"
#include "trail_ruins_templates.hpp"

// Best-effort faithful port of:
//   JigsawPlacement.addPieces / Placer  (all trail-ruins pools are RIGID)
//   StructureTemplate.processBlockInfos + RuleProcessor + CappedProcessor
//   rule.blockentity.AppendLoot
// for the 26.3 trail_ruins structure.  See the header for the known gap
// (exact WORLD_SURFACE_WG height).

namespace loots {
namespace {

constexpr int kMaxDepth = 7;       // trail_ruins.json "size"
constexpr int kMaxDistance = 80;   // max_distance_from_center
constexpr int kStartY = -15;       // start_height.absolute
constexpr int kWorldMinY = -64;
constexpr int kWorldMaxY = 319;

struct Vec3i {
    int x = 0, y = 0, z = 0;
};

Vec3i operator+(const Vec3i &a, const Vec3i &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3i operator-(const Vec3i &a, const Vec3i &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

struct Box {
    int minX = 0, minY = 0, minZ = 0, maxX = -1, maxY = -1, maxZ = -1;

    bool intersects(const Box &b) const
    {
        return maxX >= b.minX && minX <= b.maxX && maxY >= b.minY && minY <= b.maxY &&
               maxZ >= b.minZ && minZ <= b.maxZ;
    }
    bool contains(const Vec3i &p) const
    {
        return p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY && p.z >= minZ &&
               p.z <= maxZ;
    }
    bool contains(const Box &b) const
    {
        return b.minX >= minX && b.maxX <= maxX && b.minY >= minY && b.maxY <= maxY &&
               b.minZ >= minZ && b.maxZ <= maxZ;
    }
    Box moved(int dx, int dy, int dz) const
    {
        return {minX + dx, minY + dy, minZ + dz, maxX + dx, maxY + dy, maxZ + dz};
    }
    void encapsulate(const Box &b)
    {
        minX = std::min(minX, b.minX);
        minY = std::min(minY, b.minY);
        minZ = std::min(minZ, b.minZ);
        maxX = std::max(maxX, b.maxX);
        maxY = std::max(maxY, b.maxY);
        maxZ = std::max(maxZ, b.maxZ);
    }
};

int floorDiv(int v, int d)
{
    int q = v / d;
    if (v < 0 && v % d) q--;
    return q;
}

const char *stripNamespace(const char *s)
{
    if (s && std::strncmp(s, "minecraft:", 10) == 0) return s + 10;
    return s;
}

bool sameName(const char *a, const char *b)
{
    if (!a || !b) return false;
    return std::strcmp(stripNamespace(a), stripNamespace(b)) == 0;
}

/** Mth.getSeed(int, int, int). */
int64_t mthGetSeed(int x, int y, int z)
{
    const int32_t ix = static_cast<int32_t>(static_cast<uint32_t>(x) * 3129871u);
    int64_t v = static_cast<int64_t>(ix) ^ (static_cast<int64_t>(z) * 116129781LL) ^
                static_cast<int64_t>(y);
    uint64_t u = static_cast<uint64_t>(v) * static_cast<uint64_t>(v);
    u *= 42317861ULL;
    u += static_cast<uint64_t>(v) * 11ULL;
    return static_cast<int64_t>(u) >> 16;
}

template <class Container>
void shuffle(LegacyRandom &rng, Container &v)
{
    for (int n = static_cast<int>(v.size()); n > 1; --n)
        std::swap(v[static_cast<size_t>(n - 1)], v[static_cast<size_t>(rng.nextInt(n))]);
}

int rotateDir(int dir, int rot)
{
    if (dir >= 4) return dir; // up / down stay vertical
    return (dir + rot) & 3;   // N,E,S,W order matches Rotation order
}

int oppositeDir(int dir)
{
    switch (dir) {
    case 0: return 2;
    case 1: return 3;
    case 2: return 0;
    case 3: return 1;
    case 4: return 5;
    default: return 4;
    }
}

Vec3i stepDir(int dir)
{
    switch (dir) {
    case 0: return {0, 0, -1};
    case 1: return {1, 0, 0};
    case 2: return {0, 0, 1};
    case 3: return {-1, 0, 0};
    case 4: return {0, 1, 0};
    default: return {0, -1, 0};
    }
}

int stepY(int dir) { return dir == 4 ? 1 : (dir == 5 ? -1 : 0); }

Vec3i rotateLocal(int x, int y, int z, int rot)
{
    int ox = 0, oy = 0, oz = 0;
    templateTransform(x, y, z, Mirror::none, rotationByIndex(rot), 0, 0, 0, ox, oy, oz);
    return {ox, oy, oz};
}

int templateIndex(const char *location)
{
    for (size_t i = 0; i < sizeof(data::kTrailTemplates) / sizeof(data::kTrailTemplates[0]); i++)
        if (sameName(data::kTrailTemplates[i].name, location)) return static_cast<int>(i);
    return -1;
}

int poolIndex(const char *name)
{
    for (size_t i = 0; i < sizeof(data::kTrailPools) / sizeof(data::kTrailPools[0]); i++)
        if (sameName(data::kTrailPools[i].name, name)) return static_cast<int>(i);
    return -1;
}

/** getShuffledTemplates(): expanded weighted list, then Util.shuffle. */
std::vector<int> poolTemplates(const char *name)
{
    std::vector<int> out;
    const int pi = poolIndex(name);
    if (pi < 0) return out;
    const data::TrailPool &pool = data::kTrailPools[pi];
    out.reserve(pool.count);
    for (int i = 0; i < pool.count; i++) {
        const data::TrailPoolElement &e = data::kTrailPoolElements[pool.first + i];
        const int ti = templateIndex(e.location);
        if (ti < 0) continue;
        for (int w = 0; w < (e.weight ? e.weight : 1); w++) out.push_back(ti);
    }
    return out;
}

Box templateBox3(const Vec3i &pos, int rot, int tmpl)
{
    const data::TrailTemplate &t = data::kTrailTemplates[tmpl];
    const Box2i b = templateBox(pos.x, pos.z, rotationByIndex(rot), 0, 0, t.sx, t.sz);
    return {b.minX, pos.y, b.minZ, b.maxX, pos.y + t.sy - 1, b.maxZ};
}

struct JigRuntime {
    Vec3i pos;
    int facing = 0;
    int top = 4;
    bool rollable = false;
    int placePriority = 0;
    const char *name = "";
    const char *target = "";
    const char *pool = "";
};

/** SinglePoolElement.getShuffledJigsawBlocks(). */
void shuffledJigsaws(int tmpl, const Vec3i &anchor, int rot, LegacyRandom &rng,
                     std::vector<JigRuntime> &out)
{
    const data::TrailTemplate &t = data::kTrailTemplates[tmpl];
    out.clear();
    if (t.jigsawCount == 0) return;
    const data::TrailJigsaw *base = &data::kTrailJigsaws[t.firstJigsaw];
    std::vector<int> order(t.jigsawCount);
    std::iota(order.begin(), order.end(), 0);
    shuffle(rng, order); // Util.shuffle
    std::stable_sort(order.begin(), order.end(), [base](int a, int b) {
        return base[a].selectionPriority > base[b].selectionPriority;
    });
    out.reserve(order.size());
    for (int idx : order) {
        const data::TrailJigsaw &j = base[idx];
        const Vec3i local = rotateLocal(j.x, j.y, j.z, rot);
        out.push_back({anchor + local,
                       rotateDir(j.facing, rot),
                       rotateDir(j.top, rot),
                       j.rollable != 0,
                       j.placementPriority,
                       j.name,
                       j.target,
                       j.pool});
    }
}

bool canAttach(const JigRuntime &source, const JigRuntime &target)
{
    return source.facing == oppositeDir(target.facing) &&
           (source.rollable || source.top == target.top) &&
           sameName(source.target, target.name);
}

/** a \ b, split into at most six disjoint boxes. */
void subtractBox(const Box &a, const Box &b, std::vector<Box> &out)
{
    if (!a.intersects(b)) {
        out.push_back(a);
        return;
    }
    if (a.minX < b.minX) out.push_back({a.minX, a.minY, a.minZ, b.minX - 1, a.maxY, a.maxZ});
    if (a.maxX > b.maxX) out.push_back({b.maxX + 1, a.minY, a.minZ, a.maxX, a.maxY, a.maxZ});
    const int x0 = std::max(a.minX, b.minX);
    const int x1 = std::min(a.maxX, b.maxX);
    if (a.minY < b.minY) out.push_back({x0, a.minY, a.minZ, x1, b.minY - 1, a.maxZ});
    if (a.maxY > b.maxY) out.push_back({x0, b.maxY + 1, a.minZ, x1, a.maxY, a.maxZ});
    const int y0 = std::max(a.minY, b.minY);
    const int y1 = std::min(a.maxY, b.maxY);
    if (a.minZ < b.minZ) out.push_back({x0, y0, a.minZ, x1, y1, b.minZ - 1});
    if (a.maxZ > b.maxZ) out.push_back({x0, y0, b.maxZ + 1, x1, y1, a.maxZ});
}

/** True when `target` is fully covered by the union of `boxes`. */
bool covered(const Box &target, const std::vector<Box> &boxes)
{
    std::vector<Box> pending;
    pending.push_back(target);
    for (const Box &b : boxes) {
        std::vector<Box> next;
        next.reserve(pending.size() * 3);
        for (const Box &p : pending) subtractBox(p, b, next);
        pending.swap(next);
        if (pending.empty()) return true;
        if (pending.size() > 4096) return false; // runaway guard; should not happen for trail ruins
    }
    return pending.empty();
}

struct FreeCtx {
    bool global = false;
    Box aabb;
    std::vector<Box> boxes;
};

struct Piece {
    int tmpl = -1;
    Vec3i pos; // StructureTemplate anchor
    int rot = 0;
    Box box;
    int groundDelta = 1;
    int localCtx = -1;
};

struct State {
    int piece = 0;
    int ctx = -1; // -1 = global context
    int depth = 0;
};

TrailSurfaceHeightFn g_heightFn = nullptr;
bool g_usedApproxHeight = true;

int surfaceHeight(int mc, uint64_t seed, int x, int z)
{
    if (g_heightFn) {
        g_usedApproxHeight = false;
        return g_heightFn(mc, seed, x, z);
    }
    g_usedApproxHeight = true;
    return loots_surface_height(mc, seed, x, z);
}

class Placer {
public:
    Placer(int mc, uint64_t seed) : mc_(mc), seed_(seed) {}

    bool run(int startX, int startZ, StructureLoot &out)
    {
        LegacyRandom rng(static_cast<uint64_t>(largeFeatureSeedValue(mc_, seed_, startX >> 4, startZ >> 4)));
        const int centerRot = rng.nextInt(4); // Rotation.getRandom
        std::vector<int> tower = poolTemplates("minecraft:trail_ruins/tower");
        if (tower.empty()) return false;
        const int centerTmpl = tower[static_cast<size_t>(rng.nextInt(static_cast<int>(tower.size())))];

        Vec3i centerPos{startX, kStartY, startZ};
        Box centerBox = templateBox3(centerPos, centerRot, centerTmpl);
        const int centerX = (centerBox.minX + centerBox.maxX) / 2;
        const int centerZ = (centerBox.minZ + centerBox.maxZ) / 2;
        const int bottomY = surfaceHeight(mc_, seed_, centerX, centerZ) - 1;
        const int oldAbsGroundY = centerBox.minY + 1; // SinglePoolElement GROUND_LEVEL_DELTA
        const int dy = bottomY - oldAbsGroundY;
        centerPos.y += dy;
        centerBox = centerBox.moved(0, dy, 0);

        global_.global = true;
        global_.aabb = {centerX - kMaxDistance,
                        std::max(bottomY - kMaxDistance, kWorldMinY),
                        centerZ - kMaxDistance,
                        centerX + kMaxDistance,
                        std::min(bottomY + kMaxDistance, kWorldMaxY),
                        centerZ + kMaxDistance};
        global_.boxes.clear();
        global_.boxes.push_back(centerBox);

        Piece center;
        center.tmpl = centerTmpl;
        center.pos = centerPos;
        center.rot = centerRot;
        center.box = centerBox;
        center.groundDelta = 1;
        pieces_.push_back(center);
        tryPlacing(0, -1, 0, rng);
        while (!queue_.empty()) {
            auto it = queue_.begin();
            const State s = it->second.front();
            it->second.pop_front();
            if (it->second.empty()) queue_.erase(it);
            tryPlacing(s.piece, s.ctx, s.depth, rng);
        }

        if (pieces_.empty()) return false;
        out.kind = "trail_ruins";
        out.exists = true;
        out.pieces = static_cast<int>(pieces_.size());
        out.minX = out.minY = out.minZ = 0x7fffffff;
        out.maxX = out.maxY = out.maxZ = -0x7fffffff;
        for (const Piece &p : pieces_) {
#ifndef NDEBUG
            if (std::getenv("LOOTS_TRAIL_DEBUG"))
                std::fprintf(stderr, "TP %s %d %d %d rot%d\n", data::kTrailTemplates[p.tmpl].name, p.pos.x, p.pos.y, p.pos.z, p.rot);
#endif
            out.minX = std::min(out.minX, p.box.minX);
            out.minY = std::min(out.minY, p.box.minY);
            out.minZ = std::min(out.minZ, p.box.minZ);
            out.maxX = std::max(out.maxX, p.box.maxX);
            out.maxY = std::max(out.maxY, p.box.maxY);
            out.maxZ = std::max(out.maxZ, p.box.maxZ);
            collectSuspicious(p, out);
        }
        return true;
    }

private:
    void enqueue(const State &s, int priority)
    {
        queue_[priority].push_back(s);
    }

    bool isFree(int ctx, const Box &b) const
    {
        if (ctx < 0) {
            if (!global_.aabb.contains(b)) return false;
            for (const Box &placed : global_.boxes)
                if (placed.intersects(b)) return false;
            return true;
        }
        return covered(b, localCtxs_[static_cast<size_t>(ctx)].boxes);
    }

    void addToCtx(int ctx, const Box &b)
    {
        if (ctx < 0)
            global_.boxes.push_back(b);
        else
            localCtxs_[static_cast<size_t>(ctx)].boxes.push_back(b);
    }

    void tryPlacing(int pieceIndex, int ctxIndex, int depth, LegacyRandom &rng)
    {
        const Piece source = pieces_[static_cast<size_t>(pieceIndex)];
        const Box sourceBB = source.box;
        const int sourceBoxY = sourceBB.minY;

        std::vector<JigRuntime> sourceJigs;
        shuffledJigsaws(source.tmpl, source.pos, source.rot, rng, sourceJigs);

        for (const JigRuntime &sj : sourceJigs) {
            const Vec3i targetJigsawPos = sj.pos + stepDir(sj.facing);
            const int sourceJigsawLocalY = sj.pos.y - sourceBoxY;

            std::vector<int> targets;
            if (depth != kMaxDepth) {
                targets = poolTemplates(sj.pool);
                shuffle(rng, targets);
            }
            // The fallback pool of every trail-ruins pool is minecraft:empty.
            if (targets.empty()) continue;

            // JigsawPlacement.Placer: inside-source connectors get a private
            // free-shape seeded with the source bounding box.
            int childCtx = ctxIndex;
            if (sourceBB.contains(targetJigsawPos)) {
                if (pieces_[static_cast<size_t>(pieceIndex)].localCtx < 0) {
                    FreeCtx c;
                    c.global = false;
                    c.boxes.push_back(sourceBB);
                    localCtxs_.push_back(std::move(c));
                    pieces_[static_cast<size_t>(pieceIndex)].localCtx =
                        static_cast<int>(localCtxs_.size()) - 1;
                }
                childCtx = pieces_[static_cast<size_t>(pieceIndex)].localCtx;
            }

            bool attached = false;
            for (int targetTmpl : targets) {
                if (attached) break;
                std::vector<int> rotations{0, 1, 2, 3};
                shuffle(rng, rotations); // Rotation.getShuffled
                for (int rot : rotations) {
                    if (attached) break;
                    std::vector<JigRuntime> targetJigs;
                    shuffledJigsaws(targetTmpl, Vec3i{0, 0, 0}, rot, rng, targetJigs);
                    for (const JigRuntime &tj : targetJigs) {
                        if (!canAttach(sj, tj)) continue;
                        const Vec3i rawTargetBoxPos =
                            targetJigsawPos - tj.pos;
                        const Box rawTargetBB = templateBox3(rawTargetBoxPos, rot, targetTmpl);
                        const int deltaY =
                            sourceJigsawLocalY - tj.pos.y + stepY(sj.facing);
                        // Both source and target are RIGID for trail ruins.
                        const int targetBoxY = sourceBoxY + deltaY;
                        const int yOffset = targetBoxY - rawTargetBB.minY;
                        const Box targetBB = rawTargetBB.moved(0, yOffset, 0);

                        if (!isFree(childCtx, targetBB)) continue;

                        Piece child;
                        child.tmpl = targetTmpl;
                        child.pos = {rawTargetBoxPos.x, rawTargetBoxPos.y + yOffset,
                                     rawTargetBoxPos.z};
                        child.rot = rot;
                        child.box = targetBB;
                        child.groundDelta = source.groundDelta - deltaY;
                        const int childIndex = static_cast<int>(pieces_.size());
                        pieces_.push_back(child);
                        addToCtx(childCtx, targetBB);
                        if (depth + 1 <= kMaxDepth)
                            enqueue({childIndex, childCtx, depth + 1}, sj.placePriority);
                        attached = true;
                        break;
                    }
                }
            }
        }
    }

    bool firstRuleLeavesGravel(int processor, const Vec3i &w) const
    {
        // trail_ruins_tower_top_archaeology has no initial RuleProcessor.
        if (processor == 2) return true;
        LegacyRandom r(static_cast<uint64_t>(mthGetSeed(w.x, w.y, w.z)));
        if (r.nextFloat() < 0.2f) return false; // gravel -> dirt
        if (r.nextFloat() < 0.1f) return false; // gravel -> coarse_dirt
        return true;
    }

    static uint64_t lootSeedAt(const Vec3i &w)
    {
        // ProcessorRule is seeded by Mth.getSeed(worldBlockPos); TagMatchTest
        // consumes no RNG, so AppendLoot's nextLong is the first draw.
        LegacyRandom r(static_cast<uint64_t>(mthGetSeed(w.x, w.y, w.z)));
        return r.nextLong();
    }

    void collectSuspicious(const Piece &p, StructureLoot &out) const
    {
        const data::TrailTemplate &t = data::kTrailTemplates[p.tmpl];
        if (t.gravelCount == 0) return;

        std::vector<Vec3i> world;
        world.reserve(t.gravelCount);

        // Map surviving block index -> gravel position.  The two capped
        // processors in the houses list share the same seed, hence the same
        // permutation; the first one consumes its matches, the second sees the
        // rest.
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
        LegacyRandom seedRng(seed_);
        const uint64_t forkSeed = seedRng.nextLong();
        LegacyRandom capRng(static_cast<uint64_t>(mthGetSeed(p.pos.x, p.pos.y, p.pos.z)) ^ forkSeed);
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
                chest.lootSeed = lootSeedAt(w);
                out.suspicious.push_back(std::move(chest));
                byIndex[static_cast<size_t>(index)] = -1;
                if (++replaced >= caps[c].limit) break;
            }
        }
    }

    int mc_;
    uint64_t seed_;
    std::vector<Piece> pieces_;
    std::vector<FreeCtx> localCtxs_;
    FreeCtx global_;
    std::map<int, std::deque<State>, std::greater<int>> queue_;
};

} // namespace

void trailRuinsSetSurfaceHeightProvider(TrailSurfaceHeightFn fn)
{
    g_heightFn = fn;
}

TrailRuinsStatus trailRuinsStatus()
{
    TrailRuinsStatus s;
    s.complete = false;
    s.exactSurfaceHeight = g_heightFn != nullptr;
    s.templateData = true;
    s.hasChests = false;
    if (g_heightFn)
        s.missing = "in-game oracle verification; archaeology/trail_ruins_common loot table "
                    "is not in data/loot_tables.hpp";
    else
        s.missing = "exact WORLD_SURFACE_WG height (install TrailSurfaceHeightFn); in-game "
                    "oracle verification; archaeology/trail_ruins_common loot table is not in "
                    "data/loot_tables.hpp";
    return s;
}

bool trailRuinsLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.kind = "trail_ruins";
    // The template layout is stable from 1.20 onward; only the archaeology
    // loot-table band changes, and that is selected by `mc` during evaluation.
    if (mc < 25) return false;
    if (sizeof(data::kTrailTemplates) == 0) return false;

    Placer placer(mc, seed);
    const int chunkX = floorDiv(x, 16);
    const int chunkZ = floorDiv(z, 16);
    if (!placer.run(chunkX * 16, chunkZ * 16, out)) {
        out.exists = false;
        return false;
    }
    return true;
}

} // namespace loots
