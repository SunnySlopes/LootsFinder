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

/** Abandoned mineshaft at block (x, z); uses getMineshaftLoot(). */
int loots_mineshaft_pieces(int mc, unsigned long long seed, int x, int z,
                           loots_piece_result *out);

/**
 * Surface-biome guess for the mesa variant (badlands category at y=63).
 *
 * This is NOT how vanilla picks the variant: both structures share one
 * structure set, the weighted selection RNG chooses a candidate, and its biome
 * tag is checked at the jigsaw start position (which can be underground). Use
 * `loots_mineshaft_biome_allowed` plus the selection RNG for exact results;
 * this helper is kept for callers that only need the biome hint.
 */
int loots_mineshaft_type(int mc, unsigned long long seed, int x, int z);

/**
 * 1 when `biome` is allowed by the mineshaft structure tag.
 *
 * `kind` is 0 for `minecraft:mineshaft` (26.x `has_structure/mineshaft`) and 1
 * for `minecraft:mineshaft_mesa` (`has_structure/mineshaft_mesa`, badlands).
 * Both structures share one structure set, so the actual variant is chosen by
 * the weighted selection RNG and then validated against this tag at the
 * jigsaw start position (chunk middle X/Z, y = 50 + dy).
 */
int loots_mineshaft_biome_allowed(int mc, int kind, int biome);

/** 结构 piece 放置 RNG 的种子：WorldgenRandom.setLargeFeatureSeed()。
 *  这是 legacy java.util.Random 的派生值，不是 1.18+ 的 decoration seed。 */
unsigned long long loots_structure_rng_seed(int mc, unsigned long long seed,
                                            int chunk_x, int chunk_z);

/** 结构 postProcess 用的战利品随机流种子（population seed + salt）。 */
unsigned long long loots_structure_loot_seed(int mc, unsigned long long seed,
                                             int x, int z, int stype);

/** Same as loots_structure_loot_seed with an explicit step/index salt. */
unsigned long long loots_structure_loot_seed_index(int mc, unsigned long long seed,
                                                   int x, int z, int generation_step,
                                                   int decorator_index);

/** 该位置的生物群系 id（-1 = 失败）。 */
int loots_biome_at(int mc, unsigned long long seed, int x, int z);

/** Exact 1:1 biome at a block position (used for beached shipwreck). */
int loots_biome_exact(int mc, unsigned long long seed, int x, int z);

/** Exact 1:1 biome at an arbitrary Y (used for cave/biome-gated loot). */
int loots_biome_exact_y(int mc, unsigned long long seed, int x, int y, int z);

/** 1 for warm/lukewarm/deep_lukewarm/deep_warm ocean, 0 for cold. */
int loots_biome_is_warm_ocean(int mc, unsigned long long seed, int x, int z);

/** 1 when the biome at (x,y,z) is in the 26.x `mineshaft_blocking` tag (deep_dark). */
int loots_biome_is_deep_dark(int mc, unsigned long long seed, int x, int y, int z);

/** cubiomes biome2str() for the noise biome at (x,y,z); NULL when unavailable. */
const char *loots_biome_name_at(int mc, unsigned long long seed, int x, int y, int z);

typedef struct loots_found { int x; int z; } loots_found;

/**
 * Mineshaft positions in an inclusive chunk rectangle (cubiomes getMineshafts).
 * Mineshafts are chunk decorators, so loots_find_structures() cannot find them.
 * Returns the number written to `out`.
 */
int loots_mineshaft_positions(int mc, unsigned long long seed, int chunk_x0, int chunk_z0,
                              int chunk_x1, int chunk_z1, loots_found *out, int capacity);

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
