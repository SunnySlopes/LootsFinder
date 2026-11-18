// Trial chambers: decorated-pot placement inside the 1.21+ jigsaw layout.
//
// Only the four decorated pots that carry `pots/trial_chambers/corridor` are
// exposed (flow / guster / scrape / undecorated). `decor/empty_pot` (a flower
// pot) and `decor/dead_bush_pot` (a potted plant) never contain loot and are
// deliberately ignored.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace loots {

/** One decorated pot discovered in a trial chamber. */
struct TrialPot {
    int x = 0;
    int y = 0;
    int z = 0;
    std::string templateName; // e.g. "trial_chambers/decor/flow_pot"
    std::string lootTable;    // always "pots/trial_chambers/corridor"
    /** Post-process loot seed, 0 when it could not be derived. */
    uint64_t lootSeed = 0;
};

/**
 * Enumerate the decorated pots of the trial chamber starting at chunk
 * (`chunkX`, `chunkZ`). Returns false when the structure/layout is not
 * available for `mc` (trial chambers need 1.21+). The pot loot-table seed is
 * filled from the post-process stream when known; otherwise `lootSeed` is 0
 * and callers must not present a guaranteed drop.
 */
bool trialChamberPots(int mc, uint64_t seed, int chunkX, int chunkZ,
                      std::vector<TrialPot> &out);

} // namespace loots
