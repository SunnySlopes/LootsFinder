// Chest-level loot evaluation.
//
// A chest stores a loot table name plus a `LootTableSeed`. Vanilla rolls the
// table with that seed, so the only way to know whether a specific item appears
// is to replay the roll sequence exactly:
//
//   per pool : rolls = uniform(min, max)         -> 1 nextInt
//   per roll : entry = weighted pick(totalWeight) -> 1 nextInt
//   per roll : item functions (set_count etc.)    -> may consume more
//
// The tables come from `data/loot_tables.hpp`, generated from the vanilla jar by
// tools/extract_loot_tables_v2.py. Entries we do not care about keep their real
// weight and every RNG-consuming function, so the roll sequence stays exact.
#pragma once

#include <cstdint>
#include <vector>

namespace loots {

/** Armor-trim / smithing-template kinds tracked by the finder. */
enum class TrimKind : uint8_t {
    none = 0,
    ward,
    silence,
    vex,
    dune,
    wild,
    rib,
    spire,
    snout,
    coast,
    sentry,
    wayfinder,
    raiser,
    shaper,
    host,
    other,
    eye,
};

const char *trimKindName(TrimKind kind);

/**
 * Which of the tracked rewards a chest produced.
 *
 * When `evaluated` is false, the fields contain only rewards found before an
 * unsupported operation. A missing table or unseeded 26.3 container also
 * returns `evaluated = false`.
 */
struct ChestReward {
    bool trim = false;              // any armour trim template
    uint32_t trimKinds = 0;         // bit (1 << int(TrimKind)) per found kind
    bool netheriteUpgrade = false;  // netherite upgrade template
    int goldenApples = 0;           // enchanted golden apples (set_count aware)
    bool snifferEgg = false;
    std::vector<int> swiftSneakLevels;
    bool evaluated = false;

    bool hasTrim(TrimKind kind) const
    {
        return (trimKinds & (1u << static_cast<unsigned>(kind))) != 0;
    }
};

/**
 * Replay `tableName` with `lootSeed`. The loot-table RNG is always legacy
 * java.util.Random (`RandomSource.create(seed)`), independent of the worldgen
 * RNG family when the seed is nonzero. Minecraft 26.3 reserves zero to use
 * the world's random sequence.
 */
ChestReward evaluateChest(const char *tableName, uint64_t lootSeed, int mc);

/** True when the table exists in the compiled data for this version. */
bool hasLootTable(const char *tableName, int mc);
/** Compatibility helper: true when any version band defines the table. */
inline bool hasLootTable(const char *tableName) { return hasLootTable(tableName, -1); }

} // namespace loots
