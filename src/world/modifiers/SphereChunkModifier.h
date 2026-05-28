#pragma once

#include "BoundedChunkModifier.h"

class SphereChunkModifier : public BoundedChunkModifier {
public:
    SphereChunkModifier(const glm::ivec3 &cellMinVoxel, //bottom corner of bounding box.
        int sizeVoxels, //Diameter
        int chunkSizeVoxels, // chunkmanager chunksizeVoxels.
        bool fill, // true = create, false = delete
        uint8_t materialID);

    bool affectsWorldVoxel(const glm::ivec3 &voxel) const override;

private:
    glm::vec3 sphereCenter_ = glm::vec3(0.0f);
    float radiusSquared_ = 0.0f;
};
