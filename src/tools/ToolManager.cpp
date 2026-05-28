#include "ToolManager.h"

#include "../world/Materials.h"

namespace {
bool raycastToolHit(ChunkManager &chunkManager,
    const glm::vec3 &origin,
    const glm::vec3 &direction,
    float maxDistance,
    ToolRaycastHit &toolHit)
{
    ChunkManager::VoxelRaycastHit hit;
    if (!chunkManager.raycastVoxels(origin, direction, maxDistance, hit)) {
        return false;
    }

    toolHit.voxel = hit.voxel;
    toolHit.adjacent = hit.adjacent;
    toolHit.normal = hit.normal;
    toolHit.position = hit.position;
    toolHit.distance = hit.distance;
    return true;
}
}

bool ToolManager::beginAction(ChunkManager &chunkManager,
    const glm::vec3 &origin,
    const glm::vec3 &direction,
    ToolMode mode,
    ChunkEditSummary *editSummary)
{
    ToolRaycastHit toolHit;
    if (!raycastToolHit(chunkManager, origin, direction, maxUseDistance_, toolHit)) {
        return false;
    }

    ToolUseResult result;
    const int chunkSizeVoxels = chunkManager.chunkSizeInts * 32;
    if (activeTool_ == ToolKind::Cube) {
        result = cubeTool_.use(toolHit, chunkSizeVoxels, mode);
    } else if (activeTool_ == ToolKind::Area) {
        result = areaTool_.use(toolHit, chunkSizeVoxels, mode);
    } else if (activeTool_ == ToolKind::Sphere) {
        result = sphereTool_.use(toolHit, chunkSizeVoxels, mode);
    } else {
        result = organicSphereTool_.beginStroke(origin, direction, toolHit, chunkSizeVoxels, chunkManager.voxSizeMeters, mode);
    }

    if (result.modifier) {
        const ChunkEditSummary summary = chunkManager.modifyChunks(result.modifier);
        if (editSummary) {
            *editSummary = summary;
        }
    }
    return result.consumed;
}

bool ToolManager::updateAction(ChunkManager &chunkManager,
    const glm::vec3 &origin,
    const glm::vec3 &direction,
    ToolMode mode,
    ChunkEditSummary *editSummary)
{
    if (!supportsContinuousAction(mode)) {
        return false;
    }

    const int chunkSizeVoxels = chunkManager.chunkSizeInts * 32;
    ToolUseResult result = organicSphereTool_.continueStroke(origin, direction, chunkSizeVoxels, chunkManager.voxSizeMeters, mode);
    if (result.modifier) {
        const ChunkEditSummary summary = chunkManager.modifyChunks(result.modifier);
        if (editSummary) {
            *editSummary = summary;
        }
    }
    return result.consumed;
}

void ToolManager::endAction(ToolMode mode)
{
    (void)mode;
    if (activeTool_ == ToolKind::OrganicSphere) {
        organicSphereTool_.endStroke();
    }
}

bool ToolManager::supportsContinuousAction(ToolMode mode) const
{
    (void)mode;
    return activeTool_ == ToolKind::OrganicSphere;
}

ToolPreview ToolManager::getPreview(ChunkManager &chunkManager,
    const glm::vec3 &origin,
    const glm::vec3 &direction,
    ToolMode mode) const
{
    if (activeTool_ == ToolKind::OrganicSphere && supportsContinuousAction(mode)) {
        ChunkManager::VoxelRaycastHit hit;
        if (chunkManager.raycastVoxels(origin, direction, maxUseDistance_, hit)) {
            ToolRaycastHit toolHit;
            toolHit.voxel = hit.voxel;
            toolHit.adjacent = hit.adjacent;
            toolHit.normal = hit.normal;
            toolHit.position = hit.position;
            toolHit.distance = hit.distance;
            return organicSphereTool_.preview(origin, direction, &toolHit, chunkManager.voxSizeMeters, mode);
        }
        return organicSphereTool_.preview(origin, direction, nullptr, chunkManager.voxSizeMeters, mode);
    }

    ChunkManager::VoxelRaycastHit hit;
    if (!chunkManager.raycastVoxels(origin, direction, maxUseDistance_, hit)) {
        return ToolPreview {};
    }

    ToolRaycastHit toolHit;
    toolHit.voxel = hit.voxel;
    toolHit.adjacent = hit.adjacent;
    toolHit.normal = hit.normal;
    toolHit.position = hit.position;
    toolHit.distance = hit.distance;

    if (activeTool_ == ToolKind::Cube) {
        return cubeTool_.preview(toolHit, mode);
    }
    if (activeTool_ == ToolKind::Area) {
        return areaTool_.preview(toolHit, mode);
    }
    if (activeTool_ == ToolKind::Sphere) {
        return sphereTool_.preview(toolHit, mode);
    }
    return organicSphereTool_.preview(origin, direction, &toolHit, chunkManager.voxSizeMeters, mode);
}

void ToolManager::cycleTool(int direction)
{
    if (direction == 0) {
        return;
    }

    const int toolCount = static_cast<int>(ToolKind::OrganicSphere) + 1;
    int toolIndex = static_cast<int>(activeTool_) + ((direction > 0) ? 1 : -1);
    if (toolIndex < 0) {
        toolIndex = toolCount - 1;
    } else if (toolIndex >= toolCount) {
        toolIndex = 0;
    }

    const ToolKind nextTool = static_cast<ToolKind>(toolIndex);
    if (nextTool != activeTool_) {
        activeTool_ = nextTool;
        clearInactiveToolState();
    }
}

void ToolManager::cycleSize(int direction)
{
    if (activeTool_ == ToolKind::Cube) {
        cubeTool_.cycleSize(direction);
    } else if (activeTool_ == ToolKind::Area) {
        areaTool_.cycleSize(direction);
    } else if (activeTool_ == ToolKind::Sphere) {
        sphereTool_.cycleSize(direction);
    } else {
        organicSphereTool_.cycleSize(direction);
    }
}

void ToolManager::cycleMaterial(int direction)
{
    if (activeTool_ == ToolKind::Cube) {
        cubeTool_.cycleMaterial(direction);
    } else if (activeTool_ == ToolKind::Area) {
        areaTool_.cycleMaterial(direction);
    } else if (activeTool_ == ToolKind::Sphere) {
        sphereTool_.cycleMaterial(direction);
    } else {
        organicSphereTool_.cycleMaterial(direction);
    }
}

void ToolManager::clearInactiveToolState()
{
    if (activeTool_ != ToolKind::Area) {
        areaTool_.clearPending();
    }
    if (activeTool_ != ToolKind::OrganicSphere) {
        organicSphereTool_.endStroke();
    }
}

const char *ToolManager::activeToolName() const
{
    if (activeTool_ == ToolKind::Cube) {
        return "Cube";
    }
    if (activeTool_ == ToolKind::Area) {
        return "Area";
    }
    if (activeTool_ == ToolKind::Sphere) {
        return "Sphere";
    }
    return "Organic";
}

const char *ToolManager::activeMaterialName() const
{
    if (activeTool_ == ToolKind::Cube) {
        return Materials::paletteName(cubeTool_.materialIndex());
    }
    if (activeTool_ == ToolKind::Area) {
        return Materials::paletteName(areaTool_.materialIndex());
    }
    if (activeTool_ == ToolKind::Sphere) {
        return Materials::paletteName(sphereTool_.materialIndex());
    }
    return Materials::paletteName(organicSphereTool_.materialIndex());
}

bool ToolManager::activeToolUsesMeterRadius() const
{
    return activeTool_ == ToolKind::OrganicSphere;
}

int ToolManager::activeToolSize() const
{
    if (activeTool_ == ToolKind::Cube) {
        return cubeTool_.size();
    }
    if (activeTool_ == ToolKind::Area) {
        return areaTool_.size();
    }
    if (activeTool_ == ToolKind::Sphere) {
        return sphereTool_.size();
    }
    return 0;
}

float ToolManager::activeToolRadiusMeters() const
{
    if (activeTool_ == ToolKind::OrganicSphere) {
        return organicSphereTool_.radiusMeters();
    }
    return 0.0f;
}
