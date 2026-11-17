// Version and ABI information for the loots module.
#pragma once

#define LOOTS_VERSION_MAJOR 0
#define LOOTS_VERSION_MINOR 1
#define LOOTS_VERSION_PATCH 1
#define LOOTS_VERSION_STRING "0.1.1"

// Bump this whenever the public API in loots.h changes in a way that is not
// source compatible. The host (native/) checks it at compile time so a stale
// submodule checkout fails loudly instead of silently misbehaving.
#define LOOTS_ABI_VERSION 2

namespace loots {

/** The generation-data families the project treats as identical. */
enum class VersionFamily {
    unknown = 0,
    v1182,   // cubiomes MC_1_18_2
    v1192,   // cubiomes MC_1_19_2 (1.19.4 shares the tracked data)
    v1201,   // 1.20.1 data, used for cubiomes MC_1_20_6
    v1211,   // cubiomes MC_1_21_1
    v1214,   // 1.21.4/1.21.5 family
    v12110,  // 1.21.10/1.21.11 family
    v261,    // 26.1.x
    v262,    // 26.2
    v263,    // 26.3
};

VersionFamily versionFamily(int mc);
const char *versionFamilyName(int mc);
bool versionsShareData(int a, int b);

} // namespace loots
