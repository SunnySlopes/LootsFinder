#include "loots/structure_pieces.h"

#include <map>

#include "loots/rng.h"
#include "pieces_c.h"

namespace loots {
namespace {

void fill(const loots_piece_result &in, StructureLoot &out)
{
    out.exists = in.exists != 0;
    out.pieces = in.pieces;
    out.minX = in.min_x;
    out.minY = in.min_y;
    out.minZ = in.min_z;
    out.maxX = in.max_x;
    out.maxY = in.max_y;
    out.maxZ = in.max_z;
    out.chests.clear();
    out.chests.reserve(static_cast<size_t>(in.chest_count));
    for (int i = 0; i < in.chest_count; i++) {
        StructureChest chest{};
        chest.x = in.chests[i].x;
        chest.y = in.chests[i].y;
        chest.z = in.chests[i].z;
        chest.templateName = in.chests[i].template_name;
        chest.lootTable = in.chests[i].table;
        chest.lootSeed = in.chests[i].seed;
        out.chests.push_back(std::move(chest));
    }
}

/** Shared buffer: the C shim writes into a static one, so copy results out. */
loots_piece_result g_buffer;

} // namespace

void StructureLoot::evaluate(int mc)
{
    for (auto &chest : chests)
        chest.reward = evaluateChest(chest.lootTable.c_str(), chest.lootSeed, mc);
    for (auto &s : suspicious)
        s.reward = evaluateChest(s.lootTable.c_str(), s.lootSeed, mc);
}

bool netherFortressLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.kind = "nether_fortress";
    if (!loots_fortress_pieces(mc, seed, x, z, &g_buffer)) return false;
    fill(g_buffer, out);
    // cubiomes fills the old (wrong) chest positions/seeds. Recompute the
    // post-process RNG after replacing the chest coordinates.
    std::map<std::pair<int, int>, PostProcessRandom> postRngs;
    for (auto &chest : out.chests) {
        const auto key = std::make_pair(chest.x >> 4, chest.z >> 4);
        auto it = postRngs.find(key);
        if (it == postRngs.end()) {
            it = postRngs.emplace(key, PostProcessRandom(
                mc, loots_structure_loot_seed(mc, seed, chest.x, chest.z, 18)))
                     .first;
        }
        chest.lootSeed = it->second.nextLong();
    }
    return out.exists;
}

bool endCityLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.kind = "end_city";
    if (!loots_end_city_pieces(mc, seed, x, z, &g_buffer)) return false;
    fill(g_buffer, out);
    return out.exists;
}

} // namespace loots
