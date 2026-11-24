#include "loots/loot.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "enchantments.hpp"
#include "loot_tables.hpp"
#include "loot_tables_26_3.hpp"
#include "stronghold_loot_tables.hpp"
#include "loots/rng.h"
#include "util.h"

namespace loots {
namespace {

struct TableData {
    const data::Table *table;
    const data::Pool *pools;
    const data::Entry *entries;
    const data::Op *ops;
    const data::Condition *conditions;
};

/** Exact resource-path match (`chests/...` / `archaeology/...`). */
bool tableNameExact(const char *query, const char *full)
{
    return std::strcmp(full, query) == 0;
}

/** Short-name match for legacy callers that pass only `desert_pyramid`. */
bool tableNameShort(const char *shortName, const char *full)
{
    const char *slash = std::strrchr(full, '/');
    const char *tableShort = slash ? slash + 1 : full;
    return std::strcmp(tableShort, shortName) == 0;
}

TableData findTable(const char *name, int mc)
{
    if (!name || !name[0]) return {};
    const char *slash = std::strrchr(name, '/');
    const char *shortName = slash ? slash + 1 : name;
    // Full paths must match exactly so archaeology/desert_pyramid never hits
    // chests/desert_pyramid through the short-name fallback.
    const bool allowShort = slash == nullptr;

    if (mc == 35 || mc < 0) {
        for (const auto &table : data263::kTables) {
            if (tableNameExact(name, table.name))
                return {&table, data263::kPools, data263::kEntries, data263::kOps,
                        data263::kConditions};
        }
        if (allowShort) {
            for (const auto &table : data263::kTables) {
                if (tableNameShort(shortName, table.name))
                    return {&table, data263::kPools, data263::kEntries, data263::kOps,
                            data263::kConditions};
            }
        }
    }
    for (const auto &table : data_stronghold::kTables) {
        if (!tableNameExact(name, table.name)) continue;
        if (mc < 0 || (mc >= table.mcLo && mc <= table.mcHi))
            return {&table, data_stronghold::kPools, data_stronghold::kEntries,
                    data_stronghold::kOps, nullptr};
    }
    if (allowShort) {
        for (const auto &table : data_stronghold::kTables) {
            if (!tableNameShort(shortName, table.name)) continue;
            if (mc < 0 || (mc >= table.mcLo && mc <= table.mcHi))
                return {&table, data_stronghold::kPools, data_stronghold::kEntries,
                        data_stronghold::kOps, nullptr};
        }
    }
    for (const auto &table : data::kTables) {
        if (!tableNameExact(name, table.name)) continue;
        if (mc < 0 || (mc >= table.mcLo && mc <= table.mcHi))
            return {&table, data::kPools, data::kEntries, data::kOps, data::kConditions};
    }
    if (allowShort) {
        for (const auto &table : data::kTables) {
            if (!tableNameShort(shortName, table.name)) continue;
            if (mc < 0 || (mc >= table.mcLo && mc <= table.mcHi))
                return {&table, data::kPools, data::kEntries, data::kOps, data::kConditions};
        }
    }
    // Do not silently use another version's table; a missing band means the
    // structure/feature is not available in this version.
    return {};
}

bool trackedTrim(data::ItemKind kind)
{
    switch (kind) {
    case data::ItemKind::ward_trim:
    case data::ItemKind::silence_trim:
    case data::ItemKind::vex_trim:
    case data::ItemKind::dune_trim:
    case data::ItemKind::wild_trim:
    case data::ItemKind::rib_trim:
    case data::ItemKind::spire_trim:
    case data::ItemKind::snout_trim:
    case data::ItemKind::coast_trim:
    case data::ItemKind::sentry_trim:
    case data::ItemKind::wayfinder_trim:
    case data::ItemKind::raiser_trim:
    case data::ItemKind::shaper_trim:
    case data::ItemKind::eye_trim:
    case data::ItemKind::host_trim:
    case data::ItemKind::trim_other: return true;
    default: return false;
    }
}

TrimKind trimKindOf(data::ItemKind kind)
{
    switch (kind) {
    case data::ItemKind::ward_trim: return TrimKind::ward;
    case data::ItemKind::silence_trim: return TrimKind::silence;
    case data::ItemKind::vex_trim: return TrimKind::vex;
    case data::ItemKind::dune_trim: return TrimKind::dune;
    case data::ItemKind::wild_trim: return TrimKind::wild;
    case data::ItemKind::rib_trim: return TrimKind::rib;
    case data::ItemKind::spire_trim: return TrimKind::spire;
    case data::ItemKind::snout_trim: return TrimKind::snout;
    case data::ItemKind::coast_trim: return TrimKind::coast;
    case data::ItemKind::sentry_trim: return TrimKind::sentry;
    case data::ItemKind::wayfinder_trim: return TrimKind::wayfinder;
    case data::ItemKind::raiser_trim: return TrimKind::raiser;
    case data::ItemKind::shaper_trim: return TrimKind::shaper;
    case data::ItemKind::host_trim: return TrimKind::host;
    case data::ItemKind::eye_trim: return TrimKind::eye;
    case data::ItemKind::trim_other: return TrimKind::other;
    default: return TrimKind::none;
    }
}

/** Pottery-sherd bit for a generated ItemKind, or 0 when it is not a sherd. */
uint32_t sherdBitOf(data::ItemKind kind)
{
    switch (kind) {
    case data::ItemKind::sherd_burn: return LootSherd_Burn;
    case data::ItemKind::sherd_danger: return LootSherd_Danger;
    case data::ItemKind::sherd_friend: return LootSherd_Friend;
    case data::ItemKind::sherd_heart: return LootSherd_Heart;
    case data::ItemKind::sherd_heartbreak: return LootSherd_Heartbreak;
    case data::ItemKind::sherd_howl: return LootSherd_Howl;
    case data::ItemKind::sherd_sheaf: return LootSherd_Sheaf;
    case data::ItemKind::sherd_flow: return LootSherd_Flow;
    case data::ItemKind::sherd_guster: return LootSherd_Guster;
    case data::ItemKind::sherd_scrape: return LootSherd_Scrape;
    case data::ItemKind::sherd_angler: return LootSherd_Angler;
    case data::ItemKind::sherd_shelter: return LootSherd_Shelter;
    case data::ItemKind::sherd_snort: return LootSherd_Snort;
    case data::ItemKind::sherd_blade: return LootSherd_Blade;
    case data::ItemKind::sherd_explorer: return LootSherd_Explorer;
    case data::ItemKind::sherd_mourner: return LootSherd_Mourner;
    case data::ItemKind::sherd_plenty: return LootSherd_Plenty;
    case data::ItemKind::sherd_archer: return LootSherd_Archer;
    case data::ItemKind::sherd_miner: return LootSherd_Miner;
    case data::ItemKind::sherd_prize: return LootSherd_Prize;
    case data::ItemKind::sherd_skull: return LootSherd_Skull;
    case data::ItemKind::sherd_arms_up: return LootSherd_ArmsUp;
    case data::ItemKind::sherd_brewer: return LootSherd_Brewer;
    default: return 0;
    }
}

int rollCount(LegacyRandom &rng, int lo, int hi){
    if (hi <= lo) return lo;
    return lo + rng.nextInt(hi - lo + 1);
}

/** True when `biome` (cubiomes id) is one of the comma-separated names. */
bool biomeMatches(int mc, int biome, const char *names)
{
    if (biome < 0 || !names) return false;
    const char *current = biome2str(mc, biome);
    if (!current) return false;
    const char *p = names;
    while (*p) {
        const char *comma = std::strchr(p, ',');
        const size_t len = comma ? static_cast<size_t>(comma - p) : std::strlen(p);
        if (len == std::strlen(current) && std::strncmp(p, current, len) == 0) return true;
        if (!comma) break;
        p = comma + 1;
    }
    return false;
}

/**
 * Entry-level condition gate, evaluated before the weighted pick.
 *
 * `unknown` is set for a condition kind we cannot model; the whole chest then
 * reports `evaluated = false` instead of silently dropping a possible item.
 * A biome-gated entry without biome context is simply not eligible: callers
 * without biome data must not report it.
 */
bool entryAllowed(const TableData &tables, const data::Entry &entry, int mc,
                  const LootContext &context, bool &unknown)
{
    if (entry.condCount == 0) return true;
    if (!tables.conditions) {
        unknown = true;
        return false;
    }
    for (int i = 0; i < entry.condCount; i++) {
        const data::Condition &condition =
            tables.conditions[entry.firstCond + static_cast<uint16_t>(i)];
        switch (condition.type) {
        case data::ConditionType::none:
            break;
        case data::ConditionType::biome:
            if (!biomeMatches(mc, context.biome, condition.arg)) return false;
            break;
        case data::ConditionType::unsupported:
        default:
            unknown = true;
            return false;
        }
    }
    return true;
}

bool listHas(const char *const *list, int count, const char *name)
{
    for (int i = 0; i < count; i++)
        if (list[i] && std::strcmp(list[i], name) == 0) return true;
    return false;
}

bool itemSupported(const data::EnchantmentInfo &e, const char *item)
{
    return listHas(e.supported, e.supportedCount, item);
}

bool itemPrimary(const data::EnchantmentInfo &e, const char *item)
{
    if (!itemSupported(e, item)) return false;
    return e.primaryCount == 0 || listHas(e.primary, e.primaryCount, item);
}

int enchantability(const char *item)
{
    for (const auto &i : data::kEnchantItems)
        if (std::strcmp(i.name, item) == 0) return i.enchantability;
    return 0;
}

int minCost(const data::EnchantmentInfo &e, int level)
{
    return e.minBase + e.minPer * (level - 1);
}

int maxCost(const data::EnchantmentInfo &e, int level)
{
    return e.maxBase + e.maxPer * (level - 1);
}

struct EnchantOption {
    int index;
    int level;
};

int weightedPick(LegacyRandom &rng, const std::vector<EnchantOption> &options)
{
    int total = 0;
    for (const auto &o : options) total += data::kEnchantments[o.index].weight;
    if (total <= 0) return -1;
    int pick = rng.nextInt(total);
    for (const auto &o : options) {
        pick -= data::kEnchantments[o.index].weight;
        if (pick < 0) return o.index;
    }
    return options.empty() ? -1 : options.back().index;
}

bool compatible(int a, int b)
{
    if (a == b) return false;
    return (data::kEnchantments[a].exclusiveMask & data::kEnchantments[b].exclusiveMask) == 0;
}

void filterCompatible(std::vector<EnchantOption> &options, int chosen)
{
    options.erase(std::remove_if(options.begin(), options.end(),
                                 [chosen](const EnchantOption &o) { return !compatible(chosen, o.index); }),
                   options.end());
}

void applyEnchantWithLevels(LegacyRandom &rng, const char *item, int level, bool &unknown)
{
    const int ench = enchantability(item);
    if (ench <= 0) return; // non-enchantable: vanilla consumes nothing
    int cost = level + 1 + rng.nextInt(ench / 4 + 1) + rng.nextInt(ench / 4 + 1);
    float span = (rng.nextFloat() + rng.nextFloat() - 1.0f) * 0.15f;
    const float adjusted = static_cast<float>(cost) + static_cast<float>(cost) * span;
    cost = static_cast<int>(std::lround(adjusted));
    if (cost < 1) cost = 1;

    std::vector<EnchantOption> options;
    const bool book = std::strcmp(item, "book") == 0 || std::strcmp(item, "enchanted_book") == 0;
    for (uint8_t i = 0; i < sizeof(data::kOnRandomLoot) / sizeof(data::kOnRandomLoot[0]); i++) {
        const int index = data::kOnRandomLoot[i];
        const auto &e = data::kEnchantments[index];
        if (!book && !itemPrimary(e, item)) continue;
        if (book && !itemSupported(e, item) && std::strcmp(item, "book") != 0) continue;
        for (int lvl = e.maxLevel; lvl >= 1; lvl--) {
            if (cost >= minCost(e, lvl) && cost <= maxCost(e, lvl)) {
                options.push_back({index, lvl});
                break;
            }
        }
    }
    if (options.empty()) return;

    int chosen = weightedPick(rng, options);
    if (chosen < 0) return;
    while (rng.nextInt(50) <= cost) {
        filterCompatible(options, chosen);
        if (options.empty()) break;
        chosen = weightedPick(rng, options);
        if (chosen < 0) break;
        cost /= 2;
    }
    (void)unknown;
}

void applyEnchantRandomly(LegacyRandom &rng, const char *item, const char *arg, bool explicitOne,
                          int lo, int hi, int &chosenLevel, bool &unknown, int mc)
{
    const bool book = std::strcmp(item, "book") == 0 || std::strcmp(item, "enchanted_book") == 0;
    std::vector<int> options;
    if (explicitOne) {
        for (size_t i = 0; i < sizeof(data::kEnchantments) / sizeof(data::kEnchantments[0]); i++)
            if (std::strcmp(data::kEnchantments[i].name, arg) == 0) options.push_back(static_cast<int>(i));
        // Explicit tracked options are all single-enchantment lists.
    } else if (arg && (arg[0] == '#' || std::strcmp(arg, "on_random_loot") == 0)) {
        const char *tag = arg[0] == '#' ? arg + 1 : arg;
        if (std::strcmp(tag, "on_random_loot") != 0) {
            unknown = true;
            return;
        }
        for (uint8_t i = 0; i < sizeof(data::kOnRandomLoot) / sizeof(data::kOnRandomLoot[0]); i++)
            options.push_back(data::kOnRandomLoot[i]);
    } else if (arg && arg[0]) {
        // Comma-separated explicit list (normally one element).
        std::string names(arg);
        size_t start = 0;
        while (start <= names.size()) {
            size_t comma = names.find(',', start);
            std::string name = names.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            for (size_t i = 0; i < sizeof(data::kEnchantments) / sizeof(data::kEnchantments[0]); i++)
                if (std::strcmp(data::kEnchantments[i].name, name.c_str()) == 0) options.push_back(static_cast<int>(i));
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
    } else {
        for (size_t i = 0; i < sizeof(data::kEnchantments) / sizeof(data::kEnchantments[0]); i++)
            options.push_back(static_cast<int>(i));
    }

    std::vector<int> compatibleOptions;
    for (int index : options) {
        const auto &e = data::kEnchantments[index];
        if (!book && !itemSupported(e, item)) continue;
        compatibleOptions.push_back(index);
    }
    if (compatibleOptions.empty()) return;
    const int chosen = compatibleOptions[static_cast<size_t>(rng.nextInt(static_cast<int>(compatibleOptions.size())))];
    const auto &e = data::kEnchantments[chosen];
    int lvlLo = explicitOne ? lo : 1;
    int lvlHi = explicitOne ? hi : e.maxLevel;
    if (lvlHi < lvlLo) lvlHi = lvlLo;
    chosenLevel = mc == 35 ? rollCount(rng, lvlLo, lvlHi) : lvlLo + rng.nextInt(lvlHi - lvlLo + 1);
    (void)unknown;
}


} // namespace

const char *trimKindName(TrimKind kind)
{
    switch (kind) {
    case TrimKind::ward: return "ward";
    case TrimKind::silence: return "silence";
    case TrimKind::vex: return "vex";
    case TrimKind::dune: return "dune";
    case TrimKind::wild: return "wild";
    case TrimKind::rib: return "rib";
    case TrimKind::spire: return "spire";
    case TrimKind::snout: return "snout";
    case TrimKind::coast: return "coast";
    case TrimKind::sentry: return "sentry";
    case TrimKind::wayfinder: return "wayfinder";
    case TrimKind::raiser: return "raiser";
    case TrimKind::shaper: return "shaper";
    case TrimKind::host: return "host";
    case TrimKind::eye: return "eye";
    case TrimKind::other: return "other_trim";
    default: return "none";
    }
}

bool hasLootTable(const char *tableName, int mc)
{
    return findTable(tableName, mc).table != nullptr;
}

ChestReward evaluateChest(const char *tableName, uint64_t lootSeed, int mc,
                          const LootContext &context)
{
    ChestReward out;
    if (mc == 35 && lootSeed == 0) return out;
    const auto tables = findTable(tableName, mc);
    const data::Table *table = tables.table;
    if (!table) return out;

    LegacyRandom rng(lootSeed);
    bool unknown = false;

    for (int p = 0; p < table->count && !unknown; p++) {
        const data::Pool &pool = tables.pools[table->first + p];
        const int rolls = rollCount(rng, pool.rollMin, pool.rollMax);
        int totalWeight = 0;
        for (int i = 0; i < pool.count; i++) {
            const data::Entry &candidate = tables.entries[pool.first + i];
            if (entryAllowed(tables, candidate, mc, context, unknown))
                totalWeight += candidate.weight;
            if (unknown) break;
        }
        if (unknown) break;
        if (totalWeight <= 0) continue;

        for (int r = 0; r < rolls && !unknown; r++) {
            int pick = mc == 35 && pool.count == 1 ? 0 : rng.nextInt(totalWeight);
            const data::Entry *entry = nullptr;
            for (int i = 0; i < pool.count; i++) {
                const data::Entry &candidate = tables.entries[pool.first + i];
                if (!entryAllowed(tables, candidate, mc, context, unknown)) {
                    if (unknown) break;
                    continue;
                }
                pick -= candidate.weight;
                if (pick < 0) {
                    entry = &candidate;
                    break;
                }
            }
            if (unknown) break;
            if (!entry) continue;

            int count = 1;
            bool swiftSneak = false;
            int swiftSneakLevel = 0;

            for (int o = 0; o < entry->opCount; o++) {
                const data::Op &op = tables.ops[entry->firstOp + o];
                switch (op.code) {
                case data::OpCode::set_count:
                    if (op.flags & 2) count = rollCount(rng, op.min, op.max);
                    else count = op.min;
                    if (op.flags & 1) {
                        // "add" is only used on untracked items in the tables we
                        // currently consume; keep the RNG exact and treat the
                        // resulting count as additive to the default 1.
                        count += 1;
                    }
                    break;
                case data::OpCode::set_damage:
                    // MathHelper.nextFloat(random,min,max) does not consume RNG
                    // when min >= max.
                    if ((op.flags & 2) && op.max > op.min) (void) rng.nextFloat();
                    break;
                case data::OpCode::enchant_randomly: {
                    int chosenLevel = 0;
                    applyEnchantRandomly(rng, entry->item, op.arg, (op.flags & 1) != 0,
                                         op.min, op.max, chosenLevel, unknown, mc);
                    if ((op.flags & 1) && op.arg && std::strcmp(op.arg, "swift_sneak") == 0) {
                        swiftSneakLevel = chosenLevel;
                        swiftSneak = true;
                    }
                    break;
                }
                case data::OpCode::set_stew_effect: {
                    if (std::strcmp(entry->item, "suspicious_stew") != 0) break;
                    int vals[32];
                    int n = 0;
                    const char *q = op.arg;
                    while (q && *q && n < 32) {
                        vals[n++] = static_cast<int>(std::strtol(q, const_cast<char **>(&q), 10));
                        if (*q == ',') q++;
                        else break;
                    }
                    if (n < 1 || vals[0] <= 0) break;
                    const int count = vals[0];
                    const int chosen = rng.nextInt(count);
                    const int base = 1 + chosen * 2;
                    if (base + 1 < n) {
                        const int lo = vals[base];
                        const int hi = vals[base + 1];
                        if (hi >= lo) (void) rng.nextInt(hi - lo + 1);
                    }
                    break;
                }
                case data::OpCode::enchant_with_levels:
                    applyEnchantWithLevels(rng, entry->item,
                                            (op.flags & 2) ? (op.min + rng.nextInt(op.max - op.min + 1)) : op.min,
                                            unknown);
                    break;
                case data::OpCode::set_instrument:
                    (void) rng.nextInt(op.min);
                    break;
                case data::OpCode::unsupported:
                default:
                    unknown = true;
                    break;
                }
                if (unknown) break;
            }
            if (unknown) break;

            if (swiftSneak) out.swiftSneakLevels.push_back(swiftSneakLevel);
            switch (entry->kind) {
            case data::ItemKind::enchanted_golden_apple:
                out.goldenApples += count;
                out.miscKinds |= LootMisc_EnchantedGoldenApple;
                break;
            case data::ItemKind::sniffer_egg:
                out.snifferEgg = true;
                out.miscKinds |= LootMisc_SnifferEgg;
                break;
            case data::ItemKind::netherite_upgrade: out.netheriteUpgrade = true; break;
            case data::ItemKind::piglin_banner:
                out.itemKinds |= LootItem_PiglinBanner; // deprecated mirror
                out.miscKinds |= LootMisc_PiglinBanner;
                break;
            case data::ItemKind::disc_pigstep: out.discKinds |= LootDisc_Pigstep; break;
            case data::ItemKind::disc_otherside: out.discKinds |= LootDisc_Otherside; break;
            case data::ItemKind::disc_relic: out.discKinds |= LootDisc_Relic; break;
            case data::ItemKind::disc_creator_music_box:
                out.discKinds |= LootDisc_CreatorMusicBox;
                break;
            case data::ItemKind::disc_bounce: out.discKinds |= LootDisc_Bounce; break;
            case data::ItemKind::disc_fragment_5:
                out.discKinds |= LootDisc_Fragment5;
                out.discFragments = static_cast<uint16_t>(out.discFragments + count);
                break;
            default: {
                const uint32_t sherd = sherdBitOf(entry->kind);
                if (sherd) {
                    out.sherdKinds |= sherd;
                } else if (trackedTrim(entry->kind)) {
                    out.trim = true;
                    const TrimKind kind = trimKindOf(entry->kind);
                    out.trimKinds |= (1u << static_cast<unsigned>(kind));
                }
                break;
            }
            }
        }
    }

    out.evaluated = !unknown;
    return out;
}

} // namespace loots
