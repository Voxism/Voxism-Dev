#pragma once
#ifndef _CHUNKMANAGER_H_
#define _CHUNKMANAGER_H_

#include "Chunk.h"
#include "ChunkPos.h"
#include <unordered_map>
#include <memory>
#include <iomanip>
#include <deque>
#include <thread>
#include <atomic>
#include "modifiers/IChunkModifier.h"
#include "TerrainGenerator.h"

#include "../threads/ThreadPool.h"
#include <shared_mutex>

class ChunkManager {
    public:
        //initialize with the standard size of the world chunks.
        ChunkManager(int voxPerMeter, 
                     float chunkSizeMeters, 
                     int renderDistance, int renderHeight,
                     int generationDistance, int generationHeight);
        int renderDistance, renderHeight;
        int generationDistance, generationHeight;
        int terrainMinChunks, terrainMaxChunks;
        int voxPerMeter; // how many voxels there are per meter
        float voxSizeMeters; // how large in meters each voxel is.
        float chunkSizeMeters; // how many meters is the width of the chunk.
        int chunkSizeInts; // how many ints in the x direction of the chunk.
        int occupancyXsize, occupancyYsize, occupancyZsize; //used to iterate through chunks.

        // Function returns the chunk position for the passed position.
        ChunkPos getChunkPos(const glm::vec3& pos) const;
        ChunkPos getChunkPosForVoxel(const glm::ivec3 &voxel) const;
        glm::ivec3 worldToVoxel(const glm::vec3 &pos) const;
        glm::ivec3 worldToLocalVoxel(const glm::ivec3 &voxel) const;
        bool isVoxelOccupied(const glm::ivec3 &voxel);
        bool isAnyVoxelOccupied(const glm::ivec3 &minVoxel, const glm::ivec3 &maxVoxel) const;

        struct VoxelRaycastHit {
            bool hit = false;
            glm::ivec3 voxel = glm::ivec3(0);
            glm::ivec3 adjacent = glm::ivec3(0);
            glm::ivec3 normal = glm::ivec3(0);
            glm::vec3 position = glm::vec3(0.0f);
            float distance = 0.0f;
        };

        bool raycastVoxels(const glm::vec3 &origin,
            const glm::vec3 &direction,
            float maxDistance,
            VoxelRaycastHit &outHit);

        // std::shared_ptr<Chunk> generateChunk(ChunkPos& chunkPos);
        std::shared_ptr<Chunk> getChunk(const ChunkPos &chunkPos) const;
        const TerrainGenerator& terrain() const { return *terrainGenerator; };


        void generateChunks(glm::vec3 center);

        // Modify chunk is given the modifier that is then parsed and attached to the chunks it effects.
        // Chunks are then marked and setup for occupancy updates.
        void modifyChunks(const std::shared_ptr<IChunkModifier> &chunkMod);

        // updates the occupancy array and any color information for some
        // chunks in the update array.
        void updateChunks();

        // Determines what chunks should be drawn and then
        // binds the chunk data and draws it.
        void drawChunks(const Program& prog);
       
    private:
        // Stuct necessary for mapping an xyz of the chunk to the chunk.
        struct ChunkPosHash {
            size_t operator()(const ChunkPos& cp) const {
                size_t xh = std::hash<int>()(cp.x);
                size_t yh = std::hash<int>()(cp.y);
                size_t zh = std::hash<int>()(cp.z);
                return xh ^ (yh << 1) ^ (zh >> 1);
            }
        };
        std::unordered_map<ChunkPos, std::shared_ptr<Chunk>, ChunkPosHash> chunkMap;
        mutable std::mutex chunkMapMutex;
        std::mutex bufferQueueMutex;
        ThreadPool occupancyUpdatePool;
        ThreadPool meshUpdatePool;
        ThreadPool bufferUpdatePool;

        std::unique_ptr<TerrainGenerator> terrainGenerator;
        std::deque<std::shared_ptr<Chunk>> occupancyUpdateQueue;
        std::deque<std::shared_ptr<Chunk>> meshUpdateQueue;
        std::deque<std::shared_ptr<Chunk>> bufferUpdateQueue; //buffers must be updated on the main thread.

        template<typename Func>
        void forEachChunkInGenerationDistance(glm::vec3 center, Func func);
        void queueOccupancyUpdate(const std::shared_ptr<Chunk> &chunk);
        void queueMeshUpdate(const std::shared_ptr<Chunk> &chunk);
};

#endif
