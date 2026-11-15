// Woodland mansion room placement (26.3).
//
// Ports the room-placement half of WoodlandMansionPieces:
//   * MansionGrid (baseGrid, floorRooms[3], thirdFloorGrid, entrance/third-floor
//     setup, identifyRooms)
//   * MansionPiecePlacer room dispatch: 1x1 / 1x1Secret, 1x2 side/front/secret,
//     2x2 / 2x2Secret.
//
// Scope: only the room templates are placed because only they contain chests.
// Entrance, outer walls, inner walls, corridors/carpets and roofs consume no
// random numbers and contain no chests, so they are not modelled. `pieces` in
// the result therefore counts room pieces only, and `minX..maxZ` is the union of
// the room bounding boxes (Y is always 0: the structure start Y is terrain
// dependent and is not an input of this function).
//
// The random sequence matches Java's WorldgenRandom seeded with
// setLargeFeatureSeed(seed, chunkX, chunkZ): first Rotation.getRandom() (one
// nextInt(4), consumed by WoodlandMansionStructure.findGenerationPoint), then
// MansionGrid, then the room placement loop. `x, z` are the cubiomes structure
// position (the chunk's minimum block: chunkX*16, chunkZ*16).
//
// Chest loot seeds require cubiomes' per-version structure salt config. The
// bundled cubiomes has no Mansion entry, so the seeds stay 0; coordinates are
// still produced.
#pragma once

#include <cstdint>

#include "loots/structure_pieces.h"

namespace loots {

/**
 * Woodland mansion chests at the cubiomes structure position (x, z), which is
 * the chunk minimum block (the structure's own start is chunk*16 + 7).
 *
 * Returns false only when the generated template/chest data is unavailable at
 * compile time (the module is then inert, like the other data-backed features).
 */
bool woodlandMansionLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

} // namespace loots
