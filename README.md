# LootsFinder (tools/loots)

[简体中文](README.zh-CN.md) | English

Structure **content** evaluation for the All-In-One Finder map: which chests or
blocks inside a structure carry a given loot item.

The module builds two ways:

| mode | what it produces |
|---|---|
| **standalone** (`cmake -S tools/loots -B build`) | static library + `loots-cli` + self-test |
| **as a subproject** (`add_subdirectory(tools/loots)`) | static library `loots` only; the host links it |

## Boundary

This module never decides **where** a structure is. Structure positions, biomes,
noise and surface height stay in [cubiomes](../cubiomes); whatever this module
needs from there is supplied by loots' own worldgen module. That keeps the map path free of a
`cubiomes` dependency, so it can be built, tested and versioned on its own.

| layer | owns |
|---|---|
| `tools/cubiomes` | structure positions, biomes, noise, surface height |
| **`tools/loots`** (this) | piece graphs, chest / suspicious-block positions, `LootTableSeed`, loot-table rolls, enchantment rolls |
| `native/` in the host repo | RPC adaptation only |

## Interfaces

| header | audience |
|---|---|
| `include/loots/loots.h` | C++ API, used by the host; can evolve with the source |
| `include/loots/loots_capi.h` | stable C ABI (`LOOTS_CAPI_VERSION`) for the CLI and external tools |
| `include/loots/version.h` | `LOOTS_VERSION_STRING` + `LOOTS_ABI_VERSION` |

The host asserts `LOOTS_ABI_VERSION` at compile time so a stale submodule
checkout fails the build instead of silently misbehaving.

## Standalone build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build          # or: ./build/loots-selftest
```

Options (standalone only): `LOOTS_BUILD_CLI` (default `ON`),
`LOOTS_BUILD_TESTS` (default `ON`). Both default to `OFF` when the module is
included from the host build. `tests/` is local-only and gitignored; when it is
absent, CMake disables the test targets instead of failing.

## CLI

All commands print JSON on stdout, so output can be piped into other tools.
Exit codes: `0` success, `1` runtime failure, `2` usage error.

```sh
# library version / C ABI version
loots-cli info

# per-version capability table
loots-cli capability --mc 32

# evaluate one ancient city (chests + loot)
loots-cli ancient-city --mc 32 --seed 12345 --x 100 --z -200

# evaluate one ocean monument (sponge rooms)
loots-cli sponge --mc 32 --seed 12345 --x -40 --z 80
```

`--mc` is the cubiomes `MC_*` numeric id (`23` = 1.19.2, `25` = 1.20.6,
`29` = 1.21.5, `32` = 1.21.11, `35` = 26.3). `--flags` is optional
(large biomes and friends, default `0`).

Example:

```
$ loots-cli ancient-city --mc 32 --seed 12345 --x 100 --z -200
{
  "x": 100, "z": -200, "chunkX": 6, "chunkZ": -13,
  "exists": false, "pieces": 0,
  "bounds": { "minX": 0, "minY": 0, "minZ": 0, "maxX": 0, "maxY": 0, "maxZ": 0 },
  "chestCount": 0,
  "chests": [
  ]
}
```

## Layout

```
cubiomes/                    git submodule (standalone build only)
include/loots/               public C++ / C ABI / version headers
data/                        generated vanilla headers, all compiled
src/core/                    rng, transform, version, C ABI
src/loot/                    loot-table replay
src/structures/              piece extraction (pieces_c.*, structure_pieces)
src/jigsaw/                  shared exact 26.3 JigsawPlacement
src/ancient_city/            ancient city chests
src/bastion/                 bastion remnant
src/monument/                ocean monument sponge rooms
src/outpost/                 pillager outpost + allay features
src/woodland_mansion/        woodland mansion rooms / chests
src/ocean_ruin/         ocean ruin chests
src/trail_ruins/             trail ruins processor + suspicious gravel
src/trial_chambers/          trial-chamber decorated pots (Creator music box)
src/shipwreck/               shipwreck
src/desert_jungle/           desert pyramid + jungle temple
src/stronghold/              stronghold 1.18 simulator (moved from third_party)
src/cli/                     loots-cli
tests/                       local-only self-tests (gitignored)
```

Source folders are grouped by structure. The public API headers stay in
`include/loots/`; everything a translation unit compiles lives under `src/`.

## Cubiomes

Standalone builds compile the root `cubiomes/` submodule:

```
git submodule update --init --recursive
```

When the module is included by the host (`add_subdirectory(tools/loots)`), the
local submodule is **not** read or built: the host's public `tools/cubiomes`
headers are used and the host's existing cubiomes target supplies the symbols.
That is what the All-In-One package ships.

## Generated data

The compiled headers are committed so a fresh checkout builds without a
Minecraft jar. `data/end_city_layouts.*` is a reference dump no translation unit
includes and is gitignored.

| file | source | committed |
|---|---|---|
| `data/loot_tables.hpp` | client jar loot_table/** | yes |
| `data/stronghold_loot_tables.hpp` | shared 1.20+ stronghold library/corridor/crossing tables | yes |
| `data/loot_tables_26_3.hpp` | 26.3 loot-table band | yes |
| `data/chest_layouts.hpp` | structure .nbt templates | yes |
| `data/trial_pots.hpp` | trial-chamber template names + pot/container offsets | yes |
| `data/jigsaw_templates.hpp` | ancient-city template/pool subset | yes |
| `data/jigsaw_data.hpp` | full jigsaw data shared by 1.20.1-26.3 | yes |
| `data/enchantments.hpp` | enchantment tables | yes |
| `data/mansion_templates.hpp` | woodland mansion templates | yes |
| `data/trail_ruins*.hpp` | trail ruins templates | yes |
| `data/ocean_blocks.hpp` | ocean ruin block candidates | yes |
| `data/end_city_layouts.*` | end city reference | no |

Regenerate (the extraction tools are local-only and gitignored):

```
python tools/extract_loot_tables.py --out data
python tools/extract_loot_tables_v2.py --out data
python tools/extract_loot_tables_v2.py --out data --only-stronghold
python tools/extract_chest_layouts.py --structure-dir <structure/> --out data
python tools/extract_chest_layouts.py --jar %APPDATA%/.minecraft/versions/26.3/26.3.jar --out data
python tools/extract_trial_pots.py --out data
python tools/extract_jigsaw.py --jar %APPDATA%/.minecraft/versions/26.3/26.3.jar --out data
python tools/generate_jigsaw_fixtures.py --java "C:/Program Files/Zulu/zulu-25/bin/java.exe"
```

Cross-check the generated tables against an exported loot_table/ folder:

```
python tools/verify_py/verify_loot_tables.py
```

## Status

The module owns the 26.3 ancient-city layout and exact terrain queries, so
`ancientCityChests()` works in standalone builds.

| capability | C++ library | Java 26.3 query |
|---|---|---|
| ocean monument sponge rooms | done | done |
| ancient city (ward/silence/swift sneak/golden apple) | done | done |
| pillager outpost allay | done | done |
| ocean ruin sniffer egg | done | done |
| woodland mansion vex trim | done | done |
| trail ruins 4 trims | done | done |
| bastion snout / netherite / outline | done | done |
| bastion piglin banner pattern | done | done |
| desert pyramid dune / golden apple | done (hint) | done |
| jungle temple wild trim | done (hint) | done |
| nether fortress rib trim | done | done |
| end city spire trim | done | done |
| pillager outpost sentry trim | done (hint) | done |
| shipwreck coast trim | done (hint) | done |
| stronghold library eye trim | done | done |
| stronghold End portal room | done | done |
| 6 non-renewable music discs (Pigstep, Otherside, 5 fragments, Relic, Bounce, Creator) | done | done |
| abandoned mineshaft layer | done (1.18.2-26.3) | done (26.3) |
| trial-chamber decorated pots | done (positions/templates) | done (positions) |
| 17 pottery sherds | done | done |
| desert pyramid sherds (4) | done (1.20+) | done |
| desert well presence (terrain support) | done (approx) | done; solid/volume gate; map shows translucent + note |
| woodland mansion Y≥60 (5×5 box) | done (approx) | done; map shows translucent + note |
| desert well sherds (2) | done (1.20+) | done; ≤26.2 `DesertWellFeature`, 26.3 OverlayFeature + TemplateFeature |

**Shared jigsaw implementation**: bastion remnants, trail ruins and
pillager-outpost allay cages share the exact `JigsawPlacement` port in
`src/jigsaw/` (1.20.1-26.3; outpost allay also covers 1.19.2). Their templates,
pools and structure data are identical across those versions; only the terrain
height profile changes, and outposts additionally use the `outpost_rot` survival
check. Version coverage and tests are in "Jigsaw accuracy" below.

Consumers must tolerate `exists = false` and empty result vectors. The Java
26.3 backend is the oracle; it is not linked into the portable host. Loot-table
results (discs, sherds, trims) are additionally covered by the multi-version
golden oracle: `tools/generate_golden.py` reads every 1.18.2-26.3 jar and
`loots-goldentest` compares per-seed `discMask`/`sherdMask` for each supported
version.

## Known limitations

- **Desert wells**: presence uses approximate `worldSolid` / volume support (not
  exact `Blocks.SAND`); sherds use `worldSurfaceHeight` for Y and WorldgenRandom
  decoration replay (salt 40002) for offsets / `AppendLoot` seeds.
- **Abandoned mineshafts (Bounce)**: the result depends on chunk generation
  order because step 1-3 features (monster rooms, lava lakes, geodes) write
  blocks into neighbouring chunks. Replaying in the oracle's ascending
  `ChunkPos.pack` order leaves a few mineshafts that differ by 1-2 chests.
- **Ocean-ruin suspicious blocks**: coordinates are not exact yet (warm sample
  23 vs the oracle's 29; cold 10-13 vs 5-9; large ruins 87 vs 57). The sherd
  contents come from exact loot tables; the block-level `BlockRotProcessor` /
  `CappedProcessor` overwrite replay is still missing.
- **Trial-chamber pots (Creator)**: positions/templates are exact, but 26 of 47
  loot seeds match the oracle; Creator detection can be affected. Patterned-pots
  sherds (flow/guster/scrape) come from template names and are unaffected.
- **Trail ruins (Relic + 7 sherds)**: 6 of 8 in-game samples match; 2 differ by
  one block or position.
- **Version verification**: the Java worldgen oracle only runs on 26.3; older
  versions are covered by real-jar fixtures (`loots-multiversiontest`) and the
  golden loot-table oracle.


## Jigsaw accuracy

Bastion remnants, trail ruins, and pillager-outpost allay cages use the single
shared exact port of `JigsawPlacement.addPieces` / `Placer` in `src/jigsaw/`.
It covers `single` / `list` / `empty` pool elements, fallbacks, weights,
placement/selection priorities, the sequenced priority queue, the expansion
hack, junctions, and exact `WORLD_SURFACE_WG` heights.

Version coverage: the 1.20.1, 1.21.1, 1.21.4, 1.21.10, 26.1.2, 26.2 and 26.3
jars carry **identical** bastion/trail-ruins/outpost template, pool and structure
data (verified with `tools/compare_jigsaw_versions.py`; only the outpost
spawn-override serialization differs). The shared data is therefore used for
every version from 1.20.1 through 26.3, while `worldgen` selects the matching
terrain profile (1.20-1.21.4, 1.21.9-26.2, 26.3). 1.19.2 has the same outpost
templates, code-defined pools and `JigsawStructure` config; its allay-cage
placement is validated against the real 1.19.2 jar fixtures and uses generation
profile -3.

The generated data (`tools/extract_jigsaw.py`) is derived from the 26.3 jar and
the regression expectations in `tests/jigsaw_fixtures.hpp` are generated from
the real 26.3 Java oracle by `tools/generate_jigsaw_fixtures.py`. The suite
covers 15 bastion starts (13 treasure variants), 6 outposts with and without
allays, and 6 trail-ruin starts, comparing every piece
origin/rotation/bounds, every chest coordinate and loot seed, allay cages, and
every suspicious block with its trim result. `loots-multiversiontest` additionally
replays the real-jar 1.19.2 outpost and 1.20.1/1.21.10/1.21.11 outpost and
trail-ruins fixtures on all supported cubiomes version ids, and checks that the
bastion piece graph and chest coordinates are version-independent.

## Versions

`ancientCityTier()` works on cubiomes' `MC_*` numeric ids. Those ids are asserted
against the real enum in the host build (`native/map_service.cpp`), so a cubiomes
update that renumbers versions fails the build instead of silently mis-tiering.

## License

MIT - see [LICENSE](LICENSE).
