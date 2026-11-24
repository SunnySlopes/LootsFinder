#include "loots/woodland_mansion.h"

#include <algorithm>
#ifndef NDEBUG
#include <cstdlib>
#include <cstdio>
#endif
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "loots/rng.h"
#include "loots/transform.h"
#include "loots/worldgen.h"
#include "pieces_c.h"

// Generated vanilla data.  The chest layout table is gitignored (local
// extraction); without it this module stays inert, like the other data-backed
// features.  mansion_templates.hpp supplies template sizes and chest counts.
#if defined(__has_include)
#if __has_include("chest_layouts.hpp")
#define LOOTS_MANSION_HAS_LAYOUTS 1
#include "chest_layouts.hpp"
#endif
#if __has_include("mansion_templates.hpp")
#define LOOTS_MANSION_HAS_TEMPLATES 1
#include "mansion_templates.hpp"
#endif
#endif

#if defined(LOOTS_MANSION_HAS_LAYOUTS) && defined(LOOTS_MANSION_HAS_TEMPLATES)

namespace loots {
namespace {

// Direction.Plane.HORIZONTAL order: north, east, south, west.
constexpr int kDirX[4] = {0, 1, 0, -1};
constexpr int kDirZ[4] = {-1, 0, 1, 0};
constexpr int kUp = 4; // Direction.UP, used as a "door is on the ceiling" marker

struct Step {
    int x;
    int z;
};

/** Rotation.rotate(Direction) for a horizontal direction. */
Step rotateStep(int direction, int rotation)
{
    const int x = kDirX[direction];
    const int z = kDirZ[direction];
    switch (rotation & 3) {
    case 1: return {-z, x};   // CLOCKWISE_90
    case 2: return {-x, -z};  // CLOCKWISE_180
    case 3: return {z, -x};   // COUNTERCLOCKWISE_90
    default: return {x, z};
    }
}

/** BlockPos.rotate(Rotation). */
void rotateBlockPos(int x, int z, int rotation, int &outX, int &outZ)
{
    switch (rotation & 3) {
    case 1: outX = -z; outZ = x; break;
    case 2: outX = -x; outZ = -z; break;
    case 3: outX = z; outZ = -x; break;
    default: outX = x; outZ = z; break;
    }
}

/** WoodlandMansionPieces.SimpleGrid. */
struct Grid {
    static constexpr int kWidth = 11;
    static constexpr int kHeight = 11;
    static constexpr int kOutside = 5;

    int cells[kWidth][kHeight] = {};

    void set(int x, int y, int value)
    {
        if (x >= 0 && x < kWidth && y >= 0 && y < kHeight) cells[x][y] = value;
    }

    void setRange(int x0, int y0, int x1, int y1, int value)
    {
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) set(x, y, value);
    }

    int get(int x, int y) const
    {
        return (x >= 0 && x < kWidth && y >= 0 && y < kHeight) ? cells[x][y] : kOutside;
    }

    void setIf(int x, int y, int ifValue, int value)
    {
        if (get(x, y) == ifValue) set(x, y, value);
    }

    bool edgesTo(int x, int y, int ifValue) const
    {
        return get(x - 1, y) == ifValue || get(x + 1, y) == ifValue ||
               get(x, y + 1) == ifValue || get(x, y - 1) == ifValue;
    }

    /** MansionGrid.isHouse: CLEAR / CORRIDOR / ROOM / START_ROOM / TEST_ROOM. */
    bool isHouse(int x, int y) const
    {
        const int value = get(x, y);
        return value == 1 || value == 2 || value == 3 || value == 4;
    }
};

struct GridPos {
    int x;
    int y;
};

/** WoodlandMansionPieces.MansionGrid. */
class MansionGrid {
public:
    explicit MansionGrid(LegacyRandom &random) : random_(random) { build(); }

    Grid baseGrid;
    Grid thirdFloorGrid;
    Grid floorRooms[3];
    int entranceX = 7;
    int entranceY = 4;

    bool isRoomId(int floor, int x, int y, int roomId) const
    {
        return (floorRooms[floor].get(x, y) & 65535) == roomId;
    }

    /**
     * MansionGrid.get1x2RoomDirection.  The grid argument is unused by Java
     * (it always reads floorRooms[floor]); -1 replaces Java's null.
     */
    int room1x2Direction(int x, int y, int floor, int roomId) const
    {
        for (int direction = 0; direction < 4; direction++) {
            if (isRoomId(floor, x + kDirX[direction], y + kDirZ[direction], roomId))
                return direction;
        }
        return -1;
    }

private:
    void build()
    {
        baseGrid.setRange(entranceX, entranceY, entranceX + 1, entranceY + 1, 3);       // START_ROOM
        baseGrid.setRange(entranceX - 1, entranceY, entranceX - 1, entranceY + 1, 2);   // ROOM
        baseGrid.setRange(entranceX + 2, entranceY - 2, entranceX + 3, entranceY + 3, 5); // BLOCKED
        baseGrid.setRange(entranceX + 1, entranceY - 2, entranceX + 1, entranceY - 1, 1); // CORRIDOR
        baseGrid.setRange(entranceX + 1, entranceY + 2, entranceX + 1, entranceY + 3, 1);
        baseGrid.set(entranceX - 1, entranceY - 1, 1);
        baseGrid.set(entranceX - 1, entranceY + 2, 1);
        baseGrid.setRange(0, 0, Grid::kWidth, 1, 5);
        baseGrid.setRange(0, 9, Grid::kWidth, Grid::kHeight, 5);
        recursiveCorridor(baseGrid, entranceX, entranceY - 2, 3 /* WEST */, 6);
        recursiveCorridor(baseGrid, entranceX, entranceY + 3, 3 /* WEST */, 6);
        recursiveCorridor(baseGrid, entranceX - 2, entranceY - 1, 3 /* WEST */, 3);
        recursiveCorridor(baseGrid, entranceX - 2, entranceY + 2, 3 /* WEST */, 3);

        while (cleanEdges(baseGrid)) {
        }

        identifyRooms(baseGrid, floorRooms[0]);
        identifyRooms(baseGrid, floorRooms[1]);
        floorRooms[0].setRange(entranceX + 1, entranceY, entranceX + 1, entranceY + 1, 8388608);
        floorRooms[1].setRange(entranceX + 1, entranceY, entranceX + 1, entranceY + 1, 8388608);

        setupThirdFloor();
        identifyRooms(thirdFloorGrid, floorRooms[2]);
    }

    void recursiveCorridor(Grid &grid, int x, int y, int heading, int depth)
    {
        if (depth <= 0) return;

        grid.set(x, y, 1);
        grid.setIf(x + kDirX[heading], y + kDirZ[heading], 0, 1);

        for (int attempts = 0; attempts < 8; attempts++) {
            // Direction.from2DDataValue: SOUTH(0), WEST(1), NORTH(2), EAST(3),
            // mapped into this file's north,east,south,west order.
            static const int kFrom2D[4] = {2, 3, 0, 1};
            const int next = kFrom2D[random_.nextInt(4)];
            // Java: nextDir != opposite && (nextDir != EAST || !nextBoolean()).
            bool accepted = next != ((heading + 2) & 3);
            if (accepted && next == 1 /* EAST */) accepted = !random_.nextBoolean();
            if (accepted) {
                const int nx = x + kDirX[heading];
                const int nz = y + kDirZ[heading];
                if (grid.get(nx + kDirX[next], nz + kDirZ[next]) == 0 &&
                    grid.get(nx + 2 * kDirX[next], nz + 2 * kDirZ[next]) == 0) {
                    recursiveCorridor(grid, x + kDirX[heading] + kDirX[next],
                                      y + kDirZ[heading] + kDirZ[next], next, depth - 1);
                    break;
                }
            }
        }

        const int cw = (heading + 1) & 3;
        const int ccw = (heading + 3) & 3;
        grid.setIf(x + kDirX[cw], y + kDirZ[cw], 0, 2);
        grid.setIf(x + kDirX[ccw], y + kDirZ[ccw], 0, 2);
        grid.setIf(x + kDirX[heading] + kDirX[cw], y + kDirZ[heading] + kDirZ[cw], 0, 2);
        grid.setIf(x + kDirX[heading] + kDirX[ccw], y + kDirZ[heading] + kDirZ[ccw], 0, 2);
        grid.setIf(x + 2 * kDirX[heading], y + 2 * kDirZ[heading], 0, 2);
        grid.setIf(x + 2 * kDirX[cw], y + 2 * kDirZ[cw], 0, 2);
        grid.setIf(x + 2 * kDirX[ccw], y + 2 * kDirZ[ccw], 0, 2);
    }

    bool cleanEdges(Grid &grid)
    {
        bool touched = false;
        for (int y = 0; y < Grid::kHeight; y++) {
            for (int x = 0; x < Grid::kWidth; x++) {
                if (grid.get(x, y) != 0) continue;

                int directNeighbors = 0;
                directNeighbors += grid.isHouse(x + 1, y) ? 1 : 0;
                directNeighbors += grid.isHouse(x - 1, y) ? 1 : 0;
                directNeighbors += grid.isHouse(x, y + 1) ? 1 : 0;
                directNeighbors += grid.isHouse(x, y - 1) ? 1 : 0;
                if (directNeighbors >= 3) {
                    grid.set(x, y, 2);
                    touched = true;
                } else if (directNeighbors == 2) {
                    int diagonalNeighbors = 0;
                    diagonalNeighbors += grid.isHouse(x + 1, y + 1) ? 1 : 0;
                    diagonalNeighbors += grid.isHouse(x - 1, y + 1) ? 1 : 0;
                    diagonalNeighbors += grid.isHouse(x + 1, y - 1) ? 1 : 0;
                    diagonalNeighbors += grid.isHouse(x - 1, y - 1) ? 1 : 0;
                    if (diagonalNeighbors <= 1) {
                        grid.set(x, y, 2);
                        touched = true;
                    }
                }
            }
        }
        return touched;
    }

    void identifyRooms(const Grid &fromGrid, Grid &roomGrid)
    {
        std::vector<GridPos> roomPositions;
        for (int y = 0; y < Grid::kHeight; y++) {
            for (int x = 0; x < Grid::kWidth; x++) {
                if (fromGrid.get(x, y) == 2) roomPositions.push_back({x, y});
            }
        }

        // Util.shuffle(roomPos, random): same Fisher-Yates order as Collections.
        for (int i = static_cast<int>(roomPositions.size()); i > 1; i--) {
            const int swapTo = random_.nextInt(i);
            std::swap(roomPositions[static_cast<size_t>(i - 1)], roomPositions[static_cast<size_t>(swapTo)]);
        }

        int roomId = 10;
        for (const GridPos &pos : roomPositions) {
            const int x = pos.x;
            const int y = pos.y;
            if (roomGrid.get(x, y) != 0) continue;

            int x0 = x;
            int x1 = x;
            int y0 = y;
            int y1 = y;
            int type = 65536; // ROOM_1x1

            if (roomGrid.get(x + 1, y) == 0 && roomGrid.get(x, y + 1) == 0 &&
                roomGrid.get(x + 1, y + 1) == 0 && fromGrid.get(x + 1, y) == 2 &&
                fromGrid.get(x, y + 1) == 2 && fromGrid.get(x + 1, y + 1) == 2) {
                x1++;
                y1++;
                type = 262144; // ROOM_2x2
            } else if (roomGrid.get(x - 1, y) == 0 && roomGrid.get(x, y + 1) == 0 &&
                       roomGrid.get(x - 1, y + 1) == 0 && fromGrid.get(x - 1, y) == 2 &&
                       fromGrid.get(x, y + 1) == 2 && fromGrid.get(x - 1, y + 1) == 2) {
                x0--;
                y1++;
                type = 262144;
            } else if (roomGrid.get(x - 1, y) == 0 && roomGrid.get(x, y - 1) == 0 &&
                       roomGrid.get(x - 1, y - 1) == 0 && fromGrid.get(x - 1, y) == 2 &&
                       fromGrid.get(x, y - 1) == 2 && fromGrid.get(x - 1, y - 1) == 2) {
                x0--;
                y0--;
                type = 262144;
            } else if (roomGrid.get(x + 1, y) == 0 && fromGrid.get(x + 1, y) == 2) {
                x1++;
                type = 131072; // ROOM_1x2
            } else if (roomGrid.get(x, y + 1) == 0 && fromGrid.get(x, y + 1) == 2) {
                y1++;
                type = 131072;
            } else if (roomGrid.get(x - 1, y) == 0 && fromGrid.get(x - 1, y) == 2) {
                x0--;
                type = 131072;
            } else if (roomGrid.get(x, y - 1) == 0 && fromGrid.get(x, y - 1) == 2) {
                y0--;
                type = 131072;
            }

            int doorX = random_.nextBoolean() ? x0 : x1;
            int doorY = random_.nextBoolean() ? y0 : y1;
            int doorFlag = 2097152; // ROOM_DOOR_FLAG
            if (!fromGrid.edgesTo(doorX, doorY, 1)) {
                doorX = doorX == x0 ? x1 : x0;
                doorY = doorY == y0 ? y1 : y0;
                if (!fromGrid.edgesTo(doorX, doorY, 1)) {
                    doorY = doorY == y0 ? y1 : y0;
                    if (!fromGrid.edgesTo(doorX, doorY, 1)) {
                        doorX = doorX == x0 ? x1 : x0;
                        doorY = doorY == y0 ? y1 : y0;
                        if (!fromGrid.edgesTo(doorX, doorY, 1)) {
                            doorFlag = 0;
                            doorX = x0;
                            doorY = y0;
                        }
                    }
                }
            }

            for (int ry = y0; ry <= y1; ry++) {
                for (int rx = x0; rx <= x1; rx++) {
                    if (rx == doorX && ry == doorY) {
                        roomGrid.set(rx, ry, 1048576 | doorFlag | type | roomId);
                    } else {
                        roomGrid.set(rx, ry, type | roomId);
                    }
                }
            }
            roomId++;
        }
    }

    void setupThirdFloor()
    {
        std::vector<GridPos> potentialRooms;
        const Grid &floor = floorRooms[1];

        for (int y = 0; y < Grid::kHeight; y++) {
            for (int x = 0; x < Grid::kWidth; x++) {
                const int roomData = floor.get(x, y);
                const int roomType = roomData & 983040;
                if (roomType == 131072 && (roomData & 2097152) == 2097152)
                    potentialRooms.push_back({x, y});
            }
        }

        if (potentialRooms.empty()) {
            thirdFloorGrid.setRange(0, 0, Grid::kWidth, Grid::kHeight, 5);
            return;
        }

        const GridPos roomPos = potentialRooms[static_cast<size_t>(
            random_.nextInt(static_cast<int>(potentialRooms.size())))];
        const int roomData = floor.get(roomPos.x, roomPos.y);
        floorRooms[1].set(roomPos.x, roomPos.y, roomData | 4194304); // ROOM_STAIRS_FLAG
        const int roomDir =
            room1x2Direction(roomPos.x, roomPos.y, 1, roomData & 65535);
        // Java dereferences a potentially null direction here; a mansion never
        // reaches that state, but stay defensive instead of emitting UB.
        if (roomDir < 0) {
            thirdFloorGrid.setRange(0, 0, Grid::kWidth, Grid::kHeight, 5);
            floorRooms[1].set(roomPos.x, roomPos.y, roomData);
            return;
        }
        const int roomEndX = roomPos.x + kDirX[roomDir];
        const int roomEndY = roomPos.y + kDirZ[roomDir];

        for (int y = 0; y < Grid::kHeight; y++) {
            for (int x = 0; x < Grid::kWidth; x++) {
                if (!baseGrid.isHouse(x, y)) {
                    thirdFloorGrid.set(x, y, 5);
                } else if (x == roomPos.x && y == roomPos.y) {
                    thirdFloorGrid.set(x, y, 3);
                } else if (x == roomEndX && y == roomEndY) {
                    thirdFloorGrid.set(x, y, 3);
                    floorRooms[2].set(x, y, 8388608); // ROOM_CORRIDOR_FLAG
                }
            }
        }

        std::vector<int> potentialCorridors;
        for (int direction = 0; direction < 4; direction++) {
            if (thirdFloorGrid.get(roomEndX + kDirX[direction],
                                   roomEndY + kDirZ[direction]) == 0)
                potentialCorridors.push_back(direction);
        }

        if (potentialCorridors.empty()) {
            thirdFloorGrid.setRange(0, 0, Grid::kWidth, Grid::kHeight, 5);
            floorRooms[1].set(roomPos.x, roomPos.y, roomData);
            return;
        }

        const int corridorDir = potentialCorridors[static_cast<size_t>(
            random_.nextInt(static_cast<int>(potentialCorridors.size())))];
        recursiveCorridor(thirdFloorGrid, roomEndX + kDirX[corridorDir],
                          roomEndY + kDirZ[corridorDir], corridorDir, 4);

        while (cleanEdges(thirdFloorGrid)) {
        }
    }

    LegacyRandom &random_;
};

const data::MansionTemplate *findMansionTemplate(const std::string &fullName)
{
    for (const auto &t : data::kMansionTemplates) {
        if (fullName == t.name) return &t;
    }
    return nullptr;
}

const data::TemplateLayout *findTemplateLayout(const std::string &fullName)
{
    for (const auto &t : data::kTemplateLayouts) {
        if (fullName == t.name) return &t;
    }
    return nullptr;
}

struct PlacedRoom {
    std::string name;
    int x;
    int z;
    int rotation;
    int mirror;
};

/**
 * WoodlandMansionPieces.MansionPiecePlacer, restricted to room placement.
 * Outer/inner walls, entrance, corridors/carpets and roofs are skipped: they
 * contain no chests and consume no random numbers, so the room sequence (and
 * therefore every chest coordinate and per-chunk chest order) is unaffected.
 */
class MansionPlacer {
public:
    MansionPlacer(const MansionGrid &mansion, LegacyRandom &random, int mc, uint64_t seed,
                  int originX, int originZ, int rotation, StructureLoot &out)
        : mansion_(mansion), random_(random), mc_(mc), seed_(seed), originX_(originX),
          originZ_(originZ), rotation_(rotation), out_(out)
    {
        startX_ = mansion_.entranceX + 1;
        startY_ = mansion_.entranceY + 1;
        // cubiomes currently has no Mansion salt config; the probe both avoids a
        // per-chest stderr diagnostic and keeps the "unsupported -> 0" contract.
        lootSeedSupported_ = loots_structure_loot_seed(mc_, seed_, originX_, originZ_, 9) != 0;
    }

    void createMansion()
    {
        const Step south = rotateStep(2, rotation_);
        const Step east = rotateStep(1, rotation_);

        for (int floorNum = 0; floorNum < 3; floorNum++) {
            const Grid &grid = (floorNum == 2) ? mansion_.thirdFloorGrid : mansion_.baseGrid;
            const Grid &rooms = mansion_.floorRooms[floorNum];
            const int floorOriginX = originX_;
            const int floorOriginZ = originZ_;

            for (int y = 0; y < Grid::kHeight; y++) {
                for (int x = 0; x < Grid::kWidth; x++) {
                    bool thirdFloorStartRoom = floorNum == 2 && grid.get(x, y) == 3;
                    if (grid.get(x, y) != 2 && !thirdFloorStartRoom) continue;

                    const int roomData = rooms.get(x, y);
                    const int roomType = roomData & 983040;
                    const int roomId = roomData & 65535;
                    thirdFloorStartRoom =
                        thirdFloorStartRoom && (roomData & 8388608) == 8388608;

                    int doorDirs[4];
                    int doorCount = 0;
                    if ((roomData & 2097152) == 2097152) {
                        for (int direction = 0; direction < 4; direction++) {
                            if (grid.get(x + kDirX[direction], y + kDirZ[direction]) == 1)
                                doorDirs[doorCount++] = direction;
                        }
                    }
                    int doorDir = -1; // Java's null
                    if (doorCount > 0) {
                        doorDir = doorDirs[random_.nextInt(doorCount)];
                    } else if ((roomData & 1048576) == 1048576) {
                        doorDir = kUp;
                    }

                    const int southOffset = 8 + (y - startY_) * 8;
                    const int eastOffset = -1 + (x - startX_) * 8;
                    const int roomX = floorOriginX + south.x * southOffset + east.x * eastOffset;
                    const int roomZ = floorOriginZ + south.z * southOffset + east.z * eastOffset;

                    if (roomType == 65536) {
                        addRoom1x1(roomX, roomZ, doorDir, floorNum);
                    } else if (roomType == 131072 && doorDir != -1) {
                        const int roomDir =
                            mansion_.room1x2Direction(x, y, floorNum, roomId);
                        const bool isStairsRoom = (roomData & 4194304) == 4194304;
                        addRoom1x2(roomX, roomZ, roomDir, doorDir, floorNum, isStairsRoom);
                    } else if (roomType == 262144 && doorDir != -1 && doorDir != kUp) {
                        int roomDir = (doorDir + 1) & 3; // doorDir.getClockWise()
                        if (!mansion_.isRoomId(floorNum, x + kDirX[roomDir],
                                               y + kDirZ[roomDir], roomId))
                            roomDir = (roomDir + 2) & 3; // getOpposite()
                        addRoom2x2(roomX, roomZ, roomDir, doorDir, floorNum);
                    } else if (roomType == 262144 && doorDir == kUp) {
                        addRoom2x2Secret(roomX, roomZ, floorNum);
                    }
                }
            }
        }

        out_.pieces = static_cast<int>(rooms_.size());
    }

private:
    void addPiece(const std::string &name, int anchorX, int anchorZ, int rotation, int mirror)
    {
        rooms_.push_back({name, anchorX, anchorZ, rotation, mirror});
#ifndef NDEBUG
        if (std::getenv("LOOTS_MANSION_DEBUG"))
            std::fprintf(stderr, "ROOM %s %d %d rot%d mir%d\n", name.c_str(), anchorX, anchorZ, rotation, mirror);
#endif

        const std::string fullName = "woodland_mansion/" + name;
        const data::MansionTemplate *tpl = findMansionTemplate(fullName);
        if (!tpl) return;

        const Rotation pieceRotation = rotationByIndex(rotation);
        const Mirror pieceMirror = static_cast<Mirror>(mirror);

        // StructureTemplate.getBoundingBox(position, rotation, pivot=0, mirror).
        int x0, z0, x1, z1;
        templateTransform(0, 0, pieceMirror, pieceRotation, 0, 0, x0, z0);
        templateTransform(tpl->sx - 1, tpl->sz - 1, pieceMirror, pieceRotation, 0, 0, x1, z1);
        const int minX = anchorX + std::min(x0, x1);
        const int maxX = anchorX + std::max(x0, x1);
        const int minZ = anchorZ + std::min(z0, z1);
        const int maxZ = anchorZ + std::max(z0, z1);
        if (!haveBounds_) {
            out_.minX = minX;
            out_.minZ = minZ;
            out_.maxX = maxX;
            out_.maxZ = maxZ;
            haveBounds_ = true;
        } else {
            out_.minX = std::min(out_.minX, minX);
            out_.minZ = std::min(out_.minZ, minZ);
            out_.maxX = std::max(out_.maxX, maxX);
            out_.maxZ = std::max(out_.maxZ, maxZ);
        }

        const data::TemplateLayout *layout = findTemplateLayout(fullName);
        if (!layout) return;
        for (int i = 0; i < layout->count; i++) {
            const data::ChestOffset &offset = data::kChestOffsets[layout->first + i];
            int chestX, chestZ;
            // Data-marker positions: transform(local, mirror, rotation, pivot=0)
            // then offset by the piece's template position.
            templateTransform(offset.x, offset.z, pieceMirror, pieceRotation, 0, 0,
                              chestX, chestZ);
            StructureChest chest{};
            chest.x = anchorX + chestX;
            chest.z = anchorZ + chestZ;
            chest.y = 0; // start Y is terrain dependent and not an input here
            chest.templateName = fullName;
            chest.lootTable = offset.table;
            chest.lootSeed = nextLootSeed(chest.x, chest.z);
            out_.chests.push_back(std::move(chest));
        }
    }

    uint64_t nextLootSeed(int x, int z)
    {
        if (!lootSeedSupported_) return 0;
        const auto key = std::make_pair(x >> 4, z >> 4);
        auto it = postRngs_.find(key);
        if (it == postRngs_.end()) {
            it = postRngs_
                     .emplace(key, PostProcessRandom(
                                       mc_, loots_structure_loot_seed(mc_, seed_, x, z, 9)))
                     .first;
        }
        return it->second.nextLong();
    }

    /** StructureTemplate.getZeroPositionWithTransform(zeroPos, NONE, rot, 7, 7). */
    static void zeroPositionWithTransform(int zeroX, int zeroZ, int rotation, int sizeX,
                                          int sizeZ, int &outX, int &outZ)
    {
        sizeX--;
        sizeZ--;
        switch (rotation & 3) {
        case 3: outX = zeroX; outZ = zeroZ + sizeX; break;             // CCW90
        case 1: outX = zeroX + sizeZ; outZ = zeroZ; break;             // CW90
        case 2: outX = zeroX + sizeX; outZ = zeroZ + sizeZ; break;     // 180
        default: outX = zeroX; outZ = zeroZ; break;
        }
    }

    void addRoom1x1(int roomX, int roomZ, int doorDir, int floor)
    {
        int pieceRot = 0; // Rotation.NONE
        std::string name = (floor == 0 ? "1x1_a" : "1x1_b") +
                           std::to_string(random_.nextInt(5) + 1);
        if (doorDir != 1 /* EAST */) {
            if (doorDir == 0 /* NORTH */) {
                pieceRot = 3; // COUNTERCLOCKWISE_90
            } else if (doorDir == 3 /* WEST */) {
                pieceRot = 2; // CLOCKWISE_180
            } else if (doorDir == 2 /* SOUTH */) {
                pieceRot = 1; // CLOCKWISE_90
            } else {
                // doorDir is UP or null: the secret room variant.
                name = "1x1_as" + std::to_string(random_.nextInt(4) + 1);
            }
        }

        int orientationX, orientationZ;
        zeroPositionWithTransform(1, 0, pieceRot, 7, 7, orientationX, orientationZ);
        const int finalRot = (pieceRot + rotation_) & 3;
        int rotatedX, rotatedZ;
        rotateBlockPos(orientationX, orientationZ, rotation_, rotatedX, rotatedZ);
        addPiece(name, roomX + rotatedX, roomZ + rotatedZ, finalRot, 0);
    }

    void addRoom1x2(int roomX, int roomZ, int roomDir, int doorDir, int floor, bool stairs)
    {
        const Step east = rotateStep(1, rotation_);
        const Step south = rotateStep(2, rotation_);
        auto anchor = [&](int eastSteps, int southSteps, int &x, int &z) {
            x = roomX + east.x * eastSteps + south.x * southSteps;
            z = roomZ + east.z * eastSteps + south.z * southSteps;
        };
        auto sideName = [&]() -> std::string {
            if (floor == 0) return "1x2_a" + std::to_string(random_.nextInt(9) + 1);
            if (stairs) return "1x2_c_stairs";
            return "1x2_c" + std::to_string(random_.nextInt(4) + 1);
        };
        auto frontName = [&]() -> std::string {
            if (floor == 0) return "1x2_b" + std::to_string(random_.nextInt(5) + 1);
            if (stairs) return "1x2_d_stairs";
            return "1x2_d" + std::to_string(random_.nextInt(5) + 1);
        };
        auto secretName = [&]() -> std::string {
            if (floor == 0) return "1x2_s" + std::to_string(random_.nextInt(2) + 1);
            return "1x2_se" + std::to_string(random_.nextInt(1) + 1);
        };

        int x, z;
        if (doorDir == 1 && roomDir == 2) {
            anchor(1, 0, x, z);
            addPiece(sideName(), x, z, rotation_, 0);
        } else if (doorDir == 1 && roomDir == 0) {
            anchor(1, 6, x, z);
            addPiece(sideName(), x, z, rotation_, 1);
        } else if (doorDir == 3 && roomDir == 0) {
            anchor(7, 6, x, z);
            addPiece(sideName(), x, z, (rotation_ + 2) & 3, 0);
        } else if (doorDir == 3 && roomDir == 2) {
            anchor(7, 0, x, z);
            addPiece(sideName(), x, z, rotation_, 2);
        } else if (doorDir == 2 && roomDir == 1) {
            anchor(1, 0, x, z);
            addPiece(sideName(), x, z, (rotation_ + 1) & 3, 1);
        } else if (doorDir == 2 && roomDir == 3) {
            anchor(7, 0, x, z);
            addPiece(sideName(), x, z, (rotation_ + 1) & 3, 0);
        } else if (doorDir == 0 && roomDir == 3) {
            anchor(7, 6, x, z);
            addPiece(sideName(), x, z, (rotation_ + 1) & 3, 2);
        } else if (doorDir == 0 && roomDir == 1) {
            anchor(1, 6, x, z);
            addPiece(sideName(), x, z, (rotation_ + 3) & 3, 0);
        } else if (doorDir == 2 && roomDir == 0) {
            anchor(1, -8, x, z);
            addPiece(frontName(), x, z, rotation_, 0);
        } else if (doorDir == 0 && roomDir == 2) {
            anchor(7, 14, x, z);
            addPiece(frontName(), x, z, (rotation_ + 2) & 3, 0);
        } else if (doorDir == 3 && roomDir == 1) {
            anchor(15, 0, x, z);
            addPiece(frontName(), x, z, (rotation_ + 1) & 3, 0);
        } else if (doorDir == 1 && roomDir == 3) {
            anchor(-7, 6, x, z);
            addPiece(frontName(), x, z, (rotation_ + 3) & 3, 0);
        } else if (doorDir == kUp && roomDir == 1) {
            anchor(15, 0, x, z);
            addPiece(secretName(), x, z, (rotation_ + 1) & 3, 0);
        } else if (doorDir == kUp && roomDir == 2) {
            anchor(1, 0, x, z);
            addPiece(secretName(), x, z, rotation_, 0);
        }
        // Java has no fallback branch; an unmatched pair places no piece.
    }

    void addRoom2x2(int roomX, int roomZ, int roomDir, int doorDir, int floor)
    {
        int eastSteps = 0;
        int southSteps = 0;
        int rot = rotation_;
        int mirror = 0; // Mirror.NONE
        if (doorDir == 1 && roomDir == 2) {
            eastSteps = -7;
        } else if (doorDir == 1 && roomDir == 0) {
            eastSteps = -7;
            southSteps = 6;
            mirror = 1; // LEFT_RIGHT
        } else if (doorDir == 0 && roomDir == 1) {
            eastSteps = 1;
            southSteps = 14;
            rot = (rotation_ + 3) & 3;
        } else if (doorDir == 0 && roomDir == 3) {
            eastSteps = 7;
            southSteps = 14;
            rot = (rotation_ + 3) & 3;
            mirror = 1;
        } else if (doorDir == 2 && roomDir == 3) {
            eastSteps = 7;
            southSteps = -8;
            rot = (rotation_ + 1) & 3;
        } else if (doorDir == 2 && roomDir == 1) {
            eastSteps = 1;
            southSteps = -8;
            rot = (rotation_ + 1) & 3;
            mirror = 1;
        } else if (doorDir == 3 && roomDir == 0) {
            eastSteps = 15;
            southSteps = 6;
            rot = (rotation_ + 2) & 3;
        } else if (doorDir == 3 && roomDir == 2) {
            eastSteps = 15;
            mirror = 2; // FRONT_BACK
        }

        const Step east = rotateStep(1, rotation_);
        const Step south = rotateStep(2, rotation_);
        const int x = roomX + east.x * eastSteps + south.x * southSteps;
        const int z = roomZ + east.z * eastSteps + south.z * southSteps;
        const std::string name =
            (floor == 0 ? "2x2_a" : "2x2_b") + std::to_string(
                floor == 0 ? random_.nextInt(4) + 1 : random_.nextInt(5) + 1);
        addPiece(name, x, z, rot, mirror);
    }

    void addRoom2x2Secret(int roomX, int roomZ, int floor)
    {
        (void)floor;
        const Step east = rotateStep(1, rotation_);
        addPiece("2x2_s1", roomX + east.x, roomZ + east.z, rotation_, 0);
    }

    const MansionGrid &mansion_;
    LegacyRandom &random_;
    int mc_;
    uint64_t seed_;
    int originX_;
    int originZ_;
    int rotation_;
    StructureLoot &out_;
    int startX_ = 8;
    int startY_ = 5;
    bool haveBounds_ = false;
    bool lootSeedSupported_ = false;
    std::vector<PlacedRoom> rooms_;
    std::map<std::pair<int, int>, PostProcessRandom> postRngs_;
};

bool mansionHeightOk(int mc, uint64_t seed, int x, int z, int *outRotation)
{
    // Pre-1.18 mansions did not use the Y>=60 5×5 gate.
    if (mc < 22) {
        if (outRotation) *outRotation = 0;
        return true;
    }
    const int chunkX = x >> 4;
    const int chunkZ = z >> 4;
    const int blockX = chunkX * 16 + 7;
    const int blockZ = chunkZ * 16 + 7;
    LegacyRandom random(largeFeatureSeedValue(mc, seed, chunkX, chunkZ));
    int rotation = random.nextInt(4);
#ifndef NDEBUG
    if (const char *forced = std::getenv("LOOTS_MANSION_ROT")) rotation = std::atoi(forced) & 3;
#endif
    if (outRotation) *outRotation = rotation;

    int offsetX = 5;
    int offsetZ = 5;
    if (rotation == 1) offsetX = -5; // CLOCKWISE_90
    else if (rotation == 2) {
        offsetX = -5;
        offsetZ = -5;
    } // CLOCKWISE_180
    else if (rotation == 3) offsetZ = -5; // COUNTERCLOCKWISE_90

    // Structure.getLowestY(minX, minZ, sizeX, sizeZ) — size may be negative.
    const int corners[4][2] = {
        {blockX, blockZ},
        {blockX, blockZ + offsetZ},
        {blockX + offsetX, blockZ},
        {blockX + offsetX, blockZ + offsetZ},
    };
    int lowest = 0x7fffffff;
    for (const auto &c : corners) {
        const int occupied = worldSurfaceHeight(mc, seed, c[0], c[1]) - 1;
        if (occupied < lowest) lowest = occupied;
    }
    return lowest >= 60;
}

bool generateMansion(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    const int chunkX = x >> 4;
    const int chunkZ = z >> 4;
    const int originX = chunkX * 16 + 7;
    const int originZ = chunkZ * 16 + 7;

    int rotation = 0;
    if (!mansionHeightOk(mc, seed, x, z, &rotation)) {
        out.exists = false;
        return false;
    }

    // Structure.GenerationContext.makeRandom: setLargeFeatureSeed(levelSeed,
    // chunkX, chunkZ). Rotation already consumed inside mansionHeightOk — replay
    // the same seed and discard that draw before MansionGrid.
    LegacyRandom random(largeFeatureSeedValue(mc, seed, chunkX, chunkZ));
    (void)random.nextInt(4);
#ifndef NDEBUG
    if (const char *forced = std::getenv("LOOTS_MANSION_ROT")) {
        (void)forced;
        // rotation forced only for height; grid still uses the real stream.
    }
#endif

    MansionGrid grid(random);
    MansionPlacer placer(grid, random, mc, seed, originX, originZ, rotation, out);
    placer.createMansion();
    out.exists = true;
    return out.exists;
}

} // namespace

bool woodlandMansionCanPlace(int mc, uint64_t seed, int x, int z)
{
    return mansionHeightOk(mc, seed, x, z, nullptr);
}

bool woodlandMansionLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.kind = "woodland_mansion";
    return generateMansion(mc, seed, x, z, out);
}

} // namespace loots

#else // !data

namespace loots {

bool woodlandMansionCanPlace(int mc, uint64_t seed, int x, int z)
{
    // Without mansion data we can still apply the height gate.
    if (mc < 22) return true;
    const int chunkX = x >> 4;
    const int chunkZ = z >> 4;
    const int blockX = chunkX * 16 + 7;
    const int blockZ = chunkZ * 16 + 7;
    LegacyRandom random(largeFeatureSeedValue(mc, seed, chunkX, chunkZ));
    const int rotation = random.nextInt(4);
    int offsetX = 5, offsetZ = 5;
    if (rotation == 1) offsetX = -5;
    else if (rotation == 2) {
        offsetX = -5;
        offsetZ = -5;
    } else if (rotation == 3) offsetZ = -5;
    const int corners[4][2] = {
        {blockX, blockZ},
        {blockX, blockZ + offsetZ},
        {blockX + offsetX, blockZ},
        {blockX + offsetX, blockZ + offsetZ},
    };
    int lowest = 0x7fffffff;
    for (const auto &c : corners) {
        const int occupied = worldSurfaceHeight(mc, seed, c[0], c[1]) - 1;
        if (occupied < lowest) lowest = occupied;
    }
    return lowest >= 60;
}

bool woodlandMansionLoot(int mc, uint64_t seed, int x, int z, StructureLoot &out)
{
    out = StructureLoot{};
    out.x = x;
    out.z = z;
    out.kind = "woodland_mansion";
    if (!woodlandMansionCanPlace(mc, seed, x, z)) {
        out.exists = false;
        return false;
    }
    (void)mc;
    (void)seed;
    return false;
}

} // namespace loots

#endif
