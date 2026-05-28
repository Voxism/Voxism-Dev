#pragma once
#ifndef _CHUNK_H_
#define _CHUNK_H_

#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../Program.h"
#include "ChunkPos.h"
#include <iostream>
#include <cstring>
#include <deque>
#include <memory>
#include "modifiers/IChunkModifier.h"
#include <mutex>

class ChunkManager;

class Chunk
{
    public:
        mutable std::mutex mutex;
        bool negXOccluded = false, posXOccluded = false;
        bool negYOccluded = false, posYOccluded = false; 
        bool negZOccluded = false, posZOccluded = false;
        // METHODS GENERALLY CALLED ONCE PER CHUNK
        Chunk(ChunkManager& chunkManager, ChunkPos& cp); 

        // bind buffers
        void bindMesh();
        // Generate the chunk
        void generate(std::vector<float> heightMap);
        // Update Buffers
        void updateBuffer();

        // METHODS FOR UPDATING AND DRAWING
        // Update the OccupancyInts array.
        void updateOccupancy();

        // generate the mesh from the occupancyInts 
        // updates buffers to the new mesh.
        void updateMesh();

        // Binds Mesh Buffers and Draw.
        void drawMesh(const Program& prog);
        void drawDepth(const Program& prog);

        // update OccupancyInts
        void updateChunk(float deltaTime, bool gridFill, bool floor, bool sphere);
        bool updateFrameNumber(unsigned long frameNumber);
        bool isEmpty();

        void queueModifier(const std::shared_ptr<IChunkModifier> &modifier);
        bool isOccupiedLocal(int x, int y, int z) const;
        bool isOccupiedLocalUnlocked(int x, int y, int z) const;
        uint8_t getMaterialLocalUnlocked(int x, int y, int z) const;
        void setOccupiedLocal(int x, int y, int z, bool occupied);
        void setMaterialLocal(int x, int y, int z, uint8_t materialID);

        const ChunkPos &getChunkPos() const { return cp; }
        int getLocalVoxelSizeX() const;
        int getLocalVoxelSizeY() const;
        int getLocalVoxelSizeZ() const;
        bool isOccupancyQueued() const { return occupancyQueued_; }
        bool isMeshQueued() const { return meshQueued_; }
        bool isGenerated() const { return generated_; }
        void setOccupancyQueued(bool queued) { occupancyQueued_ = queued; }
        void setMeshQueued(bool queued) { meshQueued_ = queued; }
        void setGenerated(bool generated) { generated_ = generated; }
    
    private:
        
        ChunkManager& cm;
        ChunkPos cp;
        glm::vec3 worldcp;
        std::deque<std::shared_ptr<IChunkModifier>> modifierUpdateQueue;
        unsigned long frameNumber;
        bool occupancyQueued_ = false;
        bool meshQueued_ = false;
        bool materialDirty_ = false;
        bool generated_ = false;
        glm::ivec3 materialDirtyMin_ = glm::ivec3(0);
        glm::ivec3 materialDirtyMax_ = glm::ivec3(0);

        void fillTerrain(uint32_t* occupancyInt, int x, int y, int z, const float* heightMap);

        void fillMeterGrid(uint32_t* occupancyInt, int x, int y, int z);
        void fillChunkGrid(uint32_t* occupancyInt, int x, int y, int z);
        void fillFloor(uint32_t* occupancyInt, glm::vec3* voxPosCenter, int x, int z);

        // runs the algorithm with a int to find and add one greedy face.
        void addGreedyFace(uint32_t* posYMask, int maskIndex, float xPos, float yPos, float zPos, int direction);
        // adding a quad given an anchor, start, extent, and direction.
        void addExtentQuad(float anchor, float uStart, float vStart, float uExtent, int vExtent, int dir);
        void addCubePrimitive(glm::vec3* voxPos, int vertIndex);
    
        // int is 32 bits, so we can store 32 voxels in one int.
        std::vector<uint32_t>  occupancyInts;

        // BUFFER RELATED THINGS
        int bufferUpdateMethod;

        // Used for buffer update methods 0 and 2
        GLuint vaoID; // vertex array object ID.
        GLuint vBuffID; // vertex buffer ID.
        std::vector<GLfloat>vBuff; // vertex buffer.

        GLuint eBuffID; // element buffer object ID.
        std::vector<unsigned int> eBuff; // element buffer.

        GLuint nBuffID; // normal buffer ID.
        std::vector<uint32_t>nBuff; // normal buffer.

        GLuint cTexID;
        std::vector<uint8_t> cTexData;

        // last element count uploaded to the GPU (draw uses this, not live eBuff.size())
        size_t uploadedElementCount_ = 0;

        // used for buffer update method 1
        void *vPtr; 
        void *ePtr; 
        void *nPtr;

        bool isLocalInBounds(int x, int y, int z) const;
        void markMaterialDirtyCell(int x, int y, int z);
        void uploadDirtyMaterialRegion();
};

#endif
