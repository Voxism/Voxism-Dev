#include "TerrainGenerator.h"

#include "Materials.h"
#include "VoxelMath.h"

namespace {

uint32_t mixHash(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

uint32_t hashCoords(uint32_t seed, int x, int z, int salt)
{
    uint32_t h = seed;
    h ^= mixHash(static_cast<uint32_t>(x) + 0x9e3779b9u);
    h ^= mixHash(static_cast<uint32_t>(z) + 0x85ebca6bu);
    h ^= mixHash(static_cast<uint32_t>(salt) + 0xc2b2ae35u);
    return mixHash(h);
}

float hash01(uint32_t seed, int x, int z, int salt)
{
    const uint32_t h = hashCoords(seed, x, z, salt);
    return static_cast<float>(h & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

} // namespace

TerrainGenerator::TerrainGenerator(int terrainMinChunks, int terrainMaxChunks, float chunkSizeMeters, std::vector<Octave> &octaves,  uint32_t seed)
    : noise_(seed),
      seed_(seed),
      octaves(octaves),
      minHeightMeters_(static_cast<float>(terrainMinChunks) * chunkSizeMeters),
      maxHeightMeters_(static_cast<float>(terrainMaxChunks) * chunkSizeMeters),
      topSoilDepth_(0.5f),         // grass: 8 voxels at voxPerMeter=16
      subSoilDepth_(1.25f)        // grass + dirt: 8+12=20 voxels at voxPerMeter=16 
      {}

float TerrainGenerator::heightAt(float worldX, float worldZ) const {
    const float n = noise_.fbm2D(worldX, worldZ, octaves);
    return n;
}

uint8_t TerrainGenerator::materialAt(float worldX, float worldZ, float worldY, float surfaceHeight) const {
    (void)worldX;
    (void)worldZ;
    const float depthFromSurface = surfaceHeight - worldY;
    // Layered terrain:
    //   [0, topSoilDepth)       -> Grass (id 0)
    //   [topSoilDepth, subSoil) -> Dirt  (id 4)
    //   [subSoil,        ..  )  -> Stone (id 1)
    if (depthFromSurface <= topSoilDepth_) return 0u;  // Grass
    if (depthFromSurface <= subSoilDepth_) return 4u;  // Dirt
    return 1u;                                          // Stone
}

bool TerrainGenerator::loadFloraAssets(const std::string &resourceDirectory, std::string &outError)
{
    floraLoaded_ = floraAssets_.load(resourceDirectory, outError);
    return floraLoaded_;
}

const std::vector<std::shared_ptr<IChunkModifier>> TerrainGenerator::generateRocks(float worldX, float worldY, float worldZ, int numRocks, ChunkManager& cm){
  std::vector<std::shared_ptr<IChunkModifier>> rocks(numRocks);
  for (int i=0; i<numRocks; ++i){
    float radiusBase = ((rand()/(float)RAND_MAX)+0.5);
    if (radiusBase > 1.45){
      radiusBase*=1.5;
    }
    float radius = radiusBase*0.9f-0.1*i;

    if(radius<0){break;}
    float heightOffset = (rand()/(float)RAND_MAX)-0.6-0.1*i;
    float xOffset = ((rand()/(float)RAND_MAX)-0.5)*i*1.1;
    float zOffset = ((rand()/(float)RAND_MAX)-0.5)*i*1.1;
    rocks.push_back(
      std::make_shared<SphereChunkModifier>(
        glm::ivec3(glm::floor((worldX-radius+xOffset)*cm.voxPerMeter), glm::floor((worldY-radius+heightOffset)*cm.voxPerMeter), glm::floor((worldZ-radius+zOffset)*cm.voxPerMeter)),
        (radius*2)*cm.voxPerMeter,
        cm.chunkSizeInts*32,
        true,
        1u
      )
    );

  }
  return rocks;

}

// bool TerrainGenerator::canPlaceTemplate(const VoxelTemplate &templ,
//     const glm::ivec3 &anchorVoxel,
//     int chunkX,
//     int chunkZ,
//     ChunkManager &cm) const
// {
//     if (templ.empty()) {
//         return false;
//     }

//     bool hasCenterSupport = false;
//     for (const auto &cell : templ.cells) {
//         if (cell.offset.y != 0) {
//             continue;
//         }

//         const glm::ivec3 supportVoxel = anchorVoxel + glm::ivec3(cell.offset.x, -1, cell.offset.z);
//         if (!cm.isVoxelOccupied(supportVoxel)) {
//             continue;
//         }
//         if (cm.voxelMaterial(supportVoxel) != static_cast<uint8_t>(Materials::Grass)) {
//             continue;
//         }

//         // Any grounded bottom voxel is enough support for ambient flora.
//         hasCenterSupport = true;
//         if (cell.offset.x == 0 && cell.offset.z == 0) {
//             break;
//         }
//     }
//     if (!hasCenterSupport) {
//         return false;
//     }

//     for (const auto &cell : templ.cells) {
//         const glm::ivec3 worldVoxel = anchorVoxel + cell.offset;
//         const ChunkPos cp = cm.getChunkPosForVoxel(worldVoxel);
//         if (cp.x != chunkX || cp.z != chunkZ) {
//             return false;
//         }
//         if (cm.isVoxelOccupied(worldVoxel)) {
//             return false;
//         }
//     }

//     return true;
// }

const std::vector<std::shared_ptr<IChunkModifier>> TerrainGenerator::generateFlora(
    int chunkX,
    int chunkZ,
    const std::vector<float> &heightMap,
    ChunkManager& cm) const
{
    std::vector<std::shared_ptr<IChunkModifier>> flora;

    if (!floraLoaded_) {
        return flora;
    }

    const int sideVox = cm.chunkSizeInts * 32;
    const int step = std::max(8, cm.voxPerMeter);

    const VoxelTemplate &flower = floraAssets_.flower();
    const VoxelTemplate &bush = floraAssets_.bush();

    for (int localZ = 0; localZ < sideVox; localZ += step) {
        for (int localX = 0; localX < sideVox; localX += step) {
            const int worldX = chunkX * sideVox + localX;
            const int worldZ = chunkZ * sideVox + localZ;

            const float roll = hash01(seed_, worldX, worldZ, 17);

            const VoxelTemplate *selected = nullptr;

            if (roll < 0.01f) {
                selected = &flower;
            } else if (roll < 0.02f) {
                selected = &bush;
            } else {
                continue;
            }

            const bool fitsInChunk =
                localX + selected->minOffset.x >= 0 &&
                localX + selected->maxOffset.x < sideVox &&
                localZ + selected->minOffset.z >= 0 &&
                localZ + selected->maxOffset.z < sideVox;

            if (!fitsInChunk) {
                continue;
            }

            const float surfaceHeight = heightMap[localZ * sideVox + localX];

            const int supportVoxelY = static_cast<int>(
                glm::floor((surfaceHeight - 0.001f) / cm.voxSizeMeters)
            );

            const glm::ivec3 anchorVoxel(
                worldX,
                supportVoxelY + 1,
                worldZ
            );

            flora.push_back(std::make_shared<VoxelTemplateChunkModifier>(
                anchorVoxel,
                *selected,
                sideVox
            ));
        }
    }

    return flora;
}