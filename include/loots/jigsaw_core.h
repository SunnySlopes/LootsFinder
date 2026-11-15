// Generic jigsaw primitives shared by the structure engines.
//
// Provenance / licensing:
//   * The data model (Vec3 / Box / Connector / Piece) and the connector ordering
//     are the standard Minecraft jigsaw concepts, re-implemented here for this
//     module. They are NOT copied from tools/trial.
//   * The RNG below is a plain re-implementation of the well known JDK
//     `java.util.Random` LCG (48-bit state, 31-bit rejection sampling). It
//     deliberately avoids the trial module's precomputed-reciprocal / affine
//     fast paths, so no trial-derived code is required.
//   Result: this file can stay under the module's MIT license.
//
// If you ever copy trial's `bounds_` / `four_steps_` optimisations or its
// `consume_shuffle` helpers into this file, the license of tools/loots must be
// changed to GPL-3.0-only to match tools/trial.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <vector>

namespace loots {
namespace jigsaw {

struct Vec3 {
    int x = 0, y = 0, z = 0;
    bool operator==(const Vec3 &) const = default;
};

struct Box {
    Vec3 lo, hi;

    bool intersects(const Box &b) const
    {
        return hi.x >= b.lo.x && lo.x <= b.hi.x && hi.y >= b.lo.y && lo.y <= b.hi.y &&
               hi.z >= b.lo.z && lo.z <= b.hi.z;
    }
    Box moved(const Vec3 &p) const
    {
        return {{lo.x + p.x, lo.y + p.y, lo.z + p.z}, {hi.x + p.x, hi.y + p.y, hi.z + p.z}};
    }
};

enum class Facing : uint8_t { north, east, south, west, up, down };
enum class Top : uint8_t { up, down, north, east, south, west };

/** One jigsaw block inside a template. */
struct Connector {
    Vec3 pos;
    Facing facing = Facing::north;
    Top top = Top::up;
    const char *name = "";
    const char *target = "";
    const char *pool = "";
    bool rollable = false;
};

/** One template placed in the world. */
struct Piece {
    int templateIndex = -1;
    Box box;
    int rotation = 0;
    /** projection == RIGID; terrain-matching pieces anchor to the surface. */
    bool rigid = true;
};

/** Plain `java.util.Random` LCG. */
class Random {
public:
    explicit Random(uint64_t seed = 0) { seed_with(seed); }

    void seed_with(uint64_t seed) { state_ = (seed ^ 0x5deece66dULL) & kMask; }

    uint32_t bits(int n)
    {
        state_ = (state_ * 0x5deece66dULL + 0xb) & kMask;
        return uint32_t(state_ >> (48 - n));
    }

    /** java.util.Random.nextInt(bound) with the JDK's rejection sampling. */
    int integer(int bound)
    {
        if (bound <= 0) return 0;
        if ((bound & -bound) == bound) return int((uint64_t(bound) * bits(31)) >> 31);
        int b, r;
        do {
            b = int(bits(31));
            r = b % bound;
        } while (b - r + (bound - 1) < 0);
        return r;
    }

    uint64_t long_value()
    {
        const uint64_t hi = bits(32);
        const int64_t lo = int32_t(bits(32));
        return (hi << 32) + uint64_t(lo);
    }

    /** java.util.Collections.shuffle order. */
    template <class Container>
    void shuffle(Container &values)
    {
        for (int n = int(values.size()); n > 1; --n) std::swap(values[n - 1], values[integer(n)]);
    }

private:
    static constexpr uint64_t kMask = (1ULL << 48) - 1;
    uint64_t state_ = 0;
};

/**
 * Connector iteration order used by JigsawPlacement: shuffle everything, then
 * stably sort by selection priority (descending).
 */
inline void connector_order(Random &random, size_t connectorCount,
                            const std::vector<int> &selection, bool uniformPriority,
                            std::vector<int> &order)
{
    order.resize(connectorCount);
    std::iota(order.begin(), order.end(), 0);
    random.shuffle(order);
    if (uniformPriority) return;
    for (size_t i = 1; i < order.size(); ++i) {
        const int id = order[i];
        size_t j = i;
        while (j && selection[size_t(order[j - 1])] < selection[size_t(id)]) {
            order[j] = order[j - 1];
            --j;
        }
        order[j] = id;
    }
}

} // namespace jigsaw
} // namespace loots
