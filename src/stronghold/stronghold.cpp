#include "loots/stronghold.h"

#include "sh1_18.h"

// The implementation is compiled with a private symbol prefix so it can live
// in the loots library while another stronghold copy remains available for
// non-map callers.
extern "C" {
int loots_sh118_compute(Sh118Stronghold *out, Sh118Piece *pieces, int maxPieces,
                        int mc, uint64_t seed, int chunkX, int chunkZ);
int loots_sh118_locateFirst(Sh118Stronghold *out, int mc, uint64_t seed, void *gen);
int loots_sh118_versionSupported(int mc);
}

namespace {

loots::StrongholdPortal fromStronghold(const Sh118Stronghold &p)
{
    loots::StrongholdPortal out;
    out.pieces = p.pieces;
    out.truncated = p.truncated != 0;
    out.ok = p.ok != 0;
    out.x = p.startX;
    out.z = p.startZ;
    out.chunkX = p.chunkX;
    out.chunkZ = p.chunkZ;
    if (!out.ok) return out;

    out.frameMinX = p.frameMinX;
    out.frameMinZ = p.frameMinZ;
    out.frameMaxX = p.frameMaxX;
    out.frameMaxZ = p.frameMaxZ;
    out.portalX = p.portalX;
    out.portalZ = p.portalZ;
    out.eyes = p.eyes;
    out.eyeMask = p.eyeMask;
    out.roomMinX = p.roomMinX;
    out.roomMinZ = p.roomMinZ;
    out.roomMaxX = p.roomMaxX;
    out.roomMaxZ = p.roomMaxZ;
    out.roomMinY = p.roomNominalMinY;
    out.roomMaxY = p.roomNominalMaxY;
    out.portalY = p.roomNominalMinY + 1;
    out.yExact = p.dyExact != 0;
    out.footprintMinX = p.footprintMinX;
    out.footprintMinZ = p.footprintMinZ;
    out.footprintMaxX = p.footprintMaxX;
    out.footprintMaxZ = p.footprintMaxZ;
    out.footprintArea = p.footprintArea;
    return out;
}

} // namespace

namespace loots {

bool strongholdVersionSupported(int mc)
{
    return loots_sh118_versionSupported(mc) != 0;
}

bool strongholdPortalAt(int mc, uint64_t seed, int chunkX, int chunkZ,
                        StrongholdPortal &out)
{
    out = StrongholdPortal{};
    out.chunkX = chunkX;
    out.chunkZ = chunkZ;

    Sh118Stronghold p{};
    const int generated = loots_sh118_compute(
        &p, nullptr, 0, mc, seed, chunkX, chunkZ);
    if (generated <= 0) return false;
    out = fromStronghold(p);
    return true;
}

bool strongholdFirst(int mc, uint64_t seed, StrongholdPortal &out)
{
    out = StrongholdPortal{};
    Sh118Stronghold p{};
    if (!loots_sh118_locateFirst(&p, mc, seed, nullptr)) return false;
    out = fromStronghold(p);
    return true;
}

} // namespace loots
