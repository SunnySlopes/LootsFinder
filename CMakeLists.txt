# LootsFinder: structure content evaluation.
#
# Standalone use (library + CLI, compiles the root `cubiomes` submodule):
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#   ./build/loots-cli info
#
# As a subproject (the host application):
#   add_subdirectory(tools/loots)  ->  links the `loots` library, no CLI, and NO
#   second cubiomes: the host's shared tools/cubiomes is used instead. The
#   `cubiomes/` submodule is not read or built in this mode.
cmake_minimum_required(VERSION 3.16)

# Guard project() so including this file from the host does not reset its project.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(loots VERSION 0.1.0 LANGUAGES C CXX)
  set(LOOTS_STANDALONE ON)
  option(LOOTS_BUILD_CLI "Build the loots-cli executable" ON)
  option(LOOTS_BUILD_TESTS "Build the loots self-test" ON)
  option(LOOTS_WITH_INTERNAL_CUBIOMES
         "Compile the root cubiomes submodule (standalone only)" ON)
else()
  set(LOOTS_STANDALONE OFF)
  option(LOOTS_BUILD_CLI "Build the loots-cli executable" OFF)
  option(LOOTS_BUILD_TESTS "Build the loots self-test" OFF)
  # The host already compiles one cubiomes; never add a second copy.
  set(LOOTS_WITH_INTERNAL_CUBIOMES OFF)
endif()

# tests/ and tools/ are local development material (gitignored). A fresh clone
# may not have them; do not fail the build because of that.
if (LOOTS_BUILD_TESTS AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/selftest.cpp")
  message(STATUS "loots: tests/ is not present; disabling LOOTS_BUILD_TESTS")
  set(LOOTS_BUILD_TESTS OFF)
endif()

# --- cubiomes -----------------------------------------------------------------
# Standalone: the root submodule. Host build: the host's shared tools/cubiomes.
# The local submodule is deliberately NOT checked or built when included by
# the host, so the All-In-One package can ship without it.
if (LOOTS_WITH_INTERNAL_CUBIOMES)
  set(LOOTS_CUBIOMES "${CMAKE_CURRENT_SOURCE_DIR}/cubiomes")
  if (NOT EXISTS "${LOOTS_CUBIOMES}/finders.h")
    message(FATAL_ERROR "cubiomes/ is empty; run: git submodule update --init --recursive")
  endif()
  file(GLOB LOOTS_CUBIOMES_SRC
    "${LOOTS_CUBIOMES}/finders.c" "${LOOTS_CUBIOMES}/carver.c"
    "${LOOTS_CUBIOMES}/generator.c" "${LOOTS_CUBIOMES}/layers.c"
    "${LOOTS_CUBIOMES}/biomenoise.c" "${LOOTS_CUBIOMES}/biomes.c"
    "${LOOTS_CUBIOMES}/noise.c" "${LOOTS_CUBIOMES}/terrainnoise.c"
    "${LOOTS_CUBIOMES}/util.c" "${LOOTS_CUBIOMES}/features/*.c")
  add_library(loots_cubiomes STATIC ${LOOTS_CUBIOMES_SRC})
  target_include_directories(loots_cubiomes PUBLIC "${LOOTS_CUBIOMES}")
  set_target_properties(loots_cubiomes PROPERTIES
    C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
  # Match the host's numerics: same flags, no fast-math.
  target_compile_options(loots_cubiomes PRIVATE -O2 -ffp-contract=off)
elseif (LOOTS_STANDALONE)
  message(FATAL_ERROR "standalone build needs the cubiomes/ submodule (set -DLOOTS_WITH_INTERNAL_CUBIOMES=ON)")
else()
  # Subproject: use the host's public cubiomes. Symbols come from the host's
  # own cubiomes target at final link time; no local submodule is required.
  set(LOOTS_CUBIOMES "${CMAKE_SOURCE_DIR}/../tools/cubiomes")
  if (NOT EXISTS "${LOOTS_CUBIOMES}/finders.h")
    message(FATAL_ERROR "cannot find the host's tools/cubiomes (looked in ${LOOTS_CUBIOMES})")
  endif()
endif()

# --- library -------------------------------------------------------------------
add_library(loots STATIC
  src/core/rng.cpp
  src/core/transform.cpp
  src/core/version.cpp
  src/core/capi.cpp
  src/loot/loot.cpp
  src/structures/pieces_c.c
  src/structures/structure_pieces.cpp
  src/jigsaw/jigsaw.cpp
  src/ancient_city/ancient_city.cpp
  src/bastion/bastion.cpp
  src/monument/monument_sponge.cpp
  src/outpost/outpost.cpp
  src/outpost/outpost_features.cpp
  src/woodland_mansion/woodland_mansion.cpp
  src/ocean_ruin/ocean_ruin.cpp
  src/trail_ruins/trail_ruins.cpp
  src/shipwreck/shipwreck.cpp
  src/desert_jungle/fixed_structures.cpp
  src/stronghold/stronghold.cpp
  src/stronghold/sh1_18.c
  src/stronghold/vendor/stronghold_pieces.c
  src/worldgen/worldgen.cpp
  src/worldgen/perlin_float.cpp
  src/worldgen/biome_bridge_stub.cpp
  src/worldgen/geode.cpp
  src/worldgen/terrain.cpp
  src/worldgen/carver.cpp
  src/worldgen/structures.cpp
  src/worldgen/overwrites.cpp
  src/worldgen/structure_layout/layout.cpp
  src/worldgen/structure_layout/climate263.cpp
)

set(LOOTS_WORLDGEN_DEFS
  "geode=loots_wg"
  "geode_layout=loots_wg_layout"
  "samplePerlinFloatRaw=loots_samplePerlinFloatRaw"
  "geode_biomes_create=loots_biomes_create"
  "geode_biomes_free=loots_biomes_free"
  "geode_biomes_allowed=loots_biomes_allowed"
)
set_source_files_properties(
  src/worldgen/worldgen.cpp
  src/worldgen/perlin_float.cpp
  src/worldgen/biome_bridge_stub.cpp
  src/worldgen/geode.cpp
  src/worldgen/terrain.cpp
  src/worldgen/carver.cpp
  src/worldgen/structures.cpp
  src/worldgen/overwrites.cpp
  src/worldgen/structure_layout/layout.cpp
  src/worldgen/structure_layout/climate263.cpp
  PROPERTIES COMPILE_DEFINITIONS "${LOOTS_WORLDGEN_DEFS}")

# Keep loots' stronghold implementation private: another copy may coexist in
# the host for non-map users. The public API is include/loots/stronghold.h.
set(LOOTS_STRONGHOLD_PREFIX
  "sh118_minVersion=loots_sh118_minVersion"
  "sh118_versionSupported=loots_sh118_versionSupported"
  "sh118_versionName=loots_sh118_versionName"
  "sh118_pieceTypeName=loots_sh118_pieceTypeName"
  "sh118_pieceTypeCode=loots_sh118_pieceTypeCode"
  "sh118_unionArea=loots_sh118_unionArea"
  "sh118_compute=loots_sh118_compute"
  "sh118_locateAll=loots_sh118_locateAll"
  "sh118_locateFirst=loots_sh118_locateFirst"
  "sh118_listApprox=loots_sh118_listApprox"
  "sh118_relocateApprox=loots_sh118_relocateApprox"
  "getStrongholdPieces=loots_sh118_getStrongholdPieces"
  "getStrongholdPiecesEx=loots_sh118_getStrongholdPiecesEx"
  "getStrongholdLoot=loots_sh118_getStrongholdLoot"
)
set_source_files_properties(
  src/stronghold/sh1_18.c
  src/stronghold/vendor/stronghold_pieces.c
  PROPERTIES COMPILE_DEFINITIONS "${LOOTS_STRONGHOLD_PREFIX}")

target_include_directories(loots PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
# Generated vanilla data (loot tables) - implementation detail, not public.
target_include_directories(loots PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/data")
# Private implementation headers are grouped under their structure folder; keep
# the short include names used by the sources.
target_include_directories(loots PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/structures"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/outpost"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/stronghold"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/worldgen"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/worldgen/data"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/worldgen/structure_layout"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/worldgen/extensions"
)
# Structure-piece extraction includes cubiomes headers directly.
target_include_directories(loots PRIVATE "${LOOTS_CUBIOMES}")

if (LOOTS_WITH_INTERNAL_CUBIOMES)
  target_link_libraries(loots PUBLIC loots_cubiomes)
endif()

target_compile_features(loots PUBLIC cxx_std_20)
set_target_properties(loots PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  POSITION_INDEPENDENT_CODE ON
)

if (LOOTS_BUILD_CLI)
  add_executable(loots-cli
    src/cli/main.cpp
    src/cli/verify.cpp
    src/cli/ancient_geode.cpp
    src/cli/stronghold_cmd.cpp)
  target_link_libraries(loots-cli PRIVATE loots)
  target_include_directories(loots-cli PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cli"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/structures"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/stronghold")
  set(GEODE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../geode")
  if(EXISTS "${GEODE_ROOT}/build/libgeode_core.a")
    target_compile_definitions(loots-cli PRIVATE LOOTS_HAS_GEODE=1)
    target_include_directories(loots-cli PRIVATE
      "${GEODE_ROOT}/src" "${GEODE_ROOT}/data" "${GEODE_ROOT}/third_party/cubiomes")
    target_link_libraries(loots-cli PRIVATE
      "${GEODE_ROOT}/build/libgeode_core.a"
      "${GEODE_ROOT}/build/libterrain_biomes.a"
      "${GEODE_ROOT}/build/libGeodeCubiomes_Biomes.a"
      "${GEODE_ROOT}/build/libGeodeCubiomes_Noise.a"
      z)
  endif()
  target_include_directories(loots-cli PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/data" "${LOOTS_CUBIOMES}")
  set_target_properties(loots-cli PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
endif()

if (LOOTS_BUILD_TESTS)
  enable_testing()
  add_executable(loots-selftest tests/selftest.cpp)
  target_link_libraries(loots-selftest PRIVATE loots)
  set_target_properties(loots-selftest PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME loots-selftest COMMAND loots-selftest)

  add_executable(loots-loottest tests/loot_test.cpp)
  target_link_libraries(loots-loottest PRIVATE loots)
  set_target_properties(loots-loottest PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME loots-loottest COMMAND loots-loottest)

  add_executable(loots-goldentest tests/golden_test.cpp)
  target_link_libraries(loots-goldentest PRIVATE loots)
  target_include_directories(loots-goldentest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  set_target_properties(loots-goldentest PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME loots-goldentest COMMAND loots-goldentest)

  add_executable(loots-oracletest tests/oracle_test.cpp)
  target_link_libraries(loots-oracletest PRIVATE loots)
  set_target_properties(loots-oracletest PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  add_test(NAME loots-oracletest COMMAND loots-oracletest)

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../geode/build/libgeode_core.a")
    add_executable(loots-ancienttest tests/ancient_geode_test.cpp src/cli/ancient_geode.cpp)
    target_compile_definitions(loots-ancienttest PRIVATE LOOTS_HAS_GEODE=1)
    target_include_directories(loots-ancienttest PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src/cli"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/structures"
      "${CMAKE_CURRENT_SOURCE_DIR}/data"
      "${CMAKE_CURRENT_SOURCE_DIR}/../geode/src"
      "${CMAKE_CURRENT_SOURCE_DIR}/../geode/data"
      "${CMAKE_CURRENT_SOURCE_DIR}/../geode/third_party/cubiomes")
    target_link_libraries(loots-ancienttest PRIVATE
      loots
      "${CMAKE_CURRENT_SOURCE_DIR}/../geode/build/libgeode_core.a"
      "${CMAKE_CURRENT_SOURCE_DIR}/../geode/build/libterrain_biomes.a"
      "${CMAKE_CURRENT_SOURCE_DIR}/../geode/build/libGeodeCubiomes_Biomes.a"
      "${CMAKE_CURRENT_SOURCE_DIR}/../geode/build/libGeodeCubiomes_Noise.a"
      z)
    set_target_properties(loots-ancienttest PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    add_test(NAME loots-ancienttest COMMAND loots-ancienttest)
  endif()
endif()
