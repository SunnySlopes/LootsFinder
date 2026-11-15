#pragma once
#include <cstdint>
#include <vector>

#include "loots/structure_pieces.h"

namespace loots {
namespace cli {
std::vector<StructureChest> ancientCityChestsGeode(int mc, uint64_t seed, int x, int z,
                                                    int &piecesOut);

/** Install an exact WORLD_SURFACE_WG provider (geode Terrain) for trail-ruins. */
void installTrailTerrainHeight(uint64_t seed);
void clearTrailTerrainHeight();
}
} // namespace loots
