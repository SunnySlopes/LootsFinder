#include "loots/ocean_ruin.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "loots/rng.h"
#include "loots/transform.h"
#include "pieces_c.h"

#if defined(__has_include)
#if __has_include("chest_layouts.hpp")
#define LOOTS_HAS_OCEAN_DATA 1
#include "chest_layouts.hpp"
#include "ocean_blocks.hpp"
#endif
#endif

namespace loots {
namespace {

OceanRuinHeightFn g_oceanFloorHeight = nullptr;
OceanRuinSolidFn g_oceanSolid = nullptr;

/** Mth.getSeed(int, int, int). */
int64_t mthGetSeed(int x, int y, int z)
{
    // Java: `x * 3129871` is 32-bit int arithmetic (it overflows for large x),
    // then promoted to long for the XOR with the long z term.
    const int32_t ix = static_cast<int32_t>(static_cast<uint32_t>(x) * 3129871u);
    int64_t seed = static_cast<int64_t>(ix) ^ (static_cast<int64_t>(z) * 116129781LL) ^ static_cast<int64_t>(y);
    seed = seed * seed * 42317861LL + seed * 11LL;
    return seed >> 16;
}

/**
 * RandomSource.createThreadLocalInstance(seed).forkPositional().at(pos)
 * is legacy java.util.Random in 26.3 (SingleThreadedRandomSource), not
 * Xoroshiro. forkPositional() consumes one nextLong; at() XORs Mth.getSeed.
 */
class PositionalRandom {
public:
    explicit PositionalRandom(uint64_t seed) : rng_(seed) {}
    static PositionalRandom at(uint64_t seed, int x, int y, int z)
    {
        LegacyRandom base(seed);
        const uint64_t fork = base.nextLong();
        return PositionalRandom(static_cast<uint64_t>(mthGetSeed(x, y, z)) ^ fork);
    }
    int nextInt(int bound) { return rng_.nextInt(bound); }
private:
    LegacyRandom rng_;
};

/** LegacyRandomSource(Mth.getSeed(pos)).nextLong() for AppendLoot. */
uint64_t suspiciousLootSeed(int x, int y, int z)
{
    LegacyRandom rng(static_cast<uint64_t>(mthGetSeed(x, y, z)));
    return rng.nextLong();
}

template <typename Rng>
void shuffle(Rng &rng, std::vector<uint16_t> &values)
{
    for (int n = static_cast<int>(values.size()); n > 1; --n) {
        const int j = rng.nextInt(n);
        std::swap(values[static_cast<size_t>(n - 1)], values[static_cast<size_t>(j)]);
    }
}

#if defined(LOOTS_HAS_OCEAN_DATA)

struct Piece {
    const char *name;
    int rotation;
    int x, z;
    bool large;
};

const char *const kWarmSmall[] = {
    "underwater_ruin/warm_1", "underwater_ruin/warm_2", "underwater_ruin/warm_3",
    "underwater_ruin/warm_4", "underwater_ruin/warm_5", "underwater_ruin/warm_6",
    "underwater_ruin/warm_7", "underwater_ruin/warm_8",
};
const char *const kWarmBig[] = {
    "underwater_ruin/big_warm_4", "underwater_ruin/big_warm_5",
    "underwater_ruin/big_warm_6", "underwater_ruin/big_warm_7",
};
const char *const kBrickSmall[] = {
    "underwater_ruin/brick_1", "underwater_ruin/brick_2", "underwater_ruin/brick_3",
    "underwater_ruin/brick_4", "underwater_ruin/brick_5", "underwater_ruin/brick_6",
    "underwater_ruin/brick_7", "underwater_ruin/brick_8",
};
const char *const kCrackedSmall[] = {
    "underwater_ruin/cracked_1", "underwater_ruin/cracked_2", "underwater_ruin/cracked_3",
    "underwater_ruin/cracked_4", "underwater_ruin/cracked_5", "underwater_ruin/cracked_6",
    "underwater_ruin/cracked_7", "underwater_ruin/cracked_8",
};
const char *const kMossySmall[] = {
    "underwater_ruin/mossy_1", "underwater_ruin/mossy_2", "underwater_ruin/mossy_3",
    "underwater_ruin/mossy_4", "underwater_ruin/mossy_5", "underwater_ruin/mossy_6",
    "underwater_ruin/mossy_7", "underwater_ruin/mossy_8",
};
const char *const kBrickBig[] = {
    "underwater_ruin/big_brick_1", "underwater_ruin/big_brick_2",
    "underwater_ruin/big_brick_3", "underwater_ruin/big_brick_8",
};
const char *const kCrackedBig[] = {
    "underwater_ruin/big_cracked_1", "underwater_ruin/big_cracked_2",
    "underwater_ruin/big_cracked_3", "underwater_ruin/big_cracked_8",
};
const char *const kMossyBig[] = {
    "underwater_ruin/big_mossy_1", "underwater_ruin/big_mossy_2",
    "underwater_ruin/big_mossy_3", "underwater_ruin/big_mossy_8",
};

int uniform(LegacyRandom &rng, int lo, int hi)
{
    return lo + rng.nextInt(hi - lo + 1);
}

struct Box2 {
    int minX, minZ, maxX, maxZ;
    bool intersects(const Box2 &o) const
    {
        return maxX >= o.minX && minX <= o.maxX && maxZ >= o.minZ && minZ <= o.maxZ;
    }
};

Box2 cornerBox(int x, int z, int rx, int rz, int rot)
{
    int ax, az, bx, bz;
    templateTransform(0, 0, Mirror::none, rotationByIndex(rot), 0, 0, ax, az);
    templateTransform(rx, rz, Mirror::none, rotationByIndex(rot), 0, 0, bx, bz);
    return {x + std::min(ax, bx), z + std::min(az, bz),
            x + std::max(ax, bx), z + std::max(az, bz)};
}

const data::TemplateLayout *findLayout(const char *name)
{
    for (const auto &t : data::kTemplateLayouts)
        if (std::strcmp(t.name, name) == 0) return &t;
    return nullptr;
}

const data::OceanTemplateBlocks *findBlocks(const char *name)
{
    for (const auto &t : data::kOceanTemplates)
        if (std::strcmp(t.name, name) == 0) return &t;
    return nullptr;
}

template <typename T, size_t N>
const char *pick(const T (&values)[N], LegacyRandom &rng)
{
    return values[static_cast<size_t>(rng.nextInt(static_cast<int>(N)))];
}

void addPiece(std::vector<Piece> &pieces, bool warm, bool large, LegacyRandom &rng,
              int rotation, int px, int pz)
{
    if (warm) {
        pieces.push_back({large ? pick(kWarmBig, rng) : pick(kWarmSmall, rng), rotation, px, pz, large});
        return;
    }
    const int index = rng.nextInt(large ? 4 : 8);
    pieces.push_back({(large ? kBrickBig : kBrickSmall)[index], rotation, px, pz, large});
    pieces.push_back({(large ? kCrackedBig : kCrackedSmall)[index], rotation, px, pz, large});
    pieces.push_back({(large ? kMossyBig : kMossySmall)[index], rotation, px, pz, large});
}

/**
 * OceanRuinPiece.postProcess: start at OCEAN_FLOOR_WG and, if the template
 * spans a hole, descend until the surrounding floor supports it.
 */
int templateY(int mc, uint64_t seed, int x, int z, const data::OceanTemplateBlocks &t, int rotation)
{
    if (!g_oceanFloorHeight || !g_oceanSolid) return 0;
    int height = g_oceanFloorHeight(mc, seed, x, z);
    int cx, cz;
    templateTransform(t.sizeX - 1, t.sizeZ - 1, Mirror::none, rotationByIndex(rotation), 0, 0, cx, cz);
    const int cornerX = x + cx;
    const int cornerZ = z + cz;
    const int topY = height - 1;
    int minY = 512;
    int area = 0;
    const int x0 = std::min(x, cornerX);
    const int x1 = std::max(x, cornerX);
    const int z0 = std::min(z, cornerZ);
    const int z1 = std::max(z, cornerZ);
    for (int zz = z0; zz <= z1; zz++) {
        for (int xx = x0; xx <= x1; xx++) {
            int floorY = height - 1;
            // Warm oceans never carry ice; air and water are both "not solid".
            while (floorY > -63 && !g_oceanSolid(mc, seed, xx, floorY, zz)) floorY--;
            minY = std::min(minY, floorY);
            if (floorY < topY - 2) area++;
        }
    }
    const int width = std::abs(x - cornerX);
    return (topY - minY > 2 && area > width - 2) ? minY + 1 : height;
}

/**
 * CappedProcessor over the post-BlockRot / post-BlockIgnore block list.
 * BlockRotProcessor uses per-world-position legacy RNG; CappedProcessor uses
 * the world-seed positional RNG at the template position.
 */
void collectSuspicious(int mc, uint64_t seed, const Piece &p, StructureLoot &out)
{
    const data::OceanTemplateBlocks *t = findBlocks(p.name);
    if (!t || t->blockCount == 0) return;
    if (!g_oceanFloorHeight || !g_oceanSolid) return;

    const int ty = templateY(mc, seed, p.x, p.z, *t, p.rotation);
    const float integrity = p.large ? 0.9f : 0.8f;
    const Rotation rot = rotationByIndex(p.rotation);

    struct Survived {
        int x, y, z;
        uint8_t kind;
    };
    std::vector<Survived> survivors;
    survivors.reserve(t->blockCount);

    for (int i = 0; i < t->blockCount; i++) {
        const data::OceanBlock &b = data::kOceanBlocks[t->first + i];
        int tx, tz;
        templateTransform(b.x, b.z, Mirror::none, rot, 0, 0, tx, tz);
        const int wx = p.x + tx;
        const int wy = ty + b.y;
        const int wz = p.z + tz;
        LegacyRandom rng(static_cast<uint64_t>(mthGetSeed(wx, wy, wz)));
        if (rng.nextFloat() > integrity) continue; // BlockRotProcessor removes it
        survivors.push_back({wx, wy, wz, b.kind});
    }
    if (survivors.empty()) return;

    std::vector<uint16_t> order(survivors.size());
    std::iota(order.begin(), order.end(), 0);
    PositionalRandom capRng = PositionalRandom::at(seed, p.x, ty, p.z);
    shuffle(capRng, order);

    // CappedProcessor(ConstantInt.of(5)); RuleProcessor matches sand only.
    // Blocks with no support below are falling suspicious sand: vanilla's
    // FallingBlock schedules them, and the 26.3 oracle filters them out.
    int replaced = 0;
    for (uint16_t index : order) {
        const Survived &s = survivors[index];
        if (s.kind != 1) continue; // warm ruins: sand -> suspicious sand
        replaced++;
        if (!g_oceanSolid(mc, seed, s.x, s.y - 1, s.z)) {
            if (replaced >= 5) break;
            continue;
        }
        StructureChest chest{};
        chest.x = s.x;
        chest.y = s.y;
        chest.z = s.z;
        chest.templateName = p.name;
        chest.lootTable = "archaeology/ocean_ruin_warm";
        chest.lootSeed = suspiciousLootSeed(s.x, s.y, s.z);
        out.suspicious.push_back(std::move(chest));
        if (replaced >= 5) break;
    }
}

#endif

} // namespace

void oceanRuinSetTerrainProvider(OceanRuinHeightFn height, OceanRuinSolidFn solid)
{
    g_oceanFloorHeight = height;
    g_oceanSolid = solid;
}

bool oceanRuinLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.kind = "ocean_ruin";
#ifdef LOOTS_HAS_OCEAN_DATA
    const bool warm = loots_biome_is_warm_ocean(mc, seed, x, z) != 0;
    LegacyRandom rng(largeFeatureSeedValue(mc, seed, x >> 4, z >> 4));
    const int rotation = rng.nextInt(4); // Rotation.getRandom
    const bool large = rng.nextFloat() <= 0.30f;
    std::vector<Piece> pieces;
    addPiece(pieces, warm, large, rng, rotation, x, z);
    if (large && rng.nextFloat() <= 0.90f) {
        const Box2 parent = cornerBox(x, z, 15, 15, rotation);
        int bottomX = std::min(x, x + (rotation & 1 ? -15 : 0));
        int bottomZ = std::min(z, z + (rotation & 1 ? 0 : -15));
        int cx, cz;
        templateTransform(15, 15, Mirror::none, rotationByIndex(rotation), 0, 0, cx, cz);
        bottomX = std::min(x, x + cx);
        bottomZ = std::min(z, z + cz);
        std::vector<std::pair<int, int>> positions;
        positions.push_back({bottomX - 16 + uniform(rng, 1, 8), bottomZ + 16 + uniform(rng, 1, 7)});
        positions.push_back({bottomX - 16 + uniform(rng, 1, 8), bottomZ + uniform(rng, 1, 7)});
        positions.push_back({bottomX - 16 + uniform(rng, 1, 8), bottomZ - 16 + uniform(rng, 4, 8)});
        positions.push_back({bottomX + uniform(rng, 1, 7), bottomZ + 16 + uniform(rng, 1, 7)});
        positions.push_back({bottomX + uniform(rng, 1, 7), bottomZ - 16 + uniform(rng, 4, 6)});
        positions.push_back({bottomX + 16 + uniform(rng, 1, 7), bottomZ + 16 + uniform(rng, 3, 8)});
        positions.push_back({bottomX + 16 + uniform(rng, 1, 7), bottomZ + uniform(rng, 1, 7)});
        positions.push_back({bottomX + 16 + uniform(rng, 1, 7), bottomZ - 16 + uniform(rng, 4, 8)});
        const int ruins = uniform(rng, 4, 8);
        for (int i = 0; i < ruins && !positions.empty(); i++) {
            const int idx = rng.nextInt(static_cast<int>(positions.size()));
            const auto [px, pz] = positions[static_cast<size_t>(idx)];
            positions.erase(positions.begin() + idx);
            const int nextRot = rng.nextInt(4);
            const Box2 next = cornerBox(px, pz, 5, 6, nextRot);
            if (!next.intersects(parent))
                addPiece(pieces, warm, false, rng, nextRot, px, pz);
        }
    }
    // Chests: the random for the chest table is the chunk post-process random,
    // not the placement random.
    std::map<std::pair<int, int>, PostProcessRandom> postRngs;
    for (const Piece &piece : pieces) {
        const data::TemplateLayout *layout = findLayout(piece.name);
        if (!layout) continue;
        out.pieces++;
        for (int i = 0; i < layout->count; i++) {
            const data::ChestOffset &c = data::kChestOffsets[layout->first + i];
            int tx, tz;
            templateTransform(c.x, c.z, Mirror::none, rotationByIndex(piece.rotation), 0, 0, tx, tz);
            StructureChest chest{};
            chest.x = piece.x + tx;
            chest.z = piece.z + tz;
            chest.y = 0;
            chest.templateName = piece.name;
            chest.lootTable = c.table;
            const auto key = std::make_pair(chest.x >> 4, chest.z >> 4);
            auto it = postRngs.find(key);
            if (it == postRngs.end()) {
                it = postRngs.emplace(key, PostProcessRandom(
                    mc, loots_structure_loot_seed(mc, seed, chest.x, chest.z, 6))).first;
            }
            chest.lootSeed = it->second.nextLong();
            out.chests.push_back(std::move(chest));
        }
        if (warm) collectSuspicious(mc, seed, piece, out);
    }
    out.exists = out.pieces > 0;
    return out.exists;
#else
    (void)mc; (void)seed; (void)x; (void)z;
    return false;
#endif
}

} // namespace loots
