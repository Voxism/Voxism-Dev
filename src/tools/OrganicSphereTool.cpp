#include "OrganicSphereTool.h"

#include "../world/Materials.h"
#include "../world/modifiers/StrokeSphereChunkModifier.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
glm::ivec3 worldMinVoxel(const glm::vec3 &center, float radiusMeters, float voxelSizeMeters)
{
    const glm::vec3 minWorld = center - glm::vec3(radiusMeters);
    return glm::ivec3(
        static_cast<int>(std::floor(minWorld.x / voxelSizeMeters)),
        static_cast<int>(std::floor(minWorld.y / voxelSizeMeters)),
        static_cast<int>(std::floor(minWorld.z / voxelSizeMeters)));
}

glm::ivec3 worldMaxVoxel(const glm::vec3 &center, float radiusMeters, float voxelSizeMeters)
{
    const glm::vec3 maxWorld = center + glm::vec3(radiusMeters);
    return glm::ivec3(
        static_cast<int>(std::ceil(maxWorld.x / voxelSizeMeters)) - 1,
        static_cast<int>(std::ceil(maxWorld.y / voxelSizeMeters)) - 1,
        static_cast<int>(std::ceil(maxWorld.z / voxelSizeMeters)) - 1);
}
}

glm::vec3 OrganicSphereTool::currentStrokeCenter(const glm::vec3 &origin, const glm::vec3 &direction) const
{
    return origin + glm::normalize(direction) * strokeDistance_;
}

ToolPreview OrganicSphereTool::makePreview(const glm::vec3 &center, float voxelSizeMeters, ToolMode mode) const
{
    ToolPreview preview;
    preview.valid = true;
    preview.mode = mode;
    const float radiusMeters = sphereRadiusMeters();
    preview.minVoxel = worldMinVoxel(center, radiusMeters, voxelSizeMeters);
    preview.maxVoxel = worldMaxVoxel(center, radiusMeters, voxelSizeMeters);
    return preview;
}

ToolUseResult OrganicSphereTool::applyStrokeSegment(const glm::vec3 &center,
    int chunkSizeVoxels,
    float voxelSizeMeters,
    ToolMode mode,
    bool updateLastCenter)
{
    ToolUseResult result;

    std::vector<glm::vec3> centers;
    if (!strokeActive_) {
        centers.push_back(center);
    } else {
        const float radiusMeters = sphereRadiusMeters();
        const float spacing = std::max(voxelSizeMeters, radiusMeters * 0.35f);
        const glm::vec3 delta = center - lastCenter_;
        const float distance = glm::length(delta);
        if (distance <= 0.0001f) {
            return result;
        } else {
            const int steps = std::max(1, static_cast<int>(std::ceil(distance / spacing)));
            centers.reserve(static_cast<size_t>(steps));
            for (int i = 1; i <= steps; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                centers.push_back(lastCenter_ + delta * t);
            }
        }
    }

    result.consumed = true;
    const float radiusMeters = sphereRadiusMeters();
    result.modifier = std::make_shared<StrokeSphereChunkModifier>(
        centers,
        radiusMeters,
        chunkSizeVoxels,
        voxelSizeMeters,
        mode == ToolMode::Build,
        static_cast<uint8_t>(paletteIndex_));

    if (updateLastCenter) {
        lastCenter_ = center;
        strokeActive_ = true;
    }
    return result;
}

ToolUseResult OrganicSphereTool::beginStroke(const glm::vec3 &origin,
    const glm::vec3 &direction,
    const ToolRaycastHit &hit,
    int chunkSizeVoxels,
    float voxelSizeMeters,
    ToolMode mode)
{
    strokeActive_ = false;
    strokeDistance_ = hit.distance;
    return applyStrokeSegment(currentStrokeCenter(origin, direction), chunkSizeVoxels, voxelSizeMeters, mode, true);
}

ToolUseResult OrganicSphereTool::continueStroke(const glm::vec3 &origin,
    const glm::vec3 &direction,
    int chunkSizeVoxels,
    float voxelSizeMeters,
    ToolMode mode)
{
    return applyStrokeSegment(currentStrokeCenter(origin, direction), chunkSizeVoxels, voxelSizeMeters, mode, true);
}

void OrganicSphereTool::endStroke()
{
    strokeActive_ = false;
    strokeDistance_ = 0.0f;
}

ToolPreview OrganicSphereTool::preview(const glm::vec3 &origin,
    const glm::vec3 &direction,
    const ToolRaycastHit *hit,
    float voxelSizeMeters,
    ToolMode mode) const
{
    if (strokeActive_) {
        return makePreview(currentStrokeCenter(origin, direction), voxelSizeMeters, mode);
    }
    if (!hit) {
        return ToolPreview {};
    }
    return makePreview(hit->position, voxelSizeMeters, mode);
}

void OrganicSphereTool::cycleSize(int direction)
{
    const float kStep = 0.25f;
    sizeMeters_ += (direction > 0) ? kStep : -kStep;
    sizeMeters_ = glm::clamp(sizeMeters_, 0.25f, 8.0f);
}

void OrganicSphereTool::cycleMaterial(int direction)
{
    if (direction == 0) {
        return;
    }

    paletteIndex_ += (direction > 0) ? 1 : -1;
    if (paletteIndex_ < 0) {
        paletteIndex_ = Materials::paletteCount - 1;
    } else if (paletteIndex_ >= Materials::paletteCount) {
        paletteIndex_ = 0;
    }
}
