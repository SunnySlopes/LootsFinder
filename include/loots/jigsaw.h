// Jigsaw assembly: compatibility API over the shared exact JigsawPlacement
// port in src/jigsaw/placer.cpp.
//
// The placer reproduces 26.3 JigsawPlacement.addPieces / Placer: start pool,
// optional named start jigsaw, full single/list/empty pool trees, fallbacks,
// weights, placement/selection priorities, the sequenced priority queue, free
// space, the expansion hack, junctions and exact WORLD_SURFACE_WG heights.
//
// The template/pool data is generated from the 26.3 jar by
// tools/extract_jigsaw.py into data/jigsaw_data.hpp.  Without that data the
// module compiles to inert stubs so a fresh checkout still builds.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace loots {

/** One template placed in the world. */
struct PlacedTemplate {
    std::string name;
    int x = 0;   // world position of the template's minimum corner
    int y = 0;
    int z = 0;
    int maxX = 0; // inclusive maximum corner (for the bastion outer frame)
    int maxY = 0;
    int maxZ = 0;
    int rotation = 0;   // 0..3 = N / E / S / W
};

struct JigsawAssembly {
    /** False when the generated 26.3 jigsaw data is missing. */
    bool dataAvailable = false;
    /** True: piece positions come from the exact shared placer. */
    bool complete = false;
    std::string startPool;
    std::vector<PlacedTemplate> pieces;
};

/** True when data/jigsaw_data.hpp was present at compile time. */
bool hasJigsawData();

/**
 * Assemble a structure from `startPool`.
 * `x, z` is the structure's start block position, `maxDepth` its jigsaw size.
 * Structure-specific height projection is inferred from the pool name.
 */
JigsawAssembly assembleStructure(const char *startPool, int mc, uint64_t seed,
                                 int x, int z, int maxDepth);

/** Number of templates / connectors / pools available (0 without data). */
void jigsawStats(int &templates, int &connectors, int &poolEntries);

} // namespace loots
