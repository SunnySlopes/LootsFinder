// Pillager outpost: the tower is a list_pool_element, so its chest only exists
// once the shared exact JigsawPlacement port has chosen and placed the tower.
//
//   start pool : pillager_outpost/base_plates   (maxDepth 7)
//   tower      : watchtower / watchtower_overgrown (list element)  -> 1 chest
//   allay      : pillager_outpost/feature_cage_with_allays
//
// The 26.3 oracle validates exact piece graphs, allay cages, chest X/Y/Z,
// loot seeds and the outpost_rot survival used by watchtower_overgrown.
#pragma once

#include <cstdint>
#include <vector>

#include "loots/loot.h"
#include "loots/structure_pieces.h"

namespace loots {

struct OutpostLoot {
    bool exists = false;
    /** A `feature_cage_with_allays` piece was placed (1.19+). */
    bool hasAllay = false;
    /** True: the shared exact placer and post-process RNG are used. */
    bool lootSeedExact = true;
    /** Trophy chests inside placed towers (prompt-only layer: coords not drawn). */
    std::vector<StructureChest> chests;
    /** True when the game version has no Allays or the exact placer was used. */
    bool allayEvaluated = false;
    int allayCages = 0;
};

/** x/z identify a start chunk already found by the caller's structure locator. */
bool pillagerOutpostLoot(int mc, uint64_t seed, int x, int z, OutpostLoot &out);

} // namespace loots
