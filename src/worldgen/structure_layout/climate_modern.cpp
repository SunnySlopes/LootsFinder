#include "climate_modern.hpp"

namespace geode_layout {

// The map only needs the ancient-city layout at deep-dark depths. The old
// shared cubiomes does not expose the 26.3 climate sampler, so use the
// permissive branch here instead of pulling a second cubiomes copy into loots.
struct ClimateModern::Impl {};

ClimateModern::ClimateModern(uint64_t) : impl_(std::make_unique<Impl>()) {}
ClimateModern::~ClimateModern() = default;

std::pair<float, float> ClimateModern::sample(int, int, int) {
    return {0.0f, 0.0f};
}

bool ClimateModern::allowed(int, int, int) {
    return false;
}
}
