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

/** Which kind of block container a `StructureChest` describes. */
enum class ContainerKind : uint8_t {
    chest = 0,
    barrel,
    /** Decorated pot (trial chambers) - loot table `pots/...`. */
    pot,
    /** Suspicious sand/gravel (archaeology). */
    suspicious,
};

/** One chest discovered inside a structure. */
struct StructureChest {
    int x = 0;
    int y = 0;
    int z = 0;
    std::string templateName; // structure/template piece that owns this chest
    std::string lootTable;   // e.g. "chests/nether_bridge"
    uint64_t lootSeed = 0;
    ChestReward reward;      // filled by evaluate()
    ContainerKind container = ContainerKind::chest;
    /**
     * True when the position or existence is approximated rather than replayed
     * exactly. Currently used for abandoned-mineshaft chest minecarts: cubiomes
     * models their X/Z and loot seed, but not the air/floor check or the exact
     * vertical shift, so the list can differ from vanilla for some structures.
     */
    bool approximate = false;
};

/** Result for one structure instance. */
struct StructureLoot {
    int x = 0;               // structure origin
    int z = 0;
    bool exists = false;
    std::string kind;        // "nether_fortress" / "end_city"
    int pieces = 0;
    int minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0;
    /** World seed, used by evaluate() for per-chest biome conditions. */
    uint64_t seed = 0;
    std::vector<StructureChest> chests;
    std::vector<StructureChest> suspicious; // suspicious sand/gravel containers
    /** Decorated pots (trial chambers). */
    std::vector<StructureChest> pots;

    /** Evaluates every chest/pot/suspicious loot table. */
    void evaluate(int mc);
};

/** Nether fortress: chests come from getFortressPieces(). */
bool netherFortressLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

/** End city: chests come from getEndCityPieces(). */
bool endCityLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

/** Abandoned mineshaft: chests come from getMineshaftLoot(). */
bool mineshaftLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out);

} // namespace loots
