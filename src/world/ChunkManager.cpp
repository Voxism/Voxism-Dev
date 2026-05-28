#include "ChunkManager.h"
#include "TerrainGenerator.h"

#include <chrono>
#include <cmath>
#include <limits>

#include "VoxelMath.h"
#include "Octave.h"

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
    noise(53310u),
    occupancyUpdateQueue(),
    meshUpdateQueue(),
    occupancyUpdatePool(4),
    meshUpdatePool(4),
    bufferUpdatePool(4),
    chunkGenerationPool(2)
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
    std::vector<Octave> octaves;
    octaves.push_back(Octave{0.005f, 30.0f, 4.0f});
    octaves.push_back(Octave{0.04f, 5.0f, 1.0f});
    octaves.push_back(Octave{0.04f, 5.0f, 1.0f});
    octaves.push_back(Octave{0.02f, 7.0f, 1.0f});
    octaves.push_back(Octave{0.3f, 0.35f, 0.5f});
    octaves.push_back(Octave{0.6f, 0.15f, 0.5f});
    octaves.push_back(Octave{3.0f, 0.07f, 0.0f});
    terrainGenerator = std::make_unique<TerrainGenerator>(terrainMinChunks, terrainMaxChunks, this->chunkSizeMeters, octaves, 1337u);
};
ChunkManager::~ChunkManager() = default;

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
    std::lock_guard<std::mutex> lockMap(chunkMapMutex);
    auto it = chunkMap.find(chunkPos);
    if (it != chunkMap.end()) {
        return it->second;
    }
    return nullptr;
}

void ChunkManager::queueOccupancyUpdate(const std::shared_ptr<Chunk> &chunk)
{
    if (!chunk) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(chunk->mutex);
        if (chunk->isOccupancyQueued()) {
            return;
        }
        chunk->setOccupancyQueued(true);
    }

    occupancyUpdatePool.enqueue(
        [this, chunk]{
            {
                std::lock_guard<std::mutex> lock1(chunk->mutex);
                chunk->updateOccupancy();
                chunk->setOccupancyQueued(false);
            }
            queueMeshUpdate(chunk);
        }
    );
}

void ChunkManager::queueMeshUpdate(const std::shared_ptr<Chunk> &chunk)
{
    if (!chunk) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(chunk->mutex);
        if (chunk->isMeshQueued()) {
            return;
        }
        chunk->setMeshQueued(true);
    }

    meshUpdatePool.enqueue(
        [this, chunk]{
            {
                std::lock_guard<std::mutex> lock1(chunk->mutex);
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

uint8_t ChunkManager::voxelMaterial(const glm::ivec3 &voxel) const
{
    const ChunkPos chunkPos = getChunkPosForVoxel(voxel);
    auto chunk = getChunk(chunkPos);
    if (!chunk) {
        return 0;
    }

    const glm::ivec3 local = worldToLocalVoxel(voxel);
    std::lock_guard<std::mutex> lock(chunk->mutex);
    return chunk->getMaterialLocalUnlocked(local.x, local.y, local.z);
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

void ChunkManager::collectDeletedVoxels(const IChunkModifier &chunkMod,
    const glm::ivec3 &minVoxel,
    const glm::ivec3 &maxVoxel,
    ChunkEditSummary &summary) const
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

    for (int z = minChunk.z; z <= maxChunk.z; ++z) {
        for (int y = minChunk.y; y <= maxChunk.y; ++y) {
            for (int x = minChunk.x; x <= maxChunk.x; ++x) {
                const ChunkPos chunkPos {x, y, z};
                if (!chunkMod.effectsChunk(chunkPos)) {
                    continue;
                }

                const auto chunk = getChunk(chunkPos);
                if (!chunk) {
                    continue;
                }

                std::lock_guard<std::mutex> lock(chunk->mutex);
                if (!chunk->isGenerated()) {
                    continue;
                }

                const glm::ivec3 chunkVoxelMin(
                    chunkPos.x * chunkSizeVoxels,
                    chunkPos.y * chunkSizeVoxels,
                    chunkPos.z * chunkSizeVoxels);
                const glm::ivec3 chunkVoxelMax = chunkVoxelMin + glm::ivec3(chunkSizeVoxels - 1);
                const glm::ivec3 clippedMin = glm::max(minVoxel, chunkVoxelMin);
                const glm::ivec3 clippedMax = glm::min(maxVoxel, chunkVoxelMax);

                for (int worldZ = clippedMin.z; worldZ <= clippedMax.z; ++worldZ) {
                    for (int worldY = clippedMin.y; worldY <= clippedMax.y; ++worldY) {
                        for (int worldX = clippedMin.x; worldX <= clippedMax.x; ++worldX) {
                            const glm::ivec3 voxel(worldX, worldY, worldZ);
                            if (!chunkMod.affectsWorldVoxel(voxel)) {
                                continue;
                            }

                            const int localX = worldX - chunkVoxelMin.x;
                            const int localY = worldY - chunkVoxelMin.y;
                            const int localZ = worldZ - chunkVoxelMin.z;
                            if (chunk->isOccupiedLocalUnlocked(localX, localY, localZ)) {
                                ChunkEditSummary::DeletedVoxel deletedVoxel;
                                deletedVoxel.voxel = voxel;
                                deletedVoxel.materialID = chunk->getMaterialLocalUnlocked(localX, localY, localZ);
                                summary.deletedVoxels.push_back(deletedVoxel);
                            }
                        }
                    }
                }
            }
        }
    }
}

ChunkEditSummary ChunkManager::modifyChunks(const std::shared_ptr<IChunkModifier> &chunkMod){
    ChunkEditSummary summary;
    if (!chunkMod) {
        return summary;
    }

    summary.valid = true;
    summary.action = chunkMod->isFillOperation() ? ChunkEditAction::Fill : ChunkEditAction::Delete;
    chunkMod->getAffectedVoxelBounds(summary.minVoxel, summary.maxVoxel);
    if (summary.action == ChunkEditAction::Delete) {
        collectDeletedVoxels(*chunkMod, summary.minVoxel, summary.maxVoxel, summary);
        summary.affectedVoxelCount = summary.deletedVoxels.size();
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
                {
                    // std::lock_guard<std::mutex> lock(chunk->mutex);
                    if (!chunk->isGenerated()) {
                        chunk->queueModifier(chunkMod);
                        continue;
                    }
                }
                chunk->queueModifier(chunkMod);
                queueOccupancyUpdate(chunk);
            }
        }
    }
    return summary;
}

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
            C->setGenerated(true);
        }
    }
}

template<typename Func>
void ChunkManager::forEachChunkInGenerationDistance(glm::vec3 center, Func func){
    int x = glm::floor(center.x/chunkSizeMeters);
    int z = glm::floor(center.z/chunkSizeMeters);

    int maxDistance = generationDistance/chunkSizeInts;
    for (int distance = 0; distance < maxDistance; distance++){
        for (int dz = -distance; dz <= distance; dz++) {
        for (int dx = -distance; dx <= distance; dx++) {
    
            bool edge =
                dx == -distance ||
                dx == distance ||
                dz == -distance ||
                dz == distance;
    
            if (edge)
                func(x + dx, z + dz);
        }}
    }
}

void ChunkManager::generateChunks(glm::vec3 center){

    // Create Chunks and Bind Chunks (main Thread)
    forEachChunkInGenerationDistance(
        center, 
        [&](int x, int z)
        {
            int height = generationHeight/chunkSizeMeters; 
            for (int y=-height; y<height; y++){
                ChunkPos chunkPos = ChunkPos{x, y, z};
                std::lock_guard<std::mutex> lockMap(chunkMapMutex);
                auto chunk = chunkMap.find(chunkPos);
                if (chunk == chunkMap.end())
                {
                    // Make, bind, insert.
                    std::shared_ptr<Chunk> newChunk = std::make_shared<Chunk>(*this, chunkPos);
                    std::lock_guard<std::mutex> lock(newChunk->mutex);
                    newChunk->bindMesh();
                    chunkMap[chunkPos] = newChunk;
                }
            }
        }
    );
    // generate Chunks and add to bufferUpdateQueue (worker thread)
    std::thread([&, center]() {
        forEachChunkInGenerationDistance(
            center, 
            [&](int x, int z)
            {   
                chunkGenerationPool.enqueue([&, x, z]{
                    int height = generationHeight/chunkSizeMeters; 
                    // Get Height Map
                    const int sideVox = chunkSizeInts * 32;
                    std::vector<float> heightMap(sideVox * sideVox, 0.0f);
                    for (int zVox = 0; zVox < sideVox; ++zVox) {
                        const float worldZ = z*chunkSizeMeters + static_cast<float>(zVox) * voxSizeMeters;
                        for (int xVox = 0; xVox < sideVox; ++xVox) {
                            const float worldX = x*chunkSizeMeters + static_cast<float>(xVox) * voxSizeMeters;
                            heightMap[zVox * sideVox + xVox] = terrain().heightAt(worldX, worldZ);
                            // std::cout << terrain().heightAt(0.01245f,1.324f) << std::endl;
                        }
                    }
                    
                    
                    // For each chunk use height map
                    for (int y=-height; y<height; y++){
                        
                        ChunkPos chunkPos = ChunkPos{x, y, z};
                        std::shared_ptr<Chunk> chunkPtr = nullptr;
                        {
                            std::lock_guard<std::mutex> lockMap(chunkMapMutex);
                            auto chunk = chunkMap.find(chunkPos);
                            if (chunk != chunkMap.end()){
                                chunkPtr = chunk->second;
                            }
                        }
                        if (chunkPtr != nullptr && !chunkPtr->isGenerated())
                        {
                            {
                                std::lock_guard<std::mutex> lock(chunkPtr->mutex);
                                const auto startTime = std::chrono::steady_clock::now();
                                chunkPtr->generate(heightMap);
                                chunkPtr->updateOccupancy();
                                chunkPtr->updateMesh();
                                const float totalTime = std::chrono::duration<float>(
                                    std::chrono::steady_clock::now() - startTime).count();
                                (void)totalTime;
                                // std::cout << "ChunkGen (" << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << ") " << std::fixed << std::setprecision(4) << totalTime << "s" << std::endl;
                            }
                            {
                                std::lock_guard<std::mutex> lock(bufferQueueMutex);
                                bufferUpdateQueue.push_back(chunkPtr);
                            }
                        }
                    }

                    // After chunks are generated add rocks and features.
                    for (int i = 0; i < 2; i++){
                        // A random Coordinate
                        float chunkX = (rand()/(float)RAND_MAX)*chunkSizeMeters;
                        float chunkZ = (rand()/(float)RAND_MAX)*chunkSizeMeters;
                        float worldX = x*chunkSizeMeters+chunkX;
                        float worldZ = z*chunkSizeMeters+chunkZ;
                        float frequency = 0.04;
                        float regionalChance = glm::max(0.0f, (noise.noise2D(worldX*frequency, worldZ*frequency)+3)/4);
                        float individualChance = (rand()/(float)RAND_MAX);
                        if (regionalChance * individualChance > 0.70){
                        // if (regionalChance * individualChance > 0.30){
                            std::vector<shared_ptr<IChunkModifier>> rocks = terrainGenerator->generateRocks(worldX, heightMap[glm::floor(chunkZ*voxPerMeter) * sideVox + glm::floor(chunkX*voxPerMeter)], worldZ, 3, *this);
                            for (auto rock : rocks){
                                modifyChunks(rock);
                            }
                        }
                    }
                });
            }
        );
    }).detach();
}

void ChunkManager::drawChunks(const Program& prog, const FirstPersonCamera &fpc, const Frustum& frustum, unsigned long frameNumber){
    int numberOfDraws = 0;
    glm::vec3 camPos = fpc.GetCameraPos();
    ChunkPos cp = ChunkPos{
    (int) glm::floor(camPos.x/chunkSizeMeters),
    (int) glm::floor(camPos.y/chunkSizeMeters),
    (int) glm::floor(camPos.z/chunkSizeMeters)
    };
    // std::cout<< "FrameNumber: " << frameNumber << std::endl;
    // std::cout<< "ChunkPos Start: x=" << cp.x << " y=" << cp.y << " z=" << cp.z << std::endl;
    
    std::function<void(ChunkPos)> drawChunk = [&](ChunkPos cp){
        // Get the chunk
        std::shared_ptr<Chunk> chunkPtr = nullptr;
        {
            std::lock_guard<std::mutex> lockMap(chunkMapMutex);
            auto chunk = chunkMap.find(cp);
            if (chunk != chunkMap.end()){
                chunkPtr = chunk->second;
            }
        }
        
        if (chunkPtr != nullptr && chunkPtr->isGenerated()){
            // checks if visited this frame.
            bool differentFrame = chunkPtr->updateFrameNumber(frameNumber);

            if (//chunk hasn't been rendered this frame
                differentFrame && 
                // Chunk is within render distance from FPVcamera
                glm::pow(glm::pow(cp.x*chunkSizeMeters+0.5*chunkSizeMeters-camPos.x, 2) + glm::pow(cp.z*chunkSizeMeters+0.5*chunkSizeMeters-camPos.z, 2), 0.5) < renderDistance &&
                // Chunk is within the View Frustum
                !frustum.cullCube(glm::vec3(cp.x*chunkSizeMeters, cp.y*chunkSizeMeters, cp.z*chunkSizeMeters), glm::vec3((cp.x+1)*chunkSizeMeters, (cp.y+1)*chunkSizeMeters, (cp.z+1)*chunkSizeMeters))

            ){
                {
                    std::lock_guard<std::mutex> lock(chunkPtr->mutex);
                    if (!chunkPtr->isEmpty()) 
                    {
                        chunkPtr->drawMesh(prog);
                        numberOfDraws++;
                    }
                }
                        
                if (!chunkPtr->negXOccluded){drawChunk(ChunkPos{cp.x-1, cp.y, cp.z});}
                if (!chunkPtr->posXOccluded){drawChunk(ChunkPos{cp.x+1, cp.y, cp.z});}
                if (!chunkPtr->negYOccluded){drawChunk(ChunkPos{cp.x, cp.y-1, cp.z});}
                if (!chunkPtr->posYOccluded){drawChunk(ChunkPos{cp.x, cp.y+1, cp.z});}
                if (!chunkPtr->negZOccluded){drawChunk(ChunkPos{cp.x, cp.y, cp.z-1});}
                if (!chunkPtr->posZOccluded){drawChunk(ChunkPos{cp.x, cp.y, cp.z+1});}
            }
        }
    };

    // Try adjacent chunk positions as seeds in case the first one doesn't exist is it outside of frustum.
    for (int x=-1; x<1; x++){
        for (int y=-1; y<1; y++){
            for (int z=-1; z<1; z++){
                drawChunk(ChunkPos{cp.x+x, cp.y+y, cp.z+z});
            }
        }
    }

    // render distance 32 and generation height 32.
    // Without Culling:         416
    // Culling empty Chunks:    261
    // Culling Occluded Chunks:  82
    // VFC:                     
    // std::cout << "Number of Draws: " << numberOfDraws << std::endl;
}

void ChunkManager::drawChunksForShadow(const Program& prog,
    const glm::vec3 &center,
    const Frustum& lightFrustum,
    float radiusMeters)
{
    (void)lightFrustum;
    if (radiusMeters <= 0.0f) {
        return;
    }

    const ChunkPos minChunk = getChunkPos(center - glm::vec3(radiusMeters));
    const ChunkPos maxChunk = getChunkPos(center + glm::vec3(radiusMeters));

    for (int z = minChunk.z; z <= maxChunk.z; ++z) {
        for (int y = minChunk.y; y <= maxChunk.y; ++y) {
            for (int x = minChunk.x; x <= maxChunk.x; ++x) {
                const ChunkPos cp{x, y, z};
                std::shared_ptr<Chunk> chunkPtr = nullptr;
                {
                    std::lock_guard<std::mutex> lockMap(chunkMapMutex);
                    auto it = chunkMap.find(cp);
                    if (it != chunkMap.end()) {
                        chunkPtr = it->second;
                    }
                }

                if (!chunkPtr || !chunkPtr->isGenerated()) {
                    continue;
                }

                const glm::vec3 chunkMin(
                    cp.x * chunkSizeMeters,
                    cp.y * chunkSizeMeters,
                    cp.z * chunkSizeMeters);
                const glm::vec3 chunkMax = chunkMin + glm::vec3(chunkSizeMeters);
                const glm::vec3 chunkCenter = (chunkMin + chunkMax) * 0.5f;
                const float chunkRadius = glm::length(glm::vec3(chunkSizeMeters * 0.5f));
                const glm::vec3 toChunk = chunkCenter - center;
                const float expandedRadius = radiusMeters + chunkRadius;
                if (glm::dot(toChunk, toChunk) > expandedRadius * expandedRadius) {
                    continue;
                }
                std::lock_guard<std::mutex> lock(chunkPtr->mutex);
                if (!chunkPtr->isEmpty()) {
                    chunkPtr->drawDepth(prog);
                }
            }
        }
    }
}
