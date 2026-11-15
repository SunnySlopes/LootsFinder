// LootsFinder - structure *content* evaluation for the All-In-One Finder map.
//
// Scope: this module answers "what is *inside* a structure", never "where is the
// structure". Structure positions, biomes, noise and surface height stay in
// cubiomes; anything this module needs from there is injected by the caller.
//
// Currently owned by this module:
//   - ancient city: loot tier per version, per-chest evaluation (M0 skeleton)
//   - ocean monument: sponge room detection (M0 skeleton)
//
// Planned (see docs/loots-capabilities-plan.md in the host repository):
//   ward/silence/swift-sneak/golden-apple, outpost allay cage, ocean ruin sniffer
//   egg, mansion/bastion/trail-ruins/desert-pyramid/jungle-temple/fortress/
//   end-city/shipwreck/outpost trim detection.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "loots/version.h"

namespace loots {

/** Axis aligned integer box, inclusive on both ends. */
struct Box3i {
    int minX = 0;
    int minY = 0;
    int minZ = 0;
    int maxX = 0;
    int maxY = 0;
    int maxZ = 0;
};

/**
 * Overlay layers owned by this module. The ids are deliberately in a range that
 * cannot collide with cubiomes' StructureType values (0..FEATURE_NUM-1).
 */
enum OverlayFeature {
    Overlay_Ore_Diamond      = 101,   // reserved for the ore phase
    Overlay_Ore_Lapis        = 102,   // reserved for the ore phase
    Overlay_City_Silence     = 103,
    Overlay_City_SwiftSneak  = 104,
    Overlay_Nether_Debris    = 105,   // reserved for the ore phase
    Overlay_City_GoldenApple = 106,
};

/** Ancient city loot tiers; see the host plan, section 5.2. */
enum AncientCityTier {
    CityTier_None  = 0,
    CityTier_119   = 1,   // 1.19 ~ 1.19.4: no trim system, only swift sneak / golden apple
    CityTier_120_1 = 2,   // 1.20 ~ 1.20.1
    CityTier_120_2 = 3,   // 1.20.2 ~ 1.21.5
    CityTier_121_6 = 4,   // 1.21.6 ~ 1.21.11
    CityTier_26_1  = 5,   // 26.1 ~ 26.3
};

/** Which overlay layers are available on a given Minecraft version. */
struct OverlayCapability {
    int cityTier = 0;
    bool citySilence = false;
    bool citySwiftSneak = false;
    bool cityGoldenApple = false;
    bool cityWard = false;
    bool oreDiamond = false;
    bool oreLapis = false;
    bool netherDebris = false;
};

int ancientCityTier(int mc);
OverlayCapability overlayCapability(int mc);
bool overlaySupported(int mc);

/** One evaluated chest of an ancient city. */
struct CityChest {
    int x = 0;
    int y = 0;
    int z = 0;
    int kind = 0;      // 0 = chests/ancient_city, 1 = ancient_city_ice_box, 2 = fixed-item chest
    int silence = 0;   // 0/1: carries the silence armour trim
    int ward = 0;      // 0/1: carries the ward armour trim (mutually exclusive with silence)
    /** All swift sneak book levels this chest produced, ascending. */
    std::vector<int> swiftSneakLevels;
    /** Enchanted golden apples in this chest (pool0 entry0, set_count 1..2). */
    int enchantedGoldenApples = 0;
    uint64_t lootSeed = 0;
};

/** Evaluation result for one ancient city. */
struct CityChestInfo {
    int x = 0;
    int z = 0;
    int chunkX = 0;
    int chunkZ = 0;
    bool exists = false;
    int pieces = 0;
    Box3i bounds;
    std::vector<CityChest> chests;
};

/** One template placed by the host's ancient-city layout engine. */
struct AncientCityPiece {
    std::string name; // e.g. "ancient_city/structures/barracks"
    int x = 0;        // world position of the template origin
    int y = 0;
    int z = 0;
    int rotation = 0; // 0..3 = N / E / S / W (Rotation.getRandom order)
};

/**
 * The ancient-city piece graph is owned by the host's layout engine (the same
 * 26.3 engine that drives the geode finder). LootsFinder only turns the placed
 * templates into chest positions, so the engine is injected here instead of
 * adding a hard dependency on the host's terrain code.
 *
 * Returns false when the city does not exist (or the version is unsupported).
 */
using AncientCityLayoutFn = bool (*)(int mc, uint64_t seed, int x, int z,
                                     std::vector<AncientCityPiece> &out);

/** Install (or clear with nullptr) the ancient-city layout provider. */
void ancientCitySetLayoutProvider(AncientCityLayoutFn fn);

std::vector<CityChestInfo> ancientCityChests(int mc, uint64_t seed, uint32_t flags,
                                             const std::vector<std::pair<int, int>> &cities);

/** One ocean monument's sponge rooms. */
struct MonumentSponge {
    int x = 0;   // structure origin
    int z = 0;
    bool exists = false;
    int rooms = 0;        // room-graph nodes (debug)
    int spongeRooms = 0;  // number of sponge rooms found
    Box3i bounds;
    /** One representative block position per sponge room. */
    std::vector<Box3i> sponges;
};

std::vector<MonumentSponge> monumentSponge(int mc, uint64_t seed, uint32_t flags,
                                           const std::vector<std::pair<int, int>> &monuments);

} // namespace loots
