#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

enum class ChunkEditAction {
    Fill = 0,
    Delete = 1
};

struct ChunkEditSummary {
    bool valid = false;
    ChunkEditAction action = ChunkEditAction::Fill;
    glm::ivec3 minVoxel = glm::ivec3(0);
    glm::ivec3 maxVoxel = glm::ivec3(0);
    std::size_t affectedVoxelCount = 0;
    struct DeletedVoxel {
        glm::ivec3 voxel = glm::ivec3(0);
        uint8_t materialID = 0;
    };
    std::vector<DeletedVoxel> deletedVoxels;
};
