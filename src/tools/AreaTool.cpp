#include "AreaTool.h"

#include "ToolGrid.h"

#include "../world/BoxChunkModifier.h"
#include "../world/Materials.h"

#include <imgui.h>

namespace {
glm::ivec3 snappedTargetVoxel(const ToolRaycastHit &hit, ToolMode mode, int gridSize)
{
    const glm::ivec3 targetVoxel = (mode == ToolMode::Build) ? hit.adjacent : hit.voxel;
    return snapVoxelToGrid(targetVoxel, gridSize);
}
}

ToolUseResult AreaTool::use(const ToolRaycastHit &hit, int chunkSizeVoxels)
{
    ToolUseResult result;
    result.consumed = true;

    const glm::ivec3 targetCell = snappedTargetVoxel(hit, mode_, gridSize_);
    if (!hasFirstPoint_) {
        hasFirstPoint_ = true;
        firstPointVoxel_ = targetCell;
        return result;
    }

    const glm::ivec3 minVoxel = glm::min(firstPointVoxel_, targetCell);
    const glm::ivec3 maxVoxel = glm::max(firstPointVoxel_, targetCell) + glm::ivec3(gridSize_ - 1);
    result.modifier = std::make_shared<BoxChunkModifier>(
        minVoxel,
        maxVoxel,
        chunkSizeVoxels,
        mode_ == ToolMode::Build,
        static_cast<uint8_t>(paletteIndex_));
    hasFirstPoint_ = false;
    return result;
}

ToolPreview AreaTool::computePreview(const ToolRaycastHit &hit) const
{
    ToolPreview preview;
    preview.valid = true;
    preview.mode = mode_;

    const glm::ivec3 targetCell = snappedTargetVoxel(hit, mode_, gridSize_);
    if (!hasFirstPoint_) {
        preview.anchored = false;
        preview.anchorVoxel = targetCell;
        preview.minVoxel = targetCell;
        preview.maxVoxel = targetCell + glm::ivec3(gridSize_ - 1);
        return preview;
    }

    preview.anchored = true;
    preview.anchorVoxel = firstPointVoxel_;
    preview.minVoxel = glm::min(firstPointVoxel_, targetCell);
    preview.maxVoxel = glm::max(firstPointVoxel_, targetCell) + glm::ivec3(gridSize_ - 1);
    return preview;
}

ToolPreview AreaTool::preview(const ToolRaycastHit &hit) const
{
    return computePreview(hit);
}

void AreaTool::drawImGui()
{
    const ToolMode previousMode = mode_;
    const int previousGridSize = gridSize_;

    int modeIndex = static_cast<int>(mode_);
    ImGui::RadioButton("Area Build", &modeIndex, static_cast<int>(ToolMode::Build));
    ImGui::SameLine();
    ImGui::RadioButton("Area Delete", &modeIndex, static_cast<int>(ToolMode::Delete));
    mode_ = static_cast<ToolMode>(modeIndex);

    drawDiscreteSizeSelector("Area Grid Size", gridSize_);

    if (previousMode != mode_ || previousGridSize != gridSize_) {
        hasFirstPoint_ = false;
    }

    if (ImGui::BeginCombo("Area Palette", Materials::paletteName(paletteIndex_))) {
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

    if (hasFirstPoint_) {
        ImGui::Text("Area first point set");
        if (ImGui::Button("Cancel Area Selection")) {
            hasFirstPoint_ = false;
        }
    }
}
