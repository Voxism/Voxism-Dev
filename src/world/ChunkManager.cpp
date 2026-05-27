#include "ChunkManager.h"

#include <cmath>
#include <limits>

#include "VoxelMath.h"

ChunkManager::ChunkManager(
    int voxPerMeter,
    float chunkSizeMeters,
    int renderDistance, 
    int renderHeight,
    int generationDistance,
    int generationHeight):
    voxPerMeter(voxPerMeter), 
    chunkSizeMeters(chunkSizeMeters),
    renderDistance(renderDistance),
    renderHeight(renderHeight),
    generationDistance(generationDistance),
    generationHeight(generationHeight),
    terrainMinChunks(0),
    terrainMaxChunks(0),
    occupancyUpdateQueue(),
    meshUpdateQueue(),
    occupancyUpdatePool(4),
    meshUpdatePool(4),
    bufferUpdatePool(4)
{
    // Calculations for voxel and chunk sizes
    voxSizeMeters = 1.0f/voxPerMeter; // in meters.

    // chunkSizeMeters
    chunkSizeInts = glm::ceil((voxPerMeter*chunkSizeMeters)/32.0f); //1
    int chunkSizeVoxels = chunkSizeInts*32;
    this->chunkSizeMeters = chunkSizeVoxels*voxSizeMeters;

    occupancyXsize = chunkSizeInts;
    occupancyYsize = occupancyZsize = chunkSizeInts*32;
    terrainMinChunks = static_cast<int>(-renderHeight / this->chunkSizeMeters);
    terrainMaxChunks = static_cast<int>(renderHeight / this->chunkSizeMeters);
    terrainGenerator = std::make_unique<TerrainGenerator>(terrainMinChunks, terrainMaxChunks, this->chunkSizeMeters, 1337u);
};

ChunkPos ChunkManager::getChunkPos(const glm::vec3& pos) const{
    // This math keeps the chunk position rounded down even if negative.
    ChunkPos chunkPos = {
        static_cast<int>(glm::floor(pos.x/chunkSizeMeters)),
        static_cast<int>(glm::floor(pos.y/chunkSizeMeters)),
        static_cast<int>(glm::floor(pos.z/chunkSizeMeters)),
    };
    return chunkPos;
};

ChunkPos ChunkManager::getChunkPosForVoxel(const glm::ivec3 &voxel) const
{
    const int chunkSizeVoxels = chunkSizeInts * 32;
    ChunkPos chunkPos = {
        static_cast<int>(glm::floor(static_cast<float>(voxel.x) / chunkSizeVoxels)),
        static_cast<int>(glm::floor(static_cast<float>(voxel.y) / chunkSizeVoxels)),
        static_cast<int>(glm::floor(static_cast<float>(voxel.z) / chunkSizeVoxels)),
    };
    return chunkPos;
}

glm::ivec3 ChunkManager::worldToVoxel(const glm::vec3 &pos) const
{
    return glm::ivec3(
        static_cast<int>(glm::floor(pos.x / voxSizeMeters)),
        static_cast<int>(glm::floor(pos.y / voxSizeMeters)),
        static_cast<int>(glm::floor(pos.z / voxSizeMeters))
    );
}

glm::ivec3 ChunkManager::worldToLocalVoxel(const glm::ivec3 &voxel) const
{
    const int chunkSizeVoxels = chunkSizeInts * 32;
    const ChunkPos chunkPos = getChunkPosForVoxel(voxel);
    return glm::ivec3(
        voxel.x - chunkPos.x * chunkSizeVoxels,
        voxel.y - chunkPos.y * chunkSizeVoxels,
        voxel.z - chunkPos.z * chunkSizeVoxels
    );
}

std::shared_ptr<Chunk> ChunkManager::getChunk(const ChunkPos &chunkPos) const
{
    auto it = chunkMap.find(chunkPos);
    if (it != chunkMap.end()) {
        return it->second;
    }
    return nullptr;
}

void ChunkManager::queueOccupancyUpdate(const std::shared_ptr<Chunk> &chunk)
{
    if (!chunk || chunk->isOccupancyQueued()) {
        return;
    }
    // Add chunk to the occupancy update queue thread pool.
    // will update the texture and occupancy for meshupdate to work with.
    occupancyUpdatePool.enqueue(
        [this, chunk]{
            std::lock_guard<std::mutex> lock1(chunk->mutex);
            chunk->setOccupancyQueued(true);
            chunk->updateOccupancy();
            chunk->setOccupancyQueued(false);
            queueMeshUpdate(chunk);
        }
    );
}

void ChunkManager::queueMeshUpdate(const std::shared_ptr<Chunk> &chunk)
{
    if (!chunk || chunk->isMeshQueued()) {
        return;
    }
    // Add chunk the mesh update queue thread pool
    // will generate the mesh for the chunk.
    meshUpdatePool.enqueue(
        [this, chunk]{
            {
                std::lock_guard<std::mutex> lock1(chunk->mutex);
                chunk->setMeshQueued(true);
                chunk->updateMesh();
                chunk->setMeshQueued(false);
            }
            {
                std::lock_guard<std::mutex> lock(bufferQueueMutex);
                bufferUpdateQueue.push_back(chunk);
            }
        }
    );
}

bool ChunkManager::isVoxelOccupied(const glm::ivec3 &voxel)
{
    const ChunkPos chunkPos = getChunkPosForVoxel(voxel);
    auto chunk = getChunk(chunkPos);
    if (!chunk) {
        return false;
    }
    const glm::ivec3 local = worldToLocalVoxel(voxel);
    return chunk->isOccupiedLocal(local.x, local.y, local.z);
}

bool ChunkManager::isAnyVoxelOccupied(const glm::ivec3 &minVoxel, const glm::ivec3 &maxVoxel) const
{
    const int chunkSizeVoxels = chunkSizeInts * 32;
    const ChunkPos minChunk = {
        floorDiv(minVoxel.x, chunkSizeVoxels),
        floorDiv(minVoxel.y, chunkSizeVoxels),
        floorDiv(minVoxel.z, chunkSizeVoxels)
    };
    const ChunkPos maxChunk = {
        floorDiv(maxVoxel.x, chunkSizeVoxels),
        floorDiv(maxVoxel.y, chunkSizeVoxels),
        floorDiv(maxVoxel.z, chunkSizeVoxels)
    };

    for (int chunkZ = minChunk.z; chunkZ <= maxChunk.z; ++chunkZ) {
        for (int chunkY = minChunk.y; chunkY <= maxChunk.y; ++chunkY) {
            for (int chunkX = minChunk.x; chunkX <= maxChunk.x; ++chunkX) {
                const ChunkPos chunkPos {chunkX, chunkY, chunkZ};
                const auto chunk = getChunk(chunkPos);
                if (!chunk) {
                    continue;
                }

                const glm::ivec3 chunkVoxelMin(
                    chunkX * chunkSizeVoxels,
                    chunkY * chunkSizeVoxels,
                    chunkZ * chunkSizeVoxels);
                const glm::ivec3 chunkVoxelMax = chunkVoxelMin + glm::ivec3(chunkSizeVoxels - 1);
                const glm::ivec3 clippedMin = glm::max(minVoxel, chunkVoxelMin);
                const glm::ivec3 clippedMax = glm::min(maxVoxel, chunkVoxelMax);

                const int localMinX = clippedMin.x - chunkVoxelMin.x;
                const int localMinY = clippedMin.y - chunkVoxelMin.y;
                const int localMinZ = clippedMin.z - chunkVoxelMin.z;
                const int localMaxX = clippedMax.x - chunkVoxelMin.x;
                const int localMaxY = clippedMax.y - chunkVoxelMin.y;
                const int localMaxZ = clippedMax.z - chunkVoxelMin.z;

                for (int localZ = localMinZ; localZ <= localMaxZ; ++localZ) {
                    for (int localY = localMinY; localY <= localMaxY; ++localY) {
                        for (int localX = localMinX; localX <= localMaxX; ++localX) {
                            if (chunk->isOccupiedLocal(localX, localY, localZ)) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    return false;
}

bool ChunkManager::raycastVoxels(const glm::vec3 &origin,
    const glm::vec3 &direction,
    float maxDistance,
    VoxelRaycastHit &outHit)
{
    const float dirLength = glm::length(direction);
    if (dirLength <= 0.0001f) {
        return false;
    }

    const glm::vec3 rayDir = direction / dirLength;
    glm::vec3 voxelPos = origin / voxSizeMeters;
    glm::ivec3 voxel = glm::floor(voxelPos);

    glm::ivec3 step(0);
    glm::vec3 tMax(0.0f);
    glm::vec3 tDelta(0.0f);
    const float infinity = std::numeric_limits<float>::infinity();

    for (int axis = 0; axis < 3; axis++) {
        if (rayDir[axis] > 0.0f) {
            step[axis] = 1;
            tMax[axis] = ((static_cast<float>(voxel[axis] + 1) - voxelPos[axis]) / rayDir[axis]) * voxSizeMeters;
            tDelta[axis] = (1.0f / rayDir[axis]) * voxSizeMeters;
        } else if (rayDir[axis] < 0.0f) {
            step[axis] = -1;
            tMax[axis] = ((voxelPos[axis] - static_cast<float>(voxel[axis])) / -rayDir[axis]) * voxSizeMeters;
            tDelta[axis] = (1.0f / -rayDir[axis]) * voxSizeMeters;
        } else {
            step[axis] = 0;
            tMax[axis] = infinity;
            tDelta[axis] = infinity;
        }
    }

    glm::ivec3 lastNormal(0);
    float traveled = 0.0f;
    const int maxSteps = static_cast<int>(glm::ceil(maxDistance / voxSizeMeters)) + 2;

    for (int stepIndex = 0; stepIndex < maxSteps; ++stepIndex) {
        if (isVoxelOccupied(voxel)) {
            outHit.hit = true;
            outHit.voxel = voxel;
            outHit.normal = lastNormal;
            outHit.adjacent = voxel + lastNormal;
            outHit.distance = traveled;
            outHit.position = origin + rayDir * traveled;
            return true;
        }

        if (tMax.x <= tMax.y && tMax.x <= tMax.z) {
            traveled = tMax.x;
            voxel.x += step.x;
            tMax.x += tDelta.x;
            lastNormal = glm::ivec3(-step.x, 0, 0);
        } else if (tMax.y <= tMax.z) {
            traveled = tMax.y;
            voxel.y += step.y;
            tMax.y += tDelta.y;
            lastNormal = glm::ivec3(0, -step.y, 0);
        } else {
            traveled = tMax.z;
            voxel.z += step.z;
            tMax.z += tDelta.z;
            lastNormal = glm::ivec3(0, 0, -step.z);
        }

        if (traveled > maxDistance) {
            break;
        }
    }

    return false;
}

void ChunkManager::modifyChunks(const std::shared_ptr<IChunkModifier> &chunkMod){
    if (!chunkMod) {
        return;
    }

    ChunkPos minChunk {};
    ChunkPos maxChunk {};
    chunkMod->getTouchedChunkBounds(minChunk, maxChunk);

    for (int z = minChunk.z; z <= maxChunk.z; ++z) {
        for (int y = minChunk.y; y <= maxChunk.y; ++y) {
            for (int x = minChunk.x; x <= maxChunk.x; ++x) {
                ChunkPos chunkPos {x, y, z};
                if (!chunkMod->effectsChunk(chunkPos)) {
                    continue;
                }

                auto chunk = getChunk(chunkPos);
                if (!chunk) {
                    continue;
                }
                chunk->queueModifier(chunkMod);
                queueOccupancyUpdate(chunk);
            }
        }
    }
};

// Called by the main thread.
void ChunkManager::updateChunks(){
    while (true){
        std::shared_ptr<Chunk> C;
        {
            std::lock_guard<std::mutex> lock(bufferQueueMutex);
            if (bufferUpdateQueue.empty()){
                break;
            }
            C = bufferUpdateQueue.front();
            bufferUpdateQueue.pop_front();
        }

        {
            std::unique_lock<std::mutex> lock(C->mutex);
            C->updateBuffer();
        }
    }
}

template<typename Func>
void ChunkManager::forEachChunkInGenerationDistance(glm::vec3 center, Func func){
    int x = center.x/chunkSizeMeters;
    int z = center.z/chunkSizeMeters;

    // checks each chunk in a vertical slice. if it doesn't exist then generate it.
    auto forSlice = [&](int x, int z, auto func){
        int height = generationHeight/chunkSizeMeters; 
        for (int y=-height; y<height; y++){
            ChunkPos chunkPos = ChunkPos{x, y, z};
            func(chunkPos);
        }
    };
    
    // Attempt to generate the current chunk.
    forSlice(x, z, func);
    // Generate chunks in concentric rings around the player.
    int maxDistance = renderDistance/chunkSizeInts;
    for (int distance = 1; distance < maxDistance; distance++){
        int dx = -distance;
        int dz = -distance;
        // -z wall
        while(dx<distance){
            dx++;
            forSlice(x+dx, z+dz, func);
        }
        // +x wall
        while(dz<distance){
            dz++;
            forSlice(x+dx, z+dz, func);
        }
        // +Z wall
        while(dx>-distance){
            dx--;
            forSlice(x+dx, z+dz, func);
        }
        // -x wall
        while(dz>-distance){
            dz--;
            forSlice(x+dx, z+dz, func);
        }
    }
}

void ChunkManager::generateChunks(glm::vec3 center){
    // Create Chunks and Bind Chunks (main Thread)
    forEachChunkInGenerationDistance(
        center, 
        [&](ChunkPos chunkPos){
            auto chunk = chunkMap.find(chunkPos);
            if (chunk == chunkMap.end()){
                // Make, bind, insert.
                std::shared_ptr<Chunk> newChunk = std::make_shared<Chunk>(*this, chunkPos);
                std::lock_guard<std::mutex> lock(newChunk->mutex);
                newChunk->bindMesh();
                chunkMap[chunkPos] = newChunk;
            }
        }
    );
    // generate Chunks and add to bufferUpdateQueue (worker thread)
    std::thread([&, center]() {
        forEachChunkInGenerationDistance(
            center, 
            [&](ChunkPos chunkPos){
                auto chunk = chunkMap.find(chunkPos);
                if (chunk != chunkMap.end()){
                    if (!chunk->second->isGenerated()){
                        std::lock_guard<std::mutex> lock(chunk->second->mutex);
                        float startTime = glfwGetTime();
                        chunk->second->generate();
                        chunk->second->updateMesh();
                        chunk->second->setGenerated(true);
                        float totalTime = glfwGetTime()-startTime;
                        // std::cout << "ChunkGen (" << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << ") " << std::fixed << std::setprecision(4) << totalTime << "s" << std::endl;
                        bufferUpdateQueue.push_back(chunk->second);
                    }
                }
            }
        );
    }).detach();
}

void ChunkManager::drawChunks(const Program& prog){
    
    for (int z = -renderDistance/chunkSizeMeters; z<renderDistance/chunkSizeMeters; z++){
        for (int y = terrainMinChunks; y<=terrainMaxChunks; y++){
            for (int x = -renderDistance/chunkSizeMeters; x<renderDistance/chunkSizeMeters; x++){
                ChunkPos chunkPos = ChunkPos{x, y, z};
                auto chunk = chunkMap.find(chunkPos);

                if (chunk != chunkMap.end() && chunk->second->isGenerated()){
                    std::lock_guard<std::mutex> lock(chunk->second->mutex);
                    chunkMap[chunkPos]->drawMesh(prog);
                }
            }
        }
    }
}
