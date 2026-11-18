#include "loots/bastion.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "data.hpp"
#include "loots/rng.h"
#include "pieces_c.h"
#include "placer.hpp"

namespace loots {
namespace {

using jigsaw::Data;
using jigsaw::PlacedPiece;

jigsaw::JigsawConfig bastionConfig()
{
    jigsaw::JigsawConfig config;
    config.startPool = "bastion/starts";
    config.maxDepth = 6;               // bastion_remnant.json "size"
    config.maxDistanceHorizontal = 80;
    config.maxDistanceVertical = 80;
    config.startHeight = 33;           // start_height.absolute
    config.projectStartToHeightmap = false;
    config.useExpansionHack = false;
    return config;
}

} // namespace

bool bastionLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.kind = "bastion";

    const auto &data = jigsaw::dataFor(mc);
    if (!data.available()) return false;

    const auto placement = jigsaw::placeJigsaw(bastionConfig(), mc, seed, x, z);
    if (!placement.exists || placement.pieces.empty()) return false;

    out.exists = true;
    out.pieces = int(placement.pieces.size());
    out.minX = out.minY = out.minZ = 0x7fffffff;
    out.maxX = out.maxY = out.maxZ = -0x7fffffff;
    for (const PlacedPiece &piece : placement.pieces) {
        out.minX = std::min(out.minX, piece.box.lo.x);
        out.minY = std::min(out.minY, piece.box.lo.y);
        out.minZ = std::min(out.minZ, piece.box.lo.z);
        out.maxX = std::max(out.maxX, piece.box.hi.x);
        out.maxY = std::max(out.maxY, piece.box.hi.y);
        out.maxZ = std::max(out.maxZ, piece.box.hi.z);
    }

    // StructureStart.placeInChunk consumes one post-process RNG stream per
    // chunk, in piece-list order and then in the template's block-entity order.
    // That order is exactly the order in which the engine expanded pieces and
    // the generated chest list (sorted by y, x, z) exposes them.
    std::map<std::pair<int, int>, PostProcessRandom> postRngs;
    for (const PlacedPiece &piece : placement.pieces) {
        std::vector<int> templates;
        data.concreteTemplates(piece.element, templates);
        for (int templateIndex : templates) {
            const jigsaw::TemplateData &tmpl = data.templateAt(templateIndex);
            for (const jigsaw::ChestData &chest : tmpl.chests) {
                const jigsaw::Vec3 local = jigsaw::rotateLocal(chest.pos, piece.rotation);
                const int wx = piece.origin.x + local.x;
                const int wy = piece.origin.y + local.y;
                const int wz = piece.origin.z + local.z;

                const auto key = std::make_pair(wx >> 4, wz >> 4);
                auto it = postRngs.find(key);
                if (it == postRngs.end()) {
                    it = postRngs
                             .emplace(key, PostProcessRandom(
                                               mc, loots_structure_loot_seed(mc, seed, wx, wz,
                                                                             19 /* Bastion */)))
                             .first;
                }
                const uint64_t lootSeed = it->second.nextLong();
                if (chest.table.empty()) continue; // decorative container: RNG only

                StructureChest result;
                result.x = wx;
                result.y = wy;
                result.z = wz;
                result.templateName = tmpl.name;
                result.lootTable = chest.table;
                result.lootSeed = lootSeed;
                out.chests.push_back(std::move(result));
            }
        }
    }
    return true;
}

} // namespace loots
