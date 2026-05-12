#pragma once

#include <glm/glm.hpp>

int floorDiv(int value, int divisor);
int snapDownToGrid(int value, int gridSize);
glm::ivec3 snapVoxelToGrid(const glm::ivec3 &voxel, int gridSize);
glm::ivec3 worldMinVoxel(const glm::vec3 &center, float radiusMeters, float voxelSizeMeters);
glm::ivec3 worldMaxVoxel(const glm::vec3 &center, float radiusMeters, float voxelSizeMeters);
