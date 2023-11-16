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

void Chunk::Update(int x, int y)
{
	const int positionChunkX = x * CHUNK_SIZE_X; // position du chunk x dans le monde
	const int positionChunkZ = y * CHUNK_SIZE_Z; // position du chunk z dans le monde
	if (m_isDirty)
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
						AddBlockToMesh(vd, count, bt, x, y, z, positionChunkX, positionChunkZ);
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
	return m_isDirty;
}

void Chunk::AddBlockToMesh(VertexBuffer::VertexData* vd, int& count, BlockType bt, int x, int y, int z, int positionChunkX, int positionChunkZ)
{
	// ordre des faces des cubes = haut, gauche, droite, avant, arriere, bas
	for (int idx = 0; idx < 6; ++idx)
	{
		float u, v, w, h;
		m_blockinfo[bt]->GetTextureIndexToCoord(m_blockinfo[bt]->GetTextureIndex(idx), u, v, w, h);

		if (idx == 0 && m_blocks.Get(x, y + 1, z) == BTYPE_AIR)
		{
			// face haut
			vd[count++] = VertexBuffer::VertexData(x - 0.5f + positionChunkX, y + 0.5f, z + 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u + w, v + h);
			vd[count++] = VertexBuffer::VertexData(x + 0.5f + positionChunkX, y + 0.5f, z + 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u, v + h);
			vd[count++] = VertexBuffer::VertexData(x + 0.5f + positionChunkX, y + 0.5f, z - 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u, v);
			vd[count++] = VertexBuffer::VertexData(x - 0.5f + positionChunkX, y + 0.5f, z - 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u + w, v);
		}
		else if (idx == 1 && m_blocks.Get(x - 1, y, z) == BTYPE_AIR)
		{
			// face gauche 																						   
			vd[count++] = VertexBuffer::VertexData(x - 0.5f + positionChunkX, y - 0.5f, z - 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u + w, v);
			vd[count++] = VertexBuffer::VertexData(x - 0.5f + positionChunkX, y - 0.5f, z + 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u, v);
			vd[count++] = VertexBuffer::VertexData(x - 0.5f + positionChunkX, y + 0.5f, z + 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u, v + h);
			vd[count++] = VertexBuffer::VertexData(x - 0.5f + positionChunkX, y + 0.5f, z - 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u + w, v + h);
		}
		else if (idx == 2 && m_blocks.Get(x + 1, y, z) == BTYPE_AIR)
		{
			// face droite																						   
			vd[count++] = VertexBuffer::VertexData(x + 0.5f + positionChunkX, y - 0.5f, z + 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u + w, v);
			vd[count++] = VertexBuffer::VertexData(x + 0.5f + positionChunkX, y - 0.5f, z - 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u, v);
			vd[count++] = VertexBuffer::VertexData(x + 0.5f + positionChunkX, y + 0.5f, z - 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u, v + h);
			vd[count++] = VertexBuffer::VertexData(x + 0.5f + positionChunkX, y + 0.5f, z + 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u + w, v + h);
		}
		else if (idx == 3 && m_blocks.Get(x, y, z + 1) == BTYPE_AIR)
		{
			// face avant																						   
			vd[count++] = VertexBuffer::VertexData(x - 0.5f + positionChunkX, y - 0.5f, z + 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u + w, v);
			vd[count++] = VertexBuffer::VertexData(x + 0.5f + positionChunkX, y - 0.5f, z + 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u, v);
			vd[count++] = VertexBuffer::VertexData(x + 0.5f + positionChunkX, y + 0.5f, z + 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u, v + h);
			vd[count++] = VertexBuffer::VertexData(x - 0.5f + positionChunkX, y + 0.5f, z + 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u + w, v + h);
		}
		else if (idx == 4 && m_blocks.Get(x, y, z - 1) == BTYPE_AIR)
		{
			// face arriere 																						   
			vd[count++] = VertexBuffer::VertexData(x + 0.5f + positionChunkX, y - 0.5f, z - 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u + w, v);
			vd[count++] = VertexBuffer::VertexData(x - 0.5f + positionChunkX, y - 0.5f, z - 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u, v);
			vd[count++] = VertexBuffer::VertexData(x - 0.5f + positionChunkX, y + 0.5f, z - 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u, v + h);
			vd[count++] = VertexBuffer::VertexData(x + 0.5f + positionChunkX, y + 0.5f, z - 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u + w, v + h);
		}
		else if (idx == 5 && m_blocks.Get(x, y - 1, z) == BTYPE_AIR)
		{
			// face bas																					   
			vd[count++] = VertexBuffer::VertexData(x - 0.5f + positionChunkX, y - 0.5f, z - 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u + w, v + h);
			vd[count++] = VertexBuffer::VertexData(x + 0.5f + positionChunkX, y - 0.5f, z - 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u, v + h);
			vd[count++] = VertexBuffer::VertexData(x + 0.5f + positionChunkX, y - 0.5f, z + 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u, v);
			vd[count++] = VertexBuffer::VertexData(x - 0.5f + positionChunkX, y - 0.5f, z + 0.5f + positionChunkZ, 1.0f, 1.0f, 1.0f, u + w, v);
		}
	}
}

void Chunk::SetBlockInfo(BlockInfo* _m_blockinfo, BlockType bt)
{
	m_blockinfo[bt] = _m_blockinfo;
}



