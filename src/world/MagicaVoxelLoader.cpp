#include "MagicaVoxelLoader.h"

#include <algorithm>
#include <fstream>
#include <vector>

namespace {

uint32_t readLE32(const unsigned char *ptr)
{
    return static_cast<uint32_t>(ptr[0]) |
        (static_cast<uint32_t>(ptr[1]) << 8) |
        (static_cast<uint32_t>(ptr[2]) << 16) |
        (static_cast<uint32_t>(ptr[3]) << 24);
}

} // namespace

bool MagicaVoxelLoader::load(const std::string &path, MagicaVoxelModel &outModel, std::string &outError)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        outError = "failed to open file";
        return false;
    }

    char magic[4] = {};
    input.read(magic, 4);
    if (!input || std::string(magic, 4) != "VOX ") {
        outError = "missing VOX header";
        return false;
    }

    unsigned char versionBytes[4] = {};
    input.read(reinterpret_cast<char *>(versionBytes), 4);
    if (!input) {
        outError = "missing version";
        return false;
    }

    outModel = MagicaVoxelModel();
    bool sizeLoaded = false;
    bool cellsLoaded = false;

    while (true) {
        char idChars[4] = {};
        input.read(idChars, 4);
        if (!input) {
            break;
        }

        unsigned char sizeBytes[8] = {};
        input.read(reinterpret_cast<char *>(sizeBytes), 8);
        if (!input) {
            outError = "truncated chunk header";
            return false;
        }

        const uint32_t contentSize = readLE32(sizeBytes);
        const std::vector<unsigned char> content(contentSize);
        std::vector<unsigned char> mutableContent = content;
        input.read(reinterpret_cast<char *>(mutableContent.data()), static_cast<std::streamsize>(contentSize));
        if (!input) {
            outError = "truncated chunk content";
            return false;
        }

        const std::string chunkId(idChars, 4);
        if (chunkId == "SIZE" && contentSize >= 12 && !sizeLoaded) {
            outModel.size = glm::ivec3(
                static_cast<int>(readLE32(mutableContent.data())),
                static_cast<int>(readLE32(mutableContent.data() + 4)),
                static_cast<int>(readLE32(mutableContent.data() + 8)));
            sizeLoaded = true;
        } else if (chunkId == "XYZI" && contentSize >= 4 && !cellsLoaded) {
            const uint32_t cellCount = readLE32(mutableContent.data());
            const std::size_t availableCells = (contentSize - 4) / 4;
            const std::size_t count = std::min<std::size_t>(cellCount, availableCells);
            outModel.cells.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                const std::size_t base = 4 + i * 4;
                MagicaVoxelCell cell;
                cell.x = mutableContent[base + 0];
                cell.y = mutableContent[base + 1];
                cell.z = mutableContent[base + 2];
                cell.colorIndex = mutableContent[base + 3];
                outModel.cells.push_back(cell);
            }
            cellsLoaded = true;
        } else if (chunkId == "RGBA" && contentSize >= 1024) {
            for (int i = 0; i < 256; ++i) {
                outModel.palette[static_cast<std::size_t>(i)] =
                    readLE32(mutableContent.data() + i * 4);
            }
            outModel.hasPalette = true;
        }
    }

    if (!sizeLoaded) {
        outError = "missing SIZE chunk";
        return false;
    }
    if (!cellsLoaded) {
        outError = "missing XYZI chunk";
        return false;
    }

    return true;
}
