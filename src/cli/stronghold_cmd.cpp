#include "loots/stronghold.h"

#include <cstdint>
#include <cstdio>

namespace loots {
namespace cli {

int cmdStronghold(int mc, uint64_t seed)
{
    if (!strongholdVersionSupported(mc)) {
        std::fputs("stronghold requires 1.18+\n", stderr);
        return 2;
    }
    StrongholdPortal sh;
    if (!strongholdFirst(mc, seed, sh)) {
        std::fputs("stronghold not found\n", stderr);
        return 1;
    }
    std::printf("{\n");
    std::printf("  \"kind\": \"stronghold\", \"mc\": %d, \"seed\": %lld,\n", mc, (long long) seed);
    std::printf("  \"start\": {\"x\": %d, \"z\": %d, \"chunkX\": %d, \"chunkZ\": %d},\n",
                sh.x, sh.z, sh.chunkX, sh.chunkZ);
    std::printf("  \"portal\": {\"x\": %d, \"z\": %d, \"minX\": %d, \"maxX\": %d, "
                "\"minZ\": %d, \"maxZ\": %d},\n",
                sh.portalX, sh.portalZ, sh.frameMinX, sh.frameMaxX, sh.frameMinZ, sh.frameMaxZ);
    std::printf("  \"room\": {\"minX\": %d, \"maxX\": %d, \"minZ\": %d, \"maxZ\": %d},\n",
                sh.roomMinX, sh.roomMaxX, sh.roomMinZ, sh.roomMaxZ);
    std::printf("  \"eyeFrames\": [");
    int first = 1;
    const int fx = sh.portalX;
    const int fz = sh.portalZ;
    for (int dx = -2; dx <= 2; dx++) {
        for (int dz = -2; dz <= 2; dz++) {
            if (dx == 0 && dz == 0) continue;
            if ((dx == -2 || dx == 2) && (dz == -2 || dz == 2)) continue;
            if (dx > -2 && dx < 2 && dz > -2 && dz < 2) continue;
            std::printf("%s{\"x\": %d, \"z\": %d}", first ? "" : ", ", fx + dx, fz + dz);
            first = 0;
        }
    }
    std::printf("],\n");
    std::printf("  \"pieces\": %d, \"ok\": %s\n", sh.pieces, sh.ok ? "true" : "false");
    std::printf("}\n");
    return 0;
}

} // namespace cli
} // namespace loots
