#include "vertexbuffer.h"
#include <cassert>
#include <iostream>

GLuint VertexBuffer::s_indexVboId = 0;
int    VertexBuffer::s_indexQuadCapacity = 0;

VertexBuffer::VertexBuffer() : m_isValid(false), m_vertexCount(0), m_vertexVboId(0)
{
}

VertexBuffer::~VertexBuffer()
{
    Free();
}

void VertexBuffer::Free()
{
    if (m_vertexVboId != 0)
    {
        glDeleteBuffers(1, &m_vertexVboId);
        m_vertexVboId = 0;
    }
    m_isValid = false;
    m_vertexCount = 0;
}

void VertexBuffer::EnsureSharedIndices(int quadCount)
{
    if (quadCount <= s_indexQuadCapacity)
        return;

    // On grossit par paliers pour eviter de reallouer a chaque chunk
    int cap = s_indexQuadCapacity > 0 ? s_indexQuadCapacity : 4096;
    while (cap < quadCount)
        cap *= 2;

    std::vector<uint32_t> idx((size_t)cap * 6);
    for (int q = 0; q < cap; ++q)
    {
        uint32_t base = (uint32_t)q * 4;
        idx[(size_t)q * 6 + 0] = base + 0;
        idx[(size_t)q * 6 + 1] = base + 1;
        idx[(size_t)q * 6 + 2] = base + 2;
        idx[(size_t)q * 6 + 3] = base + 0;
        idx[(size_t)q * 6 + 4] = base + 2;
        idx[(size_t)q * 6 + 5] = base + 3;
    }

    if (s_indexVboId == 0)
        glGenBuffers(1, &s_indexVboId);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_indexVboId);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * idx.size(), idx.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    s_indexQuadCapacity = cap;
}

void VertexBuffer::ReleaseSharedIndices()
{
    if (s_indexVboId != 0)
    {
        glDeleteBuffers(1, &s_indexVboId);
        s_indexVboId = 0;
        s_indexQuadCapacity = 0;
    }
}

void VertexBuffer::SetMeshData(const VertexData* vd, int vertexCount)
{
    if (vertexCount <= 0)
    {
        Free();
        return;
    }

    assert(vertexCount % 4 == 0);
    EnsureSharedIndices(vertexCount / 4);

    if (m_vertexVboId == 0)
        glGenBuffers(1, &m_vertexVboId);

    m_vertexCount = vertexCount;

    glBindBuffer(GL_ARRAY_BUFFER, m_vertexVboId);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VertexData) * vertexCount, vd, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_isValid = true;
}

void VertexBuffer::Render() const
{
    if (!m_isValid || s_indexVboId == 0)
        return;

    glBindBuffer(GL_ARRAY_BUFFER, m_vertexVboId);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_SHORT, sizeof(VertexData), (char*)0);

    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(VertexData), (char*)8);

    glClientActiveTexture(GL_TEXTURE0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_SHORT, sizeof(VertexData), (char*)12);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_indexVboId);
    glDrawElements(GL_TRIANGLES, (m_vertexCount / 4) * 6, GL_UNSIGNED_INT, (char*)0);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
