// Outpost jigsaw assembly (private to the loots library).
//
// This is a faithful port of the parts of `JigsawPlacement.addPieces` that the
// pillager outpost uses, driven by the generated `data/jigsaw_templates.hpp`
// (template sizes, jigsaw blocks, weighted pools).  It exists because the
// outpost's allay cage is only placed when the feature-pool expansion actually
// reaches `pillager_outpost/feature_cage_with_allays`; the chest position alone
// does not tell us that.
//
// Known data gaps are listed in outpost_features.cpp; the important one is the
// terrain heightmap, which the rest of loots only approximates.
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

/** Exact WORLD_SURFACE_WG provider: block Y of the first free block. */
using OutpostSurfaceHeightFn = int (*)(int mc, uint64_t seed, int x, int z);

/** Install (or clear with nullptr) an exact height provider. */
void outpostSetSurfaceHeightProvider(OutpostSurfaceHeightFn fn);

/** True when an exact height provider is installed (allay expansion is only trusted then). */
bool outpostHasSurfaceHeightProvider();

struct Assembly {
    bool exists = false;
    /** True when the allay-cage template was placed by the expansion. */
    bool hasAllay = false;
    std::vector<PlacedTemplate> pieces;
};

/**
 * Assemble a pillager outpost at the structure's start block position.
 * Returns exists=false when the template data is unavailable or the start
 * pool element is empty.
 */
Assembly assembleOutpost(int mc, uint64_t seed, int x, int z);

} // namespace outpost_detail
} // namespace loots
