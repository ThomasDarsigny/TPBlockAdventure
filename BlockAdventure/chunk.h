#ifndef CHUNK_H
#define CHUNK_H

#include "blockarray3d.h"

class Chunk : public BlockArray3d {
public:
    Chunk();
    ~Chunk();

    void RemoveBlock(int x, int y, int z);
    void SetBlock(int x, int y, int z, BlockType type);
    BlockType GetBlock(int x, int y, int z) const;
};

#endif 
