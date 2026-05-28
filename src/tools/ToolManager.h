#pragma once

#include "AreaTool.h"
#include "CubeTool.h"
#include "OrganicSphereTool.h"
#include "SphereTool.h"

#include "../world/ChunkManager.h"
#include "../world/ChunkEdit.h"

class ToolManager {
public:
    bool beginAction(ChunkManager &chunkManager,
        const glm::vec3 &origin,
        const glm::vec3 &direction,
        ToolMode mode,
        ChunkEditSummary *editSummary = nullptr);
    bool updateAction(ChunkManager &chunkManager,
        const glm::vec3 &origin,
        const glm::vec3 &direction,
        ToolMode mode,
        ChunkEditSummary *editSummary = nullptr);
    void endAction(ToolMode mode);
    bool supportsContinuousAction(ToolMode mode) const;
    ToolPreview getPreview(ChunkManager &chunkManager,
        const glm::vec3 &origin,
        const glm::vec3 &direction,
        ToolMode mode) const;
    ToolKind activeToolKind() const;
    void setActiveTool(ToolKind tool);
    void cycleTool(int direction);
    void cycleSize(int direction);
    void cycleMaterial(int direction);
    const char *activeToolName() const;
    const char *activeMaterialName() const;
    bool activeToolUsesMeterRadius() const;
    int activeToolSize() const;
    float activeToolRadiusMeters() const;

private:
    void clearInactiveToolState();

    CubeTool cubeTool_;
    AreaTool areaTool_;
    SphereTool sphereTool_;
    OrganicSphereTool organicSphereTool_;
    ToolKind activeTool_ = ToolKind::Cube;
    float maxUseDistance_ = 8.0f;
};
