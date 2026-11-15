// Structure piece extraction: where the chests are inside a structure.
//
// This is the one part of LootsFinder that needs cubiomes, because cubiomes is
// what reconstructs the piece graph. It is only compiled when a cubiomes tree is
// available:
//   - standalone build : the root submodule at cubiomes/
//   - as a subproject  : the host's shared tools/cubiomes (no second copy built)
//
// The host passes the same Generator-independent arguments it uses elsewhere
// (mc id, seed, block position), so results are identical to what the map shows.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "loots/loot.h"

namespace loots {

/** One chest discovered inside a structure. */
struct StructureChest {
    int x = 0;
    int y = 0;
    int z = 0;
    std::string templateName; // structure/template piece that owns this chest
    std::string lootTable;   // e.g. "chests/nether_bridge"
    uint64_t lootSeed = 0;
    ChestReward reward;      // filled by evaluate()
};

/** Result for one structure instance. */
struct StructureLoot {
    int x = 0;               // structure origin
    int z = 0;
    bool exists = false;
    std::string kind;        // "nether_fortress" / "end_city"
    int pieces = 0;
    int minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0;
    std::vector<StructureChest> chests;
    std::vector<StructureChest> suspicious; // suspicious sand/gravel containers

    /** Evaluates every chest/suspicious loot table. */
    void evaluate(int mc);
};

/** Nether fortress: chests come from getFortressPieces(). */
bool netherFortressLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

/** End city: chests come from getEndCityPieces(). */
bool endCityLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

} // namespace loots
