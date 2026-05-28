#pragma once

#include "IChunkModifier.h"

#include <cstdint>
#include <vector>

class DenseChunkModifier : public IChunkModifier {
public:
    DenseChunkModifier(const ChunkPos &chunkPos,
        int chunkSizeVoxels,
        std::vector<uint32_t> occupancyWords,
        std::vector<uint8_t> materialTexels);

    void applyToChunk(Chunk &chunk, const ChunkPos &chunkPos) const override;
    void getTouchedChunkBounds(ChunkPos &minChunk, ChunkPos &maxChunk) const override;
    void getAffectedVoxelBounds(glm::ivec3 &minVoxel, glm::ivec3 &maxVoxel) const override;
    bool affectsWorldVoxel(const glm::ivec3 &voxel) const override;
    bool isFillOperation() const override { return true; }

    const ChunkPos &chunkPos() const { return chunkPos_; }

private:
    bool localVoxelOccupied(int x, int y, int z) const;
    uint8_t localMaterialAt(int x, int y, int z) const;

    ChunkPos chunkPos_;
    int chunkSizeVoxels_ = 0;
    int textureSize_ = 0;
    glm::ivec3 minVoxel_ = glm::ivec3(0);
    glm::ivec3 maxVoxel_ = glm::ivec3(0);
    std::vector<uint32_t> occupancyWords_;
    std::vector<uint8_t> materialTexels_;
};
