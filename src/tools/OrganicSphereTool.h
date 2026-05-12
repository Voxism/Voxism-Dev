#pragma once

#include "ToolTypes.h"

#include <glm/glm.hpp>

class OrganicSphereTool {
public:
    ToolUseResult beginStroke(const glm::vec3 &origin,
        const glm::vec3 &direction,
        const ToolRaycastHit &hit,
        int chunkSizeVoxels,
        float voxelSizeMeters);
    ToolUseResult continueStroke(const glm::vec3 &origin,
        const glm::vec3 &direction,
        int chunkSizeVoxels,
        float voxelSizeMeters);
    void endStroke();
    ToolPreview preview(const glm::vec3 &origin,
        const glm::vec3 &direction,
        const ToolRaycastHit *hit,
        float voxelSizeMeters) const;
    void drawImGui();

private:
    ToolUseResult applyStrokeSegment(const glm::vec3 &center,
        int chunkSizeVoxels,
        float voxelSizeMeters,
        bool updateLastCenter);
    glm::vec3 currentStrokeCenter(const glm::vec3 &origin, const glm::vec3 &direction) const;
    ToolPreview makePreview(const glm::vec3 &center, float voxelSizeMeters) const;

    float radiusMeters_ = 1.0f;
    int paletteIndex_ = 1;
    ToolMode mode_ = ToolMode::Build;
    bool strokeActive_ = false;
    float strokeDistance_ = 0.0f;
    glm::vec3 lastCenter_ = glm::vec3(0.0f);
};
