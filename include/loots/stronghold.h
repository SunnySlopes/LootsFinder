#pragma once

#include <cstdint>

namespace loots {

/**
 * End-portal / room summary for one stronghold.
 *
 * The layout is computed by loots' own 1.18+ StrongholdPieces port. X/Z are
 * exact block coordinates; the portal-room Y is the exact vanilla world Y
 * (moveBelowSeaLevel), not a terrain-heightmap estimate.
 */
struct StrongholdPortal {
    int x = 0;
    int z = 0;
    int chunkX = 0;
    int chunkZ = 0;
    bool ok = false;
    bool truncated = false;
    int pieces = 0;

    int frameMinX = 0;
    int frameMinZ = 0;
    int frameMaxX = 0;
    int frameMaxZ = 0;
    int portalX = 0;
    int portalZ = 0;
    int portalY = 0;
    int roomMinX = 0;
    int roomMinZ = 0;
    int roomMaxX = 0;
    int roomMaxZ = 0;
    int roomMinY = 0;
    int roomMaxY = 0;
    bool yExact = false;
    int eyes = -1;
    unsigned eyeMask = 0;

    int footprintMinX = 0;
    int footprintMinZ = 0;
    int footprintMaxX = 0;
    int footprintMaxZ = 0;
    int footprintArea = 0;
};

/** 1.18+ stronghold support. */
bool strongholdVersionSupported(int mc);

/**
 * Compute one stronghold from its start chunk. Returns true when a layout was
 * produced; inspect ok for a portal room. x/z are the start block position.
 */
bool strongholdPortalAt(int mc, uint64_t seed, int chunkX, int chunkZ,
                        StrongholdPortal &out);

/** Portal of the stronghold nearest to the origin. */
bool strongholdFirst(int mc, uint64_t seed, StrongholdPortal &out);

} // namespace loots
