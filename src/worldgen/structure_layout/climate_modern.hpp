#pragma once
#include <cstdint>
#include <memory>
#include <utility>

namespace geode_layout {
class ClimateModern {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    explicit ClimateModern(uint64_t seed);
    ~ClimateModern();
    std::pair<float, float> sample(int x, int y, int z);
    bool allowed(int x, int y, int z);
};
}
