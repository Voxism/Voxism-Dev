#include "SphereTool.h"

#include "ToolGrid.h"

#include "../world/Materials.h"
#include "../world/modifiers/SphereChunkModifier.h"
#include "../world/VoxelMath.h"

namespace {
glm::ivec3 snappedTargetVoxel(const ToolRaycastHit &hit, ToolMode mode, int gridSize)
{
    const glm::ivec3 targetVoxel = (mode == ToolMode::Build) ? hit.adjacent : hit.voxel;
    return snapVoxelToGrid(targetVoxel, gridSize);
}
}

ToolPreview SphereTool::computePreview(const ToolRaycastHit &hit, ToolMode mode) const
{
    const glm::ivec3 cellMin = snappedTargetVoxel(hit, mode, size_);

    ToolPreview preview;
    preview.valid = true;
    preview.mode = mode;
    preview.minVoxel = cellMin;
    preview.maxVoxel = cellMin + glm::ivec3(size_ - 1);
    return preview;
}

ToolUseResult SphereTool::use(const ToolRaycastHit &hit, int chunkSizeVoxels, ToolMode mode) const
{
    ToolUseResult result;
    result.consumed = true;

    const glm::ivec3 cellMin = snappedTargetVoxel(hit, mode, size_);
    result.modifier = std::make_shared<SphereChunkModifier>(
        cellMin,
        size_,
        chunkSizeVoxels,
        mode == ToolMode::Build,
        static_cast<uint8_t>(paletteIndex_));
    return result;
}

ToolPreview SphereTool::preview(const ToolRaycastHit &hit, ToolMode mode) const
{
    return computePreview(hit, mode);
}

void SphereTool::cycleSize(int direction)
{
    cycleDiscreteSize(size_, direction);
}

void SphereTool::cycleMaterial(int direction)
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
