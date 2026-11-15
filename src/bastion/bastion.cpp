#include "loots/bastion.h"

#include <cstring>
#include <map>
#include <string>

#include "loots/jigsaw.h"
#include "loots/rng.h"
#include "pieces_c.h"

#if defined(__has_include)
#if __has_include("chest_layouts.hpp")
#define LOOTS_HAS_CHEST_LAYOUTS 1
#include "chest_layouts.hpp"
#endif
#if __has_include("jigsaw_templates.hpp")
#define LOOTS_HAS_JIGSAW_DATA 1
#include "jigsaw_templates.hpp"
#endif
#endif

namespace loots {
namespace {

#if defined(LOOTS_HAS_CHEST_LAYOUTS)

const data::TemplateLayout *findLayout(const std::string &name)
{
    for (const auto &t : data::kTemplateLayouts) {
        if (name == t.name) return &t;
    }
    return nullptr;
}

#ifdef LOOTS_HAS_JIGSAW_DATA
/** Template footprint from the jigsaw data (falls back to 16x16). */
void templateSize(const std::string &name, int &sx, int &sz)
{
    sx = sz = 16;
    for (const auto &t : data::kTemplateInfos) {
        if (name == t.name) {
            sx = t.sx;
            sz = t.sz;
            return;
        }
    }
}
#else
void templateSize(const std::string &, int &sx, int &sz) { sx = sz = 16; }
#endif

/** Rotate a local (x,z) into the rotated footprint of an `sx * sz` template. */
void rotateLocal(int x, int z, int sx, int sz, int rot, int &ox, int &oz)
{
    switch (rot & 3) {
    case 0: ox = x; oz = z; break;
    case 1: ox = sz - 1 - z; oz = x; break;
    case 2: ox = sx - 1 - x; oz = sz - 1 - z; break;
    default: ox = z; oz = sx - 1 - x; break;
    }
}

#endif

} // namespace

bool bastionLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.kind = "bastion";
#ifdef LOOTS_HAS_CHEST_LAYOUTS
    const JigsawAssembly as = assembleStructure("bastion/starts", mc, seed, x, z, 6);
    if (as.pieces.empty()) return false;
    out.exists = true;
    out.pieces = int(as.pieces.size());
    out.minX = out.minZ = 0x7fffffff;
    out.maxX = out.maxZ = -0x7fffffff;
    out.minY = out.maxY = 0;
    for (const auto &piece : as.pieces) {
        if (piece.x < out.minX) out.minX = piece.x;
        if (piece.z < out.minZ) out.minZ = piece.z;
        if (piece.maxX > out.maxX) out.maxX = piece.maxX;
        if (piece.maxZ > out.maxZ) out.maxZ = piece.maxZ;
    }
    if (as.pieces.empty()) out.minX = out.minZ = out.maxX = out.maxZ = 0;

    std::map<std::pair<int, int>, PostProcessRandom> postRngs;
    for (const auto &piece : as.pieces) {
        const data::TemplateLayout *layout = findLayout(piece.name);
        if (!layout || layout->count == 0) continue;
        int sx, sz;
        templateSize(piece.name, sx, sz);
        for (int i = 0; i < layout->count; i++) {
            const data::ChestOffset &c = data::kChestOffsets[layout->first + i];
            int ox, oz;
            rotateLocal(c.x, c.z, sx, sz, piece.rotation, ox, oz);
            StructureChest chest{};
            chest.x = piece.x + ox;
            chest.z = piece.z + oz;
            chest.y = 0;   // jigsaw Y is not modelled yet; the map only needs x/z
            chest.templateName = piece.name;
            chest.lootTable = c.table;
            const auto key = std::make_pair(chest.x >> 4, chest.z >> 4);
            auto it = postRngs.find(key);
            if (it == postRngs.end()) {
                it = postRngs.emplace(key, PostProcessRandom(
                    mc, loots_structure_loot_seed(mc, seed, chest.x, chest.z, 19 /* Bastion */))).first;
            }
            chest.lootSeed = it->second.nextLong();
            out.chests.push_back(chest);
        }
    }
    return true;
#else
    (void)mc;
    (void)seed;
    (void)x;
    (void)z;
    return false;
#endif
}

} // namespace loots
