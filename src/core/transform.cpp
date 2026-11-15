#include "loots/transform.h"

#include <algorithm>

namespace loots {

Direction2D horizontalByIndex(int index)
{
    switch (index & 3) {
    case 0: return Direction2D::north;
    case 1: return Direction2D::east;
    case 2: return Direction2D::south;
    default: return Direction2D::west;
    }
}

Box2i makePieceBox(int x, int z, Direction2D direction, int width, int depth)
{
    Box2i box;
    const bool axisZ = direction == Direction2D::north || direction == Direction2D::south;
    if (axisZ) {
        box.minX = x;
        box.maxX = x + width - 1;
        box.minZ = z;
        box.maxZ = z + depth - 1;
    } else {
        box.minX = x;
        box.maxX = x + depth - 1;
        box.minZ = z;
        box.maxZ = z + width - 1;
    }
    return box;
}

int pieceWorldX(Direction2D direction, const Box2i &box, int x, int z)
{
    switch (direction) {
    case Direction2D::north:
    case Direction2D::south: return box.minX + x;
    case Direction2D::west: return box.maxX - z;
    case Direction2D::east: return box.minX + z;
    }
    return x;
}

int pieceWorldZ(Direction2D direction, const Box2i &box, int x, int z)
{
    switch (direction) {
    case Direction2D::north: return box.maxZ - z;
    case Direction2D::south: return box.minZ + z;
    case Direction2D::west:
    case Direction2D::east: return box.minZ + x;
    }
    return z;
}

int pieceWorldY(int minY, int y)
{
    return minY + y;
}

Rotation rotationByIndex(int index)
{
    switch (index & 3) {
    case 0: return Rotation::none;
    case 1: return Rotation::clockwise90;
    case 2: return Rotation::clockwise180;
    default: return Rotation::counterclockwise90;
    }
}

void templateRotatedSize(int sizeX, int sizeZ, Rotation rotation, int &outX, int &outZ)
{
    if (rotation == Rotation::clockwise90 || rotation == Rotation::counterclockwise90) {
        outX = sizeZ;
        outZ = sizeX;
    } else {
        outX = sizeX;
        outZ = sizeZ;
    }
}

void templateTransform(int x, int y, int z, Mirror mirror, Rotation rotation, int pivotX, int /*pivotY*/,
                       int pivotZ, int &outX, int &outY, int &outZ)
{
    switch (mirror) {
    case Mirror::leftRight: z = -z; break;
    case Mirror::frontBack: x = -x; break;
    case Mirror::none: break;
    }

    switch (rotation) {
    case Rotation::counterclockwise90:
        outX = pivotX - pivotZ + z;
        outZ = pivotX + pivotZ - x;
        break;
    case Rotation::clockwise90:
        outX = pivotX + pivotZ - z;
        outZ = pivotZ - pivotX + x;
        break;
    case Rotation::clockwise180:
        outX = pivotX + pivotX - x;
        outZ = pivotZ + pivotZ - z;
        break;
    case Rotation::none:
    default:
        outX = x;
        outZ = z;
        break;
    }
    outY = y;
}

Box2i templateBox(int positionX, int positionZ, Rotation rotation, int pivotX, int pivotZ, int sizeX,
                  int sizeZ)
{
    int x0, y0, z0, x1, y1, z1;
    templateTransform(0, 0, 0, Mirror::none, rotation, pivotX, 0, pivotZ, x0, y0, z0);
    templateTransform(sizeX - 1, 0, sizeZ - 1, Mirror::none, rotation, pivotX, 0, pivotZ, x1, y1, z1);
    Box2i box;
    box.minX = positionX + std::min(x0, x1);
    box.maxX = positionX + std::max(x0, x1);
    box.minZ = positionZ + std::min(z0, z1);
    box.maxZ = positionZ + std::max(z0, z1);
    return box;
}

} // namespace loots
