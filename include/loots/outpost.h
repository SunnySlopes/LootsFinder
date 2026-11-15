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

/** Exact WORLD_SURFACE_WG height provider (block Y of the first free block). */
using OutpostSurfaceHeightFn = int (*)(int mc, uint64_t seed, int x, int z);

/** Install (or clear with nullptr) the exact height provider. */
void outpostSetSurfaceHeightProvider(OutpostSurfaceHeightFn fn);

struct OutpostLoot {
    bool exists = false;
    /** A `feature_cage_with_allays` piece was placed (1.19+). */
    bool hasAllay = false;
    /** False while the outpost post-process RNG is still approximate. */
    bool lootSeedExact = false;
    /** Trophy chests inside placed towers (prompt-only layer: coords not drawn). */
    std::vector<StructureChest> chests;
};

bool pillagerOutpostLoot(int mc, uint64_t seed, int x, int z, OutpostLoot &out);

} // namespace loots
