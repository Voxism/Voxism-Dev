#pragma once
#ifndef _CHUNK_H_
#define _CHUNK_H_

#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>
#include "../Program.h"
#include "ChunkPos.h"
#include <iostream>
#include <cstring>
#include <deque>
#include <memory>
#include "IChunkModifier.h"

class ChunkManager;

class Chunk
{
    public:
        // METHODS GENERALLY CALLED ONCE PER CHUNK
        Chunk(ChunkManager& chunkManager, ChunkPos& cp); 

        // Generate the chunk
        void generate();
        // bind buffers
        void bindMesh();

        // METHODS FOR UPDATING AND DRAWING
        // Update the OccupancyInts array.
        void updateOccupancy();

        // generate the mesh from the occupancyInts 
        // updates buffers to the new mesh.
        void updateMesh();

        // Binds Mesh Buffers and Draw.
        void drawMesh(const Program& prog);

        // update OccupancyInts
        void updateChunk(float deltaTime, bool gridFill, bool floor, bool sphere);
    
    private:
        ChunkManager& cm;
        glm::vec3 worldcp;
        std::deque<std::shared_ptr<IChunkModifier>> modifierUpdateQueue;

        // For each voxel in occupancyInts will fill an outlining grid.
        // Works with verious voxel and chunk sizes.
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
        void updateBuffer();
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

        // used for buffer update method 1
        void *vPtr; 
        void *ePtr; 
        void *nPtr;
};

#endif