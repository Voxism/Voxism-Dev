#pragma once

#include "../../world/VoxelTemplate.h"
#include "BoundedChunkModifier.h"

class VoxelTemplateChunkModifier : public BoundedChunkModifier {
public:
    VoxelTemplateChunkModifier(const glm::ivec3 &anchorVoxel,
        const VoxelTemplate &templ,
        int chunkSizeVoxels);

    bool affectsWorldVoxel(const glm::ivec3 &voxel) const override;
    void applyToChunk(Chunk &chunk, const ChunkPos &chunkPos) const override;

private:
    glm::ivec3 anchorVoxel_ = glm::ivec3(0);
    VoxelTemplate templ_;
};
