#include <iostream>
#include <climits>
#include "chunk.h"

Chunk::Chunk() : m_blocks(CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z)
{
}

Chunk::~Chunk()
{
}

void Chunk::RemoveBlock(int x, int y, int z)
{
    m_blocks.Set(x, y, z, BTYPE_AIR);
}

void Chunk::SetBlock(int x, int y, int z, BlockType type)
{
    m_blocks.Set(x, y, z, type);
	m_isDirty = IsDirty();
}

BlockType Chunk::GetBlock(int x, int y, int z) const
{
    return m_blocks.Get(x, y, z);
}

void Chunk::Update()
{
   if(m_isDirty)
    {
        int maxVertexCount = (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z) * (6 * 4); VertexBuffer::VertexData* vd = new VertexBuffer::VertexData[maxVertexCount]; int count = 0;
        for (int x = 0; x < CHUNK_SIZE_X; ++x)
        {
            for (int z = 0; z < CHUNK_SIZE_Z; ++z)
            {
                for (int y = 0; y < CHUNK_SIZE_Y; ++y)
                {
                    if (count > USHRT_MAX) break;
                    BlockType bt = GetBlock(x, y, z);
                    if (bt != BTYPE_AIR)
                    {
                        AddBlockToMesh(vd, count, bt, x, y, z);
                    }
                }
            }
        }
        if (count > USHRT_MAX)
        {
            count = USHRT_MAX; std::cout << "[Chunk::Update] Chunk data truncaned, too much vertices to have a 16bit index" << std::endl;
        }
        m_vertexBuffer.SetMeshData(vd, count); delete[] vd;
    }
    m_isDirty = false;
}

void Chunk::Render() const
{
	Shader shader{};
	shader.Use();

	m_vertexBuffer.Render();

	shader.Disable();
}

bool Chunk::IsDirty() const
{
	return true;
}

void Chunk::AddBlockToMesh(VertexBuffer::VertexData* vd, int& count, BlockType bt, int x, int y, int z)
{
	// front
	vd[count++] = VertexBuffer::VertexData(x - 0.5f, y - 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f, y - 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f, y + 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f, y + 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);

	// back
	vd[count++] = VertexBuffer::VertexData(x + 0.5f, y - 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f, y - 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f, y + 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f, y + 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);

	// top
	vd[count++] = VertexBuffer::VertexData(x - 0.5f, y + 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f, y + 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f, y + 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f, y + 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);

	// bottom
	vd[count++] = VertexBuffer::VertexData(x - 0.5f, y - 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f, y - 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f, y - 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f, y - 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);

	// left
	vd[count++] = VertexBuffer::VertexData(x - 0.5f, y - 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f, y - 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f, y + 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f, y + 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);

	// right
	vd[count++] = VertexBuffer::VertexData(x + 0.5f, y - 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f, y - 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f, y + 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f, y + 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);
}



