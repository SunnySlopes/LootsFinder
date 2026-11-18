#include "loots/rng.h"

#include <bit>
#include <cstdint>

namespace loots {
namespace {

constexpr uint64_t kGoldenRatio64 = 0x9e3779b97f4a7c15ULL;
constexpr uint64_t kSilverRatio64 = 0x6a09e667f3bcc909ULL;

uint64_t mixStafford13(uint64_t z)
{
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/** RandomSupport.upgradeSeedTo128bit(long). */
void upgradeSeedTo128bit(uint64_t seed, uint64_t &lo, uint64_t &hi)
{
    lo = seed ^ kSilverRatio64;
    hi = lo + kGoldenRatio64;
    lo = mixStafford13(lo);
    hi = mixStafford13(hi);
}

/** WorldgenRandom.next(bits): delegate to the raw Xoroshiro nextLong. */
int32_t worldgenNextBits(Xoroshiro128 &random, int bits)
{
    return static_cast<int32_t>(random.nextLong() >> (64 - bits));
}

/** WorldgenRandom.nextLong(): two next(32) calls, not the raw 128-bit long. */
uint64_t worldgenNextLong(Xoroshiro128 &random)
{
    const uint64_t hi = static_cast<uint64_t>(static_cast<int64_t>(worldgenNextBits(random, 32))) << 32;
    const uint64_t lo = static_cast<uint64_t>(static_cast<int64_t>(worldgenNextBits(random, 32)));
    return hi + lo;
}

int32_t worldgenNextInt(Xoroshiro128 &random, int bound)
{
    if (bound <= 0) return 0;
    if ((bound & -bound) == bound)
        return static_cast<int32_t>((static_cast<uint64_t>(bound) * static_cast<uint32_t>(worldgenNextBits(random, 31))) >> 31);
    int32_t bits, val;
    do {
        bits = worldgenNextBits(random, 31);
        val = bits % bound;
    } while (int64_t(bits) - val + (bound - 1) > INT32_MAX);
    return val;
}

float worldgenNextFloat(Xoroshiro128 &random)
{
    return static_cast<float>(worldgenNextBits(random, 24)) * 5.9604645e-8f;
}

} // namespace

int32_t LegacyRandom::next(int bits)
{
    state_ = (state_ * 0x5deece66dULL + 0xbULL) & kMask;
    return static_cast<int32_t>(state_ >> (48 - bits));
}

int32_t LegacyRandom::nextInt(int bound)
{
    if (bound <= 0) return 0;
    if ((bound & -bound) == bound) {
        return static_cast<int32_t>((static_cast<uint64_t>(bound) * static_cast<uint32_t>(next(31))) >> 31);
    }
    int32_t bits, val;
    do {
        bits = next(31);
        val = bits % bound;
    } while (int64_t(bits) - val + (bound - 1) > INT32_MAX);
    return val;
}

uint64_t LegacyRandom::nextLong()
{
    const uint64_t hi = static_cast<uint64_t>(static_cast<int64_t>(next(32))) << 32;
    const uint64_t lo = static_cast<uint64_t>(static_cast<int64_t>(next(32)));
    return hi + lo;
}

float LegacyRandom::nextFloat()
{
    return static_cast<float>(next(24)) * 5.9604645e-8f;
}

double LegacyRandom::nextDouble()
{
    const uint64_t hi = static_cast<uint64_t>(static_cast<int64_t>(next(26))) << 27;
    const uint64_t lo = static_cast<uint64_t>(static_cast<int64_t>(next(27)));
    return static_cast<double>(hi + lo) * 0x1.0p-53;
}

Xoroshiro128::Xoroshiro128(uint64_t seedLo, uint64_t seedHi) : lo_(seedLo), hi_(seedHi)
{
    if ((lo_ | hi_) == 0) {
        lo_ = kGoldenRatio64;
        hi_ = kSilverRatio64;
    }
}

void Xoroshiro128::setSeed(uint64_t seed)
{
    upgradeSeedTo128bit(seed, lo_, hi_);
}

uint64_t Xoroshiro128::nextLong()
{
    const uint64_t s0 = lo_;
    const uint64_t s1 = hi_;
    const uint64_t result = std::rotl(s0 + s1, 17) + s0;
    const uint64_t x = s1 ^ s0;
    lo_ = std::rotl(s0, 49) ^ x ^ (x << 21);
    hi_ = std::rotl(x, 28);
    return result;
}

int32_t Xoroshiro128::nextInt(int bound)
{
    if (bound <= 0) return 0;
    const uint32_t boundU = static_cast<uint32_t>(bound);
    uint32_t randomBits = static_cast<uint32_t>(nextInt());
    uint64_t multiplied = static_cast<uint64_t>(randomBits) * boundU;
    uint32_t fractional = static_cast<uint32_t>(multiplied & 0xffffffffULL);
    const uint32_t unbiased = static_cast<uint32_t>(~boundU + 1u) % boundU;
    while (fractional < unbiased) {
        randomBits = static_cast<uint32_t>(nextInt());
        multiplied = static_cast<uint64_t>(randomBits) * boundU;
        fractional = static_cast<uint32_t>(multiplied & 0xffffffffULL);
    }
    return static_cast<int32_t>(multiplied >> 32);
}

float Xoroshiro128::nextFloat()
{
    return static_cast<float>(nextLong() >> 40) * 5.9604645e-8f;
}

double Xoroshiro128::nextDouble()
{
    return static_cast<double>(nextLong() >> 11) * 0x1.0p-53;
}

bool usesXoroshiro(int mc)
{
    // cubiomes MC_1_18_2 == 22 (MC_UNDEF occupies 0, MC_1_17_1 == 21).
    // All target versions (1.19+) use Xoroshiro for the post-process/decoration
    // random; the placement random is still legacy Java (see largeFeatureSeed).
    return mc >= 22;
}

uint64_t largeFeatureSeedValue(int /*mc*/, uint64_t seed, int chunkX, int chunkZ)
{
    LegacyRandom random(seed);
    const uint64_t xScale = random.nextLong();
    const uint64_t zScale = random.nextLong();
    const uint64_t cx = static_cast<uint64_t>(static_cast<int64_t>(chunkX));
    const uint64_t cz = static_cast<uint64_t>(static_cast<int64_t>(chunkZ));
    return (cx * xScale) ^ (cz * zScale) ^ seed;
}

LegacyRandom largeFeatureSeed(int mc, uint64_t seed, int chunkX, int chunkZ)
{
    return LegacyRandom(largeFeatureSeedValue(mc, seed, chunkX, chunkZ));
}

uint64_t decorationSeedValue(uint64_t seed, int blockX, int blockZ, bool xoroshiro)
{
    uint64_t xScale, zScale;
    if (xoroshiro) {
        Xoroshiro128 random(seed);
        xScale = worldgenNextLong(random) | 1ULL;
        zScale = worldgenNextLong(random) | 1ULL;
    } else {
        LegacyRandom random(seed);
        xScale = random.nextLong() | 1ULL;
        zScale = random.nextLong() | 1ULL;
    }
    const uint64_t bx = static_cast<uint64_t>(static_cast<int64_t>(blockX));
    const uint64_t bz = static_cast<uint64_t>(static_cast<int64_t>(blockZ));
    return (bx * xScale + bz * zScale) ^ seed;
}

Xoroshiro128 decorationSeedXoroshiro(uint64_t seed, int blockX, int blockZ)
{
    return Xoroshiro128(decorationSeedValue(seed, blockX, blockZ, true));
}

LegacyRandom decorationSeedLegacy(uint64_t seed, int blockX, int blockZ)
{
    return LegacyRandom(decorationSeedValue(seed, blockX, blockZ, false));
}

uint64_t featureSeed(uint64_t decorationSeed, int index, int step)
{
    return decorationSeed + static_cast<uint64_t>(static_cast<int64_t>(index)) +
           10000ULL * static_cast<uint64_t>(static_cast<int64_t>(step));
}

Xoroshiro128 featureSeedXoroshiro(uint64_t decorationSeed, int index, int step)
{
    return Xoroshiro128(featureSeed(decorationSeed, index, step));
}

PostProcessRandom::PostProcessRandom(int mc, uint64_t featureSeed)
    : xor_(loots::usesXoroshiro(mc)), legacy_(featureSeed), xoro_(featureSeed)
{
}

int PostProcessRandom::nextInt(int bound)
{
    return xor_ ? worldgenNextInt(xoro_, bound) : legacy_.nextInt(bound);
}

uint64_t PostProcessRandom::nextLong()
{
    return xor_ ? worldgenNextLong(xoro_) : legacy_.nextLong();
}

float PostProcessRandom::nextFloat()
{
    return xor_ ? worldgenNextFloat(xoro_) : legacy_.nextFloat();
}

bool PostProcessRandom::nextBoolean()
{
    return xor_ ? ((xoro_.nextLong() & 1ULL) != 0) : legacy_.nextBoolean();
}

} // namespace loots

extern "C" unsigned long long loots_decoration_seed_value(unsigned long long seed, int x, int z, int xoroshiro)
{
    return loots::decorationSeedValue(seed, x, z, xoroshiro != 0);
}
