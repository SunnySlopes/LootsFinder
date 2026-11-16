#pragma once

#include <cstdint>

#include "loots/structure_pieces.h"

namespace loots {

/**
 * Ocean-ruin chests and, on warm ruins, suspicious sand.
 *
 * The suspicious-sand pass replays vanilla's
 *   BlockRotProcessor -> BlockIgnoreProcessor -> CappedProcessor(5, RuleProcessor, AppendLoot)
 * chain, including the per-block `Mth.getSeed` RNG and the 26.3 template block
 * order. Terrain height/solidity comes from loots' built-in worldgen.
 */
bool oceanRuinLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

} // namespace loots
