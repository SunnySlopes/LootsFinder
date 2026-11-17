#include "terrain.hpp"

#include "loots/worldgen.h"

namespace loots {
namespace jigsaw {

int worldSurfaceHeight(int mc, uint64_t seed, int x, int z)
{
    return loots::worldSurfaceHeight(mc, seed, x, z);
}

} // namespace jigsaw
} // namespace loots
