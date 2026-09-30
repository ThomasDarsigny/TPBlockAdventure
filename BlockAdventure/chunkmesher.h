#ifndef CHUNKMESHER_H__
#define CHUNKMESHER_H__

#include "chunk.h"
#include <vector>

// ---------------------------------------------------------------------------
//  Instantane d'un chunk et de sa bordure (1 bloc dans chaque direction).
//
//  Le maillage tourne sur des threads secondaires: on ne peut donc pas lire
//  directement les chunks voisins pendant que le thread principal les
//  modifie. Le thread principal recopie donc ici tout ce dont le mailleur a
//  besoin (blocs + lumieres), puis le worker travaille sur cette copie.
// ---------------------------------------------------------------------------
struct MeshSnapshot
{
    static const int PX = CHUNK_SIZE_X + 2;
    static const int PY = CHUNK_SIZE_Y + 2;
    static const int PZ = CHUNK_SIZE_Z + 2;

    int cx, cz;
    std::vector<BlockType>     blocks;
    std::vector<unsigned char> light;
    std::vector<unsigned char> meta;

    MeshSnapshot()
        : cx(0), cz(0),
          blocks((size_t)PX * PY * PZ, BTYPE_AIR),
          light((size_t)PX * PY * PZ, 0),
          meta((size_t)PX * PY * PZ, 0) {}

    static int Index(int x, int y, int z)
    {
        return ((y + 1) * PZ + (z + 1)) * PX + (x + 1);
    }

    BlockType B(int x, int y, int z) const { return blocks[Index(x, y, z)]; }
    unsigned char Sky(int x, int y, int z) const { return (unsigned char)(light[Index(x, y, z)] >> 4); }
    unsigned char Blk(int x, int y, int z) const { return (unsigned char)(light[Index(x, y, z)] & 0x0F); }
    unsigned char M(int x, int y, int z) const { return meta[Index(x, y, z)]; }

    // Hauteur de la surface d'un liquide, en huitiemes de bloc.
    // Un liquide surmonte du meme liquide remplit tout le cube.
    int FluidHeight(int x, int y, int z) const
    {
        const BlockType t = B(x, y, z);
        if (B(x, y + 1, z) == t) return 8;
        const int level = M(x, y, z) & FLUID_LEVEL_MASK;
        int h = 7 - level;
        return (h < 1) ? 1 : h;
    }
};

namespace ChunkMesher
{
    // Construit la geometrie du chunk decrit par 'snap'. Thread-safe.
    void Build(const MeshSnapshot& snap, ChunkMeshData& out);
}

#endif // CHUNKMESHER_H__
