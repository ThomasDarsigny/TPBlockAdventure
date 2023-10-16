#ifndef CHUNK_H__
#define CHUNK_H__
#include "blockarray3d.h"

class Chunk : public BlockArray3d
{
    public:
    Chunk();
    ~Chunk();

    void RemoveBlock(int x, int y, int z);
    void SetBlock(int x, int y, int z, BlockType type);
    BlockType GetBlock(int x, int y, int z);
};

#endif // CHUNK_H__
