// Small processor-related helpers shared by the structure adapters.
//
// Full StructureProcessor simulation is structure-specific and stays in the
// adapters.  These primitives are the pieces that every processor-based replay
// needs: Mth.getSeed and the per-block LegacyRandomSource it seeds.
#pragma once

#include <cstdint>

#include "loots/jigsaw_core.h"

namespace loots {
namespace jigsaw {

/** Mth.getSeed(int, int, int) with Java's 32/64-bit overflow semantics. */
int64_t mthGetSeed(int x, int y, int z);

/**
 * BlockRotProcessor.processBlock for one block: survives when
 * `random.nextFloat() <= integrity` (integrity 0.05 keeps 5%).  The processor
 * uses `RandomSource.create(Mth.getSeed(pos))` (legacy RNG).
 */
bool blockRotSurvives(const Vec3 &pos, float integrity);

/** First nextLong() of the LegacyRandom seeded with Mth.getSeed(pos). */
uint64_t legacySeedAt(const Vec3 &pos);

} // namespace jigsaw
} // namespace loots
