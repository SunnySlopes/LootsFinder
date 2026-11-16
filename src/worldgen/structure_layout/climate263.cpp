#include "climate263.hpp"

namespace geode_layout {

// The map only needs the ancient-city layout at deep-dark depths. The old
// shared cubiomes does not expose the 26.3 climate sampler, so use the
// permissive branch here instead of pulling a second cubiomes copy into loots.
struct Climate263::Impl {};

Climate263::Climate263(uint64_t) : impl_(std::make_unique<Impl>()) {}
Climate263::~Climate263() = default;

std::pair<float, float> Climate263::sample(int, int, int) {
    return {0.0f, 0.0f};
}

bool Climate263::allowed(int, int, int) {
    return false;
}
}
