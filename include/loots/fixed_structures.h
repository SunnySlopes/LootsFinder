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

/** Desert pyramid: 4 chests, `chests/desert_pyramid`. */
bool desertPyramidLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

/** Jungle temple: 2 chests, `chests/jungle_temple`. */
bool jungleTempleLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

} // namespace loots
