#include "ChunkManager.h"

ChunkManager::ChunkManager(
    int voxPerMeter,
    float chunkSizeMeters,
    int renderDistance, 
    int renderHeight):
    voxPerMeter(voxPerMeter), 
    chunkSizeMeters(chunkSizeMeters),
    renderDistance(renderDistance),
    renderHeight(renderHeight),
    terrainMinChunks(0),
    terrainMaxChunks(0),
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
    terrainMinChunks = static_cast<int>(-renderHeight / this->chunkSizeMeters);
    terrainMaxChunks = static_cast<int>(renderHeight / this->chunkSizeMeters);
    terrainGenerator = std::make_unique<TerrainGenerator>(terrainMinChunks, terrainMaxChunks, this->chunkSizeMeters, 1337u);
};

ChunkPos ChunkManager::getChunkPos(glm::vec3& pos){
    // This math keeps the chunk position rounded down even if negative.
    ChunkPos chunkPos = {
        static_cast<int>(glm::floor(pos.x/chunkSizeMeters)),
        static_cast<int>(glm::floor(pos.y/chunkSizeMeters)),
        static_cast<int>(glm::floor(pos.z/chunkSizeMeters)),
    };
    return chunkPos;
};

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

void ChunkManager::modifyChunks(IChunkModifier& chunkMod){
    // Figure out which chunks the modifer effects.
    // Add the modifier to the chunk.
    // Add chunk to the updateQueue (possibly front if close to camera and back if far from camera)
};

void ChunkManager::updateChunks(){
    // Update Occupancy.
    // Current updates all but could be multithreaded and limited to N updates for performance.
    if (!occupancyUpdateQueue.empty()){
        std::shared_ptr<Chunk> C = occupancyUpdateQueue.front();
        C->updateOccupancy();
        occupancyUpdateQueue.pop_front();
        meshUpdateQueue.push_back(C);
    }

    // Update Meshes
    if (!meshUpdateQueue.empty()){
        std::shared_ptr<Chunk> C = meshUpdateQueue.front();
        C->updateMesh(); //also updates the buffers.
        meshUpdateQueue.pop_front();
    }
}

void ChunkManager::drawChunks(const Program& prog){
    
    for (int z = -renderDistance/chunkSizeMeters; z<renderDistance/chunkSizeMeters; z++){
        for (int y = terrainMinChunks; y<=terrainMaxChunks; y++){
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

