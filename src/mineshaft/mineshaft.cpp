// Exact abandoned-mineshaft replay for loots.
//
// The piece graph and vertical placement are ported from `tools/geode`'s
// mineshaft_gen (itself a 1:1 port of MineshaftPieces/MineshaftStructure).
// Cubiomes' getMineshaftLoot() models the same chest RNG but ignores the real
// block-state conditions, the corridor carving and the exact vertical shift,
// which is why its chest list does not match the 26.3 server.
//
// This file replays `MineshaftPieces.MineShaftCorridor.postProcess` (and the
// block writes of the room/crossing/stairs pieces, because they are visible to
// that replay) against a block-state overlay on top of loots' terrain queries:
//
//   * isInInvalidLocation(): deep_dark centre + liquid scan on the six faces;
//   * generateBox / generateMaybeBox / maybeGenerateBlock RNG consumption;
//   * placeSupport / isSupportingBox / maybePlaceCobWeb / isInterior;
//   * setPlanksBlock, floor planks, wooden pillars / iron chains;
//   * createChest(): air above a floor block, nextBoolean + nextLong;
//   * the rails loop: floor isSolidRender(), probability only affects the block.
//
// Every number here was checked against `guardian-mojmap-26.3`:
//   .../structures/MineshaftPieces.java and .../structure/StructurePiece.java
//
// Known limitation (verified with the 26.3 server oracle): mineshaft pieces read
// block states that step-1..3 *features* of previously decorated neighbouring
// chunks may already have written into this chunk (monster rooms, lava lakes,
// amethyst geodes, ...). Vanilla's own result therefore depends on chunk
// generation order; this replay assumes the ascending ChunkPos.pack order the
// oracle uses and does not replay those feature writes.
#include "loots/structure_pieces.h"

#include <algorithm>
#include <cstring>
#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

#include "block_view.h"
#include "loots/rng.h"
#include "loots/worldgen.h"
#include "mineshaft_gen.h"
#include "pieces_c.h"

namespace loots {
namespace {

constexpr int kSeaLevel = 63;
constexpr int kMinY = -64;
/* Mineshafts are placed in GenerationStep 3 (underground structures) with
 * decorator index 1; verified against the 26.3 server oracle. The mesa variant
 * is a separate structure in the same step, right after `mineshaft`, so its
 * post-process RNG index is 2. */
constexpr int kLootStep = 3;
constexpr int kLootIndex = 1;
constexpr int kLootIndexMesa = 2;

using mineshaft::BlockState;
using mineshaft::BlockView;
using mineshaft::kAir;
using mineshaft::kChain;
using mineshaft::kCobweb;
using mineshaft::kFence;
using mineshaft::kPlanks;
using mineshaft::kRail;
using mineshaft::kSpawner;
using mineshaft::kTorch;
using mineshaft::kWood;

int msVersionForMc(int mc)
{
    if (mc >= 35) return MS_VERSION_26_3;
    if (mc >= 34) return MS_VERSION_26_2;
    if (mc >= 33) return MS_VERSION_26_1_2;
    // The MineshaftPieces / MineshaftStructure logic is identical from 1.18.2
    // through 1.21.10 (each jar was decompiled and compared with 1.21.11; the
    // only diffs are decompiler inlining). MsVersion only records the source
    // baseline and supplies the same minY/seaLevel defaults for all 1.18+.
    if (mc >= 22) return MS_VERSION_1_21_11;
    return -1;
}

bool intersectsChunk(const MsPiece &piece, int chunkX, int chunkZ)
{
    const int minX = chunkX * 16;
    const int minZ = chunkZ * 16;
    return piece.bb.minX <= minX + 15 && piece.bb.maxX >= minX &&
           piece.bb.minZ <= minZ + 15 && piece.bb.maxZ >= minZ &&
           piece.bb.minY <= 320 && piece.bb.maxY >= -63;
}

/** ChunkPos.pack(x, z) -- the oracle loads chunk columns in this sorted order. */
int64_t chunkPack(int chunkX, int chunkZ)
{
    return static_cast<int64_t>((static_cast<uint64_t>(static_cast<uint32_t>(chunkZ)) << 32) |
                                static_cast<uint64_t>(static_cast<uint32_t>(chunkX)));
}

/**
 * StructurePiece.getWorldPos() for this piece.
 *
 * MineShaftRoom and MineShaftCrossing never call setOrientation(), so their
 * local coordinates are already world coordinates (getWorldPos returns them
 * unchanged). Corridors and stairs rotate.
 */
void localToWorld(const MsPiece &piece, int lx, int ly, int lz,
                  int &wx, int &wy, int &wz)
{
    if (piece.type != MS_PT_CORRIDOR && piece.type != MS_PT_STAIRS) {
        wx = lx;
        wy = ly;
        wz = lz;
        return;
    }
    switch (piece.orient) {
    case MS_DIR_NORTH:
    default:
        wx = piece.bb.minX + lx;
        wz = piece.bb.maxZ - lz;
        break;
    case MS_DIR_SOUTH:
        wx = piece.bb.minX + lx;
        wz = piece.bb.minZ + lz;
        break;
    case MS_DIR_WEST:
        wx = piece.bb.maxX - lz;
        wz = piece.bb.minZ + lx;
        break;
    case MS_DIR_EAST:
        wx = piece.bb.minX + lz;
        wz = piece.bb.minZ + lx;
        break;
    }
    wy = piece.bb.minY + ly;
}

/**
 * `pos.getY() < level.getHeight(OCEAN_FLOOR_WG, x, z)` for the block above
 * local (x, y, z). The heightmap is not refreshed by structure writes, so it
 * only depends on terrain solidity.
 */
bool isInterior(BlockView &view, const MsPiece &piece, int x, int y, int z)
{
    int wx, wy, wz;
    localToWorld(piece, x, y + 1, z, wx, wy, wz);
    if (!view.inside(wx, wy, wz)) return false;
    return view.solidAbove(wx, wy, wz);
}

/** MineShaftPiece.isInInvalidLocation(). */
bool isInInvalidLocation(BlockView &view, const MsPiece &piece, int mc, uint64_t seed)
{
    const int x0 = std::max(piece.bb.minX - 1, view.chunkMinX());
    const int y0 = std::max(piece.bb.minY - 1, -63);
    const int z0 = std::max(piece.bb.minZ - 1, view.chunkMinZ());
    const int x1 = std::min(piece.bb.maxX + 1, view.chunkMinX() + 15);
    const int y1 = std::min(piece.bb.maxY + 1, 320);
    const int z1 = std::min(piece.bb.maxZ + 1, view.chunkMinZ() + 15);
    if (x0 > x1 || y0 > y1 || z0 > z1) return false;

    // Java: (x0 + x1) / 2 truncates toward zero, like C++ int division.
    const int centerX = (x0 + x1) / 2;
    const int centerY = (y0 + y1) / 2;
    const int centerZ = (z0 + z1) / 2;
    if (worldMineshaftBlockingBiome(mc, seed, centerX, centerY, centerZ)) return true;

    for (int x = x0; x <= x1; x++) {
        for (int z = z0; z <= z1; z++) {
            if (view.raw(x, y0, z).isLiquid()) return true;
            if (view.raw(x, y1, z).isLiquid()) return true;
        }
    }
    for (int x = x0; x <= x1; x++) {
        for (int y = y0; y <= y1; y++) {
            if (view.raw(x, y, z0).isLiquid()) return true;
            if (view.raw(x, y, z1).isLiquid()) return true;
        }
    }
    for (int z = z0; z <= z1; z++) {
        for (int y = y0; y <= y1; y++) {
            if (view.raw(x0, y, z).isLiquid()) return true;
            if (view.raw(x1, y, z).isLiquid()) return true;
        }
    }
    return false;
}

/** StructurePiece.generateBox() with local coordinates from `piece`. */
void generateBox(BlockView &view, const MsPiece &piece,
                 int x0, int y0, int z0, int x1, int y1, int z1,
                 BlockState edge, BlockState fill, bool skipAir)
{
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            for (int z = z0; z <= z1; z++) {
                int wx, wy, wz;
                localToWorld(piece, x, y, z, wx, wy, wz);
                if (skipAir && view.get(wx, wy, wz).isAir()) continue;
                const bool isEdge = y == y0 || y == y1 || x == x0 || x == x1 ||
                                    z == z0 || z == z1;
                view.place(wx, wy, wz, isEdge ? edge : fill);
            }
        }
    }
}

/** StructurePiece.generateBox() over an already-world-space box (room part). */
void generateWorldBox(BlockView &view, const MsPiece &piece,
                      const MsBox &box, BlockState edge, BlockState fill, bool skipAir)
{
    generateBox(view, piece, box.minX, box.minY, box.minZ,
                box.maxX, box.maxY, box.maxZ, edge, fill, skipAir);
}

/** StructurePiece.generateMaybeBox(): one nextFloat() per box cell. */
void generateMaybeBox(BlockView &view, const MsPiece &piece, PostProcessRandom &rng,
                      float probability,
                      int x0, int y0, int z0, int x1, int y1, int z1,
                      BlockState edge, BlockState fill, bool skipAir, bool hasToBeInside)
{
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            for (int z = z0; z <= z1; z++) {
                // `!(random.nextFloat() > probability)`: the draw is unconditional.
                const float roll = rng.nextFloat();
                if (roll > probability) continue;
                int wx, wy, wz;
                localToWorld(piece, x, y, z, wx, wy, wz);
                if (skipAir && view.get(wx, wy, wz).isAir()) continue;
                if (hasToBeInside && !isInterior(view, piece, x, y, z)) continue;
                const bool isEdge = y == y0 || y == y1 || x == x0 || x == x1 ||
                                    z == z0 || z == z1;
                view.place(wx, wy, wz, isEdge ? edge : fill);
            }
        }
    }
}

/** StructurePiece.maybeGenerateBlock(): the draw is unconditional. */
void maybeGenerateBlock(BlockView &view, const MsPiece &piece, PostProcessRandom &rng,
                        float probability, int x, int y, int z, BlockState state)
{
    if (rng.nextFloat() < probability) {
        int wx, wy, wz;
        localToWorld(piece, x, y, z, wx, wy, wz);
        view.place(wx, wy, wz, state);
    }
}

/** StructurePiece.generateUpperHalfSphere() (NORMAL +-0.5 sphere, float math). */
void generateUpperHalfSphere(BlockView &view, const MsPiece &piece,
                             int x0, int y0, int z0, int x1, int y1, int z1,
                             BlockState fill, bool skipAir)
{
    const float diagX = static_cast<float>(x1 - x0 + 1);
    const float diagY = static_cast<float>(y1 - y0 + 1);
    const float diagZ = static_cast<float>(z1 - z0 + 1);
    const float cx = static_cast<float>(x0) + diagX / 2.0f;
    const float cz = static_cast<float>(z0) + diagZ / 2.0f;

    for (int y = y0; y <= y1; y++) {
        const float normalizedY = static_cast<float>(y - y0) / diagY;
        for (int x = x0; x <= x1; x++) {
            const float normalizedX = (static_cast<float>(x) - cx) / (diagX * 0.5f);
            for (int z = z0; z <= z1; z++) {
                const float normalizedZ = (static_cast<float>(z) - cz) / (diagZ * 0.5f);
                int wx, wy, wz;
                localToWorld(piece, x, y, z, wx, wy, wz);
                if (skipAir && view.get(wx, wy, wz).isAir()) continue;
                const float distance = normalizedX * normalizedX +
                                       normalizedY * normalizedY +
                                       normalizedZ * normalizedZ;
                if (distance <= 1.05f) view.place(wx, wy, wz, fill);
            }
        }
    }
}

/** MineShaftPiece.setPlanksBlock(): only inside the chunk and not face-sturdy. */
void setPlanksBlock(BlockView &view, const MsPiece &piece, BlockState planks,
                    int x, int y, int z)
{
    if (!isInterior(view, piece, x, y, z)) return;
    int wx, wy, wz;
    localToWorld(piece, x, y, z, wx, wy, wz);
    if (!view.raw(wx, wy, wz).isFaceSturdy()) view.set(wx, wy, wz, planks);
}

/** MineShaftCorridor.placeSupport(). */
void placeSupport(BlockView &view, const MsPiece &piece, PostProcessRandom &rng, int z)
{
    // isSupportingBox(level, chunkBB, x0=0, x1=2, y1=2, z0=z)
    bool supporting = true;
    for (int x = 0; x <= 2; x++) {
        int wx, wy, wz;
        localToWorld(piece, x, 3, z, wx, wy, wz);
        if (view.get(wx, wy, wz).isAir()) {
            supporting = false;
            break;
        }
    }
    if (!supporting) return;

    generateBox(view, piece, 0, 0, z, 0, 1, z, kFence, kAir, false);
    generateBox(view, piece, 2, 0, z, 2, 1, z, kFence, kAir, false);
    if (rng.nextInt(4) == 0) {
        generateBox(view, piece, 0, 2, z, 0, 2, z, kPlanks, kAir, false);
        generateBox(view, piece, 2, 2, z, 2, 2, z, kPlanks, kAir, false);
    } else {
        generateBox(view, piece, 0, 2, z, 2, 2, z, kPlanks, kAir, false);
        maybeGenerateBlock(view, piece, rng, 0.05f, 1, 2, z - 1, kTorch);
        maybeGenerateBlock(view, piece, rng, 0.05f, 1, 2, z + 1, kTorch);
    }
}

/** StructurePiece.hasSturdyNeighbours(Direction.values() order). */
bool hasSturdyNeighbours(BlockView &view, const MsPiece &piece,
                         int x, int y, int z, int required)
{
    static constexpr int kDirs[6][3] = {
        {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0},
    };
    int wx, wy, wz;
    localToWorld(piece, x, y, z, wx, wy, wz);
    int sturdy = 0;
    for (const auto &dir : kDirs) {
        const int nx = wx + dir[0];
        const int ny = wy + dir[1];
        const int nz = wz + dir[2];
        if (view.inside(nx, ny, nz) && view.raw(nx, ny, nz).isFaceSturdy()) {
            if (++sturdy >= required) return true;
        }
    }
    return false;
}

/** MineShaftCorridor.maybePlaceCobWeb(). */
void maybePlaceCobWeb(BlockView &view, const MsPiece &piece, PostProcessRandom &rng,
                      float probability, int x, int y, int z)
{
    const bool interior = isInterior(view, piece, x, y, z);
    if (interior && rng.nextFloat() < probability &&
        hasSturdyNeighbours(view, piece, x, y, z, 2)) {
        int wx, wy, wz;
        localToWorld(piece, x, y, z, wx, wy, wz);
        view.place(wx, wy, wz, kCobweb);
    }
}

void replayRoomPiece(BlockView &view, const MsPiece &piece,
                     const MsPiece *entrances, int entranceCount)
{
    if (isInInvalidLocation(view, piece, view.mc(), view.seed())) return;
    generateWorldBox(view, piece, MsBox{piece.bb.minX + 1, piece.bb.minY + 1, piece.bb.minZ,
                                        piece.bb.maxX, std::min(piece.bb.minY + 3, piece.bb.maxY),
                                        piece.bb.maxZ},
                     kAir, kAir, false);
    for (int i = 0; i < entranceCount; i++) {
        generateWorldBox(view, piece, MsBox{entrances[i].bb.minX, entrances[i].bb.maxY - 2,
                                            entrances[i].bb.minZ, entrances[i].bb.maxX,
                                            entrances[i].bb.maxY, entrances[i].bb.maxZ},
                         kAir, kAir, false);
    }
    generateUpperHalfSphere(view, piece, piece.bb.minX, piece.bb.minY + 4, piece.bb.minZ,
                            piece.bb.maxX, piece.bb.maxY, piece.bb.maxZ, kAir, false);
}

void placeSupportPillar(BlockView &view, const MsPiece &piece, int x, int y0, int z, int y1)
{
    int wx, wy, wz;
    localToWorld(piece, x, y1 + 1, z, wx, wy, wz);
    if (!view.get(wx, wy, wz).isAir()) {
        generateBox(view, piece, x, y0, z, x, y1, z, kPlanks, kAir, false);
    }
}

void replayCrossing(BlockView &view, const MsPiece &piece)
{
    if (isInInvalidLocation(view, piece, view.mc(), view.seed())) return;
    const MsBox &b = piece.bb;
    const bool twoFloored = (b.maxY - b.minY + 1) > 3;
    if (twoFloored) {
        generateBox(view, piece, b.minX + 1, b.minY, b.minZ, b.maxX - 1, b.minY + 2, b.maxZ,
                    kAir, kAir, false);
        generateBox(view, piece, b.minX, b.minY, b.minZ + 1, b.maxX, b.minY + 2, b.maxZ - 1,
                    kAir, kAir, false);
        generateBox(view, piece, b.minX + 1, b.maxY - 2, b.minZ, b.maxX - 1, b.maxY, b.maxZ,
                    kAir, kAir, false);
        generateBox(view, piece, b.minX, b.maxY - 2, b.minZ + 1, b.maxX, b.maxY, b.maxZ - 1,
                    kAir, kAir, false);
        generateBox(view, piece, b.minX + 1, b.minY + 3, b.minZ + 1, b.maxX - 1, b.minY + 3,
                    b.maxZ - 1, kAir, kAir, false);
    } else {
        generateBox(view, piece, b.minX + 1, b.minY, b.minZ, b.maxX - 1, b.maxY, b.maxZ,
                    kAir, kAir, false);
        generateBox(view, piece, b.minX, b.minY, b.minZ + 1, b.maxX, b.maxY, b.maxZ - 1,
                    kAir, kAir, false);
    }
    placeSupportPillar(view, piece, b.minX + 1, b.minY, b.minZ + 1, b.maxY);
    placeSupportPillar(view, piece, b.minX + 1, b.minY, b.maxZ - 1, b.maxY);
    placeSupportPillar(view, piece, b.maxX - 1, b.minY, b.minZ + 1, b.maxY);
    placeSupportPillar(view, piece, b.maxX - 1, b.minY, b.maxZ - 1, b.maxY);
    for (int x = b.minX; x <= b.maxX; x++)
        for (int z = b.minZ; z <= b.maxZ; z++)
            setPlanksBlock(view, piece, kPlanks, x, b.minY - 1, z);
}

void replayStairs(BlockView &view, const MsPiece &piece)
{
    if (isInInvalidLocation(view, piece, view.mc(), view.seed())) return;
    generateBox(view, piece, 0, 5, 0, 2, 7, 1, kAir, kAir, false);
    generateBox(view, piece, 0, 0, 7, 2, 2, 8, kAir, kAir, false);
    for (int i = 0; i < 5; i++) {
        const int y0 = 5 - i - (i < 4 ? 1 : 0);
        generateBox(view, piece, 0, y0, 2 + i, 2, 7 - i, 2 + i, kAir, kAir, false);
    }
}

/** MineShaftCorridor.fillPillarDownOrChainUp(). */
void fillPillarDownOrChainUp(BlockView &view, const MsPiece &piece,
                             int x, int y, int z)
{
    int wx, wy, wz;
    localToWorld(piece, x, y, z, wx, wy, wz);
    if (!view.inside(wx, wy, wz)) return;
    const int worldY = wy;
    int distance = 1;
    bool checkBelow = true;
    for (bool checkAbove = true; checkBelow || checkAbove; distance++) {
        if (checkBelow) {
            const int belowY = worldY - distance;
            const BlockState below = view.raw(wx, belowY, wz);
            const bool emptyBelow = below.isReplaceableByStructures() &&
                                    below.kind != mineshaft::BlockKind::lava;
            if (!emptyBelow && below.isFaceSturdy()) {
                // fillColumnBetween(wood, worldY - distance + 1, worldY)
                for (int py = worldY - distance + 1; py < worldY; py++)
                    view.set(wx, py, wz, kWood);
                return;
            }
            checkBelow = distance <= 20 && emptyBelow && belowY > -63;
        }
        if (checkAbove) {
            const int aboveY = worldY + distance;
            const BlockState above = view.raw(wx, aboveY, wz);
            const bool emptyAbove = above.isReplaceableByStructures();
            if (!emptyAbove && above.isFaceSturdy()) {
                view.set(wx, worldY + 1, wz, kFence);
                for (int py = worldY + 2; py < worldY + distance; py++)
                    view.set(wx, py, wz, kChain);
                return;
            }
            checkAbove = distance <= 50 && emptyAbove && aboveY < 320;
        }
    }
}

/** MineShaftCorridor.placeDoubleLowerOrUpperSupport(). */
void placeDoubleLowerOrUpperSupport(BlockView &view, const MsPiece &piece, int x, int y, int z)
{
    int wx, wy, wz;
    localToWorld(piece, x, y, z, wx, wy, wz);
    if (view.get(wx, wy, wz).kind == mineshaft::BlockKind::planks)
        fillPillarDownOrChainUp(view, piece, x, y, z);
    localToWorld(piece, x + 2, y, z, wx, wy, wz);
    if (view.get(wx, wy, wz).kind == mineshaft::BlockKind::planks)
        fillPillarDownOrChainUp(view, piece, x + 2, y, z);
}

/** MineShaftCorridor.createChest() for the chest minecart. */
void createChest(BlockView &view, const MsPiece &piece, PostProcessRandom &rng,
                 int x, int y, int z, std::vector<StructureChest> &out)
{
    int wx, wy, wz;
    localToWorld(piece, x, y, z, wx, wy, wz);
    if (!view.inside(wx, wy, wz)) return;
    if (!view.raw(wx, wy, wz).isAir()) return;
    if (view.raw(wx, wy - 1, wz).isAir()) return;

    (void) rng.nextBoolean(); // rail shape
    view.place(wx, wy, wz, kRail);
    StructureChest chest;
    chest.x = wx;
    chest.y = wy;
    chest.z = wz;
    chest.templateName = "MSCorridor";
    chest.lootTable = "chests/abandoned_mineshaft";
    chest.lootSeed = rng.nextLong();
    chest.container = ContainerKind::chest;
    chest.approximate = false;
    out.push_back(std::move(chest));
}

void replayCorridor(BlockView &view, const MsPiece &piece, PostProcessRandom &rng,
                    bool &hasPlacedSpider, std::vector<StructureChest> &out)
{
    if (isInInvalidLocation(view, piece, view.mc(), view.seed())) return;

    const int length = piece.sections * 5 - 1;
    generateBox(view, piece, 0, 0, 0, 2, 1, length, kAir, kAir, false);
    generateMaybeBox(view, piece, rng, 0.8f, 0, 2, 0, 2, 2, length, kAir, kAir, false, false);
    if (piece.flags & MS_FLAG_SPIDER) {
        generateMaybeBox(view, piece, rng, 0.6f, 0, 0, 0, 2, 1, length, kCobweb, kAir,
                         false, true);
    }

    static constexpr int kWebX[8] = {0, 2, 0, 2, 0, 2, 0, 2};
    static constexpr int kWebZ[8] = {-1, -1, 1, 1, -2, -2, 2, 2};
    static constexpr float kWebP[8] = {0.1f, 0.1f, 0.1f, 0.1f, 0.05f, 0.05f, 0.05f, 0.05f};

    for (int section = 0; section < piece.sections; section++) {
        const int z = 2 + section * 5;
        placeSupport(view, piece, rng, z);
        for (int i = 0; i < 8; i++) {
            maybePlaceCobWeb(view, piece, rng, kWebP[i], kWebX[i], 2, z + kWebZ[i]);
        }
        if (rng.nextInt(100) == 0) createChest(view, piece, rng, 2, 0, z - 1, out);
        if (rng.nextInt(100) == 0) createChest(view, piece, rng, 0, 0, z + 1, out);

        if ((piece.flags & MS_FLAG_SPIDER) && !hasPlacedSpider) {
            // The nextInt() is consumed before the position checks.
            const int newZ = z - 1 + rng.nextInt(3);
            int wx, wy, wz;
            localToWorld(piece, 1, 0, newZ, wx, wy, wz);
            if (view.inside(wx, wy, wz) && isInterior(view, piece, 1, 0, newZ)) {
                hasPlacedSpider = true;
                view.set(wx, wy, wz, kSpawner);
                // SpawnerBlockEntity.setEntityId -> BaseSpawner.getOrCreateNextSpawnData
                // -> spawnPotentials.getRandom(random). A freshly placed spawner has
                // empty potentials, where getRandomItem() returns before drawing, so
                // no additional RNG is consumed.
            }
        }
    }

    for (int x = 0; x <= 2; x++)
        for (int z = 0; z <= length; z++)
            setPlanksBlock(view, piece, kPlanks, x, -1, z);

    placeDoubleLowerOrUpperSupport(view, piece, 0, -1, 2);
    if (piece.sections > 1) {
        const int lastSupportPillar = length - 2;
        placeDoubleLowerOrUpperSupport(view, piece, 0, -1, lastSupportPillar);
    }

    if (piece.flags & MS_FLAG_RAILS) {
        for (int z = 0; z <= length; z++) {
            int wx, wy, wz;
            localToWorld(piece, 1, -1, z, wx, wy, wz);
            const BlockState floor = view.get(wx, wy, wz);
            if (!floor.isAir() && floor.isSolidRender()) {
                const float probability = isInterior(view, piece, 1, 0, z) ? 0.7f : 0.9f;
                maybeGenerateBlock(view, piece, rng, probability, 1, 0, z, kRail);
            }
        }
    }
}

} // namespace

namespace {

/**
 * Generate the piece graph for one mineshaft variant.
 *
 * MineshaftStructure.findGenerationPoint() builds the room and children first
 * and only then applies the vertical adjustment, so the piece graph itself does
 * not depend on the variant's target height.
 */
bool mineshaftGenerateVariant(int mc, uint64_t seed, int x, int z, int version, int type,
                              MineshaftGen *gen, int &count)
{
    MsGenOpts opts{};
    opts.version = version;
    opts.type = type;
    opts.seaLevel = kSeaLevel;
    opts.minY = kMinY;
    opts.mesaSurface = kSeaLevel;

    const int chunkX = x >> 4;
    const int chunkZ = z >> 4;
    count = ms_generate(gen, seed, chunkX, chunkZ, &opts);
    if (count <= 0) return false;

    if (type == MS_TYPE_MESA) {
        // MineshaftStructure: the mesa target height is WORLD_SURFACE_WG at the
        // centre of the *unshifted* piece box, then Mth.randomBetweenInclusive.
        // The piece graph does not depend on that height, so generate once to
        // learn the centre and regenerate with the real surface height. dy only
        // shifts Y, so X/Z of the box are already final.
        const MsBox &box = gen->result.structureBox;
        const int centerX = box.minX + (box.maxX - box.minX + 1) / 2;
        const int centerZ = box.minZ + (box.maxZ - box.minZ + 1) / 2;
        opts.mesaSurface = worldSurfaceHeight(mc, seed, centerX, centerZ);
        count = ms_generate(gen, seed, chunkX, chunkZ, &opts);
        if (count <= 0) return false;
    }
    return true;
}

/**
 * Pick the mineshaft variant exactly like ChunkGenerator.tryGenerateStructure:
 * the structure set holds `mineshaft` and `mineshaft_mesa` (weight 1 each) and a
 * weighted selection RNG picks a candidate whose biome tag is then validated at
 * the jigsaw start position `(chunkMiddleX, 50 + dy, chunkMinZ)`.
 *
 * Returns -1 when neither variant is valid in this chunk (vanilla then places
 * no mineshaft at all).
 */
int selectMineshaftVariant(int mc, uint64_t seed, int x, int z, int version,
                           MineshaftGen *gen, int &count)
{
    const int chunkX = x >> 4;
    const int chunkZ = z >> 4;
    LegacyRandom selection(largeFeatureSeedValue(mc, seed, chunkX, chunkZ));
    bool remaining[2] = {true, true};
    int total = 2;
    while (total > 0) {
        int choice = selection.nextInt(total);
        int index = 0;
        for (int option = 0; option < 2; option++) {
            if (!remaining[option]) continue;
            choice -= 1;
            if (choice < 0) {
                index = option;
                break;
            }
        }
        if (mineshaftGenerateVariant(mc, seed, x, z, version, index, gen, count)) {
            const int middleX = chunkX * 16 + 8;
            const int minZ = chunkZ * 16;
            const int stubY = 50 + gen->result.dy;
            const int biome = loots_biome_exact_y(mc, seed, middleX, stubY, minZ);
            if (loots_mineshaft_biome_allowed(mc, index, biome)) return index;
        }
        remaining[index] = false;
        total -= 1;
    }
    return -1;
}

/** Cubiomes fallback for ids the exact replay does not cover (below 1.18.2). */
bool mineshaftLootApprox(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    loots_piece_result buffer;
    if (!loots_mineshaft_pieces(mc, seed, x, z, &buffer)) return false;
    out.exists = buffer.exists != 0;
    out.pieces = buffer.pieces;
    out.minX = buffer.min_x;
    out.minY = buffer.min_y;
    out.minZ = buffer.min_z;
    out.maxX = buffer.max_x;
    out.maxY = buffer.max_y;
    out.maxZ = buffer.max_z;
    out.chests.clear();
    out.chests.reserve(static_cast<size_t>(buffer.chest_count));
    for (int i = 0; i < buffer.chest_count; i++) {
        StructureChest chest;
        chest.x = buffer.chests[i].x;
        chest.y = buffer.chests[i].y;
        chest.z = buffer.chests[i].z;
        chest.templateName = buffer.chests[i].template_name;
        chest.lootTable = buffer.chests[i].table;
        chest.lootSeed = buffer.chests[i].seed;
        chest.container = ContainerKind::chest;
        chest.approximate = true;
        out.chests.push_back(std::move(chest));
    }
    return out.exists;
}

} // namespace

bool mineshaftLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.seed = seed;
    out.kind = "mineshaft";

    const int version = msVersionForMc(mc);
    if (version < 0) return mineshaftLootApprox(mc, seed, x, z, out);

    MineshaftGen *gen = new MineshaftGen();
    int count = 0;
    const int type = selectMineshaftVariant(mc, seed, x, z, version, gen, count);
    if (type < 0) {
        delete gen;
        return false;
    }

    out.exists = true;
    out.pieces = count;
    out.minX = out.minY = out.minZ = 0x7fffffff;
    out.maxX = out.maxY = out.maxZ = -0x7fffffff;
    for (int i = 0; i < count; i++) {
        const MsPiece &piece = gen->pieces[i];
        out.minX = std::min(out.minX, piece.bb.minX);
        out.minY = std::min(out.minY, piece.bb.minY);
        out.minZ = std::min(out.minZ, piece.bb.minZ);
        out.maxX = std::max(out.maxX, piece.bb.maxX);
        out.maxY = std::max(out.maxY, piece.bb.maxY);
        out.maxZ = std::max(out.maxZ, piece.bb.maxZ);
    }

    // Every chunk column any piece reaches, in the same order the 26.3 oracle
    // loads them (ChunkPos.pack sorted): negative Z first, then unsigned X.
    std::vector<std::pair<int, int>> chunks;
    for (int i = 0; i < count; i++) {
        const MsPiece &piece = gen->pieces[i];
        for (int cx = piece.bb.minX >> 4; cx <= piece.bb.maxX >> 4; cx++) {
            for (int cz = piece.bb.minZ >> 4; cz <= piece.bb.maxZ >> 4; cz++) {
                chunks.emplace_back(cx, cz);
            }
        }
    }
    std::sort(chunks.begin(), chunks.end(), [](const auto &a, const auto &b) {
        return chunkPack(a.first, a.second) < chunkPack(b.first, b.second);
    });
    chunks.erase(std::unique(chunks.begin(), chunks.end()), chunks.end());

    mineshaft::BlockView view(mc, seed, 0, 0);
    std::vector<char> hasSpawner(static_cast<size_t>(count), 0);
    const int lootIndex = type == MS_TYPE_MESA ? kLootIndexMesa : kLootIndex;
    for (const auto &chunk : chunks) {
        view.setChunk(chunk.first, chunk.second);
        PostProcessRandom rng(
            mc, loots_structure_loot_seed_index(mc, seed, chunk.first * 16, chunk.second * 16,
                                                kLootStep, lootIndex));
        for (int i = 0; i < count; i++) {
            const MsPiece &piece = gen->pieces[i];
            if (!intersectsChunk(piece, chunk.first, chunk.second)) continue;
            switch (piece.type) {
            case MS_PT_ROOM:
                replayRoomPiece(view, piece, gen->entrances, gen->entranceCount);
                break;
            case MS_PT_CROSSING:
                replayCrossing(view, piece);
                break;
            case MS_PT_STAIRS:
                replayStairs(view, piece);
                break;
            case MS_PT_CORRIDOR: {
                bool spawner = hasSpawner[static_cast<size_t>(i)] != 0;
                replayCorridor(view, piece, rng, spawner, out.chests);
                hasSpawner[static_cast<size_t>(i)] = spawner ? 1 : 0;
                break;
            }
            default:
                break;
            }
        }
    }

    delete gen;
    return true;
}

} // namespace loots
