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
#include <string>
#include <glm/glm.hpp>
#include "modifiers/IChunkModifier.h"
#include "PerlinNoise.h"
// #include "TerrainGenerator.h"
class TerrainGenerator;
#include "ChunkEdit.h"

#include "../threads/ThreadPool.h"
#include <shared_mutex>
#include <../camera/FirstPersonCamera.h>
#include <../camera/Frustum.h>

class ChunkManager {
    public:
        //initialize with the standard size of the world chunks.
        ChunkManager(int voxPerMeter, 
                     float chunkSizeMeters, 
                     int renderDistance, int renderHeight,
                     int generationDistance, int generationHeight);
        ~ChunkManager();
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
        uint8_t voxelMaterial(const glm::ivec3 &voxel) const;
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
        std::vector<ChunkPos> getGeneratedChunkPositions() const;
        std::shared_ptr<Chunk> ensureChunkExists(const ChunkPos &chunkPos);
        const TerrainGenerator& terrain() const { return *terrainGenerator; };
        bool loadFloraAssets(const std::string &resourceDirectory);


        void generateChunks(glm::vec3 center);

        // Modify chunk is given the modifier that is then parsed and attached to the chunks it effects.
        // Chunks are then marked and setup for occupancy updates.
        ChunkEditSummary modifyChunks(const std::shared_ptr<IChunkModifier> &chunkMod);

        // updates the occupancy array and any color information for some
        // chunks in the update array.
        void updateChunks();

        // Determines what chunks should be drawn and then
        // binds the chunk data and draws it.
        void drawChunks(const Program& prog, const FirstPersonCamera& fpc, const Frustum& frustum, unsigned long frameNumber);
        void drawChunksForShadow(const Program& prog,
                                 const glm::vec3 &center,
                                 const Frustum& lightFrustum,
                                 float radiusMeters);

        void markShadowMapsDirty();
        bool isShadowMapsDirty() const;
        void clearShadowMapsDirty();
        bool hasPendingBufferUpdates() const;
       
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
        mutable std::mutex bufferQueueMutex;

        PerlinNoise noise;
        std::shared_ptr<TerrainGenerator> terrainGenerator;
        std::deque<std::shared_ptr<Chunk>> occupancyUpdateQueue;
        std::deque<std::shared_ptr<Chunk>> meshUpdateQueue;
        std::deque<std::shared_ptr<Chunk>> bufferUpdateQueue; //buffers must be updated on the main thread.
        std::atomic<bool> shadowMapsDirty_{true};

        // Thread pools MUST be declared last so they are destroyed FIRST.
        // Their destructors join worker threads; those threads dereference
        // `this` (including terrainGenerator and noise). If pools were
        // declared earlier they would be destroyed AFTER the data they read,
        // producing a use-after-free (e.g. on PerlinNoise::perm_).
        ThreadPool occupancyUpdatePool;
        ThreadPool meshUpdatePool;
        ThreadPool bufferUpdatePool;
        ThreadPool chunkGenerationPool;

        template<typename Func>
        void forEachChunkInGenerationDistance(glm::vec3 center, Func func);
        void queueOccupancyUpdate(const std::shared_ptr<Chunk> &chunk);
        void queueMeshUpdate(const std::shared_ptr<Chunk> &chunk);
        void collectDeletedVoxels(const IChunkModifier &chunkMod,
            const glm::ivec3 &minVoxel,
            const glm::ivec3 &maxVoxel,
            ChunkEditSummary &summary) const;
};

#endif
