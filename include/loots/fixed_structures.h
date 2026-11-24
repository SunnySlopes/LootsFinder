// Structures whose chests are placed by code (no templates).
//
// Both are `SinglePieceStructure`: the piece starts at the chunk's min block,
// gets a random horizontal direction (one `nextInt(4)` from the structure RNG,
// consumed BEFORE any chest), and places chests at fixed local offsets.
//
// Source (1.21.11, identical in 1.19.2 / 1.20.1 / 1.21.5):
//   DesertPyramidPiece: chests at (10 + stepX*2, -11, 10 + stepZ*2) for
//                       Direction.Plane.HORIZONTAL == NORTH, EAST, SOUTH, WEST
//   JungleTemplePiece : main chest (8, -3, 3), hidden chest (9, -3, 10)
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "loots/loot.h"
#include "loots/structure_pieces.h"

namespace loots {

/**
 * SinglePieceStructure.findGenerationPoint sea-level gate: lowest WORLD_SURFACE_WG
 * first-occupied height among the four corners of [x, x+sizeX]×[z, z+sizeZ] must
 * be >= sea level (63). Cubiomes placement alone misses this and produces false
 * desert pyramids / jungle temples.
 */
bool singlePieceAboveSea(int mc, uint64_t seed, int x, int z, int sizeX, int sizeZ);

/**
 * Desert-well placement / feature support check (approximate terrain).
 * Origin is WORLD_SURFACE first-occupied at (x,z). Rejects when the surface
 * column is not solid or the 5×5×2 support volume under it fails:
 *   - ≤26.2 DesertWellFeature: each column needs solid at y-1 or y-2
 *   - 26.3+ placement volumeMatch: every cell in [−2..2]×[−2..−1]×[−2..2] solid
 * Writes the approximated surface Y when non-null. Cubiomes only checks rarity
 * + desert biome, so this filters coastal / cliff false positives.
 */
bool desertWellCanPlace(int mc, uint64_t seed, int x, int z, int *surfaceY);

/** Desert pyramid: 4 chests + archaeology suspicious sand (mc >= 25). */
bool desertPyramidLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

/** Desert well: presence + archaeology suspicious sand (mc >= 25). */
bool desertWellLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

/** Jungle temple: 2 chests, `chests/jungle_temple`. */
bool jungleTempleLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

} // namespace loots
