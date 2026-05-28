#include "TerrainGenerator.h"

TerrainGenerator::TerrainGenerator(int terrainMinChunks, int terrainMaxChunks, float chunkSizeMeters, std::vector<Octave> &octaves,  uint32_t seed)
    : noise_(seed),
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
