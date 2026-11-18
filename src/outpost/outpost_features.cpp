#include "outpost_features.h"

#include <vector>

#include "data.hpp"
#include "placer.hpp"

namespace loots {
namespace outpost_detail {

Assembly assembleOutpost(int mc, uint64_t seed, int x, int z)
{
    Assembly out;

    jigsaw::JigsawConfig config;
    config.startPool = "pillager_outpost/base_plates";
    config.maxDepth = 7;                   // pillager_outpost.json "size"
    config.maxDistanceHorizontal = 80;
    config.maxDistanceVertical = 80;
    config.startHeight = 0;                // start_height.absolute
    config.projectStartToHeightmap = true; // project_start_to_heightmap
    config.useExpansionHack = true;        // use_expansion_hack

    const auto &data = jigsaw::dataFor(mc);
    if (!data.available()) return out;

    const auto placement = jigsaw::placeJigsaw(config, mc, seed, x, z);
    if (!placement.exists || placement.pieces.empty()) return out;

    out.exists = true;
    out.pieceCount = int(placement.pieces.size());
    for (const auto &piece : placement.pieces) {
        std::vector<int> templates;
        data.concreteTemplates(piece.element, templates);
        for (int templateIndex : templates) {
            const jigsaw::TemplateData &tmpl = data.templateAt(templateIndex);
            PlacedTemplate placed;
            placed.name = tmpl.name;
            placed.x = piece.origin.x;
            placed.y = piece.origin.y;
            placed.z = piece.origin.z;
            placed.rotation = piece.rotation;
            if (tmpl.name == "pillager_outpost/feature_cage_with_allays") ++out.allayCages;
            out.pieces.push_back(std::move(placed));
        }
    }
    out.hasAllay = out.allayCages > 0;
    return out;
}

} // namespace outpost_detail
} // namespace loots
