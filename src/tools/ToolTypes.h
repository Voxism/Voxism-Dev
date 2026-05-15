#pragma once

#include <memory>

#include <glm/glm.hpp>

class IChunkModifier;

enum class ToolKind {
    Cube = 0,
    Area = 1,
    Sphere = 2,
    OrganicSphere = 3
};

enum class ToolMode {
    Build = 0,
    Delete = 1
};

struct ToolRaycastHit {
    glm::ivec3 voxel = glm::ivec3(0);
    glm::ivec3 adjacent = glm::ivec3(0);
    glm::ivec3 normal = glm::ivec3(0);
    glm::vec3 position = glm::vec3(0.0f);
    float distance = 0.0f;
};

struct ToolUseResult {
    bool consumed = false;
    std::shared_ptr<IChunkModifier> modifier;
};

struct ToolPreview {
    bool valid = false;
    bool anchored = false;
    ToolMode mode = ToolMode::Build;
    glm::ivec3 minVoxel = glm::ivec3(0);
    glm::ivec3 maxVoxel = glm::ivec3(0);
    glm::ivec3 anchorVoxel = glm::ivec3(0);
};
