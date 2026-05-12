#pragma once

#include "BoundedChunkModifier.h"

class BoxChunkModifier : public BoundedChunkModifier {
public:
    BoxChunkModifier(const glm::ivec3 &minVoxel,
        const glm::ivec3 &maxVoxel,
        int chunkSizeVoxels,
        bool fill,
        uint8_t materialID);

protected:
    bool affectsWorldVoxel(const glm::ivec3 &voxel) const override;
};
