#include "loots/shipwreck.h"

#include "pieces_c.h"

// Shipwreck placement is a single template with a (4,0,15) rotation pivot.
// cubiomes already models the beached/ocean template lists, rotation, chest
// positions and the per-chunk LootTableSeed; use it instead of duplicating the
// template RNG here.
namespace loots {
namespace {

loots_piece_result g_pieces;

} // namespace

bool hasShipwreckData()
{
    return true;
}

bool shipwreckLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.kind = "shipwreck";
    if (!loots_shipwreck_pieces(mc, seed, x, z, &g_pieces)) return false;

    out.exists = g_pieces.exists != 0;
    out.pieces = g_pieces.pieces;
    out.chests.reserve(static_cast<size_t>(g_pieces.chest_count));
    for (int i = 0; i < g_pieces.chest_count; i++) {
        StructureChest chest{};
        chest.x = g_pieces.chests[i].x;
        chest.z = g_pieces.chests[i].z;
        chest.y = 0;   // vertical placement is not needed for the hint layer
        chest.templateName = g_pieces.chests[i].template_name;
        chest.lootTable = g_pieces.chests[i].table;
        chest.lootSeed = g_pieces.chests[i].seed;
        out.chests.push_back(std::move(chest));
    }
    const int minX = x & ~15, minZ = z & ~15;
    out.minX = minX;
    out.minZ = minZ;
    out.maxX = minX + 15;
    out.maxZ = minZ + 15;
    return out.exists;
}

} // namespace loots
