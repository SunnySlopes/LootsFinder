// Exact integer model of the VoxelShape free-space checks in
// JigsawPlacement.Placer.
//
// Vanilla keeps a shape and repeatedly subtracts the placed target bounding
// boxes (ONLY_FIRST).  The shape starts as either the global max-distance AABB
// minus the centre piece, or as the source piece's own AABB.  A candidate fits
// when `Shapes.joinIsNotEmpty(free, targetBB.deflate(0.25), ONLY_SECOND)` is
// false, i.e. no part of the slightly deflated target lies outside `free`.
//
// Every box here has integer inclusive bounds, equivalent to the continuous
// AABB [min, max + 1].  Deflating by 0.25 cannot change containment or overlap
// for integer boxes: a shared face has zero volume, and any overlap of at least
// one block still overlaps after the deflate.  So the exact check reduces to:
// the target is inside the bounds and does not overlap any subtracted box.
#pragma once

#include <vector>

#include "loots/jigsaw_core.h"

namespace loots {
namespace jigsaw {

struct FreeSpace {
    Box bounds;
    std::vector<Box> occupied;

    bool fits(const Box &candidate) const
    {
        if (!bounds.contains(candidate)) return false;
        for (const Box &box : occupied) {
            if (box.intersects(candidate)) return false;
        }
        return true;
    }

    void add(const Box &candidate) { occupied.push_back(candidate); }
};

} // namespace jigsaw
} // namespace loots
