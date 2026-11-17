// Terrain height queries used by JigsawPlacement.
//
// The placer only needs the exact 26.3 WORLD_SURFACE_WG height for
// `project_start_to_heightmap` and for TERRAIN_MATCHING pieces.  loots already
// owns an exact implementation in worldgen.cpp; this thin wrapper keeps the
// jigsaw engine independent of the rest of loots.
#pragma once

#include <cstdint>

namespace loots {
namespace jigsaw {

/** ChunkGenerator.getFirstFreeHeight(..., WORLD_SURFACE_WG, ...) on 26.3. */
int worldSurfaceHeight(int mc, uint64_t seed, int x, int z);

/** Inclusive world height limits used by the max-distance AABB. */
constexpr int kWorldMinY = -64;
constexpr int kWorldMaxY = 319;

} // namespace jigsaw
} // namespace loots
