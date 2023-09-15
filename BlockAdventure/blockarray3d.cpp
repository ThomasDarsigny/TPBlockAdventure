#include "blockarray3d.h"

BlockArray3d::BlockArray3d(int x, int y, int z)
    : m_x(x), m_y(y), m_z(z), m_blocks(new BlockType[x * y * z]) {
    Reset(BTYPE_AIR);
}

BlockArray3d::BlockArray3d(const BlockArray3d& other)
    : m_x(other.m_x), m_y(other.m_y), m_z(other.m_z), m_blocks(new BlockType[other.m_x * other.m_y * other.m_z]) {
    for (int i = 0; i < m_x * m_y * m_z; ++i) {
        m_blocks[i] = other.m_blocks[i];
    }
}

BlockArray3d::~BlockArray3d() {
    delete[] m_blocks;
}

void BlockArray3d::Set(int x, int y, int z, BlockType type) {
    if (x >= 0 && x < m_x && y >= 0 && y < m_y && z >= 0 && z < m_z) {
        m_blocks[CalculateIndex(x, y, z)] = type;
    }
}

BlockType BlockArray3d::Get(int x, int y, int z) const {
    if (x >= 0 && x < m_x && y >= 0 && y < m_y && z >= 0 && z < m_z) {
        return m_blocks[CalculateIndex(x, y, z)];
    }
    return BTYPE_AIR; // Default to air
}

void BlockArray3d::Reset(BlockType type)
{
    for (int i = 0; i < m_x * m_y * m_z; ++i) {
        m_blocks[i] = type;
    }
}

int BlockArray3d::CalculateIndex(int x, int y, int z) const {
    return x + (z * m_x) + (y * m_z * m_x);
}
