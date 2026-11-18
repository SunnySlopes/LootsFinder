// Exact abandoned-mineshaft replay for loots.
//
// The piece graph and vertical placement are ported from `tools/geode`'s
// mineshaft_gen (itself a 1:1 port of MineshaftPieces/MineshaftStructure).
// Cubiomes' getMineshaftLoot() models the same chest RNG but ignores the
// vanilla air/floor check and the real vertical shift, which is why its chest
// list does not match the 26.3 server.  This file keeps geode's exact pieces
// and replays MineshaftCorridor.generate's random consumption with loots'
// terrain queries.
#include "loots/structure_pieces.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include "loots/rng.h"
#include "loots/worldgen.h"
#include "mineshaft_gen.h"
#include "pieces_c.h"

namespace loots {
namespace {

constexpr int kSeaLevel = 63;
constexpr int kMinY = -64;
/* Mineshafts are placed in GenerationStep 3 (underground structures) with
 * decorator index 1; verified against the 26.3 server oracle. */
constexpr int kLootStep = 3;
constexpr int kLootIndex = 1;

int msVersionForMc(int mc)
{
    if (mc >= 35) return MS_VERSION_26_3;
    if (mc >= 34) return MS_VERSION_26_2;
    if (mc >= 33) return MS_VERSION_26_1_2;
    if (mc >= 30) return MS_VERSION_1_21_11;
    return -1;
}

struct TerrainCache {
    int mc = 0;
    uint64_t seed = 0;
    std::map<std::tuple<int, int, int>, int8_t> solid;

    bool isSolid(int x, int y, int z)
    {
        const auto key = std::make_tuple(x, y, z);
        auto it = solid.find(key);
        if (it != solid.end()) return it->second != 0;
        const bool value = worldSolid(mc, seed, x, y, z);
        solid.emplace(key, value ? 1 : 0);
        return value;
    }
};

struct PieceGeom {
    const MsPiece *piece = nullptr;
    int length = 0; // sections * 5 - 1
};

/** StructurePiece.offsetPos() for a mineshaft corridor. */
void localToWorld(const MsPiece &piece, int lx, int ly, int lz, int &wx, int &wy, int &wz)
{
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

bool inChunk(int wx, int wz, int cx, int cz)
{
    return wx >= cx * 16 && wx < cx * 16 + 16 && wz >= cz * 16 && wz < cz * 16 + 16;
}

bool supportInChunk(const MsPiece &piece, int sectionZ, int cx, int cz)
{
    for (int lx = 0; lx <= 2; lx++) {
        int wx, wy, wz;
        localToWorld(piece, lx, 2, sectionZ, wx, wy, wz);
        if (!inChunk(wx, wz, cx, cz)) return false;
    }
    return true;
}

bool inChunkAt(const MsPiece &piece, int lx, int ly, int lz, int cx, int cz)
{
    int wx, wy, wz;
    localToWorld(piece, lx, ly, lz, wx, wy, wz);
    return inChunk(wx, wz, cx, cz);
}

/** addChest(): only the air/floor half can be modelled without block states. */
bool chestFloorExists(const MsPiece &piece, int lx, int lz, int cx, int cz,
                      TerrainCache &terrain, int &wx, int &wy, int &wz)
{
    localToWorld(piece, lx, 0, lz, wx, wy, wz);
    const bool inside = inChunk(wx, wz, cx, cz);
    return inside && terrain.isSolid(wx, wy - 1, wz);
}

void collectCorridor(const MsPiece &piece, int cx, int cz, PostProcessRandom &rng,
                     bool &hasSpawner, TerrainCache &terrain,
                     std::vector<StructureChest> &out)
{
    const int length = piece.sections * 5 - 1;
    // fillWithOutlineUnderSeaLevel() consumes one nextFloat for every block in
    // the box, even when the block is not placed.
    for (int i = 0; i < 3 * (length + 1); i++) (void) rng.nextFloat();
    const bool hasCobwebs = (piece.flags & MS_FLAG_SPIDER) != 0;
    if (hasCobwebs)
        for (int i = 0; i < 6 * (length + 1); i++) (void) rng.nextFloat();

    for (int section = 0; section < piece.sections; section++) {
        const int z = 2 + section * 5;

        // generateSupports(): only consumes RNG when the ceiling is solid.
        if (supportInChunk(piece, z, cx, cz)) {
            const int choice = rng.nextInt(4);
            if (choice != 0) {
                (void) rng.nextFloat();
                (void) rng.nextFloat();
            }
        }

        // addCobwebsUnderground(): one nextFloat only when under sea level.
        const int webZ[8] = {z - 1, z - 1, z + 1, z + 1, z - 2, z - 2, z + 2, z + 2};
        const int webX[8] = {0, 2, 0, 2, 0, 2, 0, 2};
        for (int i = 0; i < 8; i++) {
            if (inChunkAt(piece, webX[i], 2, webZ[i], cx, cz))
                (void) rng.nextFloat();
        }
        // Two chest rolls, in the exact order of the vanilla loop.
        const int chestX[2] = {2, 0};
        const int chestZ[2] = {z - 1, z + 1};
        for (int i = 0; i < 2; i++) {
            if (rng.nextInt(100) != 0) continue;
            int wx, wy, wz;
            if (!chestFloorExists(piece, chestX[i], chestZ[i], cx, cz, terrain, wx, wy, wz))
                continue;
            (void) rng.nextBoolean(); // rail shape
            StructureChest chest;
            chest.x = wx;
            chest.y = wy;
            chest.z = wz;
            chest.templateName = "MSCorridor";
            chest.lootTable = "chests/abandoned_mineshaft";
            chest.lootSeed = rng.nextLong();
            chest.container = ContainerKind::chest;
            chest.approximate = true; // 4/6 oracle chests exact; still flagged
            out.push_back(std::move(chest));
        }

        // Spider spawner: nextInt(3) is consumed whenever the corridor is a
        // spider corridor and has not placed its spawner yet.
        if (hasCobwebs && !hasSpawner) {
            const int q = z - 1 + rng.nextInt(3);
            int wx, wy, wz;
            localToWorld(piece, 1, 0, q, wx, wy, wz);
            if (inChunk(wx, wz, cx, cz)) {
                hasSpawner = true;
                // MobSpawnerLogic.setEntityId -> spawnPotentials.getOrEmpty(random)
                // is empty for mineshafts, so no additional RNG call is made.
            }
        }
    }

    // Rails: addBlockWithRandomThreshold() consumes nextFloat for every column
    // whose floor is opaque.  tryPlaceFloor() places planks later, so account
    // for both the natural floor and the planks it will place.
    if (piece.flags & MS_FLAG_RAILS) {
        for (int v = 0; v <= length; v++) {
            if (inChunkAt(piece, 1, -1, v, cx, cz)) (void) rng.nextFloat();
        }
    }
}

} // namespace

namespace {

/** Cubiomes fallback for versions geode's generator does not cover (<=1.21.5). */
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

    MsGenOpts opts{};
    opts.version = version;
    opts.type = loots_mineshaft_type(mc, seed, x, z);
    opts.seaLevel = kSeaLevel;
    opts.minY = kMinY;
    opts.mesaSurface = opts.type == MS_TYPE_MESA
                           ? worldSurfaceHeight(mc, seed, x, z)
                           : kSeaLevel;

    MineshaftGen *gen = new MineshaftGen();
    const int count = ms_generate(gen, seed, x >> 4, z >> 4, &opts);
    if (count <= 0) {
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

    TerrainCache terrain;
    terrain.mc = mc;
    terrain.seed = seed;

    const int cMinX = out.minX >> 4;
    const int cMinZ = out.minZ >> 4;
    const int cMaxX = out.maxX >> 4;
    const int cMaxZ = out.maxZ >> 4;

    std::vector<char> hasSpawner(static_cast<size_t>(count), 0);
    for (int cx = cMinX; cx <= cMaxX; cx++) {
        for (int cz = cMinZ; cz <= cMaxZ; cz++) {
            PostProcessRandom rng(
                mc, loots_structure_loot_seed_index(mc, seed, cx * 16, cz * 16, kLootStep, kLootIndex));
            for (int i = 0; i < count; i++) {
                const MsPiece &piece = gen->pieces[i];
                if (piece.type != MS_PT_CORRIDOR) continue;
                if (piece.bb.maxX < cx * 16 || piece.bb.minX > cx * 16 + 15 ||
                    piece.bb.maxZ < cz * 16 || piece.bb.minZ > cz * 16 + 15)
                    continue;
                bool spawner = hasSpawner[static_cast<size_t>(i)] != 0;
                collectCorridor(piece, cx, cz, rng, spawner, terrain, out.chests);
                hasSpawner[static_cast<size_t>(i)] = spawner ? 1 : 0;
            }
        }
    }

    delete gen;
    return true;
}

} // namespace loots
