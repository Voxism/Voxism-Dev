#pragma once

#include "BoundedChunkModifier.h"

#include <vector>

class StrokeSphereChunkModifier : public BoundedChunkModifier {
public:
    StrokeSphereChunkModifier(const std::vector<glm::vec3> &centers,
        float radiusMeters,
        int chunkSizeVoxels,
        float voxelSizeMeters,
        bool fill,
        uint8_t materialID);

    bool affectsWorldVoxel(const glm::ivec3 &voxel) const override;
    bool isEmpty() const override { return centers_.empty(); }

private:
    std::vector<glm::vec3> centers_;
    float radiusSquared_ = 0.0f;
    float voxelSizeMeters_ = 1.0f;
};
