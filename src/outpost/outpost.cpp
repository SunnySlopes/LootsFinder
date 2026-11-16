#include "loots/outpost.h"
#include "outpost_features.h"

#include <map>
#include <set>
#include <string>

#include "loots/jigsaw.h"
#include "loots/rng.h"
#include "loots/transform.h"
#include "pieces_c.h"

#if defined(__has_include)
#if __has_include("chest_layouts.hpp")
#define LOOTS_HAS_CHEST_LAYOUTS 1
#include "chest_layouts.hpp"
#endif
#endif

namespace loots {
namespace {

#if defined(LOOTS_HAS_CHEST_LAYOUTS)

const data::TemplateLayout *findLayout(const std::string &name)
{
    for (const auto &t : data::kTemplateLayouts) {
        if (name == t.name) return &t;
    }
    return nullptr;
}

void rotateLocal(int x, int z, int sx, int sz, int rot, int &ox, int &oz)
{
    switch (rot & 3) {
    case 0: ox = x; oz = z; break;
    case 1: ox = sz - 1 - z; oz = x; break;
    case 2: ox = sx - 1 - x; oz = sz - 1 - z; break;
    default: ox = z; oz = sx - 1 - x; break;
    }
}

#endif

} // namespace

bool pillagerOutpostLoot(int mc, uint64_t seed, int x, int z, OutpostLoot &out)
{
    out = OutpostLoot{};
    out.lootSeedExact = true;
#ifdef LOOTS_HAS_CHEST_LAYOUTS
    const outpost_detail::Assembly as = outpost_detail::assembleOutpost(mc, seed, x, z);
    if (!as.exists || as.pieces.empty()) return false;
    out.exists = true;
    out.allayEvaluated = mc <= 35;
    if (out.allayEvaluated && mc >= 23) {
        out.allayCages = as.allayCages;
        out.hasAllay = out.allayCages > 0;
    }

    std::map<std::pair<int, int>, PostProcessRandom> postRngs;
    std::set<std::pair<int, int>> seenChests;
    for (const auto &p : as.pieces) {
        if (p.name.find("watchtower") == std::string::npos) continue;
        const data::TemplateLayout *layout = findLayout(p.name);
        if (!layout) continue;
        for (int i = 0; i < layout->count; i++) {
            const data::ChestOffset &c = data::kChestOffsets[layout->first + i];
            int ox, oz;
            loots::templateTransform(c.x, c.z, loots::Mirror::none,
                                     loots::rotationByIndex(p.rotation), 0, 0, ox, oz);
            StructureChest chest{};
            chest.x = p.x + ox;
            chest.y = p.y;
            chest.z = p.z + oz;
            if (!seenChests.insert({chest.x, chest.z}).second) continue;
            chest.templateName = p.name;
            chest.lootTable = c.table;
            const auto key = std::make_pair(chest.x >> 4, chest.z >> 4);
            auto it = postRngs.find(key);
            if (it == postRngs.end()) {
                it = postRngs.emplace(key, PostProcessRandom(
                    mc, loots_structure_loot_seed(mc, seed, chest.x, chest.z, 10 /* Outpost */))).first;
            }
            chest.lootSeed = it->second.nextLong();
            out.chests.push_back(chest);
        }
    }
    if (out.chests.empty() && !as.pieces.empty()) {
        const int startRot = as.pieces[0].rotation;
        const data::TemplateLayout *layout = findLayout("pillager_outpost/watchtower");
        if (layout && layout->count > 0) {
            const data::ChestOffset &c = data::kChestOffsets[layout->first];
            int ox, oz;
            loots::templateTransform(c.x, c.z, loots::Mirror::none,
                                     loots::rotationByIndex(startRot), 0, 0, ox, oz);
            StructureChest chest{};
            chest.x = as.pieces[0].x + ox;
            chest.z = as.pieces[0].z + oz;
            chest.templateName = "pillager_outpost/watchtower";
            chest.lootTable = c.table;
            PostProcessRandom rng(mc, loots_structure_loot_seed(mc, seed, chest.x, chest.z, 10));
            chest.lootSeed = rng.nextLong();
            out.chests.push_back(chest);
        }
    }
    return true;
#else
    (void)mc;
    (void)seed;
    (void)x;
    (void)z;
    return false;
#endif
}

} // namespace loots
