// Bastion remnant: jigsaw structure (`bastion/starts`, maxDepth 6), 26 templates
// carry 37 chests across four loot tables (treasure / bridge / hoglin_stable /
// other). This layer is coordinate-precise: the map wants the chest positions.
#pragma once

#include <cstdint>

#include "loots/structure_pieces.h"

namespace loots {

bool bastionLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

} // namespace loots
