#pragma once

#include "ToolTypes.h"

class SphereTool {
public:
    ToolUseResult use(const ToolRaycastHit &hit, int chunkSizeVoxels) const;
    ToolPreview preview(const ToolRaycastHit &hit) const;
    void drawImGui();

private:
    ToolPreview computePreview(const ToolRaycastHit &hit) const;

    int size_ = 2;
    int paletteIndex_ = 1;
    ToolMode mode_ = ToolMode::Build;
};
