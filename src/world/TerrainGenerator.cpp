#include "TerrainGenerator.h"

TerrainGenerator::TerrainGenerator(int terrainMinChunks, int terrainMaxChunks, float chunkSizeMeters, uint32_t seed)
    : noise_(seed),
      minHeightMeters_(static_cast<float>(terrainMinChunks) * chunkSizeMeters),
      maxHeightMeters_(static_cast<float>(terrainMaxChunks) * chunkSizeMeters),
      baseHeight_(0.5f * (minHeightMeters_ + maxHeightMeters_)),
      amplitude_(0.5f * (maxHeightMeters_ - minHeightMeters_)),
      invWavelength_(1.0f / std::max(4.0f, 6.0f * amplitude_)),
      topSoilDepth_(0.5f),         // grass: 8 voxels at voxPerMeter=16
      subSoilDepth_(1.25f),        // grass + dirt: 8+12=20 voxels at voxPerMeter=16
      octaves_(4),
      lacunarity_(2.0f),
      persistence_(0.5f) {
}

float TerrainGenerator::heightAt(float worldX, float worldZ) const {
    const float n = noise_.fbm2D(worldX * invWavelength_, worldZ * invWavelength_, octaves_, lacunarity_, persistence_);
    return baseHeight_ + amplitude_ * n;
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
