#include "ancient_geode.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#ifdef LOOTS_HAS_GEODE
#include "structure_layout/layout.hpp"
#include "terrain.hpp"
#include "loots/trail_ruins.h"
#include "jigsaw_templates.hpp"
#include "loots/loot.h"
#include "loots/rng.h"
#include "loots/structure_pieces.h"
#include "loots/transform.h"
#include "pieces_c.h"

namespace loots {
namespace cli {
namespace {

std::unique_ptr<geode::Terrain> gTrailTerrain;
int trailTerrainHeight(int, uint64_t, int x, int z)
{
    return gTrailTerrain ? gTrailTerrain->height_world_surface(x, z) : 64;
}

const loots::data::TemplateInfo *findLayout(const std::string &name)
{
    for (const auto &t : loots::data::kTemplateInfos)
        if (name == t.name) return &t;
    return nullptr;
}

} // namespace

void installTrailTerrainHeight(uint64_t seed)
{
    gTrailTerrain = std::make_unique<geode::Terrain>(seed, 1);
    trailRuinsSetSurfaceHeightProvider(trailTerrainHeight);
}

void clearTrailTerrainHeight()
{
    gTrailTerrain.reset();
    trailRuinsSetSurfaceHeightProvider(nullptr);
}

std::vector<StructureChest> ancientCityChestsGeode(int mc, uint64_t seed, int x, int z,
                                                    int &piecesOut)
{
    std::vector<StructureChest> chests;
    piecesOut = 0;
    if (mc < 35) return chests;
    geode_layout::LayoutEngine engine(6, seed);
    auto chamber = engine.generate(x >> 4, z >> 4);
    if (!chamber.exists) return chests;
    piecesOut = chamber.pieces;

    struct Mapping { int id; const char *name; };
    static const Mapping mappings[] = {
        {20, "ancient_city/structures/barracks"},
        {21, "ancient_city/structures/chamber_1"},
        {22, "ancient_city/structures/chamber_2"},
        {23, "ancient_city/structures/chamber_3"},
        {24, "ancient_city/structures/sauna_1"},
        {27, "ancient_city/structures/tall_ruin_1"},
        {28, "ancient_city/structures/tall_ruin_2"},
        {29, "ancient_city/structures/tall_ruin_3"},
        {30, "ancient_city/structures/tall_ruin_4"},
        {42, "ancient_city/structures/ice_box_1"},
    };
    std::map<std::pair<int, int>, PostProcessRandom> postRngs;
    for (const auto &p : chamber.placed) {
        const char *name = nullptr;
        for (const auto &m : mappings) if (m.id == p.id) { name = m.name; break; }
        if (!name) continue;
        const loots::data::TemplateInfo *layout = findLayout(name);
        if (!layout) continue;
        for (int i = 0; i < layout->chestCount; i++) {
            const loots::data::ChestSpec &c = loots::data::kChestSpecs[layout->firstChest + i];
            int tx, tz;
            loots::templateTransform(c.x, c.z, Mirror::none, loots::rotationByIndex(p.rotation), 0, 0, tx, tz);
            StructureChest chest{};
            chest.x = p.pos.x + tx;
            chest.z = p.pos.z + tz;
            chest.templateName = name;
            chest.lootTable = c.table;
            const auto key = std::make_pair(chest.x >> 4, chest.z >> 4);
            auto it = postRngs.find(key);
            if (it == postRngs.end()) {
                it = postRngs.emplace(key, PostProcessRandom(
                    mc, loots_structure_loot_seed(mc, seed, chest.x, chest.z, 13 /* Ancient_City */))).first;
            }
            chest.lootSeed = it->second.nextLong();
            chest.reward = evaluateChest(chest.lootTable.c_str(), chest.lootSeed, mc);
            chests.push_back(std::move(chest));
        }
    }
    return chests;
}

int cmdAncientGeode(int mc, uint64_t seed, int x, int z)
{
    int pieces = 0;
    const auto chests = ancientCityChestsGeode(mc, seed, x, z, pieces);
    if (pieces == 0) {
        std::printf("{\"exists\":false}\n");
        return 1;
    }
    std::printf("{\"exists\":true,\"pieces\":%d,\"chests\":[", pieces);
    for (size_t i = 0; i < chests.size(); i++) {
        const StructureChest &c = chests[i];
        std::printf("%s{\"x\":%d,\"z\":%d,\"template\":\"%s\",\"table\":\"%s\",\"lootSeed\":%lld,\"evaluated\":%s}",
                    i ? "," : "", c.x, c.z, c.templateName.c_str(), c.lootTable.c_str(),
                    (long long) c.lootSeed, c.reward.evaluated ? "true" : "false");
    }
    std::printf("]}\n");
    return 0;
}

} // namespace cli
} // namespace loots

#else
namespace loots::cli {

void installTrailTerrainHeight(uint64_t) {}
void clearTrailTerrainHeight() {}

std::vector<StructureChest> ancientCityChestsGeode(int, uint64_t, int, int, int &piecesOut)
{
    piecesOut = 0;
    return {};
}

int cmdAncientGeode(int, uint64_t, int, int)
{
    std::fputs("ancient-geode requires the optional Geode library; for 26.3 use tools/minecraft26_3/loots-26.3.ps1 ancient-city\n", stderr);
    return 1;
}

} // namespace loots::cli
#endif
