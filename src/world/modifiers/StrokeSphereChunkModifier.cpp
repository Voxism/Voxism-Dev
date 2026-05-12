#include "StrokeSphereChunkModifier.h"

#include "../VoxelMath.h"

namespace {
glm::ivec3 computeMinVoxel(const std::vector<glm::vec3> &centers, float radiusMeters, float voxelSizeMeters)
{
    if (centers.empty()) {
        return glm::ivec3(0);
    }

    glm::ivec3 minVoxel = worldMinVoxel(centers.front(), radiusMeters, voxelSizeMeters);
    for (size_t i = 1; i < centers.size(); ++i) {
        minVoxel = glm::min(minVoxel, worldMinVoxel(centers[i], radiusMeters, voxelSizeMeters));
    }
    return minVoxel;
}

glm::ivec3 computeMaxVoxel(const std::vector<glm::vec3> &centers, float radiusMeters, float voxelSizeMeters)
{
    if (centers.empty()) {
        return glm::ivec3(0);
    }

    glm::ivec3 maxVoxel = worldMaxVoxel(centers.front(), radiusMeters, voxelSizeMeters);
    for (size_t i = 1; i < centers.size(); ++i) {
        maxVoxel = glm::max(maxVoxel, worldMaxVoxel(centers[i], radiusMeters, voxelSizeMeters));
    }
    return maxVoxel;
}
}

StrokeSphereChunkModifier::StrokeSphereChunkModifier(const std::vector<glm::vec3> &centers,
    float radiusMeters,
    int chunkSizeVoxels,
    float voxelSizeMeters,
    bool fill,
    uint8_t materialID):
    BoundedChunkModifier(
        computeMinVoxel(centers, radiusMeters, voxelSizeMeters),
        computeMaxVoxel(centers, radiusMeters, voxelSizeMeters),
        chunkSizeVoxels,
        fill,
        materialID),
    centers_(centers),
    radiusSquared_(radiusMeters * radiusMeters),
    voxelSizeMeters_(voxelSizeMeters)
{
}

bool StrokeSphereChunkModifier::affectsWorldVoxel(const glm::ivec3 &voxel) const
{
    const glm::vec3 voxelCenter(
        (static_cast<float>(voxel.x) + 0.5f) * voxelSizeMeters_,
        (static_cast<float>(voxel.y) + 0.5f) * voxelSizeMeters_,
        (static_cast<float>(voxel.z) + 0.5f) * voxelSizeMeters_);

    for (const glm::vec3 &center : centers_) {
        const glm::vec3 delta = voxelCenter - center;
        if (glm::dot(delta, delta) <= radiusSquared_) {
            return true;
        }
    }
    return false;
}
