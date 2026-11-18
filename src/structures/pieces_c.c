/* See pieces_c.h for why this is C and not C++. */
#include "pieces_c.h"

#include <stdio.h>
#include <string.h>

#include "finders.h"

extern unsigned long long loots_decoration_seed_value(unsigned long long seed, int x, int z, int xoroshiro);
#include "features/fortress.h"

/*
 * Chest loot tables and seeds are only filled by the generic entry point
 * getStructurePieces() (the raw feature generators create the pieces but leave
 * lootTables/lootSeeds empty). That function also needs the structure salt
 * config for the version, e.g. ss_bastion_remnant_1194 = {4, 13}.
 */
#define FORTRESS_MAX_PIECES 512

static Piece g_pieces[FORTRESS_MAX_PIECES];

static void collect(const Piece *pieces, int count, loots_piece_result *out)
{
    int i, first = 1;
    memset(out, 0, sizeof(*out));
    out->pieces = count;
    out->exists = count > 0;
    for (i = 0; i < count; i++) {
        const Piece *p = &pieces[i];
        int c;
        if (first) {
            out->min_x = p->bb0.x;
            out->min_y = p->bb0.y;
            out->min_z = p->bb0.z;
            out->max_x = p->bb1.x;
            out->max_y = p->bb1.y;
            out->max_z = p->bb1.z;
            first = 0;
        } else {
            if (p->bb0.x < out->min_x) out->min_x = p->bb0.x;
            if (p->bb0.y < out->min_y) out->min_y = p->bb0.y;
            if (p->bb0.z < out->min_z) out->min_z = p->bb0.z;
            if (p->bb1.x > out->max_x) out->max_x = p->bb1.x;
            if (p->bb1.y > out->max_y) out->max_y = p->bb1.y;
            if (p->bb1.z > out->max_z) out->max_z = p->bb1.z;
        }
        for (c = 0; c < p->chestCount && c < 4; c++) {
            loots_piece_chest *dst;
            if (!p->lootTables[c] || !p->lootTables[c][0]) continue;
            if (out->chest_count >= LOOTS_PIECE_MAX_CHESTS) break;
            dst = &out->chests[out->chest_count++];
            dst->x = p->chestPoses[c].x;
            /* cubiomes models the horizontal piece layout only; the vertical
             * placement of 1.13+ chests is unavailable, so Y is left at 0. */
            dst->z = p->chestPoses[c].z;
            dst->seed = p->lootSeeds[c];
            snprintf(dst->table, LOOTS_TABLE_NAME_MAX, "%s", p->lootTables[c]);
            snprintf(dst->template_name, LOOTS_TABLE_NAME_MAX, "%s", p->name ? p->name : "");
        }
    }
}

static int generate_biome(int mc, uint64_t seed, int x, int z, int stype, int biome,
                          StructureVariant *sv, loots_piece_result *out)
{
    StructureSaltConfig ssconf;
    StructureVariant zero_sv;
    int n;
    if (!sv) {
        memset(&zero_sv, 0, sizeof(zero_sv));
        sv = &zero_sv;
    }
    if (!getStructureSaltConfig(stype, mc, biome, &ssconf)) return 0;
    memset(g_pieces, 0, sizeof(g_pieces));
    n = getStructurePieces(g_pieces, FORTRESS_MAX_PIECES, stype, ssconf, sv, mc, seed, x, z);
    collect(g_pieces, n, out);
    return out->exists;
}

static int generate(int mc, uint64_t seed, int x, int z, int stype,
                    loots_piece_result *out)
{
    return generate_biome(mc, seed, x, z, stype, 0, NULL, out);
}

static void fortress_chest_pos(const Piece *p, int type, int *x, int *z)
{
    /* Java: CastleSmallCorridor{Left,Right}TurnPiece calls getWorldPos with
     * local (3,2,3) / (1,2,3). getWorldPos maps local x/z according to the
     * piece orientation; the local Y is 2 above the piece's min Y. */
    const int lx = (type == CORRIDOR_TURN_LEFT) ? 3 : 1;
    const int lz = 3;
    switch (p->rot & 3) {
    case 0: // north
        *x = p->bb0.x + lx;
        *z = p->bb1.z - lz;
        break;
    case 1: // east
        *x = p->bb0.x + lz;
        *z = p->bb0.z + lx;
        break;
    case 2: // south
        *x = p->bb0.x + lx;
        *z = p->bb0.z + lz;
        break;
    default: // west
        *x = p->bb1.x - lz;
        *z = p->bb0.z + lx;
        break;
    }
}

int loots_fortress_pieces(int mc, unsigned long long seed, int x, int z,
                          loots_piece_result *out)
{
    StructureSaltConfig ssconf;
    StructureVariant sv;
    int i, n, minY = 0, maxY = 0;
    int dy;
    if (!out) return 0;
    if (!getStructureSaltConfig(Fortress, mc, 0, &ssconf)) return 0;
    memset(&sv, 0, sizeof(sv));
    memset(g_pieces, 0, sizeof(g_pieces));
    n = getStructurePieces(g_pieces, FORTRESS_MAX_PIECES, Fortress, ssconf, &sv,
                           mc, seed, x, z);
    if (n <= 0) return 0;

    for (i = 0; i < n; i++) {
        const Piece *p = &g_pieces[i];
        if (i == 0 || p->bb0.y < minY) minY = p->bb0.y;
        if (i == 0 || p->bb1.y > maxY) maxY = p->bb1.y;
    }
    /* NetherFortressStructure.generatePieces ends with
     * builder.moveInsideHeights(random, 48, 70). A normal fortress is taller
     * than the 23-block window, so heightSpan <= 1 and y0Pos is exactly 48
     * without consuming RNG. */
    dy = (mc >= MC_1_18) ? (48 - minY) : 0;

    memset(out, 0, sizeof(*out));
    out->exists = 1;
    out->pieces = n;
    out->min_x = out->min_z = 0x7fffffff;
    out->min_y = 0x7fffffff;
    out->max_x = out->max_z = -0x7fffffff;
    out->max_y = -0x7fffffff;
    for (i = 0; i < n; i++) {
        const Piece *p = &g_pieces[i];
        if (p->bb0.x < out->min_x) out->min_x = p->bb0.x;
        if (p->bb0.y + dy < out->min_y) out->min_y = p->bb0.y + dy;
        if (p->bb0.z < out->min_z) out->min_z = p->bb0.z;
        if (p->bb1.x > out->max_x) out->max_x = p->bb1.x;
        if (p->bb1.y + dy > out->max_y) out->max_y = p->bb1.y + dy;
        if (p->bb1.z > out->max_z) out->max_z = p->bb1.z;
    }

    for (i = 0; i < n; i++) {
        const Piece *p = &g_pieces[i];
        int cx, cz;
        loots_piece_chest *dst;
        if (!p->chestCount || (p->type != CORRIDOR_TURN_LEFT &&
                               p->type != CORRIDOR_TURN_RIGHT))
            continue;
        if (out->chest_count >= LOOTS_PIECE_MAX_CHESTS) break;
        fortress_chest_pos(p, p->type, &cx, &cz);
        dst = &out->chests[out->chest_count++];
        dst->x = cx;
        dst->y = p->bb0.y + dy + 2;
        dst->z = cz;
        dst->seed = 0; /* recomputed per chunk by structure_pieces.cpp */
        snprintf(dst->table, LOOTS_TABLE_NAME_MAX, "chests/nether_bridge");
        snprintf(dst->template_name, LOOTS_TABLE_NAME_MAX, "%s", p->name ? p->name : "");
    }
    return 1;
}

int loots_end_city_pieces(int mc, unsigned long long seed, int x, int z,
                          loots_piece_result *out)
{
    if (!out) return 0;
    /* The cubiomes chest offsets match the 26.3 in-game chest blocks for the
     * verified seed (the template marker positions are the same modulo the
     * piece-origin convention). Keep the proven coordinates. */
    return generate(mc, seed, x, z, End_City, out);
}

int loots_desert_pieces(int mc, unsigned long long seed, int x, int z,
                        loots_piece_result *out)
{
    if (!out) return 0;
    return generate(mc, seed, x, z, Desert_Pyramid, out);
}

int loots_jungle_pieces(int mc, unsigned long long seed, int x, int z,
                        loots_piece_result *out)
{
    if (!out) return 0;
    return generate(mc, seed, x, z, Jungle_Pyramid, out);
}

int loots_shipwreck_pieces(int mc, unsigned long long seed, int x, int z,
                           loots_piece_result *out)
{
    StructureVariant sv;
    int biome;
    if (!out) return 0;
    biome = loots_biome_exact(mc, seed, x + 9, z + 9);
    if (biome < 0 || !getVariant(&sv, Shipwreck, mc, seed, x, z, biome))
        return 0;
    return generate_biome(mc, seed, x, z, Shipwreck, biome, &sv, out);
}

int loots_mineshaft_pieces(int mc, unsigned long long seed, int x, int z,
                           loots_piece_result *out)
{
    StructureSaltConfig ssconf;
    StructureVariant sv;
    int biome, i, n;
    if (!out) return 0;
    /* The salt depends on the mesa biome category, so the surface biome is the
     * right input here.  Chest Y (below) is the piece floor and is only used to
     * sample the biome for entry conditions such as sulfur_caves. */
    biome = loots_biome_exact(mc, seed, x, z);
    if (biome < 0 || !getStructureSaltConfig(Mineshaft, mc, biome, &ssconf))
        return 0;
    memset(&sv, 0, sizeof(sv));
    memset(g_pieces, 0, sizeof(g_pieces));
    n = getStructurePieces(g_pieces, FORTRESS_MAX_PIECES, Mineshaft, ssconf, &sv,
                           mc, seed, x, z);
    if (n <= 0) return 0;

    memset(out, 0, sizeof(*out));
    out->exists = 1;
    out->pieces = n;
    out->min_x = out->min_z = 0x7fffffff;
    out->min_y = 0x7fffffff;
    out->max_x = out->max_z = -0x7fffffff;
    out->max_y = -0x7fffffff;
    for (i = 0; i < n; i++) {
        const Piece *p = &g_pieces[i];
        int c;
        if (p->bb0.x < out->min_x) out->min_x = p->bb0.x;
        if (p->bb0.y < out->min_y) out->min_y = p->bb0.y;
        if (p->bb0.z < out->min_z) out->min_z = p->bb0.z;
        if (p->bb1.x > out->max_x) out->max_x = p->bb1.x;
        if (p->bb1.y > out->max_y) out->max_y = p->bb1.y;
        if (p->bb1.z > out->max_z) out->max_z = p->bb1.z;

        for (c = 0; c < p->chestCount && c < 4; c++) {
            loots_piece_chest *dst;
            if (!p->lootTables[c] || !p->lootTables[c][0]) continue;
            if (out->chest_count >= LOOTS_PIECE_MAX_CHESTS) break;
            dst = &out->chests[out->chest_count++];
            dst->x = p->chestPoses[c].x;
            /* cubiomes has no chest Y for mineshafts; the corridor floor is at
             * the piece's minimum Y, which is accurate enough for biome
             * sampling (chests sit on that floor). */
            dst->y = p->bb0.y + 1;
            dst->z = p->chestPoses[c].z;
            dst->seed = p->lootSeeds[c];
            snprintf(dst->table, LOOTS_TABLE_NAME_MAX, "%s", p->lootTables[c]);
            snprintf(dst->template_name, LOOTS_TABLE_NAME_MAX, "%s",
                     p->name ? p->name : "");
        }
    }
    return 1;
}

int loots_mineshaft_type(int mc, unsigned long long seed, int x, int z)
{
    const int biome = loots_biome_exact(mc, seed, x, z);
    if (biome < 0) return 0;
    return getCategory(mc, biome) == mesa ? 1 : 0;
}

/* java.util.Random's nextLong(), used by WorldgenRandom.setLargeFeatureSeed().
 * This is deliberately NOT getPopulationSeed(): the latter is the 1.18+
 * Xoroshiro decoration seed and is a different value. */
static uint64_t legacy_next_long(uint64_t *state)
{
    const uint64_t mask = (1ULL << 48) - 1;
    int32_t hi, lo;
    uint64_t s = *state;
    s = (s * 0x5deece66dULL + 0xbULL) & mask;
    hi = (int32_t) (s >> 16);
    s = (s * 0x5deece66dULL + 0xbULL) & mask;
    lo = (int32_t) (s >> 16);
    *state = s;
    return ((uint64_t) (int64_t) hi << 32) + (uint64_t) (int64_t) lo;
}

unsigned long long loots_structure_rng_seed(int mc, unsigned long long seed,
                                            int chunk_x, int chunk_z)
{
    const uint64_t mask = (1ULL << 48) - 1;
    uint64_t state = ((uint64_t) seed ^ 0x5deece66dULL) & mask;
    const uint64_t x_scale = legacy_next_long(&state);
    const uint64_t z_scale = legacy_next_long(&state);
    const uint64_t cx = (uint64_t) (int64_t) chunk_x;
    const uint64_t cz = (uint64_t) (int64_t) chunk_z;
    (void) mc;
    return (unsigned long long) ((cx * x_scale) ^ (cz * z_scale) ^ (uint64_t) seed);
}

unsigned long long loots_structure_loot_seed(int mc, unsigned long long seed,
                                             int x, int z, int stype)
{
    StructureSaltConfig ssconf;
    uint64_t populationSeed;
    if (stype == 13 && mc >= 23) {
        // Ancient city is registered in underground_decoration (step 7) and has
        // been decorator index 0 since 1.19; cubiomes has no entry for it because
        // it does not generate the piece graph itself.
        ssconf = (StructureSaltConfig) {7, 0};
    } else if (stype == 9 && mc >= 35) {
        // 26.3 surface_structures index for woodland_mansion (verified against
        // the 26.3 NBT chest seed at -5474371961803057987).
        ssconf = (StructureSaltConfig) {4, 23};
    } else if (stype == 9 && mc >= 24) {
        // 1.19.4-1.21.11: the surface step gained 18 entries by 26.3, so the
        // 26.3 index 23 maps back to 5. Kept as a best-effort until a real
        // 1.20/1.21 NBT chest seed can be compared.
        ssconf = (StructureSaltConfig) {4, 5};
    } else if (stype == 6 && mc >= 35) {
        // cubiomes has no ocean-ruin post-process salt entry.  The 26.3
        // surface_structures index for ocean_ruin is 26 (verified against the
        // 26.3 server NBT for seed -5474371961803057987).
        ssconf = (StructureSaltConfig) {4, 26};
    } else if (!getStructureSaltConfig(stype, mc, 0, &ssconf)) {
        return 0;
    }
    populationSeed = loots_decoration_seed_value(seed, x & ~15, z & ~15, mc >= 22);
    return (unsigned long long) (populationSeed + ssconf.decoratorIndex +
                                 10000 * ssconf.generationStep);
}

unsigned long long loots_structure_loot_seed_index(int mc, unsigned long long seed,
                                                   int x, int z, int generation_step,
                                                   int decorator_index)
{
    const uint64_t populationSeed =
        loots_decoration_seed_value(seed, x & ~15, z & ~15, mc >= 22);
    return (unsigned long long) (populationSeed + decorator_index +
                                 10000 * generation_step);
}

int loots_biome_at(int mc, unsigned long long seed, int x, int z)
{
    static Generator gen;
    static int genMc = -1;
    static unsigned long long genSeed = 0;
    if (genMc != mc || genSeed != seed) {
        setupGenerator(&gen, mc, 0);
        applySeed(&gen, DIM_OVERWORLD, seed);
        genMc = mc;
        genSeed = seed;
    }
    return getBiomeAt(&gen, 4, x >> 2, 63, z >> 2);
}

int loots_biome_is_warm_ocean(int mc, unsigned long long seed, int x, int z)
{
    int b = loots_biome_exact(mc, seed, x, z);
    return b == warm_ocean || b == lukewarm_ocean || b == deep_lukewarm_ocean ||
           b == deep_warm_ocean;
}

int loots_biome_exact(int mc, unsigned long long seed, int x, int z)
{
    static Generator gen;
    static int genMc = -1;
    static unsigned long long genSeed = 0;
    if (genMc != mc || genSeed != seed) {
        setupGenerator(&gen, mc, 0);
        applySeed(&gen, DIM_OVERWORLD, seed);
        genMc = mc;
        genSeed = seed;
    }
    return getBiomeAt(&gen, 1, x, 63, z);
}

int loots_biome_exact_y(int mc, unsigned long long seed, int x, int y, int z)
{
    static Generator gen;
    static int genMc = -1;
    static unsigned long long genSeed = 0;
    if (genMc != mc || genSeed != seed) {
        setupGenerator(&gen, mc, 0);
        applySeed(&gen, DIM_OVERWORLD, seed);
        genMc = mc;
        genSeed = seed;
    }
    return getBiomeAt(&gen, 1, x, y, z);
}

int loots_surface_height(int mc, unsigned long long seed, int x, int z)
{
    static Generator hgen;
    static int hgenMc = -1;
    static unsigned long long hgenSeed = 0;
    float y = 0.0f;
    if (hgenMc != mc || hgenSeed != seed) {
        setupGenerator(&hgen, mc, 0);
        applySeed(&hgen, DIM_OVERWORLD, seed);
        hgenMc = mc;
        hgenSeed = seed;
    }
    // 1:4 sampled surface approximation; SurfaceNoise is not needed for >= 1.18.
    if (mapApproxHeight(&y, NULL, &hgen, NULL, x >> 2, z >> 2, 1, 1) != 0)
        return 64;
    return (int) y;
}

int loots_find_structures(int mc, unsigned long long seed, int stype, int dim,
                          int radius, int limit, loots_found *out, int outCapacity)
{
    StructureConfig conf;
    Generator gen;
    int regionBlocks, r, found = 0, rx, rz;
    if (!out || outCapacity <= 0 || limit <= 0) return 0;
    if (!getStructureConfig(stype, mc, &conf)) return 0;
    memset(&gen, 0, sizeof(gen));
    setupGenerator(&gen, mc, 0);
    applySeed(&gen, dim, seed);
    regionBlocks = conf.regionSize * 16;
    r = radius / regionBlocks + 2;
    for (rx = -r; rx <= r && found < limit; rx++) {
        for (rz = -r; rz <= r && found < limit; rz++) {
            Pos pos;
            if (!getStructurePos(stype, mc, seed, rx, rz, &pos)) continue;
            if (pos.x < -radius - 16 || pos.x > radius + 16 ||
                pos.z < -radius - 16 || pos.z > radius + 16) continue;
            if (!isViableStructurePos(stype, &gen, pos.x, pos.z, 0)) continue;
            if (found >= outCapacity) return found;
            out[found].x = pos.x;
            out[found].z = pos.z;
            found++;
        }
    }
    return found;
}
