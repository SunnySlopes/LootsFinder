// Pillager outpost: the tower is a jigsaw piece, so its single chest only exists
// once the placement engine has chosen a tower template.
//
//   start pool : pillager_outpost/base_plates   (maxDepth 7)
//   tower      : pillager_outpost/watchtower / watchtower_overgrown  -> 1 chest
//   allay      : pillager_outpost/feature_cage_with_allays (an entity marker,
//                not a container, so it is reported as a flag only)
#pragma once

#include <cstdint>
#include <vector>

#include "loots/loot.h"
#include "loots/structure_pieces.h"

namespace loots {

struct OutpostLoot {
    bool exists = false;
    /** A `feature_cage_with_allays` piece was placed (1.19+). Available for 1.19+; allayEvaluated is true when the built-in height query is available. */
    bool hasAllay = false;
    /** False while the outpost post-process RNG is still approximate. */
    bool lootSeedExact = false;
    /** Trophy chests inside placed towers (prompt-only layer: coords not drawn). */
    std::vector<StructureChest> chests;
    /** True when the game version has no Allays or the built-in exact height was used. */
    bool allayEvaluated = false;
    int allayCages = 0;
};

/** x/z identify a start chunk already found by the caller's structure locator. */
bool pillagerOutpostLoot(int mc, uint64_t seed, int x, int z, OutpostLoot &out);

} // namespace loots
