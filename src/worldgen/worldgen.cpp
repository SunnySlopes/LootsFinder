#include "loots/worldgen.h"

#include <memory>
#include <mutex>
#include <utility>

#include "terrain.hpp"
#include "structure_layout/layout.hpp"

namespace loots {
namespace {

std::mutex g_terrainMu;
std::unique_ptr<loots_wg::Terrain> g_terrain;
uint64_t g_terrainSeed = 0;

loots_wg::Terrain &terrainFor(uint64_t seed)
{
    if (!g_terrain || g_terrainSeed != seed) {
        g_terrain = std::make_unique<loots_wg::Terrain>(seed, 5);
        g_terrainSeed = seed;
    }
    return *g_terrain;
}

} // namespace

int worldSurfaceHeight(int /*mc*/, uint64_t seed, int x, int z)
{
    std::lock_guard<std::mutex> lock(g_terrainMu);
    return terrainFor(seed).height_world_surface(x, z);
}

int worldOceanFloorHeight(int mc, uint64_t seed, int x, int z)
{
    for (int y = 63; y >= -64; y--) {
        if (worldBeard(mc, seed, x, y, z) != 0.0) continue;
        if (worldSolid(mc, seed, x, y, z)) return y + 1;
    }
    return -64;
}

bool worldSolid(int /*mc*/, uint64_t seed, int x, int y, int z)
{
    std::lock_guard<std::mutex> lock(g_terrainMu);
    return terrainFor(seed).solid(x, y, z);
}

bool worldBaseSolid(int /*mc*/, uint64_t seed, int x, int y, int z)
{
    std::lock_guard<std::mutex> lock(g_terrainMu);
    return terrainFor(seed).base_solid(x, y, z);
}

double worldBeard(int /*mc*/, uint64_t seed, int x, int y, int z)
{
    std::lock_guard<std::mutex> lock(g_terrainMu);
    return terrainFor(seed).beard(x, y, z);
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
