#include "SphereChunkModifier.h"

SphereChunkModifier::SphereChunkModifier(const glm::ivec3 &cellMinVoxel,
    int sizeVoxels,
    int chunkSizeVoxels,
    bool fill,
    uint8_t materialID):
    BoundedChunkModifier(
        cellMinVoxel,
        cellMinVoxel + glm::ivec3(sizeVoxels - 1),
        chunkSizeVoxels,
        fill,
        materialID),
    sphereCenter_(glm::vec3(cellMinVoxel) + glm::vec3(sizeVoxels * 0.5f))
{
    const float radius = sizeVoxels * 0.5f;
    radiusSquared_ = radius * radius;
}

bool SphereChunkModifier::affectsWorldVoxel(const glm::ivec3 &voxel) const
{
    const glm::vec3 voxelCenter(
        static_cast<float>(voxel.x) + 0.5f,
        static_cast<float>(voxel.y) + 0.5f,
        static_cast<float>(voxel.z) + 0.5f);
    const glm::vec3 delta = voxelCenter - sphereCenter_;
    return glm::dot(delta, delta) <= radiusSquared_;
}
