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

int loots_fortress_pieces(int mc, unsigned long long seed, int x, int z,
                          loots_piece_result *out)
{
    if (!out) return 0;
    /* cubiomes' fortress chest positions were confirmed correct by in-game
     * verification for 26.3; do not replace them with the source-piece
     * transform here. The remaining issue is loot replay/template detection. */
    return generate(mc, seed, x, z, Fortress, out);
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
