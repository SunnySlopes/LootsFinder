#include "loots/fixed_structures.h"

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <vector>

#include "loots/rng.h"
#include "loots/worldgen.h"
#include "pieces_c.h"
#include "processor.hpp"

// Desert pyramid / jungle temple chests (cubiomes) plus desert pyramid / desert
// well archaeology (suspicious sand) for mc >= MC_1_20 (25). Pyramid afterPlace
// matches 1.20.1–26.3; well uses DesertWellFeature until 26.2 and OverlayFeature
// from 26.3.
namespace loots {
namespace {

constexpr int kDirNorth = 0;
constexpr int kDirEast = 1;
constexpr int kDirSouth = 2;
constexpr int kDirWest = 3;

struct Pos3 {
    int x = 0, y = 0, z = 0;
};

bool posLess(const Pos3 &a, const Pos3 &b)
{
    if (a.y != b.y) return a.y < b.y;
    if (a.z != b.z) return a.z < b.z;
    return a.x < b.x;
}

/** BlockPos.asLong(x,y,z) — desert pyramid archaeology loot seed. */
uint64_t blockPosAsLong(int x, int y, int z)
{
    constexpr int kHorizBits = 26;
    constexpr int kYBits = 12;
    constexpr uint64_t kXMask = (1ULL << kHorizBits) - 1ULL;
    constexpr uint64_t kYMask = (1ULL << kYBits) - 1ULL;
    constexpr uint64_t kZMask = (1ULL << kHorizBits) - 1ULL;
    constexpr int kYOff = 0;
    constexpr int kZOff = kYBits;
    constexpr int kXOff = kYBits + kHorizBits;
    uint64_t node = 0;
    node |= (static_cast<uint64_t>(x) & kXMask) << kXOff;
    node |= (static_cast<uint64_t>(y) & kYMask) << kYOff;
    node |= (static_cast<uint64_t>(z) & kZMask) << kZOff;
    return node;
}

/**
 * RandomSource.createThreadLocalInstance(seed).forkPositional().at(x,y,z)
 * — SingleThreadedRandomSource (legacy) in 26.3.
 */
class PositionalRandom {
public:
    static PositionalRandom at(uint64_t worldSeed, int x, int y, int z)
    {
        LegacyRandom base(worldSeed);
        const uint64_t fork = base.nextLong();
        return PositionalRandom(
            static_cast<uint64_t>(jigsaw::mthGetSeed(x, y, z)) ^ fork);
    }
    int nextInt(int bound) { return rng_.nextInt(bound); }
    /** RandomSource.nextInt(minInclusive, maxExclusive). */
    int nextIntRange(int minInclusive, int maxExclusive)
    {
        return minInclusive + rng_.nextInt(maxExclusive - minInclusive);
    }

private:
    explicit PositionalRandom(uint64_t seed) : rng_(seed) {}
    LegacyRandom rng_;
};

void shufflePositions(PositionalRandom &rng, std::vector<Pos3> &v)
{
    for (int n = static_cast<int>(v.size()); n > 1; --n) {
        const int j = rng.nextInt(n);
        std::swap(v[static_cast<size_t>(n - 1)], v[static_cast<size_t>(j)]);
    }
}

int worldX(int localX, int localZ, int orient, int minX, int maxX)
{
    switch (orient) {
    case kDirNorth:
    case kDirSouth: return minX + localX;
    case kDirWest: return maxX - localZ;
    case kDirEast: return minX + localZ;
    default: return minX + localX;
    }
}

int worldZ(int localX, int localZ, int orient, int minZ, int maxZ)
{
    switch (orient) {
    case kDirNorth: return maxZ - localZ;
    case kDirSouth: return minZ + localZ;
    case kDirWest:
    case kDirEast: return minZ + localX;
    default: return minZ + localZ;
    }
}

Pos3 worldPos(int lx, int ly, int lz, int orient, int minX, int minY, int minZ, int maxX,
              int maxZ)
{
    return {worldX(lx, lz, orient, minX, maxX), ly + minY,
            worldZ(lx, lz, orient, minZ, maxZ)};
}

void addSand(std::vector<Pos3> &out, int lx, int ly, int lz, int orient, int minX, int minY,
             int minZ, int maxX, int maxZ)
{
    out.push_back(worldPos(lx, ly, lz, orient, minX, minY, minZ, maxX, maxZ));
}

void addSandBox(std::vector<Pos3> &out, int x0, int y0, int z0, int x1, int y1, int z1, int orient,
                int minX, int minY, int minZ, int maxX, int maxZ)
{
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            for (int z = z0; z <= z1; ++z)
                addSand(out, x, y, z, orient, minX, minY, minZ, maxX, maxZ);
}

int lowestSurfaceInBb(int mc, uint64_t seed, int minX, int minZ, int maxX, int maxZ)
{
    int lowest = 0x7fffffff;
    for (int z = minZ; z <= maxZ; ++z) {
        for (int x = minX; x <= maxX; ++x) {
            const int h = worldSurfaceHeight(mc, seed, x, z);
            if (h < lowest) lowest = h;
        }
    }
    return lowest;
}

void pushSuspicious(StructureLoot &out, int x, int y, int z, const char *table, uint64_t lootSeed)
{
    StructureChest chest{};
    chest.x = x;
    chest.y = y;
    chest.z = z;
    chest.lootTable = table;
    chest.lootSeed = lootSeed;
    chest.container = ContainerKind::suspicious;
    out.suspicious.push_back(std::move(chest));
}

// MC_1_20_6 == 25 (archaeology introduced); MC_26_3 == 35 (well OverlayFeature rewrite).
constexpr int kMcArchaeology = 25;
/** cubiomes MC_26_3: OverlayFeature well + placement sand/volumeMatch. */
constexpr int kMcWellOverlay = 35;

void fillDesertPyramidSuspicious(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    if (mc < kMcArchaeology) return;

    const int minX = x & ~15;
    const int minZ = z & ~15;
    const int maxX = minX + 20;
    const int maxZ = minZ + 20;
    const int chunkX = minX >> 4;
    const int chunkZ = minZ >> 4;

    // Piece construction: GenerationContext largeFeatureSeed → nextInt(4) orientation.
    LegacyRandom structureRng = largeFeatureSeed(mc, seed, chunkX, chunkZ);
    const int orient = structureRng.nextInt(4); // NORTH,EAST,SOUTH,WEST

    // postProcess feature seed: first nextInt(3) is the height offset.
    const uint64_t featureSeed =
        loots_structure_loot_seed(mc, seed, x, z, 1 /* Desert_Pyramid */);
    PostProcessRandom postRng(mc, featureSeed);
    const int heightDown = postRng.nextInt(3);
    const int lowest = lowestSurfaceInBb(mc, seed, minX, minZ, maxX, maxZ);
    const int minY = lowest - heightDown;
    const int maxY = minY + 14;

    // Cellar roomCenter local (16, -4, 13).
    constexpr int cx = 16, cy = -4, cz = 13;
    std::vector<Pos3> candidates;
    candidates.reserve(83);
    addSandBox(candidates, cx - 2, cy + 1, cz - 2, cx + 2, cy + 3, cz + 2, orient, minX, minY,
               minZ, maxX, maxZ);
    addSand(candidates, cx + 3, cy + 1, cz, orient, minX, minY, minZ, maxX, maxZ);
    addSand(candidates, cx + 3, cy + 2, cz, orient, minX, minY, minZ, maxX, maxZ);
    addSand(candidates, cx - 3, cy + 1, cz, orient, minX, minY, minZ, maxX, maxZ);
    addSand(candidates, cx - 3, cy + 2, cz, orient, minX, minY, minZ, maxX, maxZ);
    addSand(candidates, cx, cy + 1, cz + 3, orient, minX, minY, minZ, maxX, maxZ);
    addSand(candidates, cx, cy + 2, cz + 3, orient, minX, minY, minZ, maxX, maxZ);
    addSand(candidates, cx, cy + 1, cz - 3, orient, minX, minY, minZ, maxX, maxZ);
    addSand(candidates, cx, cy + 2, cz - 3, orient, minX, minY, minZ, maxX, maxZ);

    // Collapsed roof: positional at getWorldPos(x0,y0,z0) of the roof box.
    const int roofX0 = cx - 2, roofY = cy + 4, roofZ0 = cz - 2;
    const int roofX1 = cx + 2, roofZ1 = cz + 2;
    const Pos3 roofSeedPos =
        worldPos(roofX0, roofY, roofZ0, orient, minX, minY, minZ, maxX, maxZ);
    PositionalRandom roofRng =
        PositionalRandom::at(seed, roofSeedPos.x, roofSeedPos.y, roofSeedPos.z);
    const int roofLocalX = roofRng.nextIntRange(roofX0, roofX1 + 1);
    const int roofLocalZ = roofRng.nextIntRange(roofZ0, roofZ1 + 1);
    const Pos3 roofPos =
        worldPos(roofLocalX, roofY, roofLocalZ, orient, minX, minY, minZ, maxX, maxZ);

    std::sort(candidates.begin(), candidates.end(), posLess);
    candidates.erase(std::unique(candidates.begin(), candidates.end(),
                                 [](const Pos3 &a, const Pos3 &b) {
                                     return a.x == b.x && a.y == b.y && a.z == b.z;
                                 }),
                     candidates.end());

    const int centerX = minX + (maxX - minX + 1) / 2;
    const int centerY = minY + (maxY - minY + 1) / 2;
    const int centerZ = minZ + (maxZ - minZ + 1) / 2;
    PositionalRandom placeRng = PositionalRandom::at(seed, centerX, centerY, centerZ);
    shufflePositions(placeRng, candidates);
    int toPlace = placeRng.nextIntRange(5, 8);
    if (toPlace > static_cast<int>(candidates.size()))
        toPlace = static_cast<int>(candidates.size());

    pushSuspicious(out, roofPos.x, roofPos.y, roofPos.z, "archaeology/desert_pyramid",
                   blockPosAsLong(roofPos.x, roofPos.y, roofPos.z));
    for (int i = 0; i < toPlace; ++i) {
        const Pos3 &p = candidates[static_cast<size_t>(i)];
        pushSuspicious(out, p.x, p.y, p.z, "archaeology/desert_pyramid",
                       blockPosAsLong(p.x, p.y, p.z));
    }
}

/**
 * Pre-26.3 DesertWellFeature water-centre offsets: center, east, south, west, north.
 * 26.3 RandomlySelectedPlacement xz: center, west, south, east, north.
 */
constexpr int kWellDxPre263[5] = {0, 1, 0, -1, 0};
constexpr int kWellDzPre263[5] = {0, 0, 1, 0, -1};
constexpr int kWellDx263[5] = {0, -1, 0, 1, 0};
constexpr int kWellDz263[5] = {0, 0, 1, 0, -1};

/** TemplateFeature: WeightedList(1) + Util.getRandom(4 rotations) + palette(1). */
void consumeTemplateFeatureRng(PostProcessRandom &rng)
{
    (void)rng.nextInt(1);
    (void)rng.nextInt(4);
    (void)rng.nextInt(1);
}

void fillDesertWellSuspicious(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    if (mc < kMcArchaeology) return;

    // Replay WorldgenRandom after rarity + in_square (cubiomes salt 40002).
    // Must use PostProcessRandom (Java nextInt via next(bits)), not raw
    // XoroshiroRandomSource nextInt.
    const int chunkMinX = x & ~15;
    const int chunkMinZ = z & ~15;
    const uint64_t population =
        decorationSeedValue(seed, chunkMinX, chunkMinZ, usesXoroshiro(mc));
    constexpr int kWellSalt = 40002;
    PostProcessRandom rng(mc, population + static_cast<uint64_t>(kWellSalt));
    (void)rng.nextFloat();
    (void)rng.nextInt(16);
    (void)rng.nextInt(16);

    // HEIGHTMAP (+ Offset.DOWN in 26.3) lands on top occupied sand at (x,z).
    const int waterY = worldSurfaceHeight(mc, seed, x, z) - 1;

    if (mc < kMcWellOverlay) {
        // 1.20–26.2 DesertWellFeature: two Util.getRandom(waterPositions).below(1/2);
        // lootSeed = BlockPos.asLong.
        const int i0 = rng.nextInt(5);
        const int i1 = rng.nextInt(5);
        const int sx0 = x + kWellDxPre263[i0];
        const int sz0 = z + kWellDzPre263[i0];
        const int sx1 = x + kWellDxPre263[i1];
        const int sz1 = z + kWellDzPre263[i1];
        const int y0 = waterY - 1;
        const int y1 = waterY - 2;
        pushSuspicious(out, sx0, y0, sz0, "archaeology/desert_well",
                       blockPosAsLong(sx0, y0, sz0));
        pushSuspicious(out, sx1, y1, sz1, "archaeology/desert_well",
                       blockPosAsLong(sx1, y1, sz1));
        return;
    }

    // 26.3 OverlayFeature: well TemplateFeature, then two suspicious_sand
    // PlacedFeatures (RandomlySelectedPlacement then TemplateFeature each).
    // lootSeed = AppendLoot → LegacyRandom(Mth.getSeed).nextLong().
    consumeTemplateFeatureRng(rng);
    const int i0 = rng.nextInt(5);
    consumeTemplateFeatureRng(rng);
    const int i1 = rng.nextInt(5);
    const int sx0 = x + kWellDx263[i0];
    const int sz0 = z + kWellDz263[i0];
    const int sx1 = x + kWellDx263[i1];
    const int sz1 = z + kWellDz263[i1];
    const int y0 = waterY - 1;
    const int y1 = waterY - 2;
    pushSuspicious(out, sx0, y0, sz0, "archaeology/desert_well",
                   jigsaw::legacySeedAt({sx0, y0, sz0}));
    pushSuspicious(out, sx1, y1, sz1, "archaeology/desert_well",
                   jigsaw::legacySeedAt({sx1, y1, sz1}));
}

void fillChests(const loots_piece_result &in, StructureLoot &out, bool onlyJungleChests)
{
    out.exists = in.exists != 0;
    out.pieces = in.pieces;
    out.chests.clear();
    out.chests.reserve(static_cast<size_t>(in.chest_count));
    for (int i = 0; i < in.chest_count; i++) {
        const char *table = in.chests[i].table;
        if (onlyJungleChests && std::strcmp(table, "chests/jungle_temple") != 0) continue;
        StructureChest chest{};
        chest.x = in.chests[i].x;
        chest.z = in.chests[i].z;
        chest.y = 0;
        chest.templateName = in.chests[i].template_name;
        chest.lootTable = table;
        chest.lootSeed = in.chests[i].seed;
        out.chests.push_back(std::move(chest));
    }
}

loots_piece_result g_pieces;

} // namespace

bool singlePieceAboveSea(int mc, uint64_t seed, int x, int z, int sizeX, int sizeZ)
{
    // Mirror Structure.getCornerHeights + getLowestY < seaLevel reject.
    // worldSurfaceHeight is first-free (occupied+1); first-occupied = that - 1.
    constexpr int kSeaLevel = 63;
    const int corners[4][2] = {{0, 0}, {0, sizeZ}, {sizeX, 0}, {sizeX, sizeZ}};
    int lowest = 0x7fffffff;
    for (const auto &c : corners) {
        const int occupied = worldSurfaceHeight(mc, seed, x + c[0], z + c[1]) - 1;
        if (occupied < lowest) lowest = occupied;
    }
    return lowest >= kSeaLevel;
}

bool desertWellCanPlace(int mc, uint64_t seed, int x, int z, int *surfaceY)
{
    // HEIGHTMAP (first-free) + Offset.DOWN (26.3) / Feature scan-to-solid (≤26.2)
    // both land on the top occupied block at (x,z).
    const int surfaceFree = worldSurfaceHeight(mc, seed, x, z);
    const int oy = surfaceFree - 1;
    if (surfaceY) *surfaceY = oy;
    // DesertWellFeature needs room to scan; also reject void/min-Y nonsense.
    if (oy <= -62) return false;
    // Sand proxy: top occupied must be solid (real check is Blocks.SAND).
    if (!worldSolid(mc, seed, x, oy, z)) return false;

    const bool requireBoth = mc >= kMcWellOverlay;
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            const bool a = worldSolid(mc, seed, x + dx, oy - 1, z + dz);
            const bool b = worldSolid(mc, seed, x + dx, oy - 2, z + dz);
            if (requireBoth) {
                // 26.3 volumeMatch(not air) over [−2..2]×[−2..−1]×[−2..2].
                if (!a || !b) return false;
            } else {
                // ≤26.2 DesertWellFeature: reject column if both below are empty.
                if (!a && !b) return false;
            }
        }
    }
    return true;
}

bool desertPyramidLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.seed = seed;
    out.kind = "desert_pyramid";
    const int minX = x & ~15, minZ = z & ~15;
    // SinglePieceStructure: width=21, depth=21; reject when any corner is under sea.
    if (mc >= 22 && !singlePieceAboveSea(mc, seed, minX, minZ, 21, 21)) {
        out.exists = false;
        return false;
    }
    if (!loots_desert_pieces(mc, seed, x, z, &g_pieces)) return false;
    fillChests(g_pieces, out, false);
    out.minX = minX;
    out.minZ = minZ;
    out.maxX = minX + 20;
    out.maxZ = minZ + 20;
    fillDesertPyramidSuspicious(mc, seed, x, z, out);
    return out.exists;
}

bool desertWellLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.seed = seed;
    out.kind = "desert_well";
    int surfaceY = 0;
    if (!desertWellCanPlace(mc, seed, x, z, &surfaceY)) {
        out.exists = false;
        return false;
    }
    out.exists = true;
    out.pieces = 1;
    // Footprint + sand-surface Y used by archaeology offsets.
    out.minX = x - 2;
    out.minZ = z - 2;
    out.maxX = x + 2;
    out.maxZ = z + 2;
    out.minY = surfaceY;
    out.maxY = surfaceY;
    fillDesertWellSuspicious(mc, seed, x, z, out);
    return true;
}

bool jungleTempleLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.kind = "jungle_temple";
    const int minX = x & ~15, minZ = z & ~15;
    // SinglePieceStructure: width=12, depth=15.
    if (mc >= 22 && !singlePieceAboveSea(mc, seed, minX, minZ, 12, 15)) {
        out.exists = false;
        return false;
    }
    if (!loots_jungle_pieces(mc, seed, x, z, &g_pieces)) return false;
    fillChests(g_pieces, out, true);
    out.minX = minX;
    out.minZ = minZ;
    out.maxX = minX + 14;
    out.maxZ = minZ + 14;
    return out.exists;
}

} // namespace loots
