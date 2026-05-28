#include "WorldFileIO.h"

#include "Chunk.h"
#include "ChunkManager.h"
#include "modifiers/DenseChunkModifier.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>

namespace WorldFileIO {
namespace {

constexpr char kMagic[8] = {'V', 'O', 'X', 'W', 'L', 'D', '1', '\0'};
constexpr uint32_t kVersion = 1u;

struct FileHeader {
    char magic[8];
    uint32_t version = 0;
    uint32_t chunkSizeVoxels = 0;
    uint32_t textureSize = 0;
    uint64_t chunkCount = 0;
};

struct ChunkRecordHeader {
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    uint32_t occupancyWordCount = 0;
    uint32_t materialTexelCount = 0;
};

template <typename T>
bool writeBinary(std::ofstream &stream, const T &value)
{
    stream.write(reinterpret_cast<const char *>(&value), sizeof(T));
    return static_cast<bool>(stream);
}

template <typename T>
bool readBinary(std::ifstream &stream, T &value)
{
    stream.read(reinterpret_cast<char *>(&value), sizeof(T));
    return static_cast<bool>(stream);
}

bool snapshotChunk(const std::shared_ptr<Chunk> &chunk,
    int chunkSizeVoxels,
    int textureSize,
    ChunkSnapshot &snapshot)
{
    if (!chunk) {
        return false;
    }

    const int intsPerRow = chunkSizeVoxels / 32;
    const std::size_t expectedOccupancyWordCount = static_cast<std::size_t>(intsPerRow) *
        static_cast<std::size_t>(chunkSizeVoxels) *
        static_cast<std::size_t>(chunkSizeVoxels);
    const std::size_t expectedMaterialTexelCount = static_cast<std::size_t>(textureSize) *
        static_cast<std::size_t>(textureSize) *
        static_cast<std::size_t>(textureSize);

    snapshot.chunkPos = chunk->getChunkPos();

    std::lock_guard<std::mutex> lock(chunk->mutex);
    if (!chunk->isGenerated()) {
        return false;
    }
    chunk->copySaveData(snapshot.occupancyWords, snapshot.materialTexels);
    if (snapshot.occupancyWords.size() != expectedOccupancyWordCount ||
        snapshot.materialTexels.size() != expectedMaterialTexelCount) {
        return false;
    }

    return true;
}

} // namespace

bool saveToFile(const std::string &path, ChunkManager &chunkManager, std::string &error)
{
    const std::vector<ChunkPos> positions = chunkManager.getGeneratedChunkPositions();
    const int chunkSizeVoxels = chunkManager.chunkSizeInts * 32;
    const int textureSize = chunkManager.chunkSizeInts * 16;

    const std::string tempPath = path + ".tmp";
    std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "Could not open save file for writing.";
        return false;
    }

    FileHeader header {};
    std::copy(std::begin(kMagic), std::end(kMagic), header.magic);
    header.version = kVersion;
    header.chunkSizeVoxels = static_cast<uint32_t>(chunkSizeVoxels);
    header.textureSize = static_cast<uint32_t>(textureSize);
    header.chunkCount = 0u;
    if (!writeBinary(out, header)) {
        error = "Could not write save file header.";
        return false;
    }

    uint64_t writtenChunkCount = 0u;
    for (const ChunkPos &position : positions) {
        ChunkSnapshot snapshot;
        if (!snapshotChunk(chunkManager.getChunk(position), chunkSizeVoxels, textureSize, snapshot)) {
            continue;
        }

        ChunkRecordHeader record {};
        record.x = position.x;
        record.y = position.y;
        record.z = position.z;
        record.occupancyWordCount = static_cast<uint32_t>(snapshot.occupancyWords.size());
        record.materialTexelCount = static_cast<uint32_t>(snapshot.materialTexels.size());
        if (!writeBinary(out, record)) {
            error = "Could not write chunk record header.";
            return false;
        }

        out.write(reinterpret_cast<const char *>(snapshot.occupancyWords.data()),
            static_cast<std::streamsize>(snapshot.occupancyWords.size() * sizeof(uint32_t)));
        if (!out) {
            error = "Could not write chunk occupancy data.";
            return false;
        }

        out.write(reinterpret_cast<const char *>(snapshot.materialTexels.data()),
            static_cast<std::streamsize>(snapshot.materialTexels.size() * sizeof(uint8_t)));
        if (!out) {
            error = "Could not write chunk material data.";
            return false;
        }
        ++writtenChunkCount;
    }

    header.chunkCount = writtenChunkCount;
    out.seekp(0, std::ios::beg);
    if (!writeBinary(out, header)) {
        error = "Could not update save file header.";
        return false;
    }
    out.close();
    if (!out) {
        error = "Could not finalize save file.";
        return false;
    }

    std::remove(path.c_str());
    if (std::rename(tempPath.c_str(), path.c_str()) != 0) {
        std::remove(tempPath.c_str());
        error = "Could not replace existing save file.";
        return false;
    }

    return true;
}

bool readChunkCountFromFile(const std::string &path, std::size_t &chunkCount, std::string &error)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "Could not open save file.";
        return false;
    }

    FileHeader header {};
    if (!readBinary(in, header)) {
        error = "Could not read save file header.";
        return false;
    }
    if (std::equal(std::begin(kMagic), std::end(kMagic), header.magic) == false) {
        error = "Save file magic did not match.";
        return false;
    }
    if (header.version != kVersion) {
        error = "Unsupported save file version.";
        return false;
    }
    if (header.chunkSizeVoxels == 0 || header.textureSize == 0) {
        error = "Save file chunk dimensions were invalid.";
        return false;
    }

    chunkCount = static_cast<std::size_t>(header.chunkCount);
    return true;
}

bool streamModifiersFromFile(const std::string &path,
    const std::function<bool(std::shared_ptr<IChunkModifier>)> &onModifier,
    std::string &error,
    std::size_t *outChunkCount)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "Could not open save file.";
        return false;
    }

    FileHeader header {};
    if (!readBinary(in, header)) {
        error = "Could not read save file header.";
        return false;
    }
    if (std::equal(std::begin(kMagic), std::end(kMagic), header.magic) == false) {
        error = "Save file magic did not match.";
        return false;
    }
    if (header.version != kVersion) {
        error = "Unsupported save file version.";
        return false;
    }
    if (header.chunkSizeVoxels == 0 || header.textureSize == 0) {
        error = "Save file chunk dimensions were invalid.";
        return false;
    }

    if (outChunkCount) {
        *outChunkCount = static_cast<std::size_t>(header.chunkCount);
    }

    for (uint64_t i = 0; i < header.chunkCount; ++i) {
        ChunkRecordHeader record {};
        if (!readBinary(in, record)) {
            error = "Could not read chunk record header.";
            return false;
        }

        std::vector<uint32_t> occupancyWords(record.occupancyWordCount, 0u);
        std::vector<uint8_t> materialTexels(record.materialTexelCount, 0u);
        in.read(reinterpret_cast<char *>(occupancyWords.data()),
            static_cast<std::streamsize>(occupancyWords.size() * sizeof(uint32_t)));
        if (!in) {
            error = "Could not read chunk occupancy data.";
            return false;
        }
        in.read(reinterpret_cast<char *>(materialTexels.data()),
            static_cast<std::streamsize>(materialTexels.size() * sizeof(uint8_t)));
        if (!in) {
            error = "Could not read chunk material data.";
            return false;
        }

        const ChunkPos chunkPos {record.x, record.y, record.z};
        std::shared_ptr<IChunkModifier> modifier = std::make_shared<DenseChunkModifier>(
            chunkPos,
            static_cast<int>(header.chunkSizeVoxels),
            std::move(occupancyWords),
            std::move(materialTexels));
        if (!onModifier(modifier)) {
            break;
        }
    }

    return true;
}

} // namespace WorldFileIO
