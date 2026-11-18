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
 * Deprecated merged non-trim mask, kept so older host builds that only read
 * `ChestReward::itemKinds` keep working. New code must use `miscKinds` /
 * `discKinds`; only the piglin banner is still mirrored here.
 */
enum LootItemBits : uint32_t {
    LootItem_PiglinBanner = 1u << 0,
};

/** Music discs / disc fragments, bit flags for `ChestReward::discKinds`. */
enum LootDiscBits : uint32_t {
    LootDisc_Pigstep = 1u << 0,
    LootDisc_Otherside = 1u << 1,
    LootDisc_Relic = 1u << 2,
    LootDisc_CreatorMusicBox = 1u << 3,
    LootDisc_Bounce = 1u << 4,
    /** The chest contains at least one `disc_fragment_5` (count: `discFragments`). */
    LootDisc_Fragment5 = 1u << 5,
};

/** Non-template container loot, bit flags for `ChestReward::miscKinds`. */
enum LootMiscBits : uint32_t {
    LootMisc_PiglinBanner = 1u << 0,
    LootMisc_SnifferEgg = 1u << 1,
    /** Enchanted golden apple; the amount is still in `goldenApples`. */
    LootMisc_EnchantedGoldenApple = 1u << 2,
};

/** UI category bits, see `ChestReward::categoryMask()`. */
enum LootCategoryBits : uint32_t {
    LootCategory_Misc = 1u << 0,
    LootCategory_Template = 1u << 1,
    LootCategory_Disc = 1u << 2,
    /** Reserved for the pottery-sherd plan. */
    LootCategory_Sherd = 1u << 3,
};

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
    /** Deprecated merged mask, see LootItemBits. */
    uint32_t itemKinds = 0;
    /** Music discs and fragments, see LootDiscBits. */
    uint32_t discKinds = 0;
    /** Banner / sniffer egg / golden apple, see LootMiscBits. */
    uint32_t miscKinds = 0;
    /** Reserved for the pottery-sherd plan. */
    uint32_t sherdKinds = 0;
    /** Total number of `disc_fragment_5` items in this container. */
    uint16_t discFragments = 0;
    std::vector<int> swiftSneakLevels;
    bool evaluated = false;

    bool hasTrim(TrimKind kind) const
    {
        return (trimKinds & (1u << static_cast<unsigned>(kind))) != 0;
    }
    bool hasItem(uint32_t bit) const { return (itemKinds & bit) != 0; }
    bool hasDisc(uint32_t bit) const { return (discKinds & bit) != 0; }
    bool hasMisc(uint32_t bit) const { return (miscKinds & bit) != 0; }

    bool hasTemplate() const { return trim || netheriteUpgrade; }
    bool hasDisc() const { return discKinds != 0; }
    bool hasMisc() const { return miscKinds != 0; }

    /** UI category mask: misc / template / disc. */
    uint32_t categoryMask() const
    {
        return (hasMisc() ? LootCategory_Misc : 0u) |
               (hasTemplate() ? LootCategory_Template : 0u) |
               (hasDisc() ? LootCategory_Disc : 0u);
    }
};

/**
 * Lightweight evaluation context.
 *
 * `biome` is a cubiomes biome id at the container position (or -1 when the
 * caller has none). Entries gated by a `location_check` biome condition only
 * match when this is supplied, so a structure layer without biome context never
 * reports, for example, a sulfur-caves-only Bounce.
 */
struct LootContext {
    int biome = -1;
};

/**
 * Replay `tableName` with `lootSeed`. The loot-table RNG is always legacy
 * java.util.Random (`RandomSource.create(seed)`), independent of the worldgen
 * RNG family when the seed is nonzero. Minecraft 26.3 reserves zero to use
 * the world's random sequence.
 *
 * `context` supplies the per-container biome used by entry-level conditions.
 * A missing table (or a version band that has no such table) returns
 * `evaluated = false`; callers must ignore the container rather than treat it
 * as empty.
 */
ChestReward evaluateChest(const char *tableName, uint64_t lootSeed, int mc,
                          const LootContext &context = LootContext{});

/** True when the table exists in the compiled data for this version. */
bool hasLootTable(const char *tableName, int mc);
/** Compatibility helper: true when any version band defines the table. */
inline bool hasLootTable(const char *tableName) { return hasLootTable(tableName, -1); }

} // namespace loots
