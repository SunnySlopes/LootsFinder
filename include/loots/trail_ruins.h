// Trail ruins: shared exact JigsawPlacement plus the 26.3 archaeology
// processor chain.
//
// The piece graph uses the exact WORLD_SURFACE_WG height and start_height=-15.
// The RuleProcessor / CappedProcessor / AppendLoot replay is structure-specific
// and lives in the adapter.
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
 * afterwards and inspect the specific trim kind.
 */
bool trailRuinsLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

} // namespace loots
