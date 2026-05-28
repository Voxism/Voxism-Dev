#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct MagicaVoxelCell {
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t z = 0;
    uint8_t colorIndex = 0;
};

struct MagicaVoxelModel {
    glm::ivec3 size = glm::ivec3(0);
    std::vector<MagicaVoxelCell> cells;
    std::array<uint32_t, 256> palette {};
    bool hasPalette = false;
};

class MagicaVoxelLoader {
public:
    static bool load(const std::string &path, MagicaVoxelModel &outModel, std::string &outError);
};
