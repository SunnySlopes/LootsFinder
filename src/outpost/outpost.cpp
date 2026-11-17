#include "loots/outpost.h"
#include "outpost_features.h"

#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "data.hpp"
#include "loots/rng.h"
#include "pieces_c.h"
#include "processor.hpp"

namespace loots {

bool pillagerOutpostLoot(int mc, uint64_t seed, int x, int z, OutpostLoot &out)
{
    out = OutpostLoot{};
    out.lootSeedExact = true;

    const auto &data = jigsaw::data263();
    if (!data.available()) return false;

    const outpost_detail::Assembly as = outpost_detail::assembleOutpost(mc, seed, x, z);
    if (!as.exists || as.pieces.empty()) return false;

    out.exists = true;
    out.pieces = as.pieceCount;
    out.allayEvaluated = mc <= 35;
    if (out.allayEvaluated && mc >= 23) {
        out.allayCages = as.allayCages;
        out.hasAllay = out.allayCages > 0;
    }

    // StructureStart.placeInChunk uses one post-process RNG stream per chunk.
    // Containers are consumed in piece order and then in block-entity order
    // (already sorted by y, x, z in the generated data).
    //
    // WorldGenRegion.setBlock returns true even when the block state is
    // unchanged, so every successfully placed RandomizableContainer consumes a
    // nextLong() and replaces the block entity.  The last placement at a
    // position owns the final loot seed.  (ListPoolElement places both
    // watchtower and watchtower_overgrown; the overgrown chest, when it survives
    // outpost_rot, overwrites the watchtower chest.)
    struct PlacedContainer {
        int x = 0, y = 0, z = 0;
        std::string table;
        std::string state;
        std::string templateName;
        uint64_t lootSeed = 0;
    };

    std::map<std::pair<int, int>, PostProcessRandom> postRngs;
    std::map<std::tuple<int, int, int>, PlacedContainer> containers;
    for (const auto &piece : as.pieces) {
        const int templateIndex = data.findTemplate(piece.name);
        if (templateIndex < 0) continue;
        const jigsaw::TemplateData &tmpl = data.templateAt(templateIndex);

        const bool rottable = piece.name == "pillager_outpost/watchtower_overgrown";
        for (const jigsaw::ChestData &chest : tmpl.chests) {
            const jigsaw::Vec3 local =
                jigsaw::rotateLocal(chest.pos, piece.rotation);
            const int wx = piece.x + local.x;
            const int wy = piece.y + local.y;
            const int wz = piece.z + local.z;

            // outpost_rot is a BlockRotProcessor(integrity = 0.05).  A removed
            // chest is never placed, so it does not consume a loot seed.
            if (rottable && !jigsaw::blockRotSurvives({wx, wy, wz}, 0.05f)) continue;

            const auto key = std::make_tuple(wx, wy, wz);
            auto existing = containers.find(key);

            const auto chunkKey = std::make_pair(wx >> 4, wz >> 4);
            auto it = postRngs.find(chunkKey);
            if (it == postRngs.end()) {
                it = postRngs
                         .emplace(chunkKey,
                                  PostProcessRandom(
                                      mc, loots_structure_loot_seed(mc, seed, wx, wz,
                                                                    10 /* Outpost */)))
                         .first;
            }
            const uint64_t lootSeed = it->second.nextLong();

            PlacedContainer placed;
            placed.x = wx;
            placed.y = wy;
            placed.z = wz;
            placed.table = chest.table;
            placed.state = chest.state;
            placed.templateName = tmpl.name;
            placed.lootSeed = lootSeed;
            if (existing != containers.end())
                existing->second = std::move(placed);
            else
                containers.emplace(key, std::move(placed));
        }
    }

    for (const auto &entry : containers) {
        const PlacedContainer &placed = entry.second;
        if (placed.table.empty()) continue; // decorative container: RNG only
        StructureChest result;
        result.x = placed.x;
        result.y = placed.y;
        result.z = placed.z;
        result.templateName = placed.templateName;
        result.lootTable = placed.table;
        result.lootSeed = placed.lootSeed;
        out.chests.push_back(std::move(result));
    }
    return true;
}

} // namespace loots
