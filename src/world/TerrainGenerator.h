#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>

#include "PerlinNoise.h"
#include "Octave.h"
#include "modifiers/IChunkModifier.h"
#include "modifiers/SphereChunkModifier.h"
#include "ChunkManager.h"

class TerrainGenerator {
public:
    TerrainGenerator(int terrainMinChunks, int terrainMaxChunks, float chunkSizeMeters, std::vector<Octave> &octaves,  uint32_t seed);

    float heightAt(float worldX, float worldZ) const;
    uint8_t materialAt(float worldX, float worldZ, float worldY, float surfaceHeight) const;

    const std::vector<std::shared_ptr<IChunkModifier>> generateRocks(float worldX, float worldY, float worldZ, int numRocks, ChunkManager& cm);

private:
    std::vector<Octave> octaves;

    PerlinNoise noise_;
    float minHeightMeters_;
    float maxHeightMeters_;
    float baseHeight_;
    float amplitude_;
    float invWavelength_;
    float topSoilDepth_;
    float subSoilDepth_;
    int octaves_;
    float lacunarity_;
    float persistence_;
};
