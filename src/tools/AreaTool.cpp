#include "AreaTool.h"

#include "ToolGrid.h"

#include "../world/modifiers/BoxChunkModifier.h"
#include "../world/Materials.h"
#include "../world/VoxelMath.h"

namespace {
glm::ivec3 snappedTargetVoxel(const ToolRaycastHit &hit, ToolMode mode, int gridSize)
{
    const glm::ivec3 targetVoxel = (mode == ToolMode::Build) ? hit.adjacent : hit.voxel;
    return snapVoxelToGrid(targetVoxel, gridSize);
}
}

ToolUseResult AreaTool::use(const ToolRaycastHit &hit, int chunkSizeVoxels, ToolMode mode)
{
    ToolUseResult result;
    result.consumed = true;

    const glm::ivec3 targetCell = snappedTargetVoxel(hit, mode, gridSize_);
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
        mode == ToolMode::Build,
        static_cast<uint8_t>(paletteIndex_));
    hasFirstPoint_ = false;
    return result;
}

ToolPreview AreaTool::computePreview(const ToolRaycastHit &hit, ToolMode mode) const
{
    ToolPreview preview;
    preview.valid = true;
    preview.mode = mode;

    const glm::ivec3 targetCell = snappedTargetVoxel(hit, mode, gridSize_);
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

ToolPreview AreaTool::preview(const ToolRaycastHit &hit, ToolMode mode) const
{
    return computePreview(hit, mode);
}

void AreaTool::cycleSize(int direction)
{
    const int previousGridSize = gridSize_;
    cycleDiscreteSize(gridSize_, direction);
    if (previousGridSize != gridSize_) {
        hasFirstPoint_ = false;
    }
}

void AreaTool::cycleMaterial(int direction)
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
