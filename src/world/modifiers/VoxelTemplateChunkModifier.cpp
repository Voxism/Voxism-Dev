#include "VoxelTemplateChunkModifier.h"

#include "../Chunk.h"

#include <unordered_map>

namespace {

long long voxelKey(const glm::ivec3 &voxel)
{
    return (static_cast<long long>(voxel.x) << 42) ^
        (static_cast<long long>(voxel.y) << 21) ^
        static_cast<long long>(voxel.z);
}

} // namespace

VoxelTemplateChunkModifier::VoxelTemplateChunkModifier(const glm::ivec3 &anchorVoxel,
    const VoxelTemplate &templ,
    int chunkSizeVoxels):
    BoundedChunkModifier(
        anchorVoxel + templ.minOffset,
        anchorVoxel + templ.maxOffset,
        chunkSizeVoxels,
        true,
        0u),
    anchorVoxel_(anchorVoxel),
    templ_(templ)
{
}

bool VoxelTemplateChunkModifier::affectsWorldVoxel(const glm::ivec3 &voxel) const
{
    for (const auto &cell : templ_.cells) {
        if (anchorVoxel_ + cell.offset == voxel) {
            return true;
        }
    }
    return false;
}

void VoxelTemplateChunkModifier::applyToChunk(Chunk &chunk, const ChunkPos &chunkPos) const
{
    const int chunkSizeX = chunk.getLocalVoxelSizeX();
    const int chunkSizeY = chunk.getLocalVoxelSizeY();
    const int chunkSizeZ = chunk.getLocalVoxelSizeZ();
    const glm::ivec3 chunkMin(
        chunkPos.x * chunkSizeX,
        chunkPos.y * chunkSizeY,
        chunkPos.z * chunkSizeZ);
    const glm::ivec3 chunkMax = chunkMin + glm::ivec3(chunkSizeX - 1, chunkSizeY - 1, chunkSizeZ - 1);

    for (const auto &cell : templ_.cells) {
        const glm::ivec3 worldVoxel = anchorVoxel_ + cell.offset;
        if (worldVoxel.x < chunkMin.x || worldVoxel.x > chunkMax.x ||
            worldVoxel.y < chunkMin.y || worldVoxel.y > chunkMax.y ||
            worldVoxel.z < chunkMin.z || worldVoxel.z > chunkMax.z) {
            continue;
        }

        const glm::ivec3 local = worldVoxel - chunkMin;
        chunk.setOccupiedLocal(local.x, local.y, local.z, true);
        chunk.setMaterialLocal(local.x, local.y, local.z, cell.materialID);
    }
}
