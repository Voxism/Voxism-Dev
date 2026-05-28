#pragma once

#include "ToolTypes.h"

class CubeTool {
public:
    ToolUseResult use(const ToolRaycastHit &hit, int chunkSizeVoxels, ToolMode mode) const;
    ToolPreview preview(const ToolRaycastHit &hit, ToolMode mode) const;
    void cycleSize(int direction);
    void cycleMaterial(int direction);
    void setMaterialIndex(int index) { paletteIndex_ = index; }
    int size() const { return size_; }
    int materialIndex() const { return paletteIndex_; }

private:
    ToolPreview computePreview(const ToolRaycastHit &hit, ToolMode mode) const;

    int size_ = 2;
    int paletteIndex_ = 1;
};
