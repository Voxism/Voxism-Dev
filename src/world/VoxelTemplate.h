#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct VoxelTemplateCell {
    glm::ivec3 offset = glm::ivec3(0);
    uint8_t materialID = 0;
};

struct VoxelTemplate {
    std::string name;
    glm::ivec3 size = glm::ivec3(0);
    glm::ivec3 minOffset = glm::ivec3(0);
    glm::ivec3 maxOffset = glm::ivec3(0);
    std::vector<VoxelTemplateCell> cells;

    bool empty() const { return cells.empty(); }
};
