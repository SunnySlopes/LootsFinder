#include "processor.hpp"

#include "loots/rng.h"

namespace loots {
namespace jigsaw {

int64_t mthGetSeed(int x, int y, int z)
{
    const int32_t xTerm = static_cast<int32_t>(static_cast<uint32_t>(x) * 3129871u);
    const int64_t seed =
        static_cast<int64_t>(xTerm) ^ (static_cast<int64_t>(z) * 116129781LL) ^
        static_cast<int64_t>(y);
    uint64_t value = static_cast<uint64_t>(seed) * static_cast<uint64_t>(seed);
    value *= 42317861ULL;
    value += static_cast<uint64_t>(seed) * 11ULL;
    return static_cast<int64_t>(value) >> 16;
}

bool blockRotSurvives(const Vec3 &pos, float integrity)
{
    LegacyRandom random(static_cast<uint64_t>(mthGetSeed(pos.x, pos.y, pos.z)));
    // BlockRotProcessor removes the block when nextFloat() > integrity.
    return random.nextFloat() <= integrity;
}

uint64_t legacySeedAt(const Vec3 &pos)
{
    LegacyRandom random(static_cast<uint64_t>(mthGetSeed(pos.x, pos.y, pos.z)));
    return random.nextLong();
}

} // namespace jigsaw
} // namespace loots
