#include "data.hpp"

#include <algorithm>
#include <iterator>
#include <numeric>

#include "loots/transform.h"

#if defined(__has_include)
#if __has_include("jigsaw_data.hpp")
#define LOOTS_HAS_JIGSAW_DATA 1
#include "jigsaw_data.hpp"
#endif
#endif

namespace loots {
namespace jigsaw {

struct DataBuilder {
    static Data build();
};

namespace {

constexpr Vec3 kZero{0, 0, 0};

Box invalidBox()
{
    return {{1, 1, 1}, {0, 0, 0}};
}

bool isValid(const Box &box) { return box.hi.x >= box.lo.x && box.hi.y >= box.lo.y && box.hi.z >= box.lo.z; }

} // namespace

Vec3 rotateLocal(const Vec3 &p, int rotation)
{
    int ox = 0;
    int oz = 0;
    templateTransform(p.x, p.z, Mirror::none, rotationByIndex(rotation), 0, 0, ox, oz);
    return {ox, p.y, oz};
}

Facing rotateFacing(Facing facing, int rotation)
{
    const int value = int(facing);
    if (value >= 4) return facing; // up / down are unchanged by yaw rotation
    return Facing((value + rotation) & 3);
}

Top rotateTop(Top top, int rotation)
{
    const int value = int(top);
    if (value < 2) return top; // up / down are unchanged by yaw rotation
    return Top(((value - 2 + rotation) & 3) + 2);
}

Vec3 stepOf(Facing facing)
{
    switch (facing) {
    case Facing::north: return {0, 0, -1};
    case Facing::east: return {1, 0, 0};
    case Facing::south: return {0, 0, 1};
    case Facing::west: return {-1, 0, 0};
    case Facing::up: return {0, 1, 0};
    case Facing::down: return {0, -1, 0};
    }
    return {0, 0, 0};
}

int stepY(Facing facing)
{
    return facing == Facing::up ? 1 : (facing == Facing::down ? -1 : 0);
}

Facing opposite(Facing facing)
{
    switch (facing) {
    case Facing::north: return Facing::south;
    case Facing::east: return Facing::west;
    case Facing::south: return Facing::north;
    case Facing::west: return Facing::east;
    case Facing::up: return Facing::down;
    case Facing::down: return Facing::up;
    }
    return Facing::north;
}

Box templateBoundingBox(const TemplateData &t, const Vec3 &origin, int rotation)
{
    const Box2i box = templateBox(origin.x, origin.z, rotationByIndex(rotation), 0, 0, t.sx, t.sz);
    return {{box.minX, origin.y, box.minZ}, {box.maxX, origin.y + t.sy - 1, box.maxZ}};
}

#ifdef LOOTS_HAS_JIGSAW_DATA

namespace {

constexpr int kDirectionCount = 6;
Facing facingFromInt(int value)
{
    return value >= 0 && value < kDirectionCount ? Facing(value) : Facing::north;
}

Top topFromInt(int value)
{
    return value >= 0 && value < kDirectionCount ? Top(value) : Top::up;
}

Projection projectionFromInt(int value)
{
    return value == 1 ? Projection::terrainMatching : Projection::rigid;
}

ElementKind kindFromInt(int value)
{
    switch (value) {
    case 0: return ElementKind::single;
    case 1: return ElementKind::list;
    default: return ElementKind::empty;
    }
}

} // namespace

Data DataBuilder::build()
{
    Data data;
    data.available_ = true;

    data.templates_.reserve(std::size(data::kJigsawTemplates));
    for (const auto &spec : data::kJigsawTemplates) {
        TemplateData t;
        t.name = spec.name;
        t.sx = spec.sx;
        t.sy = spec.sy;
        t.sz = spec.sz;
        t.jigsaws.reserve(spec.jigsawCount);
        for (uint32_t i = 0; i < spec.jigsawCount; ++i) {
            const auto &j = data::kJigsawBlocks[spec.firstJigsaw + i];
            JigsawBlockData block;
            block.pos = {j.x, j.y, j.z};
            block.facing = facingFromInt(j.facing);
            block.top = topFromInt(j.top);
            block.rollable = j.rollable != 0;
            block.placementPriority = j.placementPriority;
            block.selectionPriority = j.selectionPriority;
            block.name = j.name;
            block.target = j.target;
            block.pool = j.pool;
            t.jigsaws.push_back(std::move(block));
        }
        t.chests.reserve(spec.chestCount);
        for (uint32_t i = 0; i < spec.chestCount; ++i) {
            const auto &c = data::kJigsawChests[spec.firstChest + i];
            ChestData chest;
            chest.pos = {c.x, c.y, c.z};
            chest.table = c.table;
            chest.kind = c.kind;
            chest.state = c.state;
            t.chests.push_back(std::move(chest));
        }
        t.entities.reserve(spec.entityCount);
        for (uint32_t i = 0; i < spec.entityCount; ++i) {
            const auto &e = data::kJigsawEntities[spec.firstEntity + i];
            EntityData entity;
            entity.pos = {e.x, e.y, e.z};
            entity.type = e.type;
            t.entities.push_back(std::move(entity));
        }
        data.templates_.push_back(std::move(t));
    }

    data.elements_.reserve(std::size(data::kJigsawElements));
    for (const auto &spec : data::kJigsawElements) {
        ElementData element;
        element.kind = kindFromInt(spec.kind);
        element.projection = projectionFromInt(spec.projection);
        element.processors = spec.processors;
        element.templateIndex = spec.templateIndex;
        element.children.reserve(spec.childCount);
        for (uint32_t i = 0; i < spec.childCount; ++i)
            element.children.push_back(int(data::kJigsawElementChildren[spec.firstChild + i]));
        data.elements_.push_back(std::move(element));
    }

    const size_t poolCount = std::size(data::kJigsawPools);
    data.pools_.reserve(poolCount);
    for (const auto &spec : data::kJigsawPools) {
        PoolData pool;
        pool.name = spec.name;
        pool.fallback = spec.fallback;
        for (uint32_t i = 0; i < spec.entryCount; ++i) {
            const auto &entry = data::kJigsawPoolEntries[spec.firstEntry + i];
            for (int w = 0; w < entry.weight; ++w) pool.templates.push_back(entry.element);
        }
        data.pools_.push_back(std::move(pool));
    }
    data.poolMaxSizeCache_.assign(poolCount, -1);

    for (int i = 0; i < int(data.templates_.size()); ++i)
        data.templateIndex_.emplace(data.templates_[size_t(i)].name, i);
    for (int i = 0; i < int(data.pools_.size()); ++i)
        data.poolIndex_.emplace(data.pools_[size_t(i)].name, i);

    // Precompute StructureTemplatePool.getMaxSize so placement is read-only
    // and safe to run concurrently.
    data.poolMaxSizeCache_.assign(poolCount, 0);
    for (size_t p = 0; p < poolCount; ++p) {
        int maxSize = 0;
        for (int elementIndex : data.pools_[p].templates) {
            if (data.elements_[size_t(elementIndex)].kind == ElementKind::empty) continue;
            const Box box = data.elementBox(elementIndex, kZero, 0);
            if (isValid(box)) maxSize = std::max(maxSize, box.height());
        }
        data.poolMaxSizeCache_[p] = maxSize;
    }
    return data;
}

#else

Data DataBuilder::build() { return {}; }

#endif

int Data::findPool(std::string_view name) const
{
    const auto it = poolIndex_.find(std::string(name));
    return it == poolIndex_.end() ? -1 : it->second;
}

int Data::findTemplate(std::string_view name) const
{
    const auto it = templateIndex_.find(std::string(name));
    return it == templateIndex_.end() ? -1 : it->second;
}

int Data::blockCount() const
{
    int count = 0;
    for (const auto &t : templates_) count += int(t.jigsaws.size());
    return count;
}

int Data::poolRandomTemplate(int poolIndex, Random &rng) const
{
    const PoolData &pool = pools_[size_t(poolIndex)];
    if (pool.templates.empty()) return -1;
    return pool.templates[size_t(rng.integer(int(pool.templates.size())))];
}

std::vector<int> Data::poolShuffledTemplates(int poolIndex, Random &rng) const
{
    std::vector<int> result = pools_[size_t(poolIndex)].templates;
    rng.shuffle(result);
    return result;
}

int Data::poolMaxSize(int poolIndex) const
{
    if (poolIndex < 0 || poolIndex >= int(poolMaxSizeCache_.size())) return 0;
    return poolMaxSizeCache_[size_t(poolIndex)];
}

Box Data::elementBox(int elementIndex, const Vec3 &origin, int rotation) const
{
    if (elementIndex < 0 || elementIndex >= int(elements_.size())) return invalidBox();
    const ElementData &element = elements_[size_t(elementIndex)];
    switch (element.kind) {
    case ElementKind::single: {
        if (element.templateIndex < 0) return invalidBox();
        return templateBoundingBox(templates_[size_t(element.templateIndex)], origin, rotation);
    }
    case ElementKind::list: {
        Box result = invalidBox();
        bool found = false;
        for (int child : element.children) {
            if (elements_[size_t(child)].kind == ElementKind::empty) continue;
            const Box box = elementBox(child, origin, rotation);
            if (!isValid(box)) continue;
            if (!found) {
                result = box;
                found = true;
            } else {
                result.encapsulate(box);
            }
        }
        return found ? result : invalidBox();
    }
    case ElementKind::empty:
    default:
        return invalidBox();
    }
}

int Data::elementHeight(int elementIndex, int rotation) const
{
    const Box box = elementBox(elementIndex, kZero, rotation);
    return isValid(box) ? box.height() : 0;
}

int Data::groundLevelDelta(int) const { return 1; }

int Data::elementTemplate(int elementIndex) const
{
    if (elementIndex < 0 || elementIndex >= int(elements_.size())) return -1;
    const ElementData &element = elements_[size_t(elementIndex)];
    return element.kind == ElementKind::single ? element.templateIndex : -1;
}

void Data::concreteTemplates(int elementIndex, std::vector<int> &out) const
{
    if (elementIndex < 0 || elementIndex >= int(elements_.size())) return;
    const ElementData &element = elements_[size_t(elementIndex)];
    switch (element.kind) {
    case ElementKind::single:
        if (element.templateIndex >= 0) out.push_back(element.templateIndex);
        break;
    case ElementKind::list:
        for (int child : element.children) concreteTemplates(child, out);
        break;
    case ElementKind::empty:
    default:
        break;
    }
}

std::vector<JigsawBlockData> Data::elementJigsawBlocks(int elementIndex, const Vec3 &origin,
                                                       int rotation) const
{
    std::vector<JigsawBlockData> result;
    if (elementIndex < 0 || elementIndex >= int(elements_.size())) return result;
    const ElementData &element = elements_[size_t(elementIndex)];
    switch (element.kind) {
    case ElementKind::single: {
        if (element.templateIndex < 0) return result;
        const TemplateData &t = templates_[size_t(element.templateIndex)];
        result.reserve(t.jigsaws.size());
        for (const auto &source : t.jigsaws) {
            JigsawBlockData block = source;
            const Vec3 local = rotateLocal(source.pos, rotation);
            block.pos = {origin.x + local.x, origin.y + local.y, origin.z + local.z};
            block.facing = rotateFacing(source.facing, rotation);
            block.top = rotateTop(source.top, rotation);
            result.push_back(std::move(block));
        }
        return result;
    }
    case ElementKind::list:
        if (!element.children.empty()) return elementJigsawBlocks(element.children.front(), origin, rotation);
        return result;
    case ElementKind::empty:
    default:
        return result;
    }
}

std::vector<JigsawBlockData> Data::shuffledJigsawBlocks(int elementIndex, const Vec3 &origin,
                                                        int rotation, Random &rng) const
{
    std::vector<JigsawBlockData> result = elementJigsawBlocks(elementIndex, origin, rotation);
    rng.shuffle(result);
    std::stable_sort(result.begin(), result.end(),
                     [](const JigsawBlockData &a, const JigsawBlockData &b) {
                         return a.selectionPriority > b.selectionPriority;
                     });
    return result;
}

const Data &dataShared()
{
    static const Data data = DataBuilder::build();
    return data;
}

const Data &dataFor(int mc)
{
    // 1.20.1 (25) through 26.3 (35): all bastion/trail-ruins/outpost jigsaw
    // inputs are byte-identical; only the outpost spawn-override serialization
    // differs.  1.19.2 (23) has the same outpost templates/pools/config (the
    // pools are code-defined there), so it also uses this data.  Older versions
    // keep the historical fallback until a snapshot is extracted.
    (void) mc;
    return dataShared();
}

} // namespace jigsaw
} // namespace loots
