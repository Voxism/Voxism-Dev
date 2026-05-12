#include "VoxelMath.h"

#include <cmath>

int floorDiv(int value, int divisor)
{
    int result = value / divisor;
    const int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        --result;
    }
    return result;
}

int snapDownToGrid(int value, int gridSize)
{
    return floorDiv(value, gridSize) * gridSize;
}

glm::ivec3 snapVoxelToGrid(const glm::ivec3 &voxel, int gridSize)
{
    return glm::ivec3(
        snapDownToGrid(voxel.x, gridSize),
        snapDownToGrid(voxel.y, gridSize),
        snapDownToGrid(voxel.z, gridSize));
}

glm::ivec3 worldMinVoxel(const glm::vec3 &center, float radiusMeters, float voxelSizeMeters)
{
    const glm::vec3 minWorld = center - glm::vec3(radiusMeters);
    return glm::ivec3(
        static_cast<int>(std::floor(minWorld.x / voxelSizeMeters)),
        static_cast<int>(std::floor(minWorld.y / voxelSizeMeters)),
        static_cast<int>(std::floor(minWorld.z / voxelSizeMeters)));
}

glm::ivec3 worldMaxVoxel(const glm::vec3 &center, float radiusMeters, float voxelSizeMeters)
{
    const glm::vec3 maxWorld = center + glm::vec3(radiusMeters);
    return glm::ivec3(
        static_cast<int>(std::ceil(maxWorld.x / voxelSizeMeters)) - 1,
        static_cast<int>(std::ceil(maxWorld.y / voxelSizeMeters)) - 1,
        static_cast<int>(std::ceil(maxWorld.z / voxelSizeMeters)) - 1);
}
