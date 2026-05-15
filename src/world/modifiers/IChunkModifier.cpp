#include "IChunkModifier.h"

bool IChunkModifier::effectsChunk(const ChunkPos &cp) const
{
    ChunkPos minChunk {};
    ChunkPos maxChunk {};
    getTouchedChunkBounds(minChunk, maxChunk);

    return cp.x >= minChunk.x && cp.x <= maxChunk.x &&
        cp.y >= minChunk.y && cp.y <= maxChunk.y &&
        cp.z >= minChunk.z && cp.z <= maxChunk.z;
}
