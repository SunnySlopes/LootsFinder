// Exact coordinate transforms used by Minecraft structures.
//
// There are two different transform systems and mixing them is a common bug:
//   * StructurePiece (scattered features, fortress/end-city pieces): local
//     coordinates are mapped by getWorldX/getWorldZ using the piece's
//     Direction. The bounding box itself was created by makeBoundingBox().
//   * StructureTemplate (jigsaw/template pieces): local coordinates are
//     rotated/mirrored around a pivot with StructureTemplate.transform().
#pragma once

#include <cstdint>

namespace loots {

enum class Direction2D : uint8_t { north = 0, east = 1, south = 2, west = 3 };

/** Direction.Plane.HORIZONTAL order: north, east, south, west. */
Direction2D horizontalByIndex(int index);

struct Box2i {
    int minX = 0;
    int minZ = 0;
    int maxX = 0;
    int maxZ = 0;
};

/** StructurePiece.makeBoundingBox(x, y, z, direction, width, height, depth). */
Box2i makePieceBox(int x, int z, Direction2D direction, int width, int depth);

/** StructurePiece.getWorldX(x, z). */
int pieceWorldX(Direction2D direction, const Box2i &box, int x, int z);
/** StructurePiece.getWorldZ(x, z). */
int pieceWorldZ(Direction2D direction, const Box2i &box, int x, int z);
/** StructurePiece.getWorldPos(x, y, z); y is relative to the piece min Y. */
int pieceWorldY(int minY, int y);

enum class Mirror : uint8_t { none = 0, leftRight = 1, frontBack = 2 };
enum class Rotation : uint8_t { none = 0, clockwise90 = 1, clockwise180 = 2, counterclockwise90 = 3 };

/** Rotation.getRandom(random) order used by vanilla: NONE, CW90, 180, CCW90. */
Rotation rotationByIndex(int index);

/** StructureTemplate.getRotatedSize(size, rotation). */
void templateRotatedSize(int sizeX, int sizeZ, Rotation rotation, int &outX, int &outZ);

/** StructureTemplate.transform(pos, mirror, rotation, pivot). */
void templateTransform(int x, int y, int z, Mirror mirror, Rotation rotation, int pivotX, int pivotY,
                       int pivotZ, int &outX, int &outY, int &outZ);
inline void templateTransform(int x, int z, Mirror mirror, Rotation rotation, int pivotX, int pivotZ,
                              int &outX, int &outZ)
{
    int y = 0;
    templateTransform(x, 0, z, mirror, rotation, pivotX, 0, pivotZ, outX, y, outZ);
}

/** StructureTemplate.getBoundingBox(position, rotation, pivot, mirror, size). */
Box2i templateBox(int positionX, int positionZ, Rotation rotation, int pivotX, int pivotZ, int sizeX,
                  int sizeZ);

} // namespace loots
