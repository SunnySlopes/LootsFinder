// Shared exact port of Minecraft 26.3 JigsawPlacement.addPieces / Placer.
//
// This is the only jigsaw placement engine in loots.  Structure adapters supply
// a JigsawConfig and interpret the resulting pieces (chests, processors,
// allays); no structure-specific logic lives here.
#pragma once

#include <cstdint>
#include <vector>

#include "data.hpp"
#include "loots/jigsaw_core.h"

namespace loots {
namespace jigsaw {

struct JigsawConfig {
    const char *startPool = nullptr;
    const char *startJigsaw = nullptr; // nullable
    int maxDepth = 0;
    int maxDistanceHorizontal = 80;
    int maxDistanceVertical = 80;
    int startHeight = 0;               // HeightProvider sample (absolute)
    bool projectStartToHeightmap = false;
    bool useExpansionHack = false;
    int dimensionPaddingBottom = 0;
    int dimensionPaddingTop = 0;
};

struct Junction {
    Vec3 pos;
    int deltaY = 0;
    Projection projection = Projection::rigid;
};

struct PlacedPiece {
    int element = -1;
    /** Template origin (the `BlockPos position` stored in the piece). */
    Vec3 origin;
    int rotation = 0;
    int groundLevelDelta = 1;
    Box box;
    bool rigid = true;
    int depth = 0;
    std::vector<Junction> junctions;
};

struct Placement {
    bool exists = false;
    int centerX = 0;
    int centerY = 0;
    int centerZ = 0;
    std::vector<PlacedPiece> pieces;
};

/**
 * JigsawPlacement.addPieces for one structure start chunk.
 *
 * `x` / `z` are block coordinates of the structure start (the chunk minimum
 * block); they are normalised to the containing chunk internally.
 */
Placement placeJigsaw(const JigsawConfig &config, int mc, uint64_t seed, int x, int z);

/** Direction helpers reused by adapters/debug output. */
char facingLetter(Facing facing);
char topLetter(Top top);
const char *rotationName(int rotation);
const char *projectionName(Projection projection);

} // namespace jigsaw
} // namespace loots
