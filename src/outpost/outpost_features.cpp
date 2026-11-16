#include "outpost_features.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#ifndef NDEBUG
#include <cstdio>
#include <cstdlib>
#endif

#include "loots/jigsaw_core.h"
#include "loots/worldgen.h"
#include "loots/transform.h"
#include "pieces_c.h"

// Generated template data (gitignored; see .gitignore). Without it the engine is
// inert so a fresh checkout still builds.
#if defined(__has_include)
#if __has_include("jigsaw_templates.hpp")
#define LOOTS_HAS_OUTPOST_DATA 1
#include "jigsaw_templates.hpp"
#endif
#endif

namespace loots {
namespace outpost_detail {

#ifdef LOOTS_HAS_OUTPOST_DATA

namespace {

using jigsaw::Box;
using jigsaw::Random;
using jigsaw::Vec3;

constexpr int kMaxDepth = 7;

enum ElementKind { kSingle, kList, kEmpty };

struct Element {
    ElementKind kind = kEmpty;
    int templateIndex = -1;
    bool rigid = true;
    /** List elements only: template indices of every child. */
    std::vector<int> children;
};

struct Pool {
    std::vector<int> flat;   // weighted entries, duplicated like vanilla
};

std::vector<Element> g_elements;
std::vector<Pool> g_pools;
std::unordered_map<std::string, int> g_poolIndex;
std::once_flag g_init;

std::string stripNamespace(const char *name)
{
    if (!name) return {};
    const char *n = name;
    if (std::strncmp(n, "minecraft:", 10) == 0) n += 10;
    return std::string(n);
}

bool sameName(const char *a, const char *b)
{
    if (!a || !b) return false;
    return stripNamespace(a) == stripNamespace(b);
}

int addElement(Element e)
{
    g_elements.push_back(std::move(e));
    return int(g_elements.size()) - 1;
}

int ensurePool(const std::string &name)
{
    auto it = g_poolIndex.find(name);
    if (it != g_poolIndex.end()) return it->second;
    const int id = int(g_pools.size());
    g_pools.emplace_back();
    g_poolIndex.emplace(name, id);
    return id;
}

void initPools()
{

    // Vanilla empty pool, used both as a real pool and as the fallback of every
    // pillager_outpost pool.
    ensurePool("empty");

    for (const auto &e : data::kPoolEntries) {
        const std::string name = stripNamespace(e.pool);
        if (!name.starts_with("pillager_outpost/")) continue;
        const int pid = ensurePool(name);
        if (name == "pillager_outpost/towers") {
            // 26.3's towers pool is a single list_pool_element (weight 1) whose
            // children are watchtower / watchtower_overgrown. The generated
            // header flattens it to two weight-1 rows; rebuild the list so the
            // RNG and bounding box match vanilla.
            if (g_pools[size_t(pid)].flat.empty()) {
                Element el;
                el.kind = kList;
                el.rigid = e.rigid != 0;
                g_pools[size_t(pid)].flat.push_back(addElement(std::move(el)));
            }
            const int el = g_pools[size_t(pid)].flat[0];
            g_elements[size_t(el)].children.push_back(e.templateIndex);
            continue;
        }
        if (e.templateIndex < 0) {
            Element el;
            el.kind = kEmpty;
            const int id = addElement(std::move(el));
            for (int i = 0; i < e.weight; i++) g_pools[size_t(pid)].flat.push_back(id);
        } else {
            Element el;
            el.kind = kSingle;
            el.templateIndex = e.templateIndex;
            el.rigid = e.rigid != 0;
            const int id = addElement(std::move(el));
            for (int i = 0; i < e.weight; i++) g_pools[size_t(pid)].flat.push_back(id);
        }
    }
}

int findPool(const char *name)
{
    if (!name || !name[0]) return -1;
    auto it = g_poolIndex.find(stripNamespace(name));
    return it == g_poolIndex.end() ? -1 : it->second;
}

const Pool *poolOf(int id)
{
    return id < 0 ? nullptr : &g_pools[size_t(id)];
}

// ---- geometry --------------------------------------------------------------

Vec3 rotateXZ(int x, int z, int rot)
{
    int ox = 0, oz = 0;
    loots::templateTransform(x, z, loots::Mirror::none, loots::rotationByIndex(rot), 0, 0, ox, oz);
    return {ox, 0, oz};
}

int rotateFacing(int facing, int rot)
{
    if (facing < 0 || facing >= 4) return facing;   // up / down are unchanged
    return (facing + rot) & 3;
}

int rotateTop(int top, int rot)
{
    // data::Top: up=0 down=1 north=2 east=3 south=4 west=5
    int horizontal = -1;
    switch (top) {
    case 2: horizontal = 0; break;
    case 3: horizontal = 1; break;
    case 4: horizontal = 2; break;
    case 5: horizontal = 3; break;
    default: return top;
    }
    horizontal = (horizontal + rot) & 3;
    static const int kTop[4] = {2, 3, 4, 5};
    return kTop[horizontal];
}

Vec3 stepOf(int facing)
{
    switch (facing) {
    case 0: return {0, 0, -1};   // north
    case 1: return {1, 0, 0};    // east
    case 2: return {0, 0, 1};    // south
    case 3: return {-1, 0, 0};   // west
    case 4: return {0, 1, 0};    // up
    default: return {0, -1, 0};  // down
    }
}

int oppositeFacing(int f)
{
    switch (f) {
    case 0: return 2;
    case 1: return 3;
    case 2: return 0;
    case 3: return 1;
    case 4: return 5;
    default: return 4;
    }
}

struct Jig {
    Vec3 pos;          // source: world; target: relative to the element origin
    const char *name = "";
    const char *target = "";
    const char *pool = "";
    bool rollable = false;
    int front = 0;
    int top = 0;
};

const data::TemplateInfo &templateInfo(int index) { return data::kTemplateInfos[index]; }

Box templateBox(int templateIndex, const Vec3 &origin, int rot)
{
    const data::TemplateInfo &t = templateInfo(templateIndex);
    const loots::Box2i b = loots::templateBox(origin.x, origin.z, loots::rotationByIndex(rot),
                                              0, 0, t.sx, t.sz);
    Box box;
    box.lo = {b.minX, origin.y, b.minZ};
    box.hi = {b.maxX, origin.y + t.sy - 1, b.maxZ};
    return box;
}

std::vector<Jig> templateJigs(int templateIndex, const Vec3 &origin, int rot)
{
    std::vector<Jig> out;
    const data::TemplateInfo &t = templateInfo(templateIndex);
    out.reserve(size_t(t.jigsawCount));
    for (int i = 0; i < t.jigsawCount; i++) {
        const data::JigsawSpec &s = data::kJigsawSpecs[t.firstJigsaw + i];
        const Vec3 local = rotateXZ(s.x, s.z, rot);
        Jig j;
        j.pos = {origin.x + local.x, origin.y + s.y, origin.z + local.z};
        j.name = s.name;
        j.target = s.target;
        j.pool = s.pool;
        j.rollable = !s.aligned;
        j.front = rotateFacing(int(s.facing), rot);
        j.top = rotateTop(int(s.top), rot);
        out.push_back(j);
    }
    return out;
}

Box unionBox(const Box &a, const Box &b)
{
    Box r;
    r.lo = {std::min(a.lo.x, b.lo.x), std::min(a.lo.y, b.lo.y), std::min(a.lo.z, b.lo.z)};
    r.hi = {std::max(a.hi.x, b.hi.x), std::max(a.hi.y, b.hi.y), std::max(a.hi.z, b.hi.z)};
    return r;
}

Box elementBox(int elementIndex, const Vec3 &origin, int rot)
{
    const Element &e = g_elements[size_t(elementIndex)];
    if (e.kind == kEmpty) return {{origin.x, origin.y, origin.z}, {origin.x, origin.y, origin.z}};
    if (e.kind == kSingle) return templateBox(e.templateIndex, origin, rot);
    Box box = templateBox(e.children[0], origin, rot);
    for (size_t i = 1; i < e.children.size(); i++)
        box = unionBox(box, templateBox(e.children[i], origin, rot));
    return box;
}

std::vector<Jig> elementJigs(int elementIndex, const Vec3 &origin, int rot)
{
    const Element &e = g_elements[size_t(elementIndex)];
    if (e.kind == kEmpty) return {};
    if (e.kind == kSingle) return templateJigs(e.templateIndex, origin, rot);
    return templateJigs(e.children[0], origin, rot);
}

bool contains(const Box &outer, const Vec3 &p)
{
    return p.x >= outer.lo.x && p.x <= outer.hi.x && p.y >= outer.lo.y && p.y <= outer.hi.y &&
           p.z >= outer.lo.z && p.z <= outer.hi.z;
}

bool boxInside(const Box &outer, const Box &inner)
{
    return inner.lo.x >= outer.lo.x && inner.hi.x <= outer.hi.x && inner.lo.y >= outer.lo.y &&
           inner.hi.y <= outer.hi.y && inner.lo.z >= outer.lo.z && inner.hi.z <= outer.hi.z;
}

int poolMaxSize(int poolId)
{
    int maxSize = 0;
    const Pool *p = poolOf(poolId);
    if (p) {
        for (int el : p->flat) {
            if (g_elements[size_t(el)].kind == kEmpty) continue;
            const Box b = elementBox(el, {0, 0, 0}, 0);
            maxSize = std::max(maxSize, b.hi.y - b.lo.y + 1);
        }
    }
    return maxSize;
}

bool canAttach(const Jig &source, const Jig &target)
{
    if (source.front != oppositeFacing(target.front)) return false;
    if (!source.rollable && source.top != target.top) return false;
    if (!sameName(source.target, target.name)) return false;
    return true;
}

struct FreeSpace {
    Box bounds;
    std::vector<Box> occupied;

    bool fits(const Box &candidate) const
    {
        if (!boxInside(bounds, candidate)) return false;
        return std::none_of(occupied.begin(), occupied.end(),
                            [&](const Box &current) { return current.intersects(candidate); });
    }
};

struct Piece {
    int element = -1;
    Vec3 origin{};
    int rot = 0;
    Box box{};
    bool rigid = true;
    int depth = 0;
    size_t freeSpace = 0;
};

struct Ctx {
    int mc = 0;
    uint64_t seed = 0;
    Random rng;
    std::vector<Piece> pieces;
    std::vector<FreeSpace> spaces;
    std::vector<int> queue;

    Ctx(int mc_, uint64_t seed_, uint64_t rngSeed) : mc(mc_), seed(seed_), rng(rngSeed) {}
};

/** JigsawPlacement.Placer.tryPlacingChildren for one source piece. */
void expandSource(Ctx &ctx, int pieceIndex)
{
    const int sourceElement = ctx.pieces[size_t(pieceIndex)].element;
    const Vec3 sourceOrigin = ctx.pieces[size_t(pieceIndex)].origin;
    const int sourceRot = ctx.pieces[size_t(pieceIndex)].rot;
    const Box sourceBox = ctx.pieces[size_t(pieceIndex)].box;
    const bool sourceRigid = ctx.pieces[size_t(pieceIndex)].rigid;
    const int depth = ctx.pieces[size_t(pieceIndex)].depth;
    const size_t contextSpace = ctx.pieces[size_t(pieceIndex)].freeSpace;
    size_t insideSpace = 0;

    std::vector<Jig> sourceJigs = elementJigs(sourceElement, sourceOrigin, sourceRot);
    ctx.rng.shuffle(sourceJigs);
    // selection priority is 0 for every outpost jigsaw, so the stable sort is a
    // no-op and is intentionally omitted.

    for (const Jig &sourceJig : sourceJigs) {
        const int targetPoolId = findPool(sourceJig.pool);
        const Pool *targetPool = poolOf(targetPoolId);
#ifndef NDEBUG
        if (std::getenv("LOOTS_OP_DEBUG") && sourceJig.pool && std::strstr(sourceJig.pool, "features"))
            std::fprintf(stderr, "OP source=%d pool=%s targetPool=%d candidates=%zu\n", pieceIndex, sourceJig.pool, targetPoolId, targetPool ? targetPool->flat.size() : 0);
#endif
        if (!targetPool) continue;

        // Every pillager_outpost pool falls back to minecraft:empty, so the
        // fallback contributes no candidates.
        std::vector<int> targets;
        if (depth != kMaxDepth) {
            targets = targetPool->flat;
            ctx.rng.shuffle(targets);
        }

        const Vec3 step = stepOf(sourceJig.front);
        const Vec3 targetJigsawPos{sourceJig.pos.x + step.x, sourceJig.pos.y + step.y,
                                   sourceJig.pos.z + step.z};
        size_t childSpace = contextSpace;
        if (contains(sourceBox, targetJigsawPos)) {
            if (insideSpace == 0) {
                insideSpace = ctx.spaces.size();
                ctx.spaces.push_back({sourceBox, {}});
            }
            childSpace = insideSpace;
        }

        bool attached = false;
#ifndef NDEBUG
        int dbgAttach = 0, dbgFits = 0;
#endif
        for (int targetElement : targets) {
            if (g_elements[size_t(targetElement)].kind == kEmpty) break;

            std::vector<int> rotations{0, 1, 2, 3};
            ctx.rng.shuffle(rotations);

            for (int targetRot : rotations) {
                if (attached) break;

                std::vector<Jig> targetJigs =
                    elementJigs(targetElement, {0, 0, 0}, targetRot);
                ctx.rng.shuffle(targetJigs);

                const Box hackBox = elementBox(targetElement, {0, 0, 0}, targetRot);
                int expandTo = 0;
                if (hackBox.hi.y - hackBox.lo.y + 1 <= 16) {
                    for (const Jig &tj : targetJigs) {
                        const Vec3 s = stepOf(tj.front);
                        const Vec3 np{tj.pos.x + s.x, tj.pos.y + s.y, tj.pos.z + s.z};
                        if (!contains(hackBox, np)) continue;
                        const int childPool = findPool(tj.pool);
                        if (childPool >= 0) expandTo = std::max(expandTo, poolMaxSize(childPool));
                    }
                }

                const bool targetRigid = g_elements[size_t(targetElement)].rigid;
                for (const Jig &tj : targetJigs) {
                    if (!canAttach(sourceJig, tj)) continue;
#ifndef NDEBUG
                    dbgAttach++;
#endif

                    const Vec3 rawOrigin{targetJigsawPos.x - tj.pos.x,
                                         targetJigsawPos.y - tj.pos.y,
                                         targetJigsawPos.z - tj.pos.z};
                    Box targetBox = elementBox(targetElement, rawOrigin, targetRot);

                    const int targetBoxY =
                        (sourceRigid && targetRigid)
                            ? rawOrigin.y
                            : loots::worldSurfaceHeight(ctx.mc, ctx.seed, sourceJig.pos.x, sourceJig.pos.z) -
                                  tj.pos.y;
                    const int yOffset = targetBoxY - targetBox.lo.y;
                    targetBox.lo.y += yOffset;
                    targetBox.hi.y += yOffset;
                    Vec3 origin = rawOrigin;
                    origin.y += yOffset;

                    if (expandTo > 0) {
                        const int newSize =
                            std::max(expandTo + 1, targetBox.hi.y - targetBox.lo.y);
                        if (targetBox.lo.y + newSize > targetBox.hi.y)
                            targetBox.hi.y = targetBox.lo.y + newSize;
                    }

                    if (!ctx.spaces[childSpace].fits(targetBox)) continue;
                    ctx.spaces[childSpace].occupied.push_back(targetBox);
#ifndef NDEBUG
                    dbgFits++;
#endif

                    Piece child;
                    child.element = targetElement;
                    child.origin = origin;
                    child.rot = targetRot;
                    child.box = targetBox;
                    child.rigid = targetRigid;
                    child.depth = depth + 1;
                    child.freeSpace = childSpace;
                    const int childIndex = int(ctx.pieces.size());
                    ctx.pieces.push_back(child);
                    if (child.depth <= kMaxDepth) ctx.queue.push_back(childIndex);
                    attached = true;
                    break;
                }
            }
            if (attached) break;
        }
#ifndef NDEBUG
        if (std::getenv("LOOTS_OP_DEBUG") && sourceJig.pool && std::strstr(sourceJig.pool, "features"))
            std::fprintf(stderr, "OP result piece=%d attach=%d fits=%d\n", pieceIndex, dbgAttach, dbgFits);
#endif
    }
}

} // namespace

Assembly assembleOutpost(int mc, uint64_t seed, int x, int z)
{
    Assembly out;
    std::call_once(g_init, initPools);
    x &= ~15;
    z &= ~15;

    const int startPoolId = findPool("pillager_outpost/base_plates");
    const Pool *startPool = poolOf(startPoolId);
    if (!startPool || startPool->flat.empty()) return out;

    Ctx ctx(mc, seed, loots_structure_rng_seed(mc, seed, x >> 4, z >> 4));

    // JigsawPlacement.addPieces: rotation first, then the centre element.
    const int startRot = ctx.rng.integer(4);
    const int centerElement = startPool->flat[size_t(ctx.rng.integer(int(startPool->flat.size())))];
    if (g_elements[size_t(centerElement)].kind == kEmpty) return out;

    const Vec3 startPos{x, 0, z};
    Box centerBox = elementBox(centerElement, startPos, startRot);
    const int centerX = (centerBox.lo.x + centerBox.hi.x) / 2;
    const int centerZ = (centerBox.lo.z + centerBox.hi.z) / 2;
    const int bottomY = loots::worldSurfaceHeight(mc, seed, centerX, centerZ);
    const int dy = bottomY - (centerBox.lo.y + 1);
    Vec3 centerOrigin = startPos;
    centerOrigin.y += dy;
    centerBox.lo.y += dy;
    centerBox.hi.y += dy;

    const int centerY = bottomY;

    ctx.spaces.push_back({
        {{centerX - 80, std::max(-64, centerY - 80), centerZ - 80},
         {centerX + 80, std::min(319, centerY + 80), centerZ + 80}},
        {centerBox}
    });

    Piece center;
    center.element = centerElement;
    center.origin = centerOrigin;
    center.rot = startRot;
    center.box = centerBox;
    center.rigid = g_elements[size_t(centerElement)].rigid;
    center.depth = 0;
    ctx.pieces.push_back(center);
    ctx.queue.push_back(0);

    for (size_t qi = 0; qi < ctx.queue.size(); qi++) expandSource(ctx, ctx.queue[qi]);

    out.exists = true;
    for (const Piece &piece : ctx.pieces) {
        const Element &el = g_elements[size_t(piece.element)];
        const size_t first = out.pieces.size();
        auto addTemplate = [&](int templateIndex) {
            PlacedTemplate pt;
            pt.name = templateInfo(templateIndex).name;
            pt.x = piece.origin.x;
            pt.y = piece.origin.y;
            pt.z = piece.origin.z;
            pt.rotation = piece.rot;
            out.pieces.push_back(std::move(pt));
        };
        if (el.kind == kSingle) {
            addTemplate(el.templateIndex);
        } else if (el.kind == kList) {
            for (int child : el.children) addTemplate(child);
        }
        for (size_t i = first; i < out.pieces.size(); i++) {
            if (out.pieces[i].name == "pillager_outpost/feature_cage_with_allays")
                ++out.allayCages;
        }
    }
    return out;
}

#else

Assembly assembleOutpost(int, uint64_t, int, int)
{
    return {};
}

#endif

} // namespace outpost_detail
} // namespace loots
