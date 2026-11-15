#pragma once

#include <cstdint>

#include "loots/structure_pieces.h"

namespace loots {

/** Exact Heightmap.OCEAN_FLOOR_WG height (block Y of the first blocking block + 1). */
using OceanRuinHeightFn = int (*)(int mc, uint64_t seed, int x, int z);

/** Whether (x, y, z) is a solid block in the generated world (water/air = false). */
using OceanRuinSolidFn = int (*)(int mc, uint64_t seed, int x, int y, int z);

/**
 * Install (or clear with nullptr) the terrain inputs used by the ocean-ruin
 * suspicious-sand processor. Without a provider only chests are evaluated.
 */
void oceanRuinSetTerrainProvider(OceanRuinHeightFn height, OceanRuinSolidFn solid);

/**
 * Ocean-ruin chests and (on warm ruins) suspicious sand.
 *
 * The suspicious-sand pass replays vanilla's
 *   BlockRotProcessor -> BlockIgnoreProcessor -> CappedProcessor(5, RuleProcessor, AppendLoot)
 * chain, including the per-block `Mth.getSeed` RNG and the 26.3 template block
 * order. It is exact for 26.3 when the host supplies the terrain provider.
 */
bool oceanRuinLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

} // namespace loots
