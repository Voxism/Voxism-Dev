#pragma once

#include "ChunkPos.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class ChunkManager;
class IChunkModifier;

namespace WorldFileIO {

struct ChunkSnapshot {
    ChunkPos chunkPos {};
    std::vector<uint32_t> occupancyWords;
    std::vector<uint8_t> materialTexels;
};

bool saveToFile(const std::string &path, ChunkManager &chunkManager, std::string &error);
bool readChunkCountFromFile(const std::string &path, std::size_t &chunkCount, std::string &error);
bool streamModifiersFromFile(const std::string &path,
    const std::function<bool(std::shared_ptr<IChunkModifier>)> &onModifier,
    std::string &error,
    std::size_t *outChunkCount = nullptr);

} // namespace WorldFileIO
