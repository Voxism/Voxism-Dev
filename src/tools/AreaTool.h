#pragma once

#include "ToolTypes.h"

class AreaTool {
public:
    ToolUseResult use(const ToolRaycastHit &hit, int chunkSizeVoxels);
    ToolPreview preview(const ToolRaycastHit &hit) const;
    void drawImGui();
    void clearPending() { hasFirstPoint_ = false; }

private:
    ToolPreview computePreview(const ToolRaycastHit &hit) const;

    int gridSize_ = 2;
    int paletteIndex_ = 1;
    ToolMode mode_ = ToolMode::Build;
    bool hasFirstPoint_ = false;
    glm::ivec3 firstPointVoxel_ = glm::ivec3(0);
};
