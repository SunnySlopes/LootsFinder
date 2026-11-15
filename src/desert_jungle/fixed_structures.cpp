#include "loots/fixed_structures.h"

#include <cstring>

#include "pieces_c.h"

// Desert pyramid and jungle temple are single-piece structures.  cubiomes
// already models their placement rotation, chest positions and per-chunk
// LootTableSeed correctly; use that instead of reimplementing the structure
// RNG a second time.  The old local implementation used the wrong
// (normalized) rotation and the wrong RNG family for post-process seeds.
namespace loots {
namespace {

void fillChests(const loots_piece_result &in, StructureLoot &out, bool onlyJungleChests)
{
    out.exists = in.exists != 0;
    out.pieces = in.pieces;
    out.chests.clear();
    out.chests.reserve(static_cast<size_t>(in.chest_count));
    for (int i = 0; i < in.chest_count; i++) {
        const char *table = in.chests[i].table;
        if (onlyJungleChests && std::strcmp(table, "chests/jungle_temple") != 0) continue;
        StructureChest chest{};
        chest.x = in.chests[i].x;
        chest.z = in.chests[i].z;
        chest.y = 0;   // cubiomes does not expose the vertical chest offset
        chest.templateName = in.chests[i].template_name;
        chest.lootTable = table;
        chest.lootSeed = in.chests[i].seed;
        out.chests.push_back(std::move(chest));
    }
}

loots_piece_result g_pieces;

} // namespace

bool desertPyramidLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.kind = "desert_pyramid";
    if (!loots_desert_pieces(mc, seed, x, z, &g_pieces)) return false;
    fillChests(g_pieces, out, false);
    const int minX = x & ~15, minZ = z & ~15;
    out.minX = minX;
    out.minZ = minZ;
    out.maxX = minX + 20;
    out.maxZ = minZ + 20;
    return out.exists;
}

bool jungleTempleLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.kind = "jungle_temple";
    if (!loots_jungle_pieces(mc, seed, x, z, &g_pieces)) return false;
    fillChests(g_pieces, out, true);
    const int minX = x & ~15, minZ = z & ~15;
    out.minX = minX;
    out.minZ = minZ;
    // The 12x15 footprint is rotated by the structure; the hint layer does not
    // use these bounds, but keep a conservative square around the origin.
    out.maxX = minX + 14;
    out.maxZ = minZ + 14;
    return out.exists;
}

} // namespace loots
