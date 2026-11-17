// Bastion remnant: shared exact JigsawPlacement (`bastion/starts`, maxDepth 6),
// 26 templates carry 37 chests across four loot tables (treasure / bridge /
// hoglin_stable / other).  Piece origins/rotations/bounds and chest X/Y/Z plus
// per-chunk loot seeds match the 26.3 Java oracle.
#pragma once

#include <cstdint>

#include "loots/structure_pieces.h"

namespace loots {

bool bastionLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

} // namespace loots
