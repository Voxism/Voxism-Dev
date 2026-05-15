#include "BoundedChunkModifier.h"

#include "../Chunk.h"
#include "../VoxelMath.h"

BoundedChunkModifier::BoundedChunkModifier(const glm::ivec3 &minVoxel,
    const glm::ivec3 &maxVoxel,
    int chunkSizeVoxels,
    bool fill,
    uint8_t materialID):
    minVoxel_(glm::min(minVoxel, maxVoxel)),
    maxVoxel_(glm::max(minVoxel, maxVoxel)),
    chunkSizeVoxels_(chunkSizeVoxels),
    fill_(fill),
    materialID_(materialID)
{
}

void BoundedChunkModifier::getTouchedChunkBounds(ChunkPos &minChunk, ChunkPos &maxChunk) const
{
    minChunk = {
        floorDiv(minVoxel_.x, chunkSizeVoxels_),
        floorDiv(minVoxel_.y, chunkSizeVoxels_),
        floorDiv(minVoxel_.z, chunkSizeVoxels_)
    };
    maxChunk = {
        floorDiv(maxVoxel_.x, chunkSizeVoxels_),
        floorDiv(maxVoxel_.y, chunkSizeVoxels_),
        floorDiv(maxVoxel_.z, chunkSizeVoxels_)
    };
}

void BoundedChunkModifier::applyToChunk(Chunk &chunk, const ChunkPos &chunkPos) const
{
    if (isEmpty()) {
        return;
    }

    const int chunkSizeX = chunk.getLocalVoxelSizeX();
    const int chunkSizeY = chunk.getLocalVoxelSizeY();
    const int chunkSizeZ = chunk.getLocalVoxelSizeZ();

    const glm::ivec3 chunkMin(
        chunkPos.x * chunkSizeX,
        chunkPos.y * chunkSizeY,
        chunkPos.z * chunkSizeZ
    );
    const glm::ivec3 chunkMax = chunkMin + glm::ivec3(chunkSizeX - 1, chunkSizeY - 1, chunkSizeZ - 1);

    const glm::ivec3 clippedMin = glm::max(minVoxel_, chunkMin);
    const glm::ivec3 clippedMax = glm::min(maxVoxel_, chunkMax);
    if (clippedMin.x > clippedMax.x || clippedMin.y > clippedMax.y || clippedMin.z > clippedMax.z) {
        return;
    }

    for (int z = clippedMin.z; z <= clippedMax.z; ++z) {
        for (int y = clippedMin.y; y <= clippedMax.y; ++y) {
            for (int x = clippedMin.x; x <= clippedMax.x; ++x) {
                const glm::ivec3 voxel(x, y, z);
                if (!affectsWorldVoxel(voxel)) {
                    continue;
                }

                const int localX = x - chunkMin.x;
                const int localY = y - chunkMin.y;
                const int localZ = z - chunkMin.z;
                chunk.setOccupiedLocal(localX, localY, localZ, fill_);
                if (fill_) {
                    chunk.setMaterialLocal(localX, localY, localZ, materialID_);
                }
            }
        }
    }
}
