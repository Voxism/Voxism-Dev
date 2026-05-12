#include "BoxChunkModifier.h"

BoxChunkModifier::BoxChunkModifier(const glm::ivec3 &minVoxel,
    const glm::ivec3 &maxVoxel,
    int chunkSizeVoxels,
    bool fill,
    uint8_t materialID):
    BoundedChunkModifier(minVoxel, maxVoxel, chunkSizeVoxels, fill, materialID)
{
}

bool BoxChunkModifier::affectsWorldVoxel(const glm::ivec3 &voxel) const
{
    (void)voxel;
    return true;
}
