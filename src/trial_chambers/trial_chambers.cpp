#include "loots/trial_chambers.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

#include "layout.hpp"
#include "loots/rng.h"
#include "loots/transform.h"
#include "pieces_c.h"
#include "trial_pots.hpp"

namespace loots {
namespace {

constexpr const char *kPotTable = "pots/trial_chambers/corridor";
/* 1.21+ trial chambers are decorated in generation step 3 with index 4. This
 * was verified against the 26.3 server for seed 0, chunk (-50,-63): all 17
 * pot seeds appear in the stream and match the in-game block entities. */
constexpr int kLootStep = 3;
constexpr int kLootIndex = 4;

const data::TrialTemplateContainers *containerTable(int mc, int &count)
{
    if (mc >= 27) {
        count = static_cast<int>(sizeof(data::kTrialModernContainers) /
                                 sizeof(data::kTrialModernContainers[0]));
        return data::kTrialModernContainers;
    }
    count = static_cast<int>(sizeof(data::kTrialLegacyContainers) /
                             sizeof(data::kTrialLegacyContainers[0]));
    return data::kTrialLegacyContainers;
}

const data::TrialContainerOffset *containerOffsets(int mc, int &count)
{
    if (mc >= 27) {
        count = static_cast<int>(sizeof(data::kTrialModernContainerOffsets) /
                                 sizeof(data::kTrialModernContainerOffsets[0]));
        return data::kTrialModernContainerOffsets;
    }
    count = static_cast<int>(sizeof(data::kTrialLegacyContainerOffsets) /
                             sizeof(data::kTrialLegacyContainerOffsets[0]));
    return data::kTrialLegacyContainerOffsets;
}

} // namespace

bool trialChamberPots(int mc, uint64_t seed, int chunkX, int chunkZ,
                      std::vector<TrialPot> &out)
{
    out.clear();
    if (mc < 26) return false;

    int containerTemplateCount = 0;
    const data::TrialTemplateContainers *containers =
        containerTable(mc, containerTemplateCount);
    int containerOffsetCount = 0;
    const data::TrialContainerOffset *containerList =
        containerOffsets(mc, containerOffsetCount);
    if (!containers || !containerList) return false;

    // trial_data.hpp keeps two template sets: 1.21.0/1.21.1 (legacy) and
    // 1.21.2+ (modern). The layout engine selects them by generation id.
    const int generation = mc >= 27 ? 2 : 1;
    geode_layout::LayoutEngine engine(generation, seed);
    const geode_layout::Chamber chamber = engine.generate(chunkX, chunkZ);
    if (!chamber.exists) return false;

    // The per-chunk post-process RNG assigns one nextLong() to every loot
    // container in the chunk, in piece order and then in template block order.
    // Replaying it gives the exact LootTableSeed of every decorated pot.
    std::map<std::pair<int, int>, PostProcessRandom> rngs;
    auto rngFor = [&](int wx, int wz) -> PostProcessRandom & {
        const auto key = std::make_pair(wx >> 4, wz >> 4);
        auto it = rngs.find(key);
        if (it == rngs.end()) {
            it = rngs
                     .emplace(key, PostProcessRandom(
                                       mc, loots_structure_loot_seed_index(
                                               mc, seed, key.first * 16, key.second * 16,
                                               kLootStep, kLootIndex)))
                     .first;
        }
        return it->second;
    };

    for (const geode_layout::Placed &piece : chamber.placed) {
        if (piece.id < 0 || piece.id >= containerTemplateCount) continue;
        const data::TrialTemplateContainers &tmpl = containers[piece.id];        for (int i = 0; i < tmpl.count; i++) {
            const int index = tmpl.first + i;
            if (index < 0 || index >= containerOffsetCount) continue;
            const data::TrialContainerOffset &local = containerList[index];

            int dx = 0;
            int dz = 0;
            templateTransform(local.x, local.z, Mirror::none, rotationByIndex(piece.rotation),
                              0, 0, dx, dz);
            const int wx = piece.pos.x + dx;
            const int wy = piece.pos.y + local.y;
            const int wz = piece.pos.z + dz;
            const uint64_t lootSeed = static_cast<uint64_t>(rngFor(wx, wz).nextLong());
            // NBT stores the namespaced id; the public table name is the path.
            if (!std::strstr(local.table, kPotTable)) continue;

            TrialPot pot;
            pot.x = wx;
            pot.y = wy;
            pot.z = wz;
            pot.templateName = tmpl.name;
            pot.lootTable = kPotTable;
            pot.lootSeed = lootSeed;
            out.push_back(std::move(pot));
        }
    }

    // Stable presentation order: by Y, then X, then Z. The RNG assignment above
    // already happened in vanilla order.
    std::sort(out.begin(), out.end(), [](const TrialPot &a, const TrialPot &b) {
        if (a.y != b.y) return a.y < b.y;
        if (a.x != b.x) return a.x < b.x;
        return a.z < b.z;
    });
    return !out.empty();
}

} // namespace loots
