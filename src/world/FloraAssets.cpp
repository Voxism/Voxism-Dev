#include "FloraAssets.h"

#include "MagicaVoxelLoader.h"
#include "Materials.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

namespace {

struct RgbColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

RgbColor rgbaToRgb(uint32_t rgba)
{
    RgbColor rgb;
    rgb.r = static_cast<uint8_t>(rgba & 0xffu);
    rgb.g = static_cast<uint8_t>((rgba >> 8) & 0xffu);
    rgb.b = static_cast<uint8_t>((rgba >> 16) & 0xffu);
    return rgb;
}

bool mapDb16ColorToMaterial(uint32_t rgba, uint8_t &outMaterialID)
{
    const RgbColor rgb = rgbaToRgb(rgba);
    if (rgb.r == 20 && rgb.g == 12 && rgb.b == 28) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Black);
        return true;
    }
    if (rgb.r == 68 && rgb.g == 36 && rgb.b == 52) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Purple);
        return true;
    }
    if (rgb.r == 48 && rgb.g == 52 && rgb.b == 109) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Navy);
        return true;
    }
    if (rgb.r == 78 && rgb.g == 74 && rgb.b == 78) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Slate);
        return true;
    }
    if (rgb.r == 133 && rgb.g == 76 && rgb.b == 48) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Brown);
        return true;
    }
    if (rgb.r == 52 && rgb.g == 101 && rgb.b == 36) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16DarkGreen);
        return true;
    }
    if (rgb.r == 208 && rgb.g == 70 && rgb.b == 72) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Red);
        return true;
    }
    if (rgb.r == 117 && rgb.g == 113 && rgb.b == 97) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Gray);
        return true;
    }
    if (rgb.r == 89 && rgb.g == 125 && rgb.b == 206) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Blue);
        return true;
    }
    if (rgb.r == 210 && rgb.g == 125 && rgb.b == 44) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Orange);
        return true;
    }
    if (rgb.r == 133 && rgb.g == 149 && rgb.b == 161) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Silver);
        return true;
    }
    if (rgb.r == 109 && rgb.g == 170 && rgb.b == 44) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Green);
        return true;
    }
    if (rgb.r == 210 && rgb.g == 170 && rgb.b == 153) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Peach);
        return true;
    }
    if (rgb.r == 109 && rgb.g == 194 && rgb.b == 202) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Cyan);
        return true;
    }
    if (rgb.r == 218 && rgb.g == 212 && rgb.b == 94) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16Yellow);
        return true;
    }
    if (rgb.r == 222 && rgb.g == 238 && rgb.b == 214) {
        outMaterialID = static_cast<uint8_t>(Materials::DB16White);
        return true;
    }
    return false;
}

bool buildTemplate(const std::string &path,
    const std::string &name,
    VoxelTemplate &outTemplate,
    std::string &outError)
{
    MagicaVoxelModel model;
    if (!MagicaVoxelLoader::load(path, model, outError)) {
        outError = name + ": " + outError;
        return false;
    }
    if (model.cells.empty()) {
        outError = name + ": no filled voxels";
        return false;
    }

    int minY = std::numeric_limits<int>::max();
    for (const auto &cell : model.cells) {
        // Goxel .vox exports use Z as the vertical axis; convert to engine Y-up.
        minY = std::min(minY, static_cast<int>(cell.z));
    }

    outTemplate = VoxelTemplate();
    outTemplate.name = name;
    outTemplate.size = glm::ivec3(model.size.x, model.size.z, model.size.y);
    outTemplate.minOffset = glm::ivec3(std::numeric_limits<int>::max());
    outTemplate.maxOffset = glm::ivec3(std::numeric_limits<int>::min());

    const int anchorX = model.size.x / 2;
    const int anchorZ = model.size.y / 2;
    outTemplate.cells.reserve(model.cells.size());

    for (const auto &cell : model.cells) {
        uint8_t materialID = 0u;
        const uint32_t rgba = model.palette[static_cast<std::size_t>(cell.colorIndex - 1)];
        if (!mapDb16ColorToMaterial(rgba, materialID)) {
            materialID = static_cast<uint8_t>(Materials::DB16Black);
        }

        VoxelTemplateCell templateCell;
        templateCell.offset = glm::ivec3(
            static_cast<int>(cell.x) - anchorX,
            static_cast<int>(cell.z) - minY,
            static_cast<int>(cell.y) - anchorZ);
        templateCell.materialID = materialID;
        outTemplate.minOffset = glm::min(outTemplate.minOffset, templateCell.offset);
        outTemplate.maxOffset = glm::max(outTemplate.maxOffset, templateCell.offset);
        outTemplate.cells.push_back(templateCell);
    }

    return true;
}

} // namespace

bool FloraAssets::load(const std::string &resourceDirectory, std::string &outError)
{
    const std::string floraDir = resourceDirectory + "/flora";
    VoxelTemplate bushTemplate;
    VoxelTemplate flowerTemplate;

    if (!buildTemplate(
            floraDir + "/bush1.vox",
            "bush",
            bushTemplate,
            outError)) {
        return false;
    }

    if (!buildTemplate(
            floraDir + "/flower1.vox",
            "flower",
            flowerTemplate,
            outError)) {
        return false;
    }

    bush_ = bushTemplate;
    flower_ = flowerTemplate;
    loaded_ = true;
    return true;
}
