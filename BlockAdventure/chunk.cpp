#include "chunk.h"
#include <algorithm>

Chunk::Chunk(int cx, int cz)
    : generated(false), lit(false), meshDirty(true), meshPending(false), modified(false),
      m_cx(cx), m_cz(cz),
      m_blocks((size_t)CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z, BTYPE_AIR),
      m_light((size_t)CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z, 0),
      m_meta((size_t)CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z, 0),
      m_highest((size_t)CHUNK_SIZE_X * CHUNK_SIZE_Z, -1),
      m_minY(0), m_maxY(CHUNK_SIZE_Y)
{
}

Chunk::~Chunk()
{
}

void Chunk::ClearLight()
{
    std::fill(m_light.begin(), m_light.end(), (unsigned char)0);
}

void Chunk::FillSkyColumn(int x, int z, int y0, int y1, unsigned char v)
{
    if (x < 0 || x >= CHUNK_SIZE_X || z < 0 || z >= CHUNK_SIZE_Z)
        return;
    if (y0 < 0) y0 = 0;
    if (y1 > CHUNK_SIZE_Y - 1) y1 = CHUNK_SIZE_Y - 1;

    const int stride = CHUNK_SIZE_X * CHUNK_SIZE_Z;   // un etage
    unsigned char* p = m_light.data() + Index(x, y0, z);
    const unsigned char hi = (unsigned char)(v << 4);

    for (int y = y0; y <= y1; ++y, p += stride)
        *p = (unsigned char)((*p & 0x0F) | hi);
}

void Chunk::RecomputeHighest()
{
    for (int z = 0; z < CHUNK_SIZE_Z; ++z)
    {
        for (int x = 0; x < CHUNK_SIZE_X; ++x)
        {
            int h = -1;
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; --y)
            {
                BlockType t = m_blocks[Index(x, y, z)];
                if (t != BTYPE_AIR && Blocks::Opacity(t) > 0)
                {
                    h = y;
                    break;
                }
            }
            m_highest[z * CHUNK_SIZE_X + x] = h;
        }
    }
}

void Chunk::UpdateHighest(int x, int y, int z, BlockType t)
{
    if (!InBounds(x, y, z)) return;

    int& h = m_highest[z * CHUNK_SIZE_X + x];
    bool blocksLight = (t != BTYPE_AIR && Blocks::Opacity(t) > 0);

    if (blocksLight)
    {
        if (y > h) h = y;
    }
    else if (y == h)
    {
        // Le sommet vient de disparaitre: on redescend jusqu'au suivant
        int ny = -1;
        for (int yy = y - 1; yy >= 0; --yy)
        {
            BlockType b = m_blocks[Index(x, yy, z)];
            if (b != BTYPE_AIR && Blocks::Opacity(b) > 0) { ny = yy; break; }
        }
        h = ny;
    }
}

void Chunk::UploadMesh(const ChunkMeshData& data)
{
    m_solid.SetMeshData(data.solid.empty() ? 0 : data.solid.data(), (int)data.solid.size());
    m_blend.SetMeshData(data.blend.empty() ? 0 : data.blend.data(), (int)data.blend.size());
    m_minY = data.minY;
    m_maxY = data.maxY;
}

void Chunk::FreeGpuResources()
{
    m_solid.Free();
    m_blend.Free();
}
