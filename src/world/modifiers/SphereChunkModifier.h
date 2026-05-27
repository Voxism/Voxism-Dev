#pragma once

#include "BoundedChunkModifier.h"

class SphereChunkModifier : public BoundedChunkModifier {
public:
    SphereChunkModifier(const glm::ivec3 &cellMinVoxel,
        int sizeVoxels,
        int chunkSizeVoxels,
        bool fill,
        uint8_t materialID);

    bool affectsWorldVoxel(const glm::ivec3 &voxel) const override;

private:
    glm::vec3 sphereCenter_ = glm::vec3(0.0f);
    float radiusSquared_ = 0.0f;
};
