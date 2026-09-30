#ifndef VERTEXBUFFER_H__
#define VERTEXBUFFER_H__

#include "define.h"
#include <vector>

// ---------------------------------------------------------------------------
//  Vertex compact: 16 octets au lieu de 32.
//
//  Avec un monde infini on garde plusieurs centaines de chunks en memoire, il
//  faut donc que chaque sommet coute le moins cher possible.
//
//    position : 3 shorts, exprimes en 1/8 de bloc (le shader multiplie par
//               0.125). Cela suffit pour les torches, les dalles et la
//               surface d'eau abaissee a 7/8.
//    couleur  : 4 octets = (lumiere du ciel, lumiere des blocs,
//               occlusion ambiante * ombrage de face, code de face + drapeaux)
//               Garder les deux lumieres separees permet de faire varier le
//               cycle jour/nuit sans reconstruire le maillage.
//    texcoord : 2 shorts, coordonnees dans l'atlas * 32767
// ---------------------------------------------------------------------------
class VertexBuffer
{
public:
    struct VertexData
    {
        int16_t  x, y, z, pad;
        uint8_t  sky, block, ao, code;
        int16_t  u, v;

        VertexData() {}
        VertexData(int16_t x, int16_t y, int16_t z,
                   uint8_t sky, uint8_t block, uint8_t ao, uint8_t code,
                   int16_t u, int16_t v)
            : x(x), y(y), z(z), pad(0), sky(sky), block(block), ao(ao), code(code), u(u), v(v) {}
    };

    // Drapeaux ranges dans 'code' (bits 0-2: face, bits 3-4: type d'animation)
    enum { WAVE_NONE = 0, WAVE_FOLIAGE = 1, WAVE_WATER = 2 };
    static uint8_t MakeCode(int face, int wave) { return (uint8_t)(face | (wave << 3)); }

public:
    VertexBuffer();
    ~VertexBuffer();

    bool IsValid() const { return m_isValid; }
    int  Count() const { return m_vertexCount; }

    void SetMeshData(const VertexData* vd, int vertexCount);
    void Free();
    void Render() const;

    // Toutes les geometries sont des quads: un unique index buffer partage
    // (0,1,2, 0,2,3 decale de 4 en 4) suffit pour tous les chunks.
    static void EnsureSharedIndices(int quadCount);
    static void ReleaseSharedIndices();

private:
    bool   m_isValid;
    int    m_vertexCount;
    GLuint m_vertexVboId;

    static GLuint s_indexVboId;
    static int    s_indexQuadCapacity;
};

#endif // VERTEXBUFFER_H__
