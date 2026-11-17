/*
 * C shim over cubiomes' structure piece generators.
 *
 * This file is compiled as C on purpose: cubiomes' headers define a global
 * `lerp` which collides with `std::lerp` when included from a C++20 translation
 * unit. Keeping the cubiomes dependency in a C file avoids the clash without
 * macro hacks.
 */
#ifndef LOOTS_PIECES_C_H
#define LOOTS_PIECES_C_H

#ifdef __cplusplus
extern "C" {
#endif

#define LOOTS_PIECE_MAX_CHESTS 256
#define LOOTS_TABLE_NAME_MAX 64

typedef struct loots_piece_chest {
    int x;
    int y;
    int z;
    char table[LOOTS_TABLE_NAME_MAX];
    char template_name[LOOTS_TABLE_NAME_MAX];
    unsigned long long seed;
} loots_piece_chest;

typedef struct loots_piece_result {
    int exists;
    int pieces;
    int min_x, min_y, min_z;
    int max_x, max_y, max_z;
    int chest_count;
    loots_piece_chest chests[LOOTS_PIECE_MAX_CHESTS];
} loots_piece_result;

/** Nether fortress at block (x, z). Returns 1 when pieces were generated. */
int loots_fortress_pieces(int mc, unsigned long long seed, int x, int z,
                          loots_piece_result *out);

/** End city at block (x, z). Returns 1 when pieces were generated. */
int loots_end_city_pieces(int mc, unsigned long long seed, int x, int z,
                          loots_piece_result *out);

/** Desert pyramid / jungle temple: single-piece structures with exact chests. */
int loots_desert_pieces(int mc, unsigned long long seed, int x, int z,
                        loots_piece_result *out);
int loots_jungle_pieces(int mc, unsigned long long seed, int x, int z,
                        loots_piece_result *out);

/** Shipwreck: uses getVariant() for beached/ocean + rotation. */
int loots_shipwreck_pieces(int mc, unsigned long long seed, int x, int z,
                           loots_piece_result *out);

/** 结构 piece 放置 RNG 的种子：WorldgenRandom.setLargeFeatureSeed()。
 *  这是 legacy java.util.Random 的派生值，不是 1.18+ 的 decoration seed。 */
unsigned long long loots_structure_rng_seed(int mc, unsigned long long seed,
                                            int chunk_x, int chunk_z);

/** 结构 postProcess 用的战利品随机流种子（population seed + salt）。 */
unsigned long long loots_structure_loot_seed(int mc, unsigned long long seed,
                                             int x, int z, int stype);

/** 该位置的生物群系 id（-1 = 失败）。 */
int loots_biome_at(int mc, unsigned long long seed, int x, int z);

/** Exact 1:1 biome at a block position (used for beached shipwreck). */
int loots_biome_exact(int mc, unsigned long long seed, int x, int z);

/** 1 for warm/lukewarm/deep_lukewarm/deep_warm ocean, 0 for cold. */
int loots_biome_is_warm_ocean(int mc, unsigned long long seed, int x, int z);

typedef struct loots_found { int x; int z; } loots_found;

/**
 * Find up to `limit` viable structure positions of `stype` within
 * [-radius,radius]^2.  `dim` uses cubiomes' DIM_* values (0/-1/1).
 * Returns the number written to `out`.
 */
int loots_find_structures(int mc, unsigned long long seed, int stype, int dim,
                          int radius, int limit, loots_found *out, int outCapacity);

/** Approximate WORLD_SURFACE_WG height (blocks, 1:4 sampled). */
int loots_surface_height(int mc, unsigned long long seed, int x, int z);

#ifdef __cplusplus
}
#endif

#endif /* LOOTS_PIECES_C_H */
