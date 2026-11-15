// Shipwreck: one template from a fixed list, chests come from structure-block
// data markers (metadata: map_chest / treasure_chest / supply_chest) that vanilla
// turns into containers at position.below().
//
// Source (26.3, ShipwreckStructure / ShipwreckPieces):
//   rotation = Rotation.getRandom(random)            // nextInt(4)
//   offset   = (chunkMinX, 90, chunkMinZ)
//   template = Util.getRandom(list, random)          // nextInt(list.length)
//   PIVOT    = (4, 0, 15)                            // setRotationPivot
//   chest    = marker.below()
//
// Only the ocean/beached template list differs per structure instance; cubiomes
// models a single Shipwreck type, so the list is chosen from the biome at the
// position (beach -> beached list).
#pragma once

#include <cstdint>

#include "loots/loot.h"
#include "loots/structure_pieces.h"

namespace loots {

/** Shipwreck chest evaluation. Prompt-only layers only need `hasLoot`. */
bool shipwreckLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

/** Kept for API compatibility; shipwreck now uses cubiomes piece data. */
bool hasShipwreckData();

} // namespace loots
