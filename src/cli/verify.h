#pragma once

#include <cstdint>

namespace loots {
namespace cli {

/**
 * Find a handful of real structure locations for in-game verification and
 * print their computed coordinates/loot as JSON.
 *
 * This is intentionally a developer/verification command: it uses cubiomes for
 * structure placement and the normal loots evaluators for the content.
 */
int verifyPoints(int mc, uint64_t seed, int radius, int limit, const char *onlyFeature);

} // namespace cli
} // namespace loots
