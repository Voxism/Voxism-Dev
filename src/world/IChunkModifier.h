
#pragma once
#ifndef _CHUNKMODIFIER_H_
#define _CHUNKMODIFIER_H_

#include <glm/glm.hpp>
#include "ChunkPos.h"


class IChunkModifier {
    public:
        // Given an Occupancy int and it's x,y,z fill in/remove the correct bits
        virtual void checkAndFill(uint32_t& occupancyInt, int x, int y, int z) = 0;

        virtual bool effectsChunk(ChunkPos cp);
};

#endif