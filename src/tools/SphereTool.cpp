#include "SphereTool.h"

#include "ToolGrid.h"

#include "../world/Materials.h"
#include "../world/modifiers/SphereChunkModifier.h"
#include "../world/VoxelMath.h"

#include <imgui.h>

namespace {
glm::ivec3 snappedTargetVoxel(const ToolRaycastHit &hit, ToolMode mode, int gridSize)
{
    const glm::ivec3 targetVoxel = (mode == ToolMode::Build) ? hit.adjacent : hit.voxel;
    return snapVoxelToGrid(targetVoxel, gridSize);
}
}

ToolPreview SphereTool::computePreview(const ToolRaycastHit &hit) const
{
    const glm::ivec3 cellMin = snappedTargetVoxel(hit, mode_, size_);

    ToolPreview preview;
    preview.valid = true;
    preview.mode = mode_;
    preview.minVoxel = cellMin;
    preview.maxVoxel = cellMin + glm::ivec3(size_ - 1);
    return preview;
}

ToolUseResult SphereTool::use(const ToolRaycastHit &hit, int chunkSizeVoxels) const
{
    ToolUseResult result;
    result.consumed = true;

    const glm::ivec3 cellMin = snappedTargetVoxel(hit, mode_, size_);
    result.modifier = std::make_shared<SphereChunkModifier>(
        cellMin,
        size_,
        chunkSizeVoxels,
        mode_ == ToolMode::Build,
        static_cast<uint8_t>(paletteIndex_));
    return result;
}

ToolPreview SphereTool::preview(const ToolRaycastHit &hit) const
{
    return computePreview(hit);
}

void SphereTool::drawImGui()
{
    int modeIndex = static_cast<int>(mode_);
    ImGui::RadioButton("Sphere Build", &modeIndex, static_cast<int>(ToolMode::Build));
    ImGui::SameLine();
    ImGui::RadioButton("Sphere Delete", &modeIndex, static_cast<int>(ToolMode::Delete));
    mode_ = static_cast<ToolMode>(modeIndex);

    drawDiscreteSizeSelector("Sphere Size", size_);

    if (ImGui::BeginCombo("Sphere Palette", Materials::paletteName(paletteIndex_))) {
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
