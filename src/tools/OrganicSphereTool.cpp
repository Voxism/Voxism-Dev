#include "OrganicSphereTool.h"

#include "../world/Materials.h"
#include "../world/modifiers/StrokeSphereChunkModifier.h"

#include <imgui.h>

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

ToolPreview OrganicSphereTool::makePreview(const glm::vec3 &center, float voxelSizeMeters) const
{
    ToolPreview preview;
    preview.valid = true;
    preview.mode = mode_;
    preview.minVoxel = worldMinVoxel(center, radiusMeters_, voxelSizeMeters);
    preview.maxVoxel = worldMaxVoxel(center, radiusMeters_, voxelSizeMeters);
    return preview;
}

ToolUseResult OrganicSphereTool::applyStrokeSegment(const glm::vec3 &center,
    int chunkSizeVoxels,
    float voxelSizeMeters,
    bool updateLastCenter)
{
    ToolUseResult result;

    std::vector<glm::vec3> centers;
    if (!strokeActive_) {
        centers.push_back(center);
    } else {
        const float spacing = std::max(voxelSizeMeters, radiusMeters_ * 0.35f);
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
    result.modifier = std::make_shared<StrokeSphereChunkModifier>(
        centers,
        radiusMeters_,
        chunkSizeVoxels,
        voxelSizeMeters,
        mode_ == ToolMode::Build,
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
    float voxelSizeMeters)
{
    strokeActive_ = false;
    strokeDistance_ = hit.distance;
    return applyStrokeSegment(currentStrokeCenter(origin, direction), chunkSizeVoxels, voxelSizeMeters, true);
}

ToolUseResult OrganicSphereTool::continueStroke(const glm::vec3 &origin,
    const glm::vec3 &direction,
    int chunkSizeVoxels,
    float voxelSizeMeters)
{
    return applyStrokeSegment(currentStrokeCenter(origin, direction), chunkSizeVoxels, voxelSizeMeters, true);
}

void OrganicSphereTool::endStroke()
{
    strokeActive_ = false;
    strokeDistance_ = 0.0f;
}

ToolPreview OrganicSphereTool::preview(const glm::vec3 &origin,
    const glm::vec3 &direction,
    const ToolRaycastHit *hit,
    float voxelSizeMeters) const
{
    if (strokeActive_) {
        return makePreview(currentStrokeCenter(origin, direction), voxelSizeMeters);
    }
    if (!hit) {
        return ToolPreview {};
    }
    return makePreview(hit->position, voxelSizeMeters);
}

void OrganicSphereTool::drawImGui()
{
    int modeIndex = static_cast<int>(mode_);
    ImGui::RadioButton("Organic Build", &modeIndex, static_cast<int>(ToolMode::Build));
    ImGui::SameLine();
    ImGui::RadioButton("Organic Delete", &modeIndex, static_cast<int>(ToolMode::Delete));
    mode_ = static_cast<ToolMode>(modeIndex);

    ImGui::SliderFloat("Organic Radius", &radiusMeters_, 0.25f, 8.0f, "%.2f m");

    if (ImGui::BeginCombo("Organic Palette", Materials::paletteName(paletteIndex_))) {
        for (int i = 0; i < Materials::paletteCount; ++i) {
            const bool selected = (i == paletteIndex_);
            if (ImGui::Selectable(Materials::paletteName(i), selected)) {
                paletteIndex_ = i;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}
