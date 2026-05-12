#pragma once

#include "IChunkModifier.h"

#include <glm/glm.hpp>

class BoundedChunkModifier : public IChunkModifier {
public:
    void applyToChunk(Chunk &chunk, const ChunkPos &chunkPos) const final;
    void getTouchedChunkBounds(ChunkPos &minChunk, ChunkPos &maxChunk) const final;

protected:
    BoundedChunkModifier(const glm::ivec3 &minVoxel,
        const glm::ivec3 &maxVoxel,
        int chunkSizeVoxels,
        bool fill,
        uint8_t materialID);

    virtual bool affectsWorldVoxel(const glm::ivec3 &voxel) const = 0;
    virtual bool isEmpty() const { return false; }

    bool isFill() const { return fill_; }
    uint8_t materialID() const { return materialID_; }
    const glm::ivec3 &minVoxel() const { return minVoxel_; }
    const glm::ivec3 &maxVoxel() const { return maxVoxel_; }

private:
    glm::ivec3 minVoxel_;
    glm::ivec3 maxVoxel_;
    int chunkSizeVoxels_ = 32;
    bool fill_ = true;
    uint8_t materialID_ = 0;
};
