#pragma once

#include "../ChunkPos.h"

#include <glm/glm.hpp>

class Chunk;

class IChunkModifier {
public:
    virtual ~IChunkModifier() = default;

    virtual void applyToChunk(Chunk &chunk, const ChunkPos &chunkPos) const = 0;
    virtual void getTouchedChunkBounds(ChunkPos &minChunk, ChunkPos &maxChunk) const = 0;
    virtual void getAffectedVoxelBounds(glm::ivec3 &minVoxel, glm::ivec3 &maxVoxel) const = 0;
    virtual bool affectsWorldVoxel(const glm::ivec3 &voxel) const = 0;
    virtual bool isFillOperation() const = 0;

    virtual bool effectsChunk(const ChunkPos &cp) const;
};
