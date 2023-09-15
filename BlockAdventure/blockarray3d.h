#ifndef BLOCKARRAY3D_H
#define BLOCKARRAY3D_H

#include "define.h"

class BlockArray3d {
public:
    BlockArray3d(int x, int y, int z); //constructor


    BlockArray3d(const BlockArray3d& other);
    ~BlockArray3d();



    void Set(int x, int y, int z, BlockType type);
    BlockType Get(int x, int y, int z) const;
    void Reset(BlockType type);

private:
    int m_x;
    int m_y;
    int m_z;
    BlockType* m_blocks;

    int CalculateIndex(int x, int y, int z) const;
};

#endif 
