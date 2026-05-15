#pragma once

#include "ToolTypes.h"

class AreaTool {
public:
    ToolUseResult use(const ToolRaycastHit &hit, int chunkSizeVoxels, ToolMode mode);
    ToolPreview preview(const ToolRaycastHit &hit, ToolMode mode) const;
    void cycleSize(int direction);
    void cycleMaterial(int direction);
    void clearPending() { hasFirstPoint_ = false; }
    int size() const { return gridSize_; }
    int materialIndex() const { return paletteIndex_; }

private:
    ToolPreview computePreview(const ToolRaycastHit &hit, ToolMode mode) const;

    int gridSize_ = 2;
    int paletteIndex_ = 1;
    bool hasFirstPoint_ = false;
    glm::ivec3 firstPointVoxel_ = glm::ivec3(0);
};
