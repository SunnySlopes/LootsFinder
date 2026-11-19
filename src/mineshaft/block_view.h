// Block-state layer for the exact abandoned-mineshaft postProcess replay.
//
// MineshaftPieces decides where chest minecarts go by reading block states it
// (and the other pieces placed for the same chunk) wrote earlier:
//
//   * generateBox()/generateMaybeBox() carve CAVE_AIR and place planks/fences;
//   * isSupportingBox() checks the block at y1+1;
//   * createChest() requires air above a non-air block;
//   * the rails loop checks !isAir() && isSolidRender();
//   * isInterior() compares Y against the OCEAN_FLOOR_WG height.
//
// A terrain-only query (cubiomes' worldSolid) cannot answer those questions,
// because a carved corridor is air even though the terrain below it is stone.
// This view keeps an overlay of every block written while replaying the
// structure, falling back to the version-aware terrain for untouched blocks.
//
// Reads replicate both accessors used by vanilla:
//   get()   = StructurePiece.getBlock(): AIR outside the current chunk's
//             writable box (ChunkGenerator.getWritableArea()).
//   raw()   = level.getBlockState(): no writable-box clip; used by createChest,
//             isInInvalidLocation and fillPillarDownOrChainUp.
#pragma once

#include <cstdint>
#include <unordered_map>

#include "loots/worldgen.h"

namespace loots {
namespace mineshaft {

/** The block classes that change a MineshaftPieces decision. */
enum class BlockKind : uint8_t {
    air = 0,
    solid,   // any full-cube terrain block (stone/dirt/sand/...)
    water,
    lava,
    cobweb,
    planks,
    wood,
    fence,
    rail,
    spawner,
    chain,
    torch,
};

struct BlockState {
    BlockKind kind = BlockKind::air;

    bool isAir() const { return kind == BlockKind::air; }
    bool isLiquid() const { return kind == BlockKind::water || kind == BlockKind::lava; }
    /** BlockState.isSolidRender(): full opaque cube. */
    bool isSolidRender() const
    {
        return kind == BlockKind::solid || kind == BlockKind::planks || kind == BlockKind::wood;
    }
    /** BlockState.isFaceSturdy(level, pos, direction, SupportType.FULL). */
    bool isFaceSturdy() const { return isSolidRender(); }
    /** isReplaceableByStructures(): air / liquid (glow lichen & seagrass do not
     *  occur inside a mineshaft corridor in the first chunk generation pass). */
    bool isReplaceableByStructures() const { return isAir() || isLiquid(); }
    /** MineShaftPiece.canBeReplaced(): the piece refuses to overwrite its own
     *  structural blocks. */
    bool blocksPieceWrite() const
    {
        return kind == BlockKind::planks || kind == BlockKind::wood ||
               kind == BlockKind::fence || kind == BlockKind::chain;
    }
};

constexpr BlockState kAir{BlockKind::air};
constexpr BlockState kCobweb{BlockKind::cobweb};
constexpr BlockState kPlanks{BlockKind::planks};
constexpr BlockState kWood{BlockKind::wood};
constexpr BlockState kFence{BlockKind::fence};
constexpr BlockState kRail{BlockKind::rail};
constexpr BlockState kSpawner{BlockKind::spawner};
constexpr BlockState kChain{BlockKind::chain};
constexpr BlockState kTorch{BlockKind::torch};

/**
 * Structure-local block view.
 *
 * `setChunk()` selects the writable box used by get()/place(); the overlay
 * itself spans the whole structure so that a level.setBlock() performed while
 * replaying one chunk is visible when a neighbouring chunk is replayed later,
 * exactly like the block states persisted by the real WorldGenRegion.
 */
class BlockView {
public:
    BlockView(int mc, uint64_t seed, int chunkX, int chunkZ)
        : mc_(mc), seed_(seed)
    {
        setChunk(chunkX, chunkZ);
    }

    void setChunk(int chunkX, int chunkZ)
    {
        minX_ = chunkX * 16;
        minZ_ = chunkZ * 16;
    }

    int chunkMinX() const { return minX_; }
    int chunkMinZ() const { return minZ_; }
    int mc() const { return mc_; }
    uint64_t seed() const { return seed_; }

    /** BoundingBox.isInside() for the chunk writable area. */
    bool inside(int x, int y, int z) const
    {
        return x >= minX_ && x <= minX_ + 15 && z >= minZ_ && z <= minZ_ + 15 &&
               y >= kChunkMinY && y <= kChunkMaxY;
    }

    /** StructurePiece.getBlock(): AIR outside the writable box. */
    BlockState get(int x, int y, int z)
    {
        return inside(x, y, z) ? raw(x, y, z) : kAir;
    }

    /** level.getBlockState(): overlay first, then version-aware terrain. */
    BlockState raw(int x, int y, int z)
    {
        const auto found = overlay_.find(key(x, y, z));
        if (found != overlay_.end()) return BlockState{static_cast<BlockKind>(found->second)};
        return terrain(x, y, z);
    }

    /** StructurePiece.placeBlock(): writable box + canBeReplaced(). */
    void place(int x, int y, int z, BlockState state)
    {
        if (!inside(x, y, z)) return;
        if (raw(x, y, z).blocksPieceWrite()) return;
        overlay_[key(x, y, z)] = static_cast<uint8_t>(state.kind);
    }

    /** level.setBlock(): no canBeReplaced(), not clipped to the writable box. */
    void set(int x, int y, int z, BlockState state)
    {
        overlay_[key(x, y, z)] = static_cast<uint8_t>(state.kind);
    }

    /**
     * `startY < level.getHeight(OCEAN_FLOOR_WG, x, z)`.
     *
     * The heightmap stores topmost-solid + 1 and is not refreshed by structure
     * writes (proto-chunks do not track it), so this is "some terrain solid at
     * or above startY". Scanning upward from startY is cheap for the usual
     * underground corridor and avoids building a full 384-block column.
     */
    bool solidAbove(int x, int startY, int z)
    {
        const int from = std::max(startY, kWorldMinY);
        for (int y = from; y <= kWorldMaxY; y++)
            if (terrain(x, y, z).kind == BlockKind::solid) return true;
        return false;
    }

private:
    static constexpr int kWorldMinY = -64;   // world bottom (inclusive)
    static constexpr int kWorldMaxY = 319;   // world top (inclusive): height 320
    static constexpr int kChunkMinY = -63;   // LevelHeightAccessor.getMinY() + 1
    static constexpr int kChunkMaxY = 320;   // LevelHeightAccessor.getMaxY()

    BlockState terrain(int x, int y, int z)
    {
        if (y < kWorldMinY || y > kWorldMaxY) return kAir;
        const uint64_t k = key(x, y, z);
        const auto found = terrain_.find(k);
        if (found != terrain_.end()) return BlockState{static_cast<BlockKind>(found->second)};

        BlockKind kind;
        if (worldSolid(mc_, seed_, x, y, z)) {
            kind = BlockKind::solid;
        } else {
            const int fluid = worldFluidType(mc_, seed_, x, y, z);
            kind = fluid == 2 ? BlockKind::lava : (fluid == 1 ? BlockKind::water : BlockKind::air);
        }
        if (terrain_.size() >= (1u << 20)) terrain_.clear();
        terrain_.emplace(k, static_cast<uint8_t>(kind));
        return BlockState{kind};
    }

    /** 26 bits each for X/Z (world limit is +-30M) and 9 bits for Y. */
    static uint64_t key(int x, int y, int z)
    {
        constexpr uint64_t kMask = (1ULL << 26) - 1;
        constexpr int kBias = 1 << 25;
        const uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(x + kBias)) & kMask;
        const uint64_t uz = static_cast<uint64_t>(static_cast<uint32_t>(z + kBias)) & kMask;
        const uint64_t uy = static_cast<uint64_t>(y + 64) & 0x1ff;
        return ux | (uz << 26) | (uy << 52);
    }

    int mc_ = 0;
    uint64_t seed_ = 0;
    int minX_ = 0;
    int minZ_ = 0;
    std::unordered_map<uint64_t, uint8_t> overlay_;
    std::unordered_map<uint64_t, uint8_t> terrain_;
};

} // namespace mineshaft
} // namespace loots
