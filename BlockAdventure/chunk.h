#ifndef CHUNK_H__
#define CHUNK_H__

#include "define.h"
#include "vertexbuffer.h"
#include "blockinfo.h"
#include <vector>

// Geometrie produite par le thread de maillage, en attente d'envoi au GPU
struct ChunkMeshData
{
    std::vector<VertexBuffer::VertexData> solid;   // opaque + alpha test
    std::vector<VertexBuffer::VertexData> blend;   // eau, verre, glace
    int minY;
    int maxY;

    ChunkMeshData() : minY(0), maxY(CHUNK_SIZE_Y) {}
};

// ---------------------------------------------------------------------------
//  Un chunk = une colonne de 16 x CHUNK_SIZE_Y x 16 blocs.
//
//  Le tableau de blocs et celui des lumieres sont volontairement plats:
//  index = (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X + x
//  Une tranche horizontale est donc contigue en memoire, ce qui accelere
//  nettement les boucles de maillage.
// ---------------------------------------------------------------------------
class Chunk
{
public:
    Chunk(int cx, int cz);
    ~Chunk();

    int Cx() const { return m_cx; }
    int Cz() const { return m_cz; }

    static bool InBounds(int x, int y, int z)
    {
        return x >= 0 && x < CHUNK_SIZE_X && y >= 0 && y < CHUNK_SIZE_Y && z >= 0 && z < CHUNK_SIZE_Z;
    }

    static int Index(int x, int y, int z)
    {
        return (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X + x;
    }

    BlockType GetBlock(int x, int y, int z) const
    {
        if (!InBounds(x, y, z)) return BTYPE_AIR;
        return m_blocks[Index(x, y, z)];
    }

    void SetBlock(int x, int y, int z, BlockType t)
    {
        if (!InBounds(x, y, z)) return;
        m_blocks[Index(x, y, z)] = t;
    }

    unsigned char GetMeta(int x, int y, int z) const
    {
        if (!InBounds(x, y, z)) return 0;
        return m_meta[Index(x, y, z)];
    }

    void SetMeta(int x, int y, int z, unsigned char m)
    {
        if (!InBounds(x, y, z)) return;
        m_meta[Index(x, y, z)] = m;
    }

    unsigned char GetSkyLight(int x, int y, int z) const
    {
        if (!InBounds(x, y, z)) return (y >= CHUNK_SIZE_Y) ? MAX_LIGHT : 0;
        return (unsigned char)(m_light[Index(x, y, z)] >> 4);
    }

    unsigned char GetBlockLight(int x, int y, int z) const
    {
        if (!InBounds(x, y, z)) return 0;
        return (unsigned char)(m_light[Index(x, y, z)] & 0x0F);
    }

    void SetSkyLight(int x, int y, int z, unsigned char v)
    {
        if (!InBounds(x, y, z)) return;
        unsigned char& l = m_light[Index(x, y, z)];
        l = (unsigned char)((l & 0x0F) | (v << 4));
    }

    void SetBlockLight(int x, int y, int z, unsigned char v)
    {
        if (!InBounds(x, y, z)) return;
        unsigned char& l = m_light[Index(x, y, z)];
        l = (unsigned char)((l & 0xF0) | (v & 0x0F));
    }

    void ClearLight();

    // Remplit d'un coup [y0..y1] d'une colonne avec la meme lumiere du ciel.
    // Une colonne fait 160 cellules: passer par SetSkyLight pour chacune
    // coutait un test de bornes et un calcul d'indice a chaque etage.
    void FillSkyColumn(int x, int z, int y0, int y1, unsigned char v);

    // Hauteur du plus haut bloc opaque de la colonne (-1 si aucune)
    int  Highest(int x, int z) const { return m_highest[z * CHUNK_SIZE_X + x]; }
    void RecomputeHighest();
    void UpdateHighest(int x, int y, int z, BlockType t);

    // Envoi de la geometrie au GPU (thread principal uniquement)
    void UploadMesh(const ChunkMeshData& data);
    void FreeGpuResources();

    void RenderSolid() const { m_solid.Render(); }
    void RenderBlend() const { m_blend.Render(); }

    bool HasSolid() const { return m_solid.IsValid(); }
    bool HasBlend() const { return m_blend.IsValid(); }

    int VertexCount() const { return m_solid.Count() + m_blend.Count(); }

    const std::vector<BlockType>& Blocks() const { return m_blocks; }
    std::vector<BlockType>&       Blocks()       { return m_blocks; }
    const std::vector<unsigned char>& Light() const { return m_light; }
    const std::vector<unsigned char>& Meta() const { return m_meta; }
    std::vector<unsigned char>&       Meta()       { return m_meta; }

    int MinY() const { return m_minY; }
    int MaxY() const { return m_maxY; }

public:
    // Etat gere par World
    bool generated;     // les blocs sont en place
    bool lit;           // la lumiere initiale a ete calculee
    bool meshDirty;     // le maillage doit etre reconstruit
    bool meshPending;   // un job de maillage est deja en vol
    bool modified;      // le joueur a modifie ce chunk (a sauvegarder)

private:
    int m_cx, m_cz;

    std::vector<BlockType>     m_blocks;
    std::vector<unsigned char> m_light;
    std::vector<unsigned char> m_meta;
    std::vector<int>           m_highest;

    VertexBuffer m_solid;
    VertexBuffer m_blend;

    int m_minY, m_maxY;
};

#endif // CHUNK_H__
