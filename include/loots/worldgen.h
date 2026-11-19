#pragma once

#include <cstdint>
#include <vector>

#include "loots/loots.h"

namespace loots {

/**
 * Exact 26.3 ancient-city piece graph, evaluated inside loots.
 *
 * The host no longer has to inject geode's layout engine: this is the same
 * piece-id mapping the map used before, backed by loots' own copy of the
 * layout/climate data.
 */
bool ancientCityLayout(int mc, uint64_t seed, int x, int z,
                       std::vector<AncientCityPiece> &out);

/** Version-aware WORLD_SURFACE_WG height (y + 1 of the first non-air block). */
int worldSurfaceHeight(int mc, uint64_t seed, int x, int z);

/** Version-aware OCEAN_FLOOR_WG height (y + 1 of the first blocking block). */
int worldOceanFloorHeight(int mc, uint64_t seed, int x, int z);

/** Version-aware terrain solidity, including carvers but without structure beards. */
bool worldSolid(int mc, uint64_t seed, int x, int y, int z);

/** Fluid at a non-solid block: 0 air, 1 water, 2 lava. */
int worldFluidType(int mc, uint64_t seed, int x, int y, int z);

/** True when the biome at (x, y, z) is in the 26.x `mineshaft_blocking` tag. */
bool worldMineshaftBlockingBiome(int mc, uint64_t seed, int x, int y, int z);

/** Density-only solidity before carvers. */
bool worldBaseSolid(int mc, uint64_t seed, int x, int y, int z);

/** Structure beard contribution at a block (0 when no structure). */
double worldBeard(int mc, uint64_t seed, int x, int y, int z);

} // namespace loots
