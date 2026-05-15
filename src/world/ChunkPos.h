#pragma once
#ifndef _CHUNKPOS_H_
#define _CHUNKPOS_H_

// Stuct used to store the chunk position relative to other chunk positions.
struct ChunkPos {
    int x, y, z;
    bool operator==(const ChunkPos& otherPos) const{
        return x == otherPos.x && 
                y == otherPos.y && 
                z == otherPos.z;
    }
};

#endif