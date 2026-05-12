#include "CubeTool.h"

#include "ToolGrid.h"

#include "../world/BoxChunkModifier.h"
#include "../world/Materials.h"

#include <imgui.h>

ToolPreview CubeTool::computePreview(const ToolRaycastHit &hit) const
{
    const int size = size_;
    const glm::ivec3 anchor = (mode_ == ToolMode::Build) ? hit.adjacent : hit.voxel;
    const glm::ivec3 extrude = (mode_ == ToolMode::Build) ? hit.normal : -hit.normal;

    ToolPreview preview;
    preview.valid = true;
    preview.mode = mode_;

    glm::ivec3 minVoxel(0);
    glm::ivec3 maxVoxel(0);

    for (int axis = 0; axis < 3; ++axis) {
        const int snapped = snapDownToGrid(anchor[axis], size);
        if (extrude[axis] > 0) {
            minVoxel[axis] = snapped;
            maxVoxel[axis] = snapped + size - 1;
        } else if (extrude[axis] < 0) {
            maxVoxel[axis] = snapped + size - 1;
            minVoxel[axis] = maxVoxel[axis] - (size - 1);
        } else {
            minVoxel[axis] = snapped;
            maxVoxel[axis] = snapped + size - 1;
        }
    }

    preview.minVoxel = minVoxel;
    preview.maxVoxel = maxVoxel;
    return preview;
}

ToolPreview CubeTool::preview(const ToolRaycastHit &hit) const
{
    return computePreview(hit);
}

ToolUseResult CubeTool::use(const ToolRaycastHit &hit, int chunkSizeVoxels) const
{
    ToolUseResult result;
    result.consumed = true;

    const ToolPreview previewBox = computePreview(hit);
    result.modifier = std::make_shared<BoxChunkModifier>(
        previewBox.minVoxel,
        previewBox.maxVoxel,
        chunkSizeVoxels,
        mode_ == ToolMode::Build,
        static_cast<uint8_t>(paletteIndex_));
    return result;
}

void CubeTool::drawImGui()
{
    int modeIndex = static_cast<int>(mode_);
    ImGui::RadioButton("Cube Build", &modeIndex, static_cast<int>(ToolMode::Build));
    ImGui::SameLine();
    ImGui::RadioButton("Cube Delete", &modeIndex, static_cast<int>(ToolMode::Delete));
    mode_ = static_cast<ToolMode>(modeIndex);

    drawDiscreteSizeSelector("Cube Size", size_);

    if (ImGui::BeginCombo("Cube Palette", Materials::paletteName(paletteIndex_))) {
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
