#include "verify.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "loots/bastion.h"
#include "loots/fixed_structures.h"
#include "loots/loot.h"
#include "loots/loots.h"
#include "loots/outpost.h"
#include "loots/shipwreck.h"
#include "loots/structure_pieces.h"
#include "loots/ocean_ruin.h"
#include "loots/woodland_mansion.h"
#include "ancient_geode.h"
#include "loots/version.h"

#include "pieces_c.h"

namespace loots {
namespace cli {
namespace {

struct FeatureDef {
    int type;
    const char *key;
    int dim;
    const char *status;
};

constexpr int kOverworld = 0;
constexpr int kNether = -1;
constexpr int kEnd = 1;

// cubiomes StructureType values (see finders.h); mirrored here so this C++
// translation unit does not have to include cubiomes' C headers.
constexpr int kDesert = 1, kJungle = 2, kShipwreck = 7, kMonument = 8, kMansion = 9;
constexpr int kOutpost = 10, kAncient = 13, kFortress = 18, kBastion = 19, kEndCity = 21;
constexpr int kOceanRuin = 6, kTrailRuins = 24;

const FeatureDef kFeatures[] = {
    {kDesert, "desert_pyramid", kOverworld, "exact-cubiomes"},
    {kJungle, "jungle_temple", kOverworld, "exact-cubiomes"},
    {kShipwreck, "shipwreck", kOverworld, "exact-cubiomes"},
    {kMonument, "ocean_monument", kOverworld, "hint-only"},
    {kOutpost, "pillager_outpost", kOverworld, "exact-jigsaw"},
    {kBastion, "bastion", kNether, "exact-jigsaw"},
    {kFortress, "nether_fortress", kNether, "exact-cubiomes"},
    {kEndCity, "end_city", kEnd, "exact-cubiomes"},
    {kAncient, "ancient_city", kOverworld, "exact-geode"},
    {kTrailRuins, "trail_ruins", kOverworld, "exact-jigsaw"},
    {kOceanRuin, "ocean_ruin", kOverworld, "partial-processor"},
    {kMansion, "woodland_mansion", kOverworld, "exact-layout"},
};

const char *dimName(int dim)
{
    switch (dim) {
    case kNether: return "the_nether";
    case kEnd: return "the_end";
    default: return "overworld";
    }
}

void printTrimKinds(const ChestReward &r)
{
    std::printf("[");
    bool first = true;
    for (int i = 1; i <= static_cast<int>(TrimKind::eye); i++) {
        if ((r.trimKinds & (1u << i)) == 0) continue;
        std::printf("%s\"%s\"", first ? "" : ",", trimKindName(static_cast<TrimKind>(i)));
        first = false;
    }
    std::printf("]");
}

void printSwiftSneak(const ChestReward &r)
{
    std::printf("[");
    for (size_t i = 0; i < r.swiftSneakLevels.size(); i++) {
        std::printf("%s%d", i ? "," : "", r.swiftSneakLevels[i]);
    }
    std::printf("]");
}

void printChestJson(const StructureChest &c, bool trailingComma)
{
    const ChestReward &r = c.reward;
    std::printf(
        "{\"x\":%d,\"z\":%d,\"template\":\"%s\",\"table\":\"%s\",\"lootSeed\":%lld,\"lootSeedU\":%llu,"
        "\"evaluated\":%s,\"trim\":%s,\"trimKinds\":",
        c.x, c.z, c.templateName.c_str(), c.lootTable.c_str(), static_cast<long long>(c.lootSeed),
        static_cast<unsigned long long>(c.lootSeed),
        r.evaluated ? "true" : "false", r.trim ? "true" : "false");
    printTrimKinds(r);
    std::printf(",\"swiftSneakLevels\":");
    printSwiftSneak(r);
    std::printf(
        ",\"goldenApples\":%d,\"netherite\":%s,\"snifferEgg\":%s,"
        "\"piglinBanner\":%s}%s",
        r.goldenApples, r.netheriteUpgrade ? "true" : "false",
        r.snifferEgg ? "true" : "false",
        r.hasItem(LootItem_PiglinBanner) ? "true" : "false",
        trailingComma ? "," : "");
}

void beginPoint(bool &first)
{
    if (!first) std::printf(",\n");
    first = false;
}

void printStructureLoot(const StructureLoot &sl, const char *feature, const char *status, int dim,
                        bool &first)
{
    beginPoint(first);
    std::printf(
        "{\"feature\":\"%s\",\"status\":\"%s\",\"dim\":\"%s\",\"x\":%d,\"z\":%d,"
        "\"pieces\":%d,\"bounds\":{\"minX\":%d,\"minY\":%d,\"minZ\":%d,"
        "\"maxX\":%d,\"maxY\":%d,\"maxZ\":%d},\"chests\":[",
        feature, status, dimName(dim), sl.x, sl.z, sl.pieces, sl.minX, sl.minY, sl.minZ,
        sl.maxX, sl.maxY, sl.maxZ);
    for (size_t i = 0; i < sl.chests.size(); i++) {
        printChestJson(sl.chests[i], i + 1 < sl.chests.size());
    }
    std::printf("]}\n");
}


bool emitPoint(const FeatureDef &f, int mc, uint64_t seed, int x, int z, bool &first)
{
    switch (f.type) {
    case kDesert: {
        StructureLoot sl;
        if (!desertPyramidLoot(mc, seed, x, z, sl)) return false;
        sl.evaluate(mc);
        printStructureLoot(sl, f.key, f.status, f.dim, first);
        return true;
    }
    case kJungle: {
        StructureLoot sl;
        if (!jungleTempleLoot(mc, seed, x, z, sl)) return false;
        sl.evaluate(mc);
        printStructureLoot(sl, f.key, f.status, f.dim, first);
        return true;
    }
    case kShipwreck: {
        StructureLoot sl;
        if (!shipwreckLoot(mc, seed, x, z, sl)) return false;
        sl.evaluate(mc);
        printStructureLoot(sl, f.key, f.status, f.dim, first);
        return true;
    }
    case kFortress: {
        StructureLoot sl;
        if (!netherFortressLoot(mc, seed, x, z, sl)) return false;
        sl.evaluate(mc);
        printStructureLoot(sl, f.key, f.status, f.dim, first);
        return true;
    }
    case kEndCity: {
        StructureLoot sl;
        if (!endCityLoot(mc, seed, x, z, sl)) return false;
        sl.evaluate(mc);
        printStructureLoot(sl, f.key, f.status, f.dim, first);
        return true;
    }
    case kBastion: {
        StructureLoot sl;
        if (!bastionLoot(mc, seed, x, z, sl)) return false;
        sl.evaluate(mc);
        printStructureLoot(sl, f.key, f.status, f.dim, first);
        return true;
    }
    case kOutpost: {
        OutpostLoot ol;
        if (!pillagerOutpostLoot(mc, seed, x, z, ol)) return false;
        for (auto &c : ol.chests) {
            c.reward = evaluateChest(c.lootTable.c_str(), c.lootSeed, mc);
            // The shared placer and post-process RNG are exact for 26.3.
            if (!ol.lootSeedExact) c.reward = ChestReward{};
        }
        beginPoint(first);
        std::printf(
            "{\"feature\":\"%s\",\"status\":\"%s\",\"dim\":\"%s\",\"x\":%d,\"z\":%d,"
            "\"exists\":%s,\"hasAllay\":%s,\"allayEvaluated\":%s,\"allayCages\":%d,\"chests\":[",
            f.key, f.status, dimName(f.dim), x, z, ol.exists ? "true" : "false",
            ol.hasAllay ? "true" : "false", ol.allayEvaluated ? "true" : "false", ol.allayCages);
        for (size_t i = 0; i < ol.chests.size(); i++) {
            printChestJson(ol.chests[i], i + 1 < ol.chests.size());
        }
        std::printf("]}\n");
        return true;
    }
    case kMonument: {
        std::vector<std::pair<int, int>> monuments{{x, z}};
        const auto sp = monumentSponge(mc, seed, 0, monuments);
        if (sp.empty()) return false;
        beginPoint(first);
        std::printf(
            "{\"feature\":\"%s\",\"status\":\"%s\",\"dim\":\"%s\",\"x\":%d,\"z\":%d,"
            "\"exists\":%s,\"rooms\":%d,\"spongeRooms\":%d,\"sponges\":[",
            f.key, f.status, dimName(f.dim), x, z, sp[0].exists ? "true" : "false",
            sp[0].rooms, sp[0].spongeRooms);
        for (size_t i = 0; i < sp[0].sponges.size(); i++) {
            const Box3i &b = sp[0].sponges[i];
            std::printf("%s{\"x\":%d,\"y\":%d,\"z\":%d}", i ? "," : "", b.minX, b.minY, b.minZ);
        }
        std::printf("]}\n");
        return true;
    }
    case kMansion: {
        StructureLoot sl{};
        if (!woodlandMansionLoot(mc, seed, x, z, sl)) return false;
        sl.evaluate(mc);
        beginPoint(first);
        std::printf(
            "{\"feature\":\"%s\",\"status\":\"%s\",\"dim\":\"%s\",\"x\":%d,\"z\":%d,"
            "\"exists\":%s,\"pieces\":%d,\"chests\":[",
            f.key, f.status, dimName(f.dim), x, z, sl.exists ? "true" : "false", sl.pieces);
        for (size_t i = 0; i < sl.chests.size(); i++) printChestJson(sl.chests[i], i + 1 < sl.chests.size());
        std::printf("]}\n");
        return true;
    }
    case kOceanRuin: {
        StructureLoot sl{};
        if (!oceanRuinLoot(mc, seed, x, z, sl)) return false;
        sl.evaluate(mc);
        beginPoint(first);
        std::printf(
            "{\"feature\":\"%s\",\"status\":\"%s\",\"dim\":\"%s\",\"x\":%d,\"z\":%d,"
            "\"exists\":%s,\"pieces\":%d,\"chests\":[",
            f.key, f.status, dimName(f.dim), x, z, sl.exists ? "true" : "false", sl.pieces);
        for (size_t i = 0; i < sl.chests.size(); i++) printChestJson(sl.chests[i], i + 1 < sl.chests.size());
        std::printf("]}\n");
        return true;
    }
    case kAncient: {
        int pieces = 0;
        const auto chests = ancientCityChestsGeode(mc, seed, x, z, pieces);
        if (pieces == 0) return false;
        beginPoint(first);
        std::printf(
            "{\"feature\":\"%s\",\"status\":\"%s\",\"dim\":\"%s\",\"x\":%d,\"z\":%d,"
            "\"exists\":true,\"pieces\":%d,\"chests\":[",
            f.key, f.status, dimName(f.dim), x, z, pieces);
        for (size_t i = 0; i < chests.size(); i++) printChestJson(chests[i], i + 1 < chests.size());
        std::printf("]}\n");
        return true;
    }
    default:
        beginPoint(first);
        std::printf(
            "{\"feature\":\"%s\",\"status\":\"%s\",\"dim\":\"%s\",\"x\":%d,\"z\":%d,"
            "\"exists\":true,\"implemented\":false}\n",
            f.key, f.status, dimName(f.dim), x, z);
        return true;
    }
}

} // namespace

int verifyPoints(int mc, uint64_t seed, int radius, int limit, const char *onlyFeature)
{
    if (radius < 64) radius = 64;
    if (radius > 20000) radius = 20000;
    if (limit < 1) limit = 1;
    if (limit > 16) limit = 16;

    std::printf("{\"mc\":%d,\"versionFamily\":\"%s\",\"seed\":%lld,\"seedU\":%llu,\"radius\":%d,\"limit\":%d,\"points\":[\n",
                mc, versionFamilyName(mc), static_cast<long long>(seed),
                static_cast<unsigned long long>(seed), radius, limit);

    bool first = true;
    for (const FeatureDef &f : kFeatures) {
        if (onlyFeature && onlyFeature[0] && std::strcmp(onlyFeature, f.key) != 0) continue;
        loots_found found[64];
        const int n = loots_find_structures(mc, seed, f.type, f.dim, radius, limit, found, 64);
        for (int i = 0; i < n; i++) {
            if (!emitPoint(f, mc, seed, found[i].x, found[i].z, first)) continue;
        }
    }

    std::printf("\n]}\n");
    return 0;
}

} // namespace cli
} // namespace loots
