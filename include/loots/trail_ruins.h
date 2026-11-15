// Trail ruins: jigsaw placement + archaeology processor chain (26.3).
//
// STATUS: best effort.  The piece graph is a direct port of
// JigsawPlacement.Placer for the all-rigid trail-ruins pools, and the
// suspicious-gravel pass replays RuleProcessor / CappedProcessor.  It is NOT
// verified against an in-game oracle, and one input is still approximate:
//
//   * start_height is absolute -15 and project_start_to_heightmap is
//     WORLD_SURFACE_WG, so the centre piece's Y (and therefore every loot
//     seed, which is seeded by the world block position) needs the exact
//     vanilla heightmap.  The library only has cubiomes' 1:4 approximation,
//     so by default `trailRuinsStatus().complete` is false.
//
// A host that has an exact WORLD_SURFACE_WG height can install it with
// trailRuinsSetSurfaceHeightProvider(); the provider must return
// ChunkGenerator.getFirstFreeHeight(x, z, WORLD_SURFACE_WG) in block Y.
//
// Trail ruins contain no chests.  Everything interesting is suspicious gravel,
// which is written to StructureLoot::suspicious.
#pragma once

#include <cstdint>

#include "loots/structure_pieces.h"

namespace loots {

/** Exact WORLD_SURFACE_WG height provider: block Y of the first free block. */
using TrailSurfaceHeightFn = int (*)(int mc, uint64_t seed, int x, int z);

/** Install (or clear with nullptr) the exact height provider. */
void trailRuinsSetSurfaceHeightProvider(TrailSurfaceHeightFn fn);

struct TrailRuinsStatus {
    /** False until the height input and an in-game oracle are both available. */
    bool complete = false;
    /** True when the host installed an exact height provider. */
    bool exactSurfaceHeight = false;
    /** True when the generated 26.3 template table is compiled in. */
    bool templateData = false;
    /** Trail ruins have no chests in 26.3; always false. */
    bool hasChests = false;
    /** Human-readable list of what is still missing. */
    const char *missing = "";
};

TrailRuinsStatus trailRuinsStatus();

/**
 * Trail-ruins suspicious gravel at the structure whose start chunk contains
 * block (x, z).  1.20+ (the 26.3 template layout is shared by those versions).
 * `x` and `z` are the structure start
 * position as returned by the structure finder (the chunk minimum is used
 * internally).
 *
 * Returns true when a piece graph was produced.  `out.suspicious` holds one
 * entry per suspicious gravel block (position, template name, archaeology
 * loot table and LootTableSeed).  Call StructureLoot::evaluate() afterwards.
 */
bool trailRuinsLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

} // namespace loots
