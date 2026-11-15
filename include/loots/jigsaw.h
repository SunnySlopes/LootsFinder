// Jigsaw assembly: template-pool placement for outpost / bastion / ancient city.
//
// STATUS: data access + the pieces that can be verified offline are done; the
// placement loop itself is NOT yet a faithful port of JigsawPlacement.Placer.
// `assembleStructure()` therefore returns `complete = false` and only reports the
// pieces it could place with confidence (the start template). Do not use its
// coordinates for gameplay decisions yet.
//
// The template/pool data lives in a generated header that is NOT committed
// (see .gitignore). Without it the module compiles to inert stubs so a fresh
// checkout still builds; run
//   python tools/extract_jigsaw_templates.py --structure-dir <structure/> \
//          --pool-source <26.3.jar> --out data
// to enable the data path.
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
    /** False when the generated template data is missing. */
    bool dataAvailable = false;
    /** False until the placement loop is a complete, verified port. */
    bool complete = false;
    std::string startPool;
    std::vector<PlacedTemplate> pieces;
};

/** True when data/jigsaw_templates.hpp was present at compile time. */
bool hasJigsawData();

/**
 * Attempt to assemble a structure from `startPool`.
 * `x, z` is the structure's block position, `maxDepth` its jigsaw size.
 */
JigsawAssembly assembleStructure(const char *startPool, int mc, uint64_t seed,
                                 int x, int z, int maxDepth);

/** Number of templates / connectors / pool entries available (0 without data). */
void jigsawStats(int &templates, int &connectors, int &poolEntries);

} // namespace loots
