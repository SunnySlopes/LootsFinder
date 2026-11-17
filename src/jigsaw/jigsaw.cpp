#include "loots/jigsaw.h"

#include <cstring>
#include <string>
#include <vector>

#include "data.hpp"
#include "placer.hpp"

// Compatibility wrapper for the historical public API.  The actual placement
// work is done by the single shared engine in src/jigsaw/placer.cpp.
namespace loots {
namespace {

jigsaw::JigsawConfig configForPool(const char *startPool, int maxDepth)
{
    jigsaw::JigsawConfig config;
    config.startPool = startPool;
    config.maxDepth = maxDepth;
    config.maxDistanceHorizontal = 80;
    config.maxDistanceVertical = 80;
    const std::string pool = startPool ? startPool : "";
    if (pool.find("trail_ruins") != std::string::npos) {
        config.startHeight = -15;
        config.projectStartToHeightmap = true;
    } else if (pool.find("pillager_outpost") != std::string::npos) {
        config.startHeight = 0;
        config.projectStartToHeightmap = true;
        config.useExpansionHack = true;
    } else if (pool.find("bastion") != std::string::npos) {
        config.startHeight = 33;
    }
    return config;
}

} // namespace

bool hasJigsawData() { return jigsaw::data263().available(); }

void jigsawStats(int &templates, int &connectors, int &poolEntries)
{
    const auto &data = jigsaw::data263();
    templates = data.templateCount();
    connectors = data.blockCount();
    poolEntries = data.poolCount();
}

JigsawAssembly assembleStructure(const char *startPool, int mc, uint64_t seed, int x, int z,
                                 int maxDepth)
{
    JigsawAssembly out;
    out.startPool = startPool ? startPool : "";
    const auto &data = jigsaw::data263();
    out.dataAvailable = data.available();
    if (!out.dataAvailable || !startPool) return out;

    const auto placement =
        jigsaw::placeJigsaw(configForPool(startPool, maxDepth), mc, seed, x, z);
    if (!placement.exists) return out;

    for (const auto &piece : placement.pieces) {
        std::vector<int> templates;
        data.concreteTemplates(piece.element, templates);
        for (int templateIndex : templates) {
            const auto &tmpl = data.templateAt(templateIndex);
            PlacedTemplate placed;
            placed.name = tmpl.name;
            placed.x = piece.origin.x;
            placed.y = piece.origin.y;
            placed.z = piece.origin.z;
            placed.maxX = piece.box.hi.x;
            placed.maxY = piece.box.hi.y;
            placed.maxZ = piece.box.hi.z;
            placed.rotation = piece.rotation;
            out.pieces.push_back(std::move(placed));
        }
    }
    out.complete = true;
    return out;
}

} // namespace loots
