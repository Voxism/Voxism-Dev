#pragma once

#include "IChunkModifier.h"

#include <glm/glm.hpp>

class BoundedChunkModifier : public IChunkModifier {
public:
    virtual void applyToChunk(Chunk &chunk, const ChunkPos &chunkPos) const override;
    void getTouchedChunkBounds(ChunkPos &minChunk, ChunkPos &maxChunk) const final;
    void getAffectedVoxelBounds(glm::ivec3 &minVoxel, glm::ivec3 &maxVoxel) const final;
    bool isFillOperation() const final { return fill_; }
    uint8_t materialID() const { return materialID_; }

protected:
    BoundedChunkModifier(const glm::ivec3 &minVoxel,
        const glm::ivec3 &maxVoxel,
        int chunkSizeVoxels,
        bool fill,
        uint8_t materialID);

    virtual bool isEmpty() const { return false; }

private:
    glm::ivec3 minVoxel_;
    glm::ivec3 maxVoxel_;
    int chunkSizeVoxels_ = 32;
    bool fill_ = true;
    uint8_t materialID_ = 0;
};
