#include "loots/worldgen.h"

#include <memory>
#include <mutex>
#include <utility>

#include "terrain.hpp"
#include "structure_layout/layout.hpp"
#include "pieces_c.h"

namespace loots {
namespace {

std::mutex g_terrainMu;
std::unique_ptr<loots_wg::Terrain> g_terrain;
uint64_t g_terrainSeed = 0;
int g_terrainGeneration = -1000;
int g_terrainMc = -1000;

/**
 * Cubiomes MC_* id -> geode/loots terrain generation profile.
 *
 * Mirrors tools/geode/src/search.cpp set_version():
 *   1.19.2                 -> -3
 *   1.19.4                 -> 0
 *   1.20.x                 -> 0
 *   1.21.0/1.21.1          -> 1
 *   1.21.2 .. 1.21.8       -> 2
 *   1.21.9+ / 26.1 / 26.2  -> 4
 *   26.3                    -> 5
 * The pre-generated terrain_data.hpp covers <= 26.2 and terrain_modern_data.hpp
 * covers 26.3; the remaining values select version-specific aquifer branches.
 */
int terrainGenerationForMc(int mc)
{
    if (mc >= 35) return 5; // 26.3
    if (mc >= 31) return 4; // 1.21.9+ / 26.1 / 26.2
    if (mc >= 27) return 2; // 1.21.2 .. 1.21.8
    if (mc >= 26) return 1; // 1.21.0 / 1.21.1
    if (mc >= 25) return 0; // 1.20.x
    if (mc == 23) return -3; // 1.19.2
    if (mc == 24) return 0;  // 1.19.4
    return 5;                // legacy callers keep the previous 26.3 profile
}

loots_wg::Terrain &terrainFor(int mc, uint64_t seed)
{
    const int generation = terrainGenerationForMc(mc);
    // The version id is part of the key: several MC ids share one generation
    // profile but differ in aquifer/biome branches, and the CLI/tests switch
    // versions inside one process.
    if (!g_terrain || g_terrainSeed != seed || g_terrainGeneration != generation ||
        g_terrainMc != mc) {
        g_terrain = std::make_unique<loots_wg::Terrain>(seed, generation, mc);
        g_terrainSeed = seed;
        g_terrainGeneration = generation;
        g_terrainMc = mc;
    }
    return *g_terrain;
}

} // namespace

int worldSurfaceHeight(int mc, uint64_t seed, int x, int z)
{
    std::lock_guard<std::mutex> lock(g_terrainMu);
    return terrainFor(mc, seed).height_world_surface(x, z);
}

int worldOceanFloorHeight(int mc, uint64_t seed, int x, int z)
{
    for (int y = 63; y >= -64; y--) {
        if (worldBeard(mc, seed, x, y, z) != 0.0) continue;
        if (worldSolid(mc, seed, x, y, z)) return y + 1;
    }
    return -64;
}

bool worldSolid(int mc, uint64_t seed, int x, int y, int z)
{
    std::lock_guard<std::mutex> lock(g_terrainMu);
    return terrainFor(mc, seed).solid(x, y, z);
}

int worldFluidType(int mc, uint64_t seed, int x, int y, int z)
{
    std::lock_guard<std::mutex> lock(g_terrainMu);
    return terrainFor(mc, seed).fluid_type(x, y, z);
}

bool worldMineshaftBlockingBiome(int mc, uint64_t seed, int x, int y, int z)
{
    return loots_biome_is_deep_dark(mc, seed, x, y, z) != 0;
}

bool worldBaseSolid(int mc, uint64_t seed, int x, int y, int z)
{
    std::lock_guard<std::mutex> lock(g_terrainMu);
    return terrainFor(mc, seed).base_solid(x, y, z);
}

double worldBeard(int mc, uint64_t seed, int x, int y, int z)
{
    std::lock_guard<std::mutex> lock(g_terrainMu);
    return terrainFor(mc, seed).beard(x, y, z);
}

bool ancientCityLayout(int mc, uint64_t seed, int x, int z,
                       std::vector<AncientCityPiece> &out)
{
    if (mc < 23) return false;

    loots_wg_layout::LayoutEngine engine(6, seed);
    auto chamber = engine.generate(x >> 4, z >> 4);
    if (!chamber.exists) return false;

    static const struct {
        int id;
        const char *name;
    } kPieces[] = {
        {20, "ancient_city/structures/barracks"},
        {21, "ancient_city/structures/chamber_1"},
        {22, "ancient_city/structures/chamber_2"},
        {23, "ancient_city/structures/chamber_3"},
        {24, "ancient_city/structures/sauna_1"},
        {27, "ancient_city/structures/tall_ruin_1"},
        {28, "ancient_city/structures/tall_ruin_2"},
        {29, "ancient_city/structures/tall_ruin_3"},
        {30, "ancient_city/structures/tall_ruin_4"},
        {42, "ancient_city/structures/ice_box_1"},
    };

    for (const auto &p : chamber.placed) {
        const char *name = nullptr;
        for (const auto &m : kPieces) {
            if (m.id == p.id) {
                name = m.name;
                break;
            }
        }
        if (!name) continue;
        AncientCityPiece piece;
        piece.name = name;
        piece.x = p.pos.x;
        piece.y = p.pos.y;
        piece.z = p.pos.z;
        piece.rotation = p.rotation;
        out.push_back(std::move(piece));
    }
    return !out.empty();
}

} // namespace loots
