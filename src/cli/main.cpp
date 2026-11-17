// loots-cli: search / inspect structures from the command line.
//
//   loots-cli capability   --mc 32
//   loots-cli ancient-city --mc 32 --seed 0 --x 100 --z -200
//   loots-cli sponge       --mc 32 --seed 0 --x 100 --z -200
//   loots-cli chest        --table chests/nether_bridge --seed 12345
//   loots-cli fortress     --mc 32 --seed 0 --x 100 --z -200
//   loots-cli end-city     --mc 32 --seed 0 --x 100 --z -200
//   loots-cli info
//
// Everything is printed as JSON on stdout so the output can be piped into other
// tools. Exit codes: 0 success, 1 runtime failure, 2 usage error.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "loots/loot.h"
#include "loots/jigsaw.h"
#include "loots/outpost.h"
#include "loots/bastion.h"
#include "loots/shipwreck.h"
#include "loots/fixed_structures.h"

#include "verify.h"
#include "loots/loots_capi.h"
#include "loots/structure_pieces.h"
#include "loots/ocean_ruin.h"
#include "loots/woodland_mansion.h"
#include "loots/trail_ruins.h"
#include "ancient_geode.h"
#include "pieces_c.h"
#include "stronghold_cmd.h"
#include "placer.hpp"

namespace loots { namespace cli { int cmdAncientGeode(int mc, uint64_t seed, int x, int z); } }

namespace {

void usage()
{
    std::fputs(
        "Usage:\n"
        "  loots-cli info\n"
        "  loots-cli verify --mc <id> --seed <n> [--radius <n>] [--limit <n>] [--feature <key>]\n"
        "  loots-cli capability   --mc <id>\n"
        "  loots-cli ancient-city --mc <id> --seed <n> --x <n> --z <n> [--flags <n>]\n"
        "  loots-cli sponge       --mc <id> --seed <n> --x <n> --z <n> [--flags <n>]\n"
        "  loots-cli chest        --table <name> [--seed <n>] [--mc <id>]\n"
        "  loots-cli fortress     --mc <id> --seed <n> --x <n> --z <n>\n"
        "  loots-cli end-city     --mc <id> --seed <n> --x <n> --z <n>\n"
        "  loots-cli desert-pyramid --mc <id> --seed <n> --x <n> --z <n>\n"
        "  loots-cli jungle-temple  --mc <id> --seed <n> --x <n> --z <n>\n"
        "  loots-cli jigsaw-debug --kind bastion|trail-ruins|outpost --seed <n> --x <n> --z <n>\n"
        "\n"
        "Options:\n"
        "  --mc     Minecraft version id (cubiomes MC_* value, e.g. 32 = 1.21.11)\n"
        "  --seed   world seed (accepts negative decimals)\n"
        "  --x --z  structure origin in blocks\n"
        "  --flags  map flags (largeBiomes etc.), default 0\n",
        stderr);
}

const char *jsonBool(int v)
{
    return v ? "true" : "false";
}

int parseArgs(int argc, char **argv, std::vector<std::string> &pos, std::vector<std::string> &flags)
{
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a.rfind("--", 0) == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", a.c_str());
                return 0;
            }
            flags.push_back(a.substr(2));
            flags.push_back(argv[++i]);
        } else {
            pos.push_back(a);
        }
    }
    return 1;
}

bool flagValue(const std::vector<std::string> &flags, const char *name, long long *out)
{
    for (size_t i = 0; i + 1 < flags.size(); i += 2) {
        if (flags[i] == name) {
            char *end = nullptr;
            const long long v = std::strtoll(flags[i + 1].c_str(), &end, 10);
            if (!end || *end) return false;
            *out = v;
            return true;
        }
    }
    return false;
}

std::string flagString(const std::vector<std::string> &flags, const char *name)
{
    for (size_t i = 0; i + 1 < flags.size(); i += 2) {
        if (flags[i] == name) return flags[i + 1];
    }
    return std::string();
}

int cmdVerify(const std::vector<std::string> &flags)
{
    // Defaults are the current verification target requested for 26.3.
    long long mc = 35, seed = -5474371961803057987LL, radius = 4096, limit = 1;
    flagValue(flags, "mc", &mc);
    flagValue(flags, "seed", &seed);
    flagValue(flags, "radius", &radius);
    flagValue(flags, "limit", &limit);
    const std::string only = flagString(flags, "feature");
    return loots::cli::verifyPoints(static_cast<int>(mc), static_cast<uint64_t>(seed),
                                   static_cast<int>(radius), static_cast<int>(limit),
                                   only.empty() ? nullptr : only.c_str());
}

int cmdInfo()
{
    std::printf("{ \"name\": \"loots\", \"version\": \"%s\", \"capiVersion\": %d }\n",
                loots_version_string(), loots_capi_version());
    return 0;
}

int cmdCapability(const std::vector<std::string> &flags)
{
    long long mc = 0;
    if (!flagValue(flags, "mc", &mc)) {
        std::fputs("capability needs --mc\n", stderr);
        return 2;
    }
    loots_capability cap{};
    loots_capability_query(static_cast<int>(mc), &cap);
    std::printf(
        "{\n"
        "  \"mc\": %lld,\n"
        "  \"cityTier\": %d,\n"
        "  \"citySilence\": %s,\n"
        "  \"citySwiftSneak\": %s,\n"
        "  \"cityGoldenApple\": %s,\n"
        "  \"cityWard\": %s,\n"
        "  \"oreDiamond\": %s,\n"
        "  \"oreLapis\": %s,\n"
        "  \"netherDebris\": %s,\n"
        "  \"supported\": %s\n"
        "}\n",
        mc, cap.city_tier, jsonBool(cap.city_silence), jsonBool(cap.city_swift_sneak),
        jsonBool(cap.city_golden_apple), jsonBool(cap.city_ward), jsonBool(cap.ore_diamond),
        jsonBool(cap.ore_lapis), jsonBool(cap.nether_debris), jsonBool(cap.supported));
    return 0;
}

int cmdAncientCity(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0, x = 0, z = 0, mapFlags = 0;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
        !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
        std::fputs("ancient-city needs --mc --seed --x --z\n", stderr);
        return 2;
    }
    flagValue(flags, "flags", &mapFlags);

    loots_city city{};
    if (!loots_ancient_city_query(static_cast<int>(mc), static_cast<unsigned long long>(seed),
                                  static_cast<unsigned>(mapFlags), static_cast<int>(x),
                                  static_cast<int>(z), &city)) {
        std::fputs("ancient-city query failed\n", stderr);
        return 1;
    }
    std::printf("{\n  \"x\": %d, \"z\": %d, \"chunkX\": %d, \"chunkZ\": %d,\n"
                "  \"exists\": %s, \"pieces\": %d,\n"
                "  \"bounds\": { \"minX\": %d, \"minY\": %d, \"minZ\": %d, \"maxX\": %d, "
                "\"maxY\": %d, \"maxZ\": %d },\n"
                "  \"chestCount\": %d,\n  \"chests\": [\n",
                city.x, city.z, city.chunk_x, city.chunk_z, jsonBool(city.exists), city.pieces,
                city.min_x, city.min_y, city.min_z, city.max_x, city.max_y, city.max_z,
                city.chest_count);
    for (int i = 0; i < city.chest_count; i++) {
        const loots_chest &c = city.chests[i];
        std::printf("    { \"x\": %d, \"y\": %d, \"z\": %d, \"kind\": %d, \"silence\": %s, "
                    "\"ward\": %s, \"goldenApples\": %d, \"lootSeed\": %llu }%s\n",
                    c.x, c.y, c.z, c.kind, jsonBool(c.silence), jsonBool(c.ward),
                    c.golden_apples, c.loot_seed, i + 1 < city.chest_count ? "," : "");
    }
    std::printf("  ]\n}\n");
    return 0;
}

int cmdSponge(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0, x = 0, z = 0, mapFlags = 0;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
        !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
        std::fputs("sponge needs --mc --seed --x --z\n", stderr);
        return 2;
    }
    flagValue(flags, "flags", &mapFlags);

    loots_sponge sp{};
    if (!loots_sponge_query(static_cast<int>(mc), static_cast<unsigned long long>(seed),
                            static_cast<unsigned>(mapFlags), static_cast<int>(x),
                            static_cast<int>(z), &sp)) {
        std::fputs("sponge query failed\n", stderr);
        return 1;
    }
    std::printf("{\n  \"x\": %d, \"z\": %d,\n  \"exists\": %s, \"rooms\": %d, "
                "\"spongeRooms\": %d,\n"
                "  \"bounds\": { \"minX\": %d, \"minY\": %d, \"minZ\": %d, \"maxX\": %d, "
                "\"maxY\": %d, \"maxZ\": %d },\n"
                "  \"sponges\": [\n",
                sp.x, sp.z, jsonBool(sp.exists), sp.rooms, sp.sponge_rooms, sp.min_x, sp.min_y,
                sp.min_z, sp.max_x, sp.max_y, sp.max_z);
    for (int i = 0; i < sp.sponge_count; i++) {
        std::printf("    { \"x\": %d, \"y\": %d, \"z\": %d }%s\n", sp.sponges[i].x,
                    sp.sponges[i].y, sp.sponges[i].z, i + 1 < sp.sponge_count ? "," : "");
    }
    std::printf("  ]\n}\n");
    return 0;
}

void printTrimKinds(uint32_t mask)
{
    std::printf("[");
    bool first = true;
    for (int i = 1; i <= static_cast<int>(loots::TrimKind::eye); i++) {
        if ((mask & (1u << i)) == 0) continue;
        if (!first) std::printf(", ");
        first = false;
        std::printf("\"%s\"", loots::trimKindName(static_cast<loots::TrimKind>(i)));
    }
    std::printf("]");
}

void printSwiftSneak(const std::vector<int> &levels)
{
    std::printf("[");
    for (size_t i = 0; i < levels.size(); i++) {
        std::printf("%s%d", i ? ", " : "", levels[i]);
    }
    std::printf("]");
}

int cmdChest(const std::vector<std::string> &flags)
{
    long long seed = 0, mc = 32;
    const std::string table = flagString(flags, "table");
    flagValue(flags, "seed", &seed);
    flagValue(flags, "mc", &mc);
    if (table.empty()) {
        std::fputs("chest needs --table <name>\n", stderr);
        return 2;
    }
    const loots::ChestReward r =
        loots::evaluateChest(table.c_str(), static_cast<uint64_t>(seed), static_cast<int>(mc));
    std::printf("{\n  \"table\": \"%s\", \"seed\": %lld,\n"
                "  \"evaluated\": %s,\n  \"trim\": %s,\n  \"trimKinds\": ",
                table.c_str(), seed, jsonBool(r.evaluated), jsonBool(r.trim));
    printTrimKinds(r.trimKinds);
    std::printf(",\n  \"swiftSneakLevels\": ");
    printSwiftSneak(r.swiftSneakLevels);
    std::printf(",\n  \"netheriteUpgrade\": %s,\n"
                "  \"goldenApples\": %d,\n  \"snifferEgg\": %s,\n"
                "  \"piglinBanner\": %s\n}\n",
                jsonBool(r.netheriteUpgrade), r.goldenApples, jsonBool(r.snifferEgg),
                jsonBool(r.hasItem(loots::LootItem_PiglinBanner)));
    return 0;
}

/** Shared printer for the two piece-based structures. */
int printStructureLoot(const loots::StructureLoot &sl, const char *suspiciousStatus = nullptr)
{
    std::printf("{\n  \"kind\": \"%s\", \"x\": %d, \"z\": %d,\n"
                "  \"exists\": %s, \"pieces\": %d, \"chests\": %zu,\n"
                "  \"bounds\": { \"minX\": %d, \"minY\": %d, \"minZ\": %d, \"maxX\": %d, "
                "\"maxY\": %d, \"maxZ\": %d },\n",
                sl.kind.c_str(), sl.x, sl.z, jsonBool(sl.exists), sl.pieces, sl.chests.size(),
                sl.minX, sl.minY, sl.minZ, sl.maxX, sl.maxY, sl.maxZ);
    std::printf("  \"loot\": [\n");
    for (size_t i = 0; i < sl.chests.size(); i++) {
        const loots::StructureChest &c = sl.chests[i];
        std::printf("    { \"x\": %d, \"y\": %d, \"z\": %d, \"template\": \"%s\", \"table\": \"%s\", \"lootSeed\": %lld, \"lootSeedU\": %llu, "
                    "\"trim\": %s, \"netherite\": %s, \"goldenApples\": %d, "
                    "\"snifferEgg\": %s, \"piglinBanner\": %s }%s\n",
                    c.x, c.y, c.z, c.templateName.c_str(), c.lootTable.c_str(), static_cast<long long>(c.lootSeed), static_cast<unsigned long long>(c.lootSeed), jsonBool(c.reward.trim),
                    jsonBool(c.reward.netheriteUpgrade), c.reward.goldenApples,
                    jsonBool(c.reward.snifferEgg),
                    jsonBool(c.reward.hasItem(loots::LootItem_PiglinBanner)),
                    i + 1 < sl.chests.size() ? "," : "");
    }
    std::printf("  ],\n");
    if (suspiciousStatus) std::printf("  \"suspiciousStatus\": \"%s\",\n", suspiciousStatus);
    std::printf("  \"suspicious\": [\n");
    for (size_t i = 0; i < sl.suspicious.size(); i++) {
        const loots::StructureChest &c = sl.suspicious[i];
        std::printf("    { \"x\": %d, \"y\": %d, \"z\": %d, \"template\": \"%s\", \"table\": \"%s\", "
                    "\"lootSeed\": %lld, \"evaluated\": %s, \"approximate\": true, \"trimKinds\": ",
                    c.x, c.y, c.z, c.templateName.c_str(), c.lootTable.c_str(),
                    (long long) c.lootSeed, jsonBool(c.reward.evaluated));
        printTrimKinds(c.reward.trimKinds);
        std::printf(",\"piglinBanner\": %s }%s\n",
                    jsonBool(c.reward.hasItem(loots::LootItem_PiglinBanner)),
                    i + 1 < sl.suspicious.size() ? "," : "");
    }
    std::printf("  ]\n}\n");
    return 0;
}

int cmdFortress(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0, x = 0, z = 0;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
        !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
        std::fputs("fortress needs --mc --seed --x --z\n", stderr);
        return 2;
    }
    loots::StructureLoot sl{};
    loots::netherFortressLoot(static_cast<int>(mc), static_cast<uint64_t>(seed),
                              static_cast<int>(x), static_cast<int>(z), sl);
    sl.evaluate(static_cast<int>(mc));
    return printStructureLoot(sl);
}

int cmdEndCity(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0, x = 0, z = 0;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
        !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
        std::fputs("end-city needs --mc --seed --x --z\n", stderr);
        return 2;
    }
    loots::StructureLoot sl{};
    loots::endCityLoot(static_cast<int>(mc), static_cast<uint64_t>(seed),
                       static_cast<int>(x), static_cast<int>(z), sl);
    sl.evaluate(static_cast<int>(mc));
    return printStructureLoot(sl);
}

int cmdSilence(const std::vector<std::string> &flags)
{
#ifndef LOOTS_HAS_GEODE
    std::fputs("silence requires the optional Geode library; for 26.3 use tools/minecraft26_3/loots-26.3.ps1 silence\n", stderr);
    return 1;
#else
    long long mc = 0, seed = 0, radius = 20000;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed)) {
        std::fputs("silence needs --mc --seed [--radius]\n", stderr);
        return 2;
    }
    (void) flagValue(flags, "radius", &radius);
    loots_found cities[512];
    const int n = loots_find_structures(static_cast<int>(mc),
                                        static_cast<uint64_t>(seed),
                                        13 /* Ancient_City */, 0,
                                        static_cast<int>(radius), 512, cities, 512);
    std::printf("{\n  \"kind\": \"ancient_silence\", \"seed\": %lld, \"chests\": [",
                seed);
    bool first = true;
    for (int i = 0; i < n; i++) {
        int pieces = 0;
        const auto chests = loots::cli::ancientCityChestsGeode(
            static_cast<int>(mc), static_cast<uint64_t>(seed), cities[i].x, cities[i].z, pieces);
        for (const auto &c : chests) {
            if (!c.reward.hasTrim(loots::TrimKind::silence)) continue;
            std::printf("%s{\"cityX\": %d, \"cityZ\": %d, \"x\": %d, \"z\": %d, "
                        "\"template\": \"%s\", \"lootSeed\": %lld}",
                        first ? "" : ", ", cities[i].x, cities[i].z, c.x, c.z,
                        c.templateName.c_str(), (long long) c.lootSeed);
            first = false;
        }
    }
    std::printf("]}\n");
    return 0;
#endif
}

int cmdStronghold(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed)) {
        std::fputs("stronghold needs --mc --seed\n", stderr);
        return 2;
    }
    return loots::cli::cmdStronghold(static_cast<int>(mc), static_cast<uint64_t>(seed));
}

int cmdTrailRuins(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0, x = 0, z = 0;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
        !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
        std::fputs("trail-ruins needs --mc --seed --x --z\n", stderr);
        return 2;
    }
    loots::StructureLoot sl{};
    const bool ok = loots::trailRuinsLoot(static_cast<int>(mc), static_cast<uint64_t>(seed),
                                          static_cast<int>(x), static_cast<int>(z), sl);
    if (!ok) return 1;
    sl.evaluate(static_cast<int>(mc));
    return printStructureLoot(sl, "exact");
}

int cmdWoodlandMansion(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0, x = 0, z = 0;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
        !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
        std::fputs("woodland-mansion needs --mc --seed --x --z\n", stderr);
        return 2;
    }
    loots::StructureLoot sl{};
    loots::woodlandMansionLoot(static_cast<int>(mc), static_cast<uint64_t>(seed),
                               static_cast<int>(x), static_cast<int>(z), sl);
    sl.evaluate(static_cast<int>(mc));
    return printStructureLoot(sl);
}

int cmdOceanRuin(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0, x = 0, z = 0;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
        !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
        std::fputs("ocean-ruin needs --mc --seed --x --z\n", stderr);
        return 2;
    }
    loots::StructureLoot sl{};
    loots::oceanRuinLoot(static_cast<int>(mc), static_cast<uint64_t>(seed),
                              static_cast<int>(x), static_cast<int>(z), sl);
    sl.evaluate(static_cast<int>(mc));
    return printStructureLoot(sl);
}

int cmdDesert(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0, x = 0, z = 0;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
        !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
        std::fputs("desert-pyramid needs --mc --seed --x --z\n", stderr);
        return 2;
    }
    loots::StructureLoot sl{};
    loots::desertPyramidLoot(static_cast<int>(mc), static_cast<uint64_t>(seed),
                             static_cast<int>(x), static_cast<int>(z), sl);
    sl.evaluate(static_cast<int>(mc));
    return printStructureLoot(sl);
}

int cmdJungle(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0, x = 0, z = 0;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
        !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
        std::fputs("jungle-temple needs --mc --seed --x --z\n", stderr);
        return 2;
    }
    loots::StructureLoot sl{};
    loots::jungleTempleLoot(static_cast<int>(mc), static_cast<uint64_t>(seed),
                            static_cast<int>(x), static_cast<int>(z), sl);
    sl.evaluate(static_cast<int>(mc));
    return printStructureLoot(sl);
}

int cmdShipwreck(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0, x = 0, z = 0;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
        !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
        std::fputs("shipwreck needs --mc --seed --x --z\n", stderr);
        return 2;
    }
    loots::StructureLoot sl{};
    loots::shipwreckLoot(static_cast<int>(mc), static_cast<uint64_t>(seed),
                         static_cast<int>(x), static_cast<int>(z), sl);
    sl.evaluate(static_cast<int>(mc));
    return printStructureLoot(sl);
}

int cmdJigsaw(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0, x = 0, z = 0, depth = 7;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
        !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
        std::fputs("jigsaw needs --mc --seed --x --z --pool <name> [--depth n]\n", stderr);
        return 2;
    }
    flagValue(flags, "depth", &depth);
    const std::string pool = flagString(flags, "pool");

    int templates = 0, connectors = 0, poolEntries = 0;
    loots::jigsawStats(templates, connectors, poolEntries);
    const loots::JigsawAssembly as =
        loots::assembleStructure(pool.c_str(), static_cast<int>(mc),
                                 static_cast<uint64_t>(seed), static_cast<int>(x),
                                 static_cast<int>(z), static_cast<int>(depth));

    std::printf("{\n  \"dataAvailable\": %s, \"complete\": %s,\n",
                jsonBool(as.dataAvailable), jsonBool(as.complete));
    std::printf("  \"templates\": %d, \"connectors\": %d, \"poolEntries\": %d,\n",
                templates, connectors, poolEntries);
    std::printf("  \"pieces\": %zu,\n  \"list\": [\n", as.pieces.size());
    for (size_t i = 0; i < as.pieces.size(); i++) {
        const loots::PlacedTemplate &p = as.pieces[i];
        std::printf("    { \"name\": \"%s\", \"x\": %d, \"y\": %d, \"z\": %d, \"rot\": %d }%s\n",
                    p.name.c_str(), p.x, p.y, p.z, p.rotation,
                    i + 1 < as.pieces.size() ? "," : "");
    }
    std::printf("  ]\n}\n");
    return 0;
}

int cmdJigsawDebug(const std::vector<std::string> &flags)
{
    long long mc = 35, seed = 0, x = 0, z = 0;
    if (!flagValue(flags, "seed", &seed) || !flagValue(flags, "x", &x) ||
        !flagValue(flags, "z", &z)) {
        std::fputs("jigsaw-debug needs --seed --x --z [--kind bastion|trail-ruins|outpost] "
                   "[--mc 35]\n", stderr);
        return 2;
    }
    flagValue(flags, "mc", &mc);
    const std::string kind = flagString(flags, "kind");

    loots::jigsaw::JigsawConfig config;
    config.maxDistanceHorizontal = 80;
    config.maxDistanceVertical = 80;
    if (kind == "bastion") {
        config.startPool = "bastion/starts";
        config.maxDepth = 6;
        config.startHeight = 33;
    } else if (kind == "trail-ruins") {
        config.startPool = "trail_ruins/tower";
        config.maxDepth = 7;
        config.startHeight = -15;
        config.projectStartToHeightmap = true;
    } else if (kind == "outpost") {
        config.startPool = "pillager_outpost/base_plates";
        config.maxDepth = 7;
        config.startHeight = 0;
        config.projectStartToHeightmap = true;
        config.useExpansionHack = true;
    } else {
        std::fputs("jigsaw-debug needs --kind bastion|trail-ruins|outpost\n", stderr);
        return 2;
    }

    const auto placement = loots::jigsaw::placeJigsaw(
        config, static_cast<int>(mc), static_cast<uint64_t>(seed), static_cast<int>(x),
        static_cast<int>(z));
    const auto &data = loots::jigsaw::data263();
    std::printf("{\n  \"kind\": \"%s\", \"exists\": %s,\n", kind.c_str(),
                jsonBool(placement.exists));
    std::printf("  \"center\": { \"x\": %d, \"y\": %d, \"z\": %d },\n", placement.centerX,
                placement.centerY, placement.centerZ);
    std::printf("  \"pieces\": [\n");
    for (size_t i = 0; i < placement.pieces.size(); i++) {
        const auto &piece = placement.pieces[i];
        std::vector<int> concrete;
        data.concreteTemplates(piece.element, concrete);
        std::printf("    { \"templates\": [");
        for (size_t t = 0; t < concrete.size(); t++)
            std::printf("%s\"%s\"", t ? ", " : "", data.templateAt(concrete[t]).name.c_str());
        std::printf("], \"origin\": { \"x\": %d, \"y\": %d, \"z\": %d }, \"rotation\": \"%s\", "
                    "\"bounds\": { \"minX\": %d, \"minY\": %d, \"minZ\": %d, \"maxX\": %d, "
                    "\"maxY\": %d, \"maxZ\": %d }, \"ground\": %d, \"rigid\": %s, "
                    "\"junctions\": [",
                    piece.origin.x, piece.origin.y, piece.origin.z,
                    loots::jigsaw::rotationName(piece.rotation), piece.box.lo.x, piece.box.lo.y,
                    piece.box.lo.z, piece.box.hi.x, piece.box.hi.y, piece.box.hi.z,
                    piece.groundLevelDelta, jsonBool(piece.rigid));
        for (size_t j = 0; j < piece.junctions.size(); j++) {
            const auto &junction = piece.junctions[j];
            std::printf("%s{ \"x\": %d, \"y\": %d, \"z\": %d, \"deltaY\": %d, \"projection\": "
                        "\"%s\" }",
                        j ? ", " : "", junction.pos.x, junction.pos.y, junction.pos.z,
                        junction.deltaY, loots::jigsaw::projectionName(junction.projection));
        }
        std::printf("] }%s\n", i + 1 < placement.pieces.size() ? "," : "");
    }
    std::printf("  ]\n}\n");
    return 0;
}

int cmdOutpost(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0, x = 0, z = 0;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
        !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
        std::fputs("outpost needs --mc --seed --x --z\n", stderr);
        return 2;
    }
    loots::OutpostLoot o{};
    loots::pillagerOutpostLoot(static_cast<int>(mc), static_cast<uint64_t>(seed),
                               static_cast<int>(x), static_cast<int>(z), o);
    bool hasAny = false;
    for (auto &c : o.chests) {
        c.reward = loots::evaluateChest(c.lootTable.c_str(), c.lootSeed, static_cast<int>(mc));
        if (c.reward.trim) hasAny = true;
    }
    std::printf("{\n  \"exists\": %s, \"hasAllay\": %s, \"allayEvaluated\": %s, \"allayCages\": %d, "
                "\"chests\": %zu, \"hasTrim\": %s,\n",
                jsonBool(o.exists), jsonBool(o.hasAllay), jsonBool(o.allayEvaluated), o.allayCages,
                o.chests.size(), jsonBool(hasAny));
    std::printf("  \"list\": [\n");
    for (size_t i = 0; i < o.chests.size(); i++) {
        const auto &c = o.chests[i];
        std::printf("    { \"x\": %d, \"y\": %d, \"z\": %d, \"table\": \"%s\", \"lootSeed\": %lld, \"trim\": %s }%s\n",
                    c.x, c.y, c.z, c.lootTable.c_str(), (long long) c.lootSeed,
                    jsonBool(c.reward.trim), i + 1 < o.chests.size() ? "," : "");
    }
    std::printf("  ]\n}\n");
    return 0;
}

int cmdBastion(const std::vector<std::string> &flags)
{
    long long mc = 0, seed = 0, x = 0, z = 0;
    if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
        !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
        std::fputs("bastion needs --mc --seed --x --z\n", stderr);
        return 2;
    }
    loots::StructureLoot sl{};
    loots::bastionLoot(static_cast<int>(mc), static_cast<uint64_t>(seed),
                       static_cast<int>(x), static_cast<int>(z), sl);
    sl.evaluate(static_cast<int>(mc));
    return printStructureLoot(sl);
}

} // namespace

int main(int argc, char **argv)
{
    std::vector<std::string> pos, flags;
    if (!parseArgs(argc, argv, pos, flags)) return 2;
    if (pos.empty()) {
        usage();
        return 2;
    }
    const std::string &cmd = pos[0];
    if (cmd == "info") return cmdInfo();
    if (cmd == "verify") return cmdVerify(flags);
    if (cmd == "capability") return cmdCapability(flags);
    if (cmd == "ancient-city") return cmdAncientCity(flags);
    if (cmd == "sponge") return cmdSponge(flags);
    if (cmd == "chest") return cmdChest(flags);
    if (cmd == "fortress") return cmdFortress(flags);
    if (cmd == "end-city") return cmdEndCity(flags);
    // `underwater-ruin` is kept as a deprecated alias for older scripts.
    if (cmd == "ocean-ruin" || cmd == "underwater-ruin") return cmdOceanRuin(flags);
    if (cmd == "woodland-mansion") return cmdWoodlandMansion(flags);
    if (cmd == "trail-ruins") return cmdTrailRuins(flags);
    if (cmd == "stronghold") return cmdStronghold(flags);
    if (cmd == "silence") return cmdSilence(flags);
    if (cmd == "ancient-geode") {
        long long mc = 0, seed = 0, x = 0, z = 0;
        if (!flagValue(flags, "mc", &mc) || !flagValue(flags, "seed", &seed) ||
            !flagValue(flags, "x", &x) || !flagValue(flags, "z", &z)) {
            std::fputs("ancient-geode needs --mc --seed --x --z\n", stderr);
            return 2;
        }
        return loots::cli::cmdAncientGeode(static_cast<int>(mc), static_cast<uint64_t>(seed),
                                    static_cast<int>(x), static_cast<int>(z));
    }
    if (cmd == "shipwreck") return cmdShipwreck(flags);
    if (cmd == "desert-pyramid") return cmdDesert(flags);
    if (cmd == "jungle-temple") return cmdJungle(flags);
    if (cmd == "jigsaw") return cmdJigsaw(flags);
    if (cmd == "jigsaw-debug") return cmdJigsawDebug(flags);
    if (cmd == "outpost") return cmdOutpost(flags);
    if (cmd == "bastion") return cmdBastion(flags);
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        usage();
        return 0;
    }
    std::fprintf(stderr, "unknown command: %s\n", cmd.c_str());
    usage();
    return 2;
}
