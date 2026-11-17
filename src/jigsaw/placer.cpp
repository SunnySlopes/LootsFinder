#include "placer.hpp"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>

#include "free_space.hpp"
#include "loots/rng.h"
#include "terrain.hpp"
#include "pieces_c.h"

namespace loots {
namespace jigsaw {
namespace {

constexpr Vec3 kZero{0, 0, 0};

bool sameName(const std::string &a, const std::string &b)
{
    if (b.empty()) return true; // JigsawBlock.canAttach: target.name() == null
    return a == b;
}

bool canAttach(const JigsawBlockData &source, const JigsawBlockData &target)
{
    return source.facing == opposite(target.facing) &&
           (source.rollable || source.top == target.top) && sameName(source.target, target.name);
}

bool debugEnabled()
{
    static const bool enabled = std::getenv("LOOTS_JIGSAW_DEBUG") != nullptr;
    return enabled;
}

std::string elementLabel(const Data &data, int element)
{
    std::vector<int> templates;
    data.concreteTemplates(element, templates);
    std::string label;
    for (size_t i = 0; i < templates.size(); i++) {
        if (i) label += "|";
        label += data.templateAt(templates[i]).name;
    }
    return label;
}

struct State {
    size_t piece = 0;
    int freeSpace = 0;
    int depth = 0;
};

class Placer {
public:
    Placer(const JigsawConfig &config, const Data &data, int mc, uint64_t seed, int startX,
           int startZ)
        : config_(config), data_(data), mc_(mc), seed_(seed),
          rng_(loots_structure_rng_seed(mc, seed, startX >> 4, startZ >> 4)), startX_(startX),
          startZ_(startZ)
    {
    }

    Placement run()
    {
        Placement result;

        const int centerPool = data_.findPool(config_.startPool ? config_.startPool : "");
        if (centerPool < 0 || data_.poolAt(centerPool).templates.empty()) return result;

        const int centerRotation = rng_.integer(4); // Rotation.getRandom
        const int centerElement = data_.poolRandomTemplate(centerPool, rng_);
        if (centerElement < 0 || data_.elementAt(centerElement).kind == ElementKind::empty)
            return result;

        Vec3 position{startX_, config_.startHeight, startZ_};
        Vec3 anchored = position;
        if (config_.startJigsaw && config_.startJigsaw[0]) {
            const auto jigsaws =
                data_.shuffledJigsawBlocks(centerElement, position, centerRotation, rng_);
            bool found = false;
            for (const auto &jigsaw : jigsaws) {
                if (jigsaw.name == config_.startJigsaw) {
                    anchored = jigsaw.pos;
                    found = true;
                    break;
                }
            }
            if (!found) return result;
        }

        const Vec3 localAnchor{anchored.x - position.x, anchored.y - position.y,
                               anchored.z - position.z};
        Vec3 adjustedPosition{position.x - localAnchor.x, position.y - localAnchor.y,
                              position.z - localAnchor.z};

        PlacedPiece center;
        center.element = centerElement;
        center.origin = adjustedPosition;
        center.rotation = centerRotation;
        center.groundLevelDelta = data_.groundLevelDelta(centerElement);
        center.box = data_.elementBox(centerElement, adjustedPosition, centerRotation);
        center.rigid = data_.elementAt(centerElement).projection == Projection::rigid;
        center.depth = 0;
        if (center.box.hi.y < center.box.lo.y) return result;

        const int centerX = (center.box.hi.x + center.box.lo.x) / 2;
        const int centerZ = (center.box.hi.z + center.box.lo.z) / 2;
        int bottomY = adjustedPosition.y;
        if (config_.projectStartToHeightmap) {
            const int surface = worldSurfaceHeight(mc_, seed_, centerX, centerZ);
            bottomY = position.y + surface;
            if (debugEnabled()) {
                std::fprintf(stderr, "START center=%d,%d surface=%d bottomY=%d\n", centerX,
                             centerZ, surface, bottomY);
            }
        }

        const int oldAbsoluteGroundY = center.box.lo.y + center.groundLevelDelta;
        const int dy = bottomY - oldAbsoluteGroundY;
        center.origin.y += dy;
        center.box = center.box.moved({0, dy, 0});

        const int centerY = bottomY + localAnchor.y;
        const int maxH = config_.maxDistanceHorizontal;
        const int maxV = config_.maxDistanceVertical;
        const Box worldBounds{
            {centerX - maxH, std::max(centerY - maxV, kWorldMinY + config_.dimensionPaddingBottom),
             centerZ - maxH},
            {centerX + maxH, std::min(centerY + maxV, kWorldMaxY - config_.dimensionPaddingTop),
             centerZ + maxH}};

        result.exists = true;
        result.centerX = centerX;
        result.centerY = centerY;
        result.centerZ = centerZ;

        spaces_.push_back(FreeSpace{worldBounds, {center.box}});
        pieces_.push_back(std::move(center));

        if (config_.maxDepth > 0) {
            tryPlacingChildren(0, 0, 0);
            while (nextState(state_)) tryPlacingChildren(state_.piece, state_.freeSpace, state_.depth);
        }

        result.pieces = std::move(pieces_);
        return result;
    }

private:
    void enqueue(const State &state, int priority)
    {
        placing_[priority].push_back(state);
    }

    bool nextState(State &out)
    {
        if (placing_.empty()) return false;
        auto it = placing_.begin();
        out = it->second.front();
        it->second.pop_front();
        if (it->second.empty()) placing_.erase(it);
        return true;
    }

    void tryPlacingChildren(size_t pieceIndex, int contextFree, int depth)
    {
        const PlacedPiece source = pieces_[pieceIndex];

        const ElementData &sourceElement = data_.elementAt(source.element);
        const bool sourceRigid = sourceElement.projection == Projection::rigid;
        const Box sourceBB = source.box;
        const int sourceBoxY = sourceBB.lo.y;
        const int sourceGroundLevelDelta = source.groundLevelDelta;

        int sourceFree = -1;
        const auto sourceJigsaws =
            data_.shuffledJigsawBlocks(source.element, source.origin, source.rotation, rng_);
        if (debugEnabled()) {
            std::fprintf(stderr, "SOURCE piece=%zu element=%d %s origin=%d,%d,%d depth=%d\n",
                         pieceIndex, source.element, elementLabel(data_, source.element).c_str(),
                         source.origin.x, source.origin.y, source.origin.z, depth);
        }

        for (const JigsawBlockData &sourceJigsaw : sourceJigsaws) {
            const Vec3 sourceJigsawPos = sourceJigsaw.pos;
            const Vec3 targetJigsawPos = sourceJigsawPos + stepOf(sourceJigsaw.facing);
            const int sourceJigsawLocalY = sourceJigsawPos.y - sourceBoxY;
            int sourceJigsawBaseHeight = INT_MIN;

            const int targetPool = data_.findPool(sourceJigsaw.pool);
            if (targetPool < 0) continue;
            const PoolData &pool = data_.poolAt(targetPool);
            if (pool.templates.empty() && pool.name != "empty") continue;
            if (debugEnabled()) {
                std::fprintf(stderr,
                             "  JIGSAW piece=%zu pos=%d,%d,%d face=%c top=%c target=%s pool=%s "
                             "inside=%d\n",
                             pieceIndex, sourceJigsawPos.x, sourceJigsawPos.y,
                             sourceJigsawPos.z, facingLetter(sourceJigsaw.facing),
                             topLetter(sourceJigsaw.top), sourceJigsaw.target.c_str(),
                             sourceJigsaw.pool.c_str(), int(sourceBB.contains(targetJigsawPos)));
            }

            int childrenFree = contextFree;
            if (sourceBB.contains(targetJigsawPos)) {
                if (sourceFree < 0) {
                    spaces_.push_back(FreeSpace{sourceBB, {}});
                    sourceFree = int(spaces_.size()) - 1;
                }
                childrenFree = sourceFree;
            }

            std::vector<int> targetPieces;
            if (depth != config_.maxDepth)
                targetPieces = data_.poolShuffledTemplates(targetPool, rng_);
            if (pool.fallback >= 0) {
                const auto fallback = data_.poolShuffledTemplates(pool.fallback, rng_);
                targetPieces.insert(targetPieces.end(), fallback.begin(), fallback.end());
            }

            const int placementPriority = sourceJigsaw.placementPriority;

            for (int targetElement : targetPieces) {
                if (targetElement < 0 ||
                    data_.elementAt(targetElement).kind == ElementKind::empty)
                    break;

                bool attached = false;
                std::vector<int> rotations{0, 1, 2, 3};
                rng_.shuffle(rotations); // Rotation.getShuffled

                for (int rotation : rotations) {
                    const auto targetJigsaws =
                        data_.shuffledJigsawBlocks(targetElement, kZero, rotation, rng_);
                    const Box hackBox = data_.elementBox(targetElement, kZero, rotation);
                    if (hackBox.hi.y < hackBox.lo.y) continue;

                    int expandTo = 0;
                    if (config_.useExpansionHack && hackBox.height() <= 16) {
                        for (const JigsawBlockData &targetJigsaw : targetJigsaws) {
                            const Vec3 next = targetJigsaw.pos + stepOf(targetJigsaw.facing);
                            if (!hackBox.contains(next)) continue;
                            const int childPool = data_.findPool(targetJigsaw.pool);
                            if (childPool < 0) continue;
                            const int childFallback = data_.poolAt(childPool).fallback;
                            expandTo = std::max(
                                expandTo, std::max(data_.poolMaxSize(childPool),
                                                   data_.poolMaxSize(childFallback)));
                        }
                    }

                    const Projection targetProjection =
                        data_.elementAt(targetElement).projection;
                    const bool targetRigid = targetProjection == Projection::rigid;

                    for (const JigsawBlockData &targetJigsaw : targetJigsaws) {
                        if (!canAttach(sourceJigsaw, targetJigsaw)) continue;

                        const Vec3 targetJigsawLocalPos = targetJigsaw.pos;
                        const Vec3 rawTargetBoxPos = targetJigsawPos - targetJigsawLocalPos;
                        const Box rawTargetBB =
                            data_.elementBox(targetElement, rawTargetBoxPos, rotation);
                        const int targetJigsawLocalY = targetJigsawLocalPos.y;
                        const int deltaY = sourceJigsawLocalY - targetJigsawLocalY +
                                           stepY(sourceJigsaw.facing);

                        int targetBoxY;
                        if (sourceRigid && targetRigid) {
                            targetBoxY = sourceBoxY + deltaY;
                        } else {
                            if (sourceJigsawBaseHeight == INT_MIN) {
                                sourceJigsawBaseHeight =
                                    worldSurfaceHeight(mc_, seed_, sourceJigsawPos.x,
                                                       sourceJigsawPos.z);
                            }
                            targetBoxY = sourceJigsawBaseHeight - targetJigsawLocalY;
                        }

                        const int yOffset = targetBoxY - rawTargetBB.lo.y;
                        Box targetBB = rawTargetBB.moved({0, yOffset, 0});
                        Vec3 targetBoxPosition{rawTargetBoxPos.x, rawTargetBoxPos.y + yOffset,
                                               rawTargetBoxPos.z};

                        if (expandTo > 0) {
                            const int newSize =
                                std::max(expandTo + 1, targetBB.hi.y - targetBB.lo.y);
                            targetBB.encapsulate(Vec3{targetBB.lo.x,
                                                      targetBB.lo.y + newSize,
                                                      targetBB.lo.z});
                        }

                        if (!spaces_[size_t(childrenFree)].fits(targetBB)) {
                            if (debugEnabled()) {
                                std::fprintf(stderr,
                                             "    REJECT %s rot=%d box=%d,%d,%d..%d,%d,%d free=%d\n",
                                             elementLabel(data_, targetElement).c_str(), rotation,
                                             targetBB.lo.x, targetBB.lo.y, targetBB.lo.z,
                                             targetBB.hi.x, targetBB.hi.y, targetBB.hi.z,
                                             childrenFree);
                            }
                            continue;
                        }
                        spaces_[size_t(childrenFree)].add(targetBB);

                        int targetGroundLevelDelta;
                        if (targetRigid)
                            targetGroundLevelDelta = sourceGroundLevelDelta - deltaY;
                        else
                            targetGroundLevelDelta = data_.groundLevelDelta(targetElement);

                        int junctionY;
                        if (sourceRigid) {
                            junctionY = sourceBoxY + sourceJigsawLocalY;
                        } else if (targetRigid) {
                            junctionY = targetBoxY + targetJigsawLocalY;
                        } else {
                            if (sourceJigsawBaseHeight == INT_MIN) {
                                sourceJigsawBaseHeight =
                                    worldSurfaceHeight(mc_, seed_, sourceJigsawPos.x,
                                                       sourceJigsawPos.z);
                            }
                            junctionY = sourceJigsawBaseHeight + deltaY / 2;
                        }

                        const Vec3 sourceJunctionPos{
                            targetJigsawPos.x,
                            junctionY - sourceJigsawLocalY + sourceGroundLevelDelta,
                            targetJigsawPos.z};
                        const Vec3 targetJunctionPos{
                            sourceJigsawPos.x,
                            junctionY - targetJigsawLocalY + targetGroundLevelDelta,
                            sourceJigsawPos.z};

                        pieces_[pieceIndex].junctions.push_back(
                            {sourceJunctionPos, deltaY, targetProjection});

                        PlacedPiece target;
                        target.element = targetElement;
                        target.origin = targetBoxPosition;
                        target.rotation = rotation;
                        target.groundLevelDelta = targetGroundLevelDelta;
                        target.box = targetBB;
                        target.rigid = targetRigid;
                        target.depth = depth + 1;
                        target.junctions.push_back(
                            {targetJunctionPos, -deltaY, sourceElement.projection});
                        pieces_.push_back(std::move(target));
                        if (debugEnabled()) {
                            const auto &added = pieces_.back();
                            std::fprintf(stderr,
                                         "    ATTACH %s rot=%d origin=%d,%d,%d box=%d,%d,%d..%d,%d,%d\n",
                                         elementLabel(data_, targetElement).c_str(), rotation,
                                         added.origin.x, added.origin.y, added.origin.z,
                                         added.box.lo.x, added.box.lo.y, added.box.lo.z,
                                         added.box.hi.x, added.box.hi.y, added.box.hi.z);
                        }

                        if (depth + 1 <= config_.maxDepth) {
                            enqueue({pieces_.size() - 1, childrenFree, depth + 1},
                                    placementPriority);
                        }
                        attached = true;
                        break;
                    }
                    if (attached) break;
                }
                if (attached) break;
            }
        }
    }

    JigsawConfig config_;
    const Data &data_;
    int mc_ = 0;
    uint64_t seed_ = 0;
    Random rng_;
    int startX_ = 0;
    int startZ_ = 0;
    std::vector<PlacedPiece> pieces_;
    std::vector<FreeSpace> spaces_;
    std::map<int, std::deque<State>, std::greater<int>> placing_;
    State state_;
};

} // namespace

Placement placeJigsaw(const JigsawConfig &config, int mc, uint64_t seed, int x, int z)
{
    const int startX = (x >> 4) << 4;
    const int startZ = (z >> 4) << 4;
    Placer placer(config, data263(), mc, seed, startX, startZ);
    return placer.run();
}

char facingLetter(Facing facing)
{
    static const char letters[] = {'N', 'E', 'S', 'W', 'U', 'D'};
    const int value = int(facing);
    return value >= 0 && value < 6 ? letters[value] : '?';
}

char topLetter(Top top)
{
    static const char letters[] = {'U', 'D', 'N', 'E', 'S', 'W'};
    const int value = int(top);
    return value >= 0 && value < 6 ? letters[value] : '?';
}

const char *rotationName(int rotation)
{
    switch (rotation & 3) {
    case 0: return "none";
    case 1: return "clockwise_90";
    case 2: return "clockwise_180";
    default: return "counterclockwise_90";
    }
}

const char *projectionName(Projection projection)
{
    return projection == Projection::rigid ? "rigid" : "terrain_matching";
}

} // namespace jigsaw
} // namespace loots
