#include "chunk.h"

Chunk::Chunk() : BlockArray3d(CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z)
{}

Chunk::~Chunk() {}

void Chunk::RemoveBlock(int x, int y, int z) {
    SetBlock(x, y, z, BTYPE_AIR);
}

void Chunk::SetBlock(int x, int y, int z, BlockType type) {
    BlockArray3d::Set(x, y, z, type);
}

BlockType Chunk::GetBlock(int x, int y, int z) const {
    return BlockArray3d::Get(x, y, z);
}