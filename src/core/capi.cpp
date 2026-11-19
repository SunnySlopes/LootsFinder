#include "loots/loots_capi.h"

#include <cstring>

#include "loots/loots.h"
#include "loots/version.h"

namespace {

loots_city toC(const loots::CityChestInfo &in)
{
    loots_city out{};
    out.x = in.x;
    out.z = in.z;
    out.chunk_x = in.chunkX;
    out.chunk_z = in.chunkZ;
    out.exists = in.exists ? 1 : 0;
    out.pieces = in.pieces;
    out.min_x = in.bounds.minX;
    out.min_y = in.bounds.minY;
    out.min_z = in.bounds.minZ;
    out.max_x = in.bounds.maxX;
    out.max_y = in.bounds.maxY;
    out.max_z = in.bounds.maxZ;
    const int n = static_cast<int>(in.chests.size());
    out.chest_count = n > LOOTS_MAX_CHESTS_PER_CITY ? LOOTS_MAX_CHESTS_PER_CITY : n;
    for (int i = 0; i < out.chest_count; i++) {
        const loots::CityChest &c = in.chests[static_cast<size_t>(i)];
        out.chests[i].x = c.x;
        out.chests[i].y = c.y;
        out.chests[i].z = c.z;
        out.chests[i].kind = c.kind;
        out.chests[i].silence = c.silence ? 1 : 0;
        out.chests[i].ward = c.ward ? 1 : 0;
        out.chests[i].golden_apples = c.enchantedGoldenApples;
        out.chests[i].disc_kinds = c.discKinds;
        out.chests[i].misc_kinds = c.miscKinds;
        out.chests[i].disc_fragments = c.discFragments;
        out.chests[i].loot_seed = static_cast<unsigned long long>(c.lootSeed);
    }
    return out;
}

loots_sponge toC(const loots::MonumentSponge &in)
{
    loots_sponge out{};
    out.x = in.x;
    out.z = in.z;
    out.exists = in.exists ? 1 : 0;
    out.rooms = in.rooms;
    out.sponge_rooms = in.spongeRooms;
    out.min_x = in.bounds.minX;
    out.min_y = in.bounds.minY;
    out.min_z = in.bounds.minZ;
    out.max_x = in.bounds.maxX;
    out.max_y = in.bounds.maxY;
    out.max_z = in.bounds.maxZ;
    const int n = static_cast<int>(in.sponges.size());
    out.sponge_count = n > 16 ? 16 : n;
    for (int i = 0; i < out.sponge_count; i++) {
        const loots::Box3i &b = in.sponges[static_cast<size_t>(i)];
        out.sponges[i].x = b.minX;
        out.sponges[i].y = b.minY;
        out.sponges[i].z = b.minZ;
    }
    return out;
}

} // namespace

extern "C" {

int loots_capi_version(void)
{
    return LOOTS_CAPI_VERSION;
}

const char *loots_version_string(void)
{
    return LOOTS_VERSION_STRING;
}

void loots_capability_query(int mc, loots_capability *out)
{
    if (!out) return;
    const loots::OverlayCapability cap = loots::overlayCapability(mc);
    out->city_tier = cap.cityTier;
    out->city_silence = cap.citySilence ? 1 : 0;
    out->city_swift_sneak = cap.citySwiftSneak ? 1 : 0;
    out->city_golden_apple = cap.cityGoldenApple ? 1 : 0;
    out->city_ward = cap.cityWard ? 1 : 0;
    out->supported = loots::overlaySupported(mc) ? 1 : 0;
}

int loots_ancient_city_query(int mc, unsigned long long seed, unsigned int flags,
                             int x, int z, loots_city *out)
{
    if (!out) return 0;
    std::vector<std::pair<int, int>> cities;
    cities.emplace_back(x, z);
    const auto res = loots::ancientCityChests(mc, static_cast<uint64_t>(seed), flags, cities);
    if (res.empty()) {
        std::memset(out, 0, sizeof(*out));
        return 0;
    }
    *out = toC(res.front());
    return 1;
}

int loots_sponge_query(int mc, unsigned long long seed, unsigned int flags,
                       int x, int z, loots_sponge *out)
{
    if (!out) return 0;
    std::vector<std::pair<int, int>> monuments;
    monuments.emplace_back(x, z);
    const auto res = loots::monumentSponge(mc, static_cast<uint64_t>(seed), flags, monuments);
    if (res.empty()) {
        std::memset(out, 0, sizeof(*out));
        return 0;
    }
    *out = toC(res.front());
    return 1;
}

} // extern "C"
