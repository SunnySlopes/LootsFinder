// Random sources used by Minecraft worldgen and loot tables.
//
// Two families matter for the structure work:
//   * LegacyRandom: java.util.Random's 48-bit LCG. Structure placement (the
//     WorldgenRandom created by Structure.makeRandom) and loot-table rolls use
//     this family.
//   * Xoroshiro128++: 1.18+ WorldgenRandom family. Biome decoration and the
//     post-process RNG that assigns LootTableSeed use it.
//
// Keeping both in one place makes the distinction explicit: mixing them is the
// single most common source of "looks plausible but is wrong" structure data.
#pragma once

#include <cstdint>

namespace loots {

class LegacyRandom {
public:
    explicit LegacyRandom(uint64_t seed = 0) { setSeed(seed); }

    void setSeed(uint64_t seed) { state_ = (seed ^ 0x5deece66dULL) & kMask; }

    /** java.util.Random.next(int). */
    int32_t next(int bits);

    /** java.util.Random.nextInt(). */
    int32_t nextInt() { return next(32); }
    /** java.util.Random.nextInt(int). */
    int32_t nextInt(int bound);
    /** java.util.Random.nextLong(). */
    uint64_t nextLong();
    /** java.util.Random.nextFloat(). */
    float nextFloat();
    /** java.util.Random.nextDouble(). */
    double nextDouble();
    /** java.util.Random.nextBoolean(). */
    bool nextBoolean() { return next(1) != 0; }

    uint64_t state() const { return state_; }

private:
    static constexpr uint64_t kMask = (1ULL << 48) - 1;
    uint64_t state_ = 0;
};

class Xoroshiro128 {
public:
    explicit Xoroshiro128(uint64_t seed = 0) { setSeed(seed); }
    Xoroshiro128(uint64_t seedLo, uint64_t seedHi);

    void setSeed(uint64_t seed);

    /** Xoroshiro128PlusPlus.nextLong(). */
    uint64_t nextLong();
    /** XoroshiroRandomSource.nextInt(). */
    int32_t nextInt() { return static_cast<int32_t>(nextLong()); }
    /** XoroshiroRandomSource.nextInt(int), including its rejection loop. */
    int32_t nextInt(int bound);
    float nextFloat();
    double nextDouble();
    bool nextBoolean() { return (nextLong() & 1ULL) != 0; }

private:
    uint64_t lo_ = 0;
    uint64_t hi_ = 0;
};

/** True when this cubiomes mc id uses the 1.18+ Xoroshiro worldgen RNG. */
bool usesXoroshiro(int mc);

/** WorldgenRandom.setLargeFeatureSeed(seed, chunkX, chunkZ), then the RNG it seeds. */
LegacyRandom largeFeatureSeed(int mc, uint64_t seed, int chunkX, int chunkZ);
/** The raw seed value produced by setLargeFeatureSeed (for tests/debug). */
uint64_t largeFeatureSeedValue(int mc, uint64_t seed, int chunkX, int chunkZ);

/** WorldgenRandom.setDecorationSeed(seed, blockX, blockZ), then the RNG it seeds. */
uint64_t decorationSeedValue(uint64_t seed, int blockX, int blockZ, bool xoroshiro);
Xoroshiro128 decorationSeedXoroshiro(uint64_t seed, int blockX, int blockZ);
LegacyRandom decorationSeedLegacy(uint64_t seed, int blockX, int blockZ);

/** setFeatureSeed(decorationSeed, index, step) for the 1.18+ post-process RNG. */
uint64_t featureSeed(uint64_t decorationSeed, int index, int step);
/** Equivalent Xoroshiro source after setFeatureSeed. */
Xoroshiro128 featureSeedXoroshiro(uint64_t decorationSeed, int index, int step);

/**
 * The post-process random for a chunk's feature seed.  Uses Xoroshiro on 1.18+
 * and legacy Java before that.  This is the RNG that assigns chest
 * LootTableSeed and drives suspicious-block processors.
 */
class PostProcessRandom {
public:
    PostProcessRandom(int mc, uint64_t featureSeed);

    int nextInt(int bound);
    uint64_t nextLong();
    float nextFloat();
    bool isXoroshiro() const { return xor_; }

private:
    bool xor_ = true;
    LegacyRandom legacy_;
    Xoroshiro128 xoro_;
};

} // namespace loots
