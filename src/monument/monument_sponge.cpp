#include "loots/loots.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "loots/transform.h"
#include "pieces_c.h"

// Ocean monument sponge rooms: a faithful port of OceanMonumentPieces'
// room graph (1.21.x / 26.x source, `generateRoomGraph` + the seven
// MonumentRoomFitter implementations).
//
// Grid: getRoomIndex(x,y,z) = y*25 + z*5 + x, 75 cells.
//   level 0/1: x in 0..4, z in 0..3
//   level 2  : x in 1..3, z in 0..1
// Rooms with index >= 75 are the three "special" rooms (roof 1003, left wing
// 1001, right wing 1002) and are never fitted.
//
// Sponge rooms are the cells assigned to FitSimpleTopRoom (a cell with no
// remaining opening in W/E/N/S/UP). Because every fitter's `fits()` is free of
// RNG, the sponge-room set depends only on the room graph and the pruning loop.
namespace loots {
namespace {

// Direction indices follow vanilla's Direction.get3DDataValue():
// 0 DOWN, 1 UP, 2 NORTH, 3 SOUTH, 4 WEST, 5 EAST.
constexpr int kDown = 0, kUp = 1, kNorth = 2, kSouth = 3, kWest = 4, kEast = 5;

constexpr int kStepX[6] = {0, 0, 0, 0, -1, 1};
constexpr int kStepY[6] = {-1, 1, 0, 0, 0, 0};
constexpr int kStepZ[6] = {0, 0, -1, 1, 0, 0};
constexpr int kOpposite[6] = {kUp, kDown, kSouth, kNorth, kEast, kWest};

/** Java `Collections.shuffle` (consumes nextInt(size-1)..nextInt(1)). */
class JavaRandom {
public:
    explicit JavaRandom(uint64_t seed) : state_((seed ^ 0x5DEECE66DULL) & kMask) {}

    uint32_t bits(int n)
    {
        state_ = (state_ * 0x5DEECE66DULL + 0xB) & kMask;
        return uint32_t(state_ >> (48 - n));
    }

    int nextInt(int bound)
    {
        if (bound <= 0) return 0;
        if ((bound & -bound) == bound) return int((uint64_t(bound) * bits(31)) >> 31);
        int b, r;
        do {
            b = int(bits(31));
            r = b % bound;
        } while (b - r + (bound - 1) < 0);
        return r;
    }

private:
    static constexpr uint64_t kMask = (1ULL << 48) - 1;
    uint64_t state_ = 0;
};

struct RoomDef {
    int index = 0;
    int conn[6] = {-1, -1, -1, -1, -1, -1};
    bool hasOpening[6] = {false, false, false, false, false, false};
    bool claimed = false;
    bool isSource = false;
    int scanIndex = 0;

    bool isSpecial() const { return index >= 75; }
};

int roomIndex(int x, int y, int z) { return y * 25 + z * 5 + x; }

class Graph {
public:
    std::vector<RoomDef> rooms;   // 75 grid cells + 3 specials at 75..77
    std::vector<int> order;       // shuffled room list

    Graph()
    {
        rooms.resize(78);
        for (int i = 0; i < 75; i++) rooms[i].index = i;
        rooms[75].index = 1003;   // roof room
        rooms[76].index = 1001;   // left wing
        rooms[77].index = 1002;   // right wing
    }

    bool present(int cell) const { return present_[cell]; }

    void setConnection(int a, int dir, int b)
    {
        rooms[a].conn[dir] = b;
        rooms[b].conn[kOpposite[dir]] = a;
    }

    void updateOpenings(int a)
    {
        for (int i = 0; i < 6; i++) rooms[a].hasOpening[i] = rooms[a].conn[i] >= 0;
    }

    bool findSource(int a, int scan)
    {
        if (rooms[a].isSource) return true;
        rooms[a].scanIndex = scan;
        for (int i = 0; i < 6; i++) {
            const int b = rooms[a].conn[i];
            if (b >= 0 && rooms[a].hasOpening[i] && rooms[b].scanIndex != scan && findSource(b, scan))
                return true;
        }
        return false;
    }

    bool present_[75] = {};

    void buildGraph(JavaRandom &rng)
    {
        for (int x = 0; x < 5; x++)
            for (int z = 0; z < 4; z++) present_[roomIndex(x, 0, z)] = true;
        for (int x = 0; x < 5; x++)
            for (int z = 0; z < 4; z++) present_[roomIndex(x, 1, z)] = true;
        for (int x = 1; x < 4; x++)
            for (int z = 0; z < 2; z++) present_[roomIndex(x, 2, z)] = true;

        const int source = roomIndex(2, 0, 0);
        rooms[source].isSource = true;
        // The MonumentBuilding constructor claims the entrance/source room
        // before the fitter loop; it must not be fitted again.
        rooms[source].claimed = true;

        // link neighbours, mirroring the vanilla loop exactly
        for (int x = 0; x < 5; x++)
            for (int z = 0; z < 5; z++)
                for (int y = 0; y < 3; y++) {
                    const int a = roomIndex(x, y, z);
                    if (!present_[a]) continue;
                    for (int d = 0; d < 6; d++) {
                        const int nx = x + kStepX[d], ny = y + kStepY[d], nz = z + kStepZ[d];
                        if (nx < 0 || nx >= 5 || nz < 0 || nz >= 5 || ny < 0 || ny >= 3) continue;
                        const int b = roomIndex(nx, ny, nz);
                        if (!present_[b]) continue;
                        if (nz == z) setConnection(a, d, b);
                        else setConnection(a, kOpposite[d], b);
                    }
                }

        setConnection(roomIndex(2, 2, 0), kUp, 75);
        setConnection(roomIndex(0, 1, 0), kSouth, 76);
        setConnection(roomIndex(4, 1, 0), kSouth, 77);
        rooms[75].claimed = rooms[76].claimed = rooms[77].claimed = true;

        const int core = roomIndex(rng.nextInt(4), 0, 2);
        rooms[core].claimed = true;
        int e = rooms[core].conn[kEast], n = rooms[core].conn[kNorth], u = rooms[core].conn[kUp];
        if (e >= 0) rooms[e].claimed = true;
        if (n >= 0) rooms[n].claimed = true;
        if (u >= 0) rooms[u].claimed = true;
        if (e >= 0 && rooms[e].conn[kNorth] >= 0) rooms[rooms[e].conn[kNorth]].claimed = true;
        if (e >= 0 && rooms[e].conn[kUp] >= 0) rooms[rooms[e].conn[kUp]].claimed = true;
        if (n >= 0 && rooms[n].conn[kUp] >= 0) rooms[rooms[n].conn[kUp]].claimed = true;
        if (e >= 0 && rooms[e].conn[kNorth] >= 0 && rooms[rooms[e].conn[kNorth]].conn[kUp] >= 0)
            rooms[rooms[rooms[e].conn[kNorth]].conn[kUp]].claimed = true;

        order.clear();
        for (int i = 0; i < 75; i++) {
            if (!present_[i]) continue;
            updateOpenings(i);
            order.push_back(i);
        }
        updateOpenings(75);

        // Util.shuffle -> Collections.shuffle
        for (int i = int(order.size()) - 1; i > 0; i--) std::swap(order[size_t(i)], order[size_t(rng.nextInt(i + 1))]);

        // pruning loop: close up to two "leaf" openings per room
        int scan = 1;
        for (int cell : order) {
            int closeCount = 0, attempts = 0;
            while (closeCount < 2 && attempts < 5) {
                attempts++;
                const int f = rng.nextInt(6);
                if (!rooms[cell].hasOpening[f]) continue;
                const int b = rooms[cell].conn[f];
                if (b < 0) continue;
                const int o = kOpposite[f];
                rooms[cell].hasOpening[f] = false;
                rooms[b].hasOpening[o] = false;
                if (findSource(cell, scan++) && findSource(b, scan++)) {
                    closeCount++;
                } else {
                    rooms[cell].hasOpening[f] = true;
                    rooms[b].hasOpening[o] = true;
                }
            }
        }

        order.push_back(75);
        order.push_back(76);
        order.push_back(77);
    }

    /** Mirror of the seven fitters, in their vanilla order. Returns 0..6. */
    int pickFitter(int a) const
    {
        const RoomDef &d = rooms[size_t(a)];
        auto open = [&](int dir) { return d.hasOpening[dir]; };
        auto unclaimed = [&](int dir) { return d.conn[dir] >= 0 && !rooms[size_t(d.conn[dir])].claimed; };

        // 0 FitDoubleXY
        if (open(kEast) && unclaimed(kEast) && open(kUp) && unclaimed(kUp)) {
            const RoomDef &e = rooms[size_t(d.conn[kEast])];
            if (e.hasOpening[kUp] && e.conn[kUp] >= 0 && !rooms[size_t(e.conn[kUp])].claimed) return 0;
        }
        // 1 FitDoubleYZ
        if (open(kNorth) && unclaimed(kNorth) && open(kUp) && unclaimed(kUp)) {
            const RoomDef &n = rooms[size_t(d.conn[kNorth])];
            if (n.hasOpening[kUp] && n.conn[kUp] >= 0 && !rooms[size_t(n.conn[kUp])].claimed) return 1;
        }
        // 2 FitDoubleZ
        if (open(kNorth) && unclaimed(kNorth)) return 2;
        // 3 FitDoubleX
        if (open(kEast) && unclaimed(kEast)) return 3;
        // 4 FitDoubleY
        if (open(kUp) && unclaimed(kUp)) return 4;
        // 5 FitSimpleTopRoom
        if (!open(kWest) && !open(kEast) && !open(kNorth) && !open(kSouth) && !open(kUp)) return 5;
        // 6 FitSimpleRoom
        return 6;
    }

    void claim(int a, int fitter)
    {
        RoomDef &d = rooms[size_t(a)];
        d.claimed = true;
        auto claimAt = [&](int dir) {
            if (d.conn[dir] >= 0) rooms[size_t(d.conn[dir])].claimed = true;
        };
        switch (fitter) {
        case 0:
            claimAt(kEast); claimAt(kUp);
            if (d.conn[kEast] >= 0 && rooms[size_t(d.conn[kEast])].conn[kUp] >= 0)
                rooms[size_t(rooms[size_t(d.conn[kEast])].conn[kUp])].claimed = true;
            break;
        case 1:
            claimAt(kNorth); claimAt(kUp);
            if (d.conn[kNorth] >= 0 && rooms[size_t(d.conn[kNorth])].conn[kUp] >= 0)
                rooms[size_t(rooms[size_t(d.conn[kNorth])].conn[kUp])].claimed = true;
            break;
        case 2:
            claimAt(kNorth);
            break;
        case 3:
            claimAt(kEast);
            break;
        case 4:
            claimAt(kUp);
            break;
        default:
            break;
        }
    }
};

} // namespace

std::vector<MonumentSponge> monumentSponge(int mc, uint64_t seed, uint32_t flags,
                                           const std::vector<std::pair<int, int>> &monuments)
{
    (void)flags;
    std::vector<MonumentSponge> out;
    out.reserve(monuments.size());
    for (const auto &monument : monuments) {
        MonumentSponge item{};
        item.x = monument.first;
        item.z = monument.second;
        item.exists = true;

        const int chunkX = monument.first >> 4;
        const int chunkZ = monument.second >> 4;
        JavaRandom rng(loots_structure_rng_seed(mc, seed, chunkX, chunkZ));

        // createTopPiece: Direction.Plane.HORIZONTAL.getRandomDirection(random)
        const int orientation = rng.nextInt(4);

        Graph g;
        g.buildGraph(rng);

        // building origin: (chunkMinX - 29, 39, chunkMinZ - 29)
        const int west = chunkX * 16 - 29;
        const int north = chunkZ * 16 - 29;
        constexpr int kFootprint = 58;

        for (int cell : g.order) {
            if (g.rooms[size_t(cell)].index >= 75) continue;
            if (g.rooms[size_t(cell)].claimed) continue;
            const int fitter = g.pickFitter(cell);
            g.claim(cell, fitter);
            if (fitter != 5) continue;   // only FitSimpleTopRoom carries sponges

            item.spongeRooms++;
            const int y = cell / 25;
            const int z = (cell % 25) / 5;
            const int x = cell % 5;

            // Grid cell -> building-local block position (rooms are 8x8x4).
            const int lx = 9 + x * 8 + 4;
            const int ly = y * 4 + 1;
            const int lz = 22 + z * 8 + 4;
            const Direction2D dir = horizontalByIndex(orientation);
            const Box2i buildingBox = makePieceBox(west, north, dir, kFootprint, kFootprint);
            Box3i box{};
            box.minX = pieceWorldX(dir, buildingBox, lx, lz);
            box.minY = 39 + ly;
            box.minZ = pieceWorldZ(dir, buildingBox, lx, lz);
            box.maxX = box.minX;
            box.maxY = box.minY;
            box.maxZ = box.minZ;
            item.sponges.push_back(box);
        }
        item.rooms = int(g.order.size());
        item.bounds.minX = west;
        item.bounds.minZ = north;
        item.bounds.maxX = west + kFootprint - 1;
        item.bounds.maxZ = north + kFootprint - 1;
        out.push_back(std::move(item));
    }
    return out;
}

} // namespace loots
