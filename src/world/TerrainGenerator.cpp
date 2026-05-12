#include "TerrainGenerator.h"

TerrainGenerator::TerrainGenerator(int terrainMinChunks, int terrainMaxChunks, float chunkSizeMeters, uint32_t seed)
    : noise_(seed),
      minHeightMeters_(static_cast<float>(terrainMinChunks) * chunkSizeMeters),
      maxHeightMeters_(static_cast<float>(terrainMaxChunks) * chunkSizeMeters),
      baseHeight_(0.5f * (minHeightMeters_ + maxHeightMeters_)),
      amplitude_(0.5f * (maxHeightMeters_ - minHeightMeters_)),
      invWavelength_(1.0f / std::max(4.0f, 6.0f * amplitude_)),
      topSoilDepth_(1.0f),
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
    return (depthFromSurface <= topSoilDepth_) ? 0u : 1u;
}
