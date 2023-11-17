#ifndef CHUNK_H__
#define CHUNK_H__

#include "array3d.h"
#include "shader.h"
#include "vertexbuffer.h"
#include "blockinfo.h"

class Chunk
{
public:
    Chunk();
    ~Chunk();

    void RemoveBlock(int x, int y, int z);
    void SetBlock(int x, int y, int z, BlockType type);
    BlockType GetBlock(int x, int y, int z) const;
    void Update(int x, int y);
    void Render() const;
    bool IsDirty() const;
    void SetBlockInfo(BlockInfo* _m_blockinfo, BlockType bt);
   

private:
    Array3d<BlockType> m_blocks;
    VertexBuffer m_vertexBuffer;
    Shader m_shader;

    BlockInfo* m_blockinfo[BTYPE_FIN];

    bool m_isDirty = false;

    void AddBlockToMesh(VertexBuffer::VertexData* vd, int& count, BlockType bt, int x, int y, int z, int positionChunkX, int positionChunkY);
};

#endif // CHUNK_H__
