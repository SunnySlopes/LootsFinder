// Runtime view of the generated 26.3 jigsaw data.
//
// The generated header keeps the original template-pool tree (single / list /
// empty elements, fallbacks, weights, priorities).  This layer turns that flat
// generated representation into lookup tables and the vanilla geometry helpers
// used by the shared placer.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "loots/jigsaw_core.h"
#include "loots/transform.h"

namespace loots {
namespace jigsaw {

enum class Projection : uint8_t { rigid = 0, terrainMatching = 1 };
enum class ElementKind : uint8_t { single = 0, list = 1, empty = 2 };

struct JigsawBlockData {
    Vec3 pos;
    Facing facing = Facing::north;
    Top top = Top::up;
    bool rollable = false;
    int placementPriority = 0;
    int selectionPriority = 0;
    std::string name;
    std::string target;
    std::string pool;
};

struct ChestData {
    Vec3 pos;
    std::string table;   // empty for a decorative RandomizableContainer
    std::string kind;    // chest / barrel / ...
    std::string state;   // canonical block state, e.g. minecraft:chest[facing=north,...]
};

struct EntityData {
    Vec3 pos;
    std::string type;
};

struct TemplateData {
    std::string name;
    int sx = 0, sy = 0, sz = 0;
    std::vector<JigsawBlockData> jigsaws;
    std::vector<ChestData> chests;
    std::vector<EntityData> entities;
};

struct ElementData {
    ElementKind kind = ElementKind::empty;
    Projection projection = Projection::rigid;
    std::string processors;
    int templateIndex = -1;
    std::vector<int> children;
};

struct PoolData {
    std::string name;
    int fallback = -1;
    // Element indices, repeated exactly `weight` times in JSON order, matching
    // StructureTemplatePool.templates.
    std::vector<int> templates;
};

/** Direction helper shared by the placer and adapters. */
Vec3 rotateLocal(const Vec3 &p, int rotation);
Facing rotateFacing(Facing facing, int rotation);
Top rotateTop(Top top, int rotation);
Vec3 stepOf(Facing facing);
int stepY(Facing facing);
Facing opposite(Facing facing);
Box templateBoundingBox(const TemplateData &t, const Vec3 &origin, int rotation);

struct DataBuilder;

class Data {
public:
    bool available() const { return available_; }

    int findPool(std::string_view name) const;
    int findTemplate(std::string_view name) const;
    const PoolData &poolAt(int index) const { return pools_[size_t(index)]; }
    const TemplateData &templateAt(int index) const { return templates_[size_t(index)]; }
    const ElementData &elementAt(int index) const { return elements_[size_t(index)]; }
    int poolCount() const { return int(pools_.size()); }
    int templateCount() const { return int(templates_.size()); }
    int elementCount() const { return int(elements_.size()); }
    int blockCount() const;

    /** StructureTemplatePool.getRandomTemplate(random). */
    int poolRandomTemplate(int poolIndex, Random &rng) const;
    /** StructureTemplatePool.getShuffledTemplates(random). */
    std::vector<int> poolShuffledTemplates(int poolIndex, Random &rng) const;
    /** StructureTemplatePool.getMaxSize(manager): max Y span of non-empty elements. */
    int poolMaxSize(int poolIndex) const;

    Box elementBox(int elementIndex, const Vec3 &origin, int rotation) const;
    int elementHeight(int elementIndex, int rotation) const;
    int groundLevelDelta(int elementIndex) const;
    /** Concrete template index for a single element; -1 for list/empty. */
    int elementTemplate(int elementIndex) const;
    /** Every concrete template in an element tree (list children included). */
    void concreteTemplates(int elementIndex, std::vector<int> &out) const;
    /** StructurePoolElement.getJigsaws(position, rotation) in list order. */
    std::vector<JigsawBlockData> elementJigsawBlocks(int elementIndex, const Vec3 &origin,
                                                     int rotation) const;
    /** getShuffledJigsawBlocks(): shuffle, then stable selection-priority sort. */
    std::vector<JigsawBlockData> shuffledJigsawBlocks(int elementIndex, const Vec3 &origin,
                                                      int rotation, Random &rng) const;

private:
    friend struct DataBuilder;
    friend const Data &data263();

    bool available_ = false;
    std::vector<TemplateData> templates_;
    std::vector<ElementData> elements_;
    std::vector<PoolData> pools_;
    std::unordered_map<std::string, int> templateIndex_;
    std::unordered_map<std::string, int> poolIndex_;
    std::vector<int> poolMaxSizeCache_;
};

/** The compiled-in 26.3 data (empty/inert when the generated header is absent). */
const Data &data263();

} // namespace jigsaw
} // namespace loots
