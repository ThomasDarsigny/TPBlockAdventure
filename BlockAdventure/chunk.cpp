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
	m_isDirty = true;
}

void Chunk::SetBlock(int x, int y, int z, BlockType type)
{
    m_blocks.Set(x, y, z, type);
	m_isDirty = true;
}

BlockType Chunk::GetBlock(int x, int y, int z) const
{
    return m_blocks.Get(x, y, z);
}

void Chunk::Update(int worldX, int worldZ, TextureAtlas texture)
{
	if (m_isDirty)
	{
		const int WorldX = worldX * CHUNK_SIZE_X;
		const int WorldZ = worldZ * CHUNK_SIZE_Z;
		int maxVertexCount = (CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z) * (6 * 4);
		VertexBuffer::VertexData* vd = new VertexBuffer::VertexData[maxVertexCount];
		int count = 0;

		for (int localX = 0; localX < CHUNK_SIZE_X; ++localX)
		{
			for (int localZ = 0; localZ < CHUNK_SIZE_Z; ++localZ)
			{
				for (int y = 0; y < CHUNK_SIZE_Y; ++y)
				{
					if (count > USHRT_MAX) break;
					BlockType bt = GetBlock(localX, y, localZ);
					if (bt != BTYPE_AIR)
					{
						AddBlockToMesh(vd, count, bt, localX, y, localZ, WorldX, WorldZ, texture);
					}
				}
			}
		}

		if (count > USHRT_MAX)
		{
			count = USHRT_MAX;
			std::cout << "[Chunk::Update] Chunk data truncated, too many vertices to have a 16-bit index" << std::endl;
		}

		m_vertexBuffer.SetMeshData(vd, count);
		delete[] vd;
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
	return m_isDirty;
}

void Chunk::AddBlockToMesh(VertexBuffer::VertexData* vd, int& count, BlockType bt, int x, int y, int z, const int WorldX, const int WorldZ, TextureAtlas texture)
{

	float u, v, h, w;
	TextureAtlas m_textureAtlas = texture;
	m_textureAtlas.TextureIndexToCoord(BTYPE_DIRT - 1, u, v, h, w);


	// front
	vd[count++] = VertexBuffer::VertexData(x - 0.5f+WorldX, y - 0.5f, z + 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f+WorldX, y - 0.5f, z + 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f+WorldX, y + 0.5f, z + 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f+WorldX, y + 0.5f, z + 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);

	// back
	vd[count++] = VertexBuffer::VertexData(x + 0.5f+WorldX, y - 0.5f, z - 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f+WorldX, y - 0.5f, z - 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f+WorldX, y + 0.5f, z - 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f+WorldX, y + 0.5f, z - 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);

	// top
	vd[count++] = VertexBuffer::VertexData(x - 0.5f+WorldX, y + 0.5f, z + 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f+WorldX, y + 0.5f, z + 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f+WorldX, y + 0.5f, z - 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f+WorldX, y + 0.5f, z - 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);

	// bottom
	vd[count++] = VertexBuffer::VertexData(x - 0.5f+WorldX, y - 0.5f, z - 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f+WorldX, y - 0.5f, z - 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f+WorldX, y - 0.5f, z + 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f+WorldX, y - 0.5f, z + 0.5f+WorldZ, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);

	// left
	vd[count++] = VertexBuffer::VertexData(x - 0.5f+WorldX, y - 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f+WorldX, y - 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f+WorldX, y + 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	vd[count++] = VertexBuffer::VertexData(x - 0.5f+WorldX, y + 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);

	// right
	vd[count++] = VertexBuffer::VertexData(x + 0.5f+WorldX, y - 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f+WorldX, y - 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f+WorldX, y + 0.5f, z - 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
	vd[count++] = VertexBuffer::VertexData(x + 0.5f+WorldX, y + 0.5f, z + 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f);
}



