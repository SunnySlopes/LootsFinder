#include "loots/loots.h"

// The tier boundaries mirror the host plan (section 5.2). Note that this
// cubiomes version table has no distinct value for 1.20 / 1.20.1 (MC_1_20 is an
// alias for 1.20.6), so tier 2 is currently unreachable and 1.20.6 reports tier
// 3. Both tiers carry the same items, so capability answers are unaffected; only
// the loot-table schema lookup (M2) has to distinguish them by mc range.
namespace loots {

// These are cubiomes' MC_* numeric ids (probed with docs/ore_bench/probe_mc_ids.c).
// The host asserts them at compile time (native/map_service.cpp), so a cubiomes
// update that renumbers versions fails the build instead of mis-tiering.
enum : int {
    kMc1192 = 23,
    kMc1194 = 24,
    kMc1206 = 25,
    kMc216  = 30,
    kMc2111 = 32,
    kMc261  = 33,
};

int ancientCityTier(int mc)
{
    if (mc < kMc1192) return CityTier_None;
    if (mc <= kMc1194) return CityTier_119;
    if (mc >= kMc261) return CityTier_26_1;
    if (mc >= kMc216) return CityTier_121_6;
    if (mc >= kMc1206) return CityTier_120_2;
    return CityTier_119;
}

OverlayCapability overlayCapability(int mc)
{
    OverlayCapability cap;
    cap.cityTier = ancientCityTier(mc);
    // 1.19 has no trim system: no silence/ward, but swift sneak and golden apples exist.
    cap.citySilence = cap.cityTier >= CityTier_120_1;
    cap.cityWard = cap.cityTier >= CityTier_120_1;
    cap.citySwiftSneak = cap.cityTier >= CityTier_119;
    cap.cityGoldenApple = cap.cityTier >= CityTier_119;
    // Ore overlays stay gated on 1.21.11 until the feature-tag behaviour is verified.
    const bool oreReady = mc >= kMc2111;
    cap.oreDiamond = oreReady;
    cap.oreLapis = oreReady;
    cap.netherDebris = oreReady;
    return cap;
}

bool overlaySupported(int mc)
{
    const OverlayCapability cap = overlayCapability(mc);
    return cap.citySilence || cap.citySwiftSneak || cap.cityGoldenApple || cap.cityWard ||
           cap.oreDiamond || cap.oreLapis || cap.netherDebris;
}

VersionFamily versionFamily(int mc)
{
    if (mc == 22) return VersionFamily::v1182;
    if (mc == 23 || mc == 24) return VersionFamily::v1192;
    if (mc == 25) return VersionFamily::v1201;
    if (mc == 26) return VersionFamily::v1211;
    if (mc >= 27 && mc <= 29) return VersionFamily::v1214;
    if (mc >= 30 && mc <= 32) return VersionFamily::v12110;
    if (mc == 33) return VersionFamily::v261;
    if (mc == 34) return VersionFamily::v262;
    if (mc == 35) return VersionFamily::v263;
    return VersionFamily::unknown;
}

const char *versionFamilyName(int mc)
{
    switch (versionFamily(mc)) {
    case VersionFamily::v1182: return "1.18.2";
    case VersionFamily::v1192: return "1.19.2";
    case VersionFamily::v1201: return "1.20.1";
    case VersionFamily::v1211: return "1.21.1";
    case VersionFamily::v1214: return "1.21.4";
    case VersionFamily::v12110: return "1.21.10";
    case VersionFamily::v261: return "26.1";
    case VersionFamily::v262: return "26.2";
    case VersionFamily::v263: return "26.3";
    default: return "unknown";
    }
}

bool versionsShareData(int a, int b)
{
    const VersionFamily fa = versionFamily(a);
    return fa != VersionFamily::unknown && fa == versionFamily(b);
}

} // namespace loots
