#pragma once

#include "ToolTypes.h"

#include <glm/glm.hpp>

class OrganicSphereTool {
public:
    ToolUseResult beginStroke(const glm::vec3 &origin,
        const glm::vec3 &direction,
        const ToolRaycastHit &hit,
        int chunkSizeVoxels,
        float voxelSizeMeters,
        ToolMode mode);
    ToolUseResult continueStroke(const glm::vec3 &origin,
        const glm::vec3 &direction,
        int chunkSizeVoxels,
        float voxelSizeMeters,
        ToolMode mode);
    void endStroke();
    ToolPreview preview(const glm::vec3 &origin,
        const glm::vec3 &direction,
        const ToolRaycastHit *hit,
        float voxelSizeMeters,
        ToolMode mode) const;
    void cycleSize(int direction);
    void cycleMaterial(int direction);
    void setMaterialIndex(int index) { paletteIndex_ = index; }
    float radiusMeters() const { return sizeMeters_; }
    int materialIndex() const { return paletteIndex_; }

private:
    ToolUseResult applyStrokeSegment(const glm::vec3 &center,
        int chunkSizeVoxels,
        float voxelSizeMeters,
        ToolMode mode,
        bool updateLastCenter);
    glm::vec3 currentStrokeCenter(const glm::vec3 &origin, const glm::vec3 &direction) const;
    ToolPreview makePreview(const glm::vec3 &center, float voxelSizeMeters, ToolMode mode) const;
    float sphereRadiusMeters() const { return sizeMeters_ * 0.5f; }

    float sizeMeters_ = 1.0f;
    int paletteIndex_ = 1;
    bool strokeActive_ = false;
    float strokeDistance_ = 0.0f;
    glm::vec3 lastCenter_ = glm::vec3(0.0f);
};
