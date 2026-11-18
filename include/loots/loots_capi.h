// Minimal C ABI for LootsFinder.
//
// Purpose:
//   - let the CLI (and any other tool) use the module without C++ or the host
//   - keep a stable, versioned surface for external consumers
//
// The C++ API in loots.h stays the primary interface for the host application;
// it can evolve with the source. This header only changes together with
// LOOTS_CAPI_VERSION.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bump when the layout of any struct below changes. */
#define LOOTS_CAPI_VERSION 2

/** Matches the host limit; a city never carries more evaluated chests than this. */
#define LOOTS_MAX_CHESTS_PER_CITY 64

typedef struct loots_capability {
    int city_tier;
    int city_silence;
    int city_swift_sneak;
    int city_golden_apple;
    int city_ward;
    int ore_diamond;
    int ore_lapis;
    int nether_debris;
    int supported;
} loots_capability;

typedef struct loots_chest {
    int x;
    int y;
    int z;
    int kind;              // 0 = chests/ancient_city, 1 = ice box, 2 = fixed item chest
    int silence;
    int ward;
    int golden_apples;
    /** Music disc/fragment bits, see loots::LootDiscBits. */
    unsigned int disc_kinds;
    /** Banner/egg/golden-apple bits, see loots::LootMiscBits. */
    unsigned int misc_kinds;
    /** Total disc_fragment_5 items. */
    int disc_fragments;
    unsigned long long loot_seed;
} loots_chest;

typedef struct loots_city {
    int x;
    int z;
    int chunk_x;
    int chunk_z;
    int exists;
    int pieces;
    int min_x;
    int min_y;
    int min_z;
    int max_x;
    int max_y;
    int max_z;
    int chest_count;
    loots_chest chests[LOOTS_MAX_CHESTS_PER_CITY];
} loots_city;

typedef struct loots_sponge {
    int x;
    int z;
    int exists;
    int rooms;
    int sponge_rooms;
    int min_x;
    int min_y;
    int min_z;
    int max_x;
    int max_y;
    int max_z;
    /** Number of entries written into `sponges` (one box per sponge room). */
    int sponge_count;
    /** Each sponge room as a single-block box at its representative position. */
    struct {
        int x;
        int y;
        int z;
    } sponges[16];
} loots_sponge;

int loots_capi_version(void);

/** Version string of the library, e.g. "0.1.0". */
const char *loots_version_string(void);

void loots_capability_query(int mc, loots_capability *out);

/** Returns 1 on success, 0 on failure (bad argument). */
int loots_ancient_city_query(int mc, unsigned long long seed, unsigned int flags,
                             int x, int z, loots_city *out);

/** Returns 1 on success, 0 on failure (bad argument). */
int loots_sponge_query(int mc, unsigned long long seed, unsigned int flags,
                       int x, int z, loots_sponge *out);

#ifdef __cplusplus
}
#endif
