#include "ChunkManager.h"

#include <cmath>
#include <limits>

ChunkManager::ChunkManager(
    int voxPerMeter,
    float chunkSizeMeters,
    int renderDistance, 
    int renderHeight):
    voxPerMeter(voxPerMeter), 
    chunkSizeMeters(chunkSizeMeters),
    renderDistance(renderDistance),
    renderHeight(renderHeight),
    occupancyUpdateQueue(),
    meshUpdateQueue()
{
    // Calculations for voxel and chunk sizes
    voxSizeMeters = 1.0f/voxPerMeter; // in meters.

    // chunkSizeMeters
    chunkSizeInts = glm::ceil((voxPerMeter*chunkSizeMeters)/32.0f); //1
    int chunkSizeVoxels = chunkSizeInts*32;
    this->chunkSizeMeters = chunkSizeVoxels*voxSizeMeters;

    occupancyXsize = chunkSizeInts;
    occupancyYsize = occupancyZsize = chunkSizeInts*32;
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

std::shared_ptr<Chunk> ChunkManager::generateChunk(ChunkPos& chunkPos){
    std::shared_ptr<Chunk> newChunk = std::make_shared<Chunk>(*this, chunkPos);
    // std::cout << "binding" << std::endl;
    newChunk->bindMesh();
    // std::cout << "generating" << std::endl;
    newChunk->generate();
    // std::cout << "updating" << std::endl;
    newChunk->updateMesh();
    // std::cout << "Return" << std::endl;
    return newChunk; 
}

std::shared_ptr<Chunk> ChunkManager::getOrCreateChunk(const ChunkPos &chunkPos)
{
    auto it = chunkMap.find(chunkPos);
    if (it != chunkMap.end()) {
        return it->second;
    }

    ChunkPos mutablePos = chunkPos;
    auto newChunk = generateChunk(mutablePos);
    chunkMap[chunkPos] = newChunk;
    return newChunk;
}

void ChunkManager::queueOccupancyUpdate(const std::shared_ptr<Chunk> &chunk)
{
    if (!chunk || chunk->isOccupancyQueued()) {
        return;
    }
    chunk->setOccupancyQueued(true);
    occupancyUpdateQueue.push_back(chunk);
}

void ChunkManager::queueMeshUpdate(const std::shared_ptr<Chunk> &chunk)
{
    if (!chunk || chunk->isMeshQueued()) {
        return;
    }
    chunk->setMeshQueued(true);
    meshUpdateQueue.push_back(chunk);
}

bool ChunkManager::isVoxelOccupied(const glm::ivec3 &voxel)
{
    const ChunkPos chunkPos = getChunkPosForVoxel(voxel);
    auto chunk = getOrCreateChunk(chunkPos);
    const glm::ivec3 local = worldToLocalVoxel(voxel);
    return chunk->isOccupiedLocal(local.x, local.y, local.z);
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

                auto chunk = getOrCreateChunk(chunkPos);
                chunk->queueModifier(chunkMod);
                queueOccupancyUpdate(chunk);

                for (int axis = 0; axis < 3; ++axis) {
                    for (int dir = -1; dir <= 1; dir += 2) {
                        ChunkPos neighborPos = chunkPos;
                        if (axis == 0) {
                            neighborPos.x += dir;
                        } else if (axis == 1) {
                            neighborPos.y += dir;
                        } else {
                            neighborPos.z += dir;
                        }
                        auto neighbor = getOrCreateChunk(neighborPos);
                        queueMeshUpdate(neighbor);
                    }
                }
            }
        }
    }
};

void ChunkManager::updateChunks(){
    // Update Occupancy.
    // Current updates all but could be multithreaded and limited to N updates for performance.
    if (!occupancyUpdateQueue.empty()){
        std::shared_ptr<Chunk> C = occupancyUpdateQueue.front();
        C->updateOccupancy();
        C->setOccupancyQueued(false);
        occupancyUpdateQueue.pop_front();
        queueMeshUpdate(C);
    }

    // Update Meshes
    if (!meshUpdateQueue.empty()){
        std::shared_ptr<Chunk> C = meshUpdateQueue.front();
        C->updateMesh(); //also updates the buffers.
        C->setMeshQueued(false);
        meshUpdateQueue.pop_front();
    }
}

void ChunkManager::drawChunks(const Program& prog){
    
    for (int z = -renderDistance/chunkSizeMeters; z<renderDistance/chunkSizeMeters; z++){
        for (int y = -renderHeight/chunkSizeMeters; y<renderHeight/chunkSizeMeters; y++){
            for (int x = -renderDistance/chunkSizeMeters; x<renderDistance/chunkSizeMeters; x++){
                ChunkPos chunkPos = ChunkPos{x, y, z};
                auto chunk = chunkMap.find(chunkPos);
                if (chunk == chunkMap.end()){
                    
                    float startTime = glfwGetTime();
                    chunkMap[chunkPos] = generateChunk(chunkPos);
                    float totalTime = glfwGetTime()-startTime;
                    std::cout << "ChunkGen (" << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << ") " << std::fixed << std::setprecision(4) << totalTime << "s" << std::endl;
                }
                chunkMap[chunkPos]->drawMesh(prog);
            }
        }
    }
}
