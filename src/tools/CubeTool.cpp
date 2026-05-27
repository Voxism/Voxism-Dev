#include "CubeTool.h"

#include "ToolGrid.h"

#include "../world/modifiers/BoxChunkModifier.h"
#include "../world/VoxelMath.h"
#include "../world/Materials.h"

ToolPreview CubeTool::computePreview(const ToolRaycastHit &hit, ToolMode mode) const
{
    const int size = size_;
    const glm::ivec3 anchor = (mode == ToolMode::Build) ? hit.adjacent : hit.voxel;
    const glm::ivec3 extrude = (mode == ToolMode::Build) ? hit.normal : -hit.normal;

    ToolPreview preview;
    preview.valid = true;
    preview.mode = mode;

    glm::ivec3 minVoxel(0);
    glm::ivec3 maxVoxel(0);

    for (int axis = 0; axis < 3; axis++) {
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

ToolPreview CubeTool::preview(const ToolRaycastHit &hit, ToolMode mode) const
{
    return computePreview(hit, mode);
}

ToolUseResult CubeTool::use(const ToolRaycastHit &hit, int chunkSizeVoxels, ToolMode mode) const
{
    ToolUseResult result;
    result.consumed = true;

    const ToolPreview previewBox = computePreview(hit, mode);
    result.modifier = std::make_shared<BoxChunkModifier>(
        previewBox.minVoxel,
        previewBox.maxVoxel,
        chunkSizeVoxels,
        mode == ToolMode::Build,
        static_cast<uint8_t>(paletteIndex_));
    return result;
}

void CubeTool::cycleSize(int direction)
{
    cycleDiscreteSize(size_, direction);
}

void CubeTool::cycleMaterial(int direction)
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
