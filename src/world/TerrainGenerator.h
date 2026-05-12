#pragma once

#include <algorithm>
#include <cstdint>

#include "PerlinNoise.h"

class TerrainGenerator {
public:
    TerrainGenerator(int terrainMinChunks, int terrainMaxChunks, float chunkSizeMeters, uint32_t seed = 1337u);

    float heightAt(float worldX, float worldZ) const;
    uint8_t materialAt(float worldX, float worldZ, float worldY, float surfaceHeight) const;

private:
    PerlinNoise noise_;
    float minHeightMeters_;
    float maxHeightMeters_;
    float baseHeight_;
    float amplitude_;
    float invWavelength_;
    float topSoilDepth_;
    int octaves_;
    float lacunarity_;
    float persistence_;
};
