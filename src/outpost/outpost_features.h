// Outpost jigsaw assembly (private to the loots library).
//
// The piece graph comes from the shared JigsawPlacement port in
// src/jigsaw/placer.cpp.  This adapter only expands list_pool_element children
// into concrete templates and counts feature_cage_with_allays, because the
// allay cage is a placement result rather than a loot table.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace loots {
namespace outpost_detail {

/** One concrete template placed in the world. List pool elements are expanded. */
struct PlacedTemplate {
    std::string name;
    int x = 0;   // template origin (jigsaw position, not the rotated box corner)
    int y = 0;
    int z = 0;
    int rotation = 0;   // 0..3 = NONE / CW90 / CW180 / CCW90
};

struct Assembly {
    bool exists = false;
    bool hasAllay = false;
    int allayCages = 0;
    std::vector<PlacedTemplate> pieces;
};

/** Assemble a pillager outpost using the built-in exact terrain height. */
Assembly assembleOutpost(int mc, uint64_t seed, int x, int z);

} // namespace outpost_detail
} // namespace loots
