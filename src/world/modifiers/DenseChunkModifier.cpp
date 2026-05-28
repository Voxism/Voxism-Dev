#include "DenseChunkModifier.h"

#include "../Chunk.h"

#include <algorithm>

DenseChunkModifier::DenseChunkModifier(const ChunkPos &chunkPos,
    int chunkSizeVoxels,
    std::vector<uint32_t> occupancyWords,
    std::vector<uint8_t> materialTexels):
    chunkPos_(chunkPos),
    chunkSizeVoxels_(chunkSizeVoxels),
    textureSize_(std::max(1, chunkSizeVoxels / 2)),
    minVoxel_(
        chunkPos.x * chunkSizeVoxels,
        chunkPos.y * chunkSizeVoxels,
        chunkPos.z * chunkSizeVoxels),
    maxVoxel_(minVoxel_ + glm::ivec3(chunkSizeVoxels - 1)),
    occupancyWords_(std::move(occupancyWords)),
    materialTexels_(std::move(materialTexels))
{
}

void DenseChunkModifier::getTouchedChunkBounds(ChunkPos &minChunk, ChunkPos &maxChunk) const
{
    minChunk = chunkPos_;
    maxChunk = chunkPos_;
}

void DenseChunkModifier::getAffectedVoxelBounds(glm::ivec3 &minVoxel, glm::ivec3 &maxVoxel) const
{
    minVoxel = minVoxel_;
    maxVoxel = maxVoxel_;
}

bool DenseChunkModifier::localVoxelOccupied(int x, int y, int z) const
{
    if (x < 0 || y < 0 || z < 0 ||
        x >= chunkSizeVoxels_ || y >= chunkSizeVoxels_ || z >= chunkSizeVoxels_) {
        return false;
    }

    const int intsPerRow = chunkSizeVoxels_ / 32;
    const int wordIndex = z * intsPerRow * chunkSizeVoxels_ + y * intsPerRow + (x / 32);
    if (wordIndex < 0 || static_cast<size_t>(wordIndex) >= occupancyWords_.size()) {
        return false;
    }

    const uint32_t mask = 1u << (31 - (x % 32));
    return (occupancyWords_[static_cast<size_t>(wordIndex)] & mask) != 0u;
}

uint8_t DenseChunkModifier::localMaterialAt(int x, int y, int z) const
{
    if (x < 0 || y < 0 || z < 0 ||
        x >= chunkSizeVoxels_ || y >= chunkSizeVoxels_ || z >= chunkSizeVoxels_) {
        return 0u;
    }

    const int texX = x / 2;
    const int texY = y / 2;
    const int texZ = z / 2;
    const int index = texZ * textureSize_ * textureSize_ + texY * textureSize_ + texX;
    if (index < 0 || static_cast<size_t>(index) >= materialTexels_.size()) {
        return 0u;
    }
    return materialTexels_[static_cast<size_t>(index)];
}

bool DenseChunkModifier::affectsWorldVoxel(const glm::ivec3 &voxel) const
{
    if (voxel.x < minVoxel_.x || voxel.x > maxVoxel_.x ||
        voxel.y < minVoxel_.y || voxel.y > maxVoxel_.y ||
        voxel.z < minVoxel_.z || voxel.z > maxVoxel_.z) {
        return false;
    }

    const glm::ivec3 local = voxel - minVoxel_;
    return localVoxelOccupied(local.x, local.y, local.z);
}

void DenseChunkModifier::applyToChunk(Chunk &chunk, const ChunkPos &chunkPos) const
{
    if (!(chunkPos == chunkPos_)) {
        return;
    }

    const int sizeX = std::min(chunk.getLocalVoxelSizeX(), chunkSizeVoxels_);
    const int sizeY = std::min(chunk.getLocalVoxelSizeY(), chunkSizeVoxels_);
    const int sizeZ = std::min(chunk.getLocalVoxelSizeZ(), chunkSizeVoxels_);

    for (int z = 0; z < sizeZ; ++z) {
        for (int y = 0; y < sizeY; ++y) {
            for (int x = 0; x < sizeX; ++x) {
                chunk.setOccupiedLocal(x, y, z, localVoxelOccupied(x, y, z));
            }
        }
    }

    for (int z = 0; z < sizeZ; z += 2) {
        for (int y = 0; y < sizeY; y += 2) {
            for (int x = 0; x < sizeX; x += 2) {
                chunk.setMaterialLocal(x, y, z, localMaterialAt(x, y, z));
            }
        }
    }
}
