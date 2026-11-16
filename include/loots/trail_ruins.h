// Trail ruins: jigsaw placement + archaeology processor chain (26.3).
//
// The graph and processors need the original surface height at the rotated
// start-piece center. This module now queries loots' built-in exact
// WORLD_SURFACE_WG terrain directly.
#pragma once

#include <cstdint>

#include "loots/structure_pieces.h"

namespace loots {

/**
 * Trail-ruins suspicious gravel at the structure whose start chunk contains
 * block (x, z). 1.20-26.3. The caller's structure locator must validate the
 * start; this function replays a known start.
 *
 * Returns true when a piece graph was produced. `out.suspicious` holds worldgen
 * gravel coordinates and loot table seeds. Call StructureLoot::evaluate()
 * afterwards and inspect the specific trim kind. Terrain updates after
 * placement and overlapping external features can affect the final state of a
 * real world.
 */
bool trailRuinsLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

} // namespace loots
