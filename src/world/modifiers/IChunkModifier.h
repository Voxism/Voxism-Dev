#pragma once

#include "../ChunkPos.h"

class Chunk;

class IChunkModifier {
public:
    virtual ~IChunkModifier() = default;

    virtual void applyToChunk(Chunk &chunk, const ChunkPos &chunkPos) const = 0;
    virtual void getTouchedChunkBounds(ChunkPos &minChunk, ChunkPos &maxChunk) const = 0;

    virtual bool effectsChunk(const ChunkPos &cp) const;
};
