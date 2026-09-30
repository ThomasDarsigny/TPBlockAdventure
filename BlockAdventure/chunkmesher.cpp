#include "chunkmesher.h"
#include <cmath>

namespace
{
    // Reperes locaux de chaque face: normale + axes du plan (a = axe U, b = axe V)
    struct FaceGeom
    {
        int nx, ny, nz;
        int ax, ay, az;
        int bx, by, bz;
        float shade;      // ombrage directionnel fixe, facon Minecraft
    };

    const FaceGeom kFaces[6] =
    {
        /* FACE_TOP    */ {  0,  1,  0,   1, 0,  0,   0, 0, -1,  1.00f },
        /* FACE_BOTTOM */ {  0, -1,  0,   1, 0,  0,   0, 0,  1,  0.50f },
        /* FACE_LEFT   */ { -1,  0,  0,   0, 0,  1,   0, 1,  0,  0.62f },
        /* FACE_RIGHT  */ {  1,  0,  0,   0, 0, -1,   0, 1,  0,  0.62f },
        /* FACE_FRONT  */ {  0,  0,  1,   1, 0,  0,   0, 1,  0,  0.82f },
        /* FACE_BACK   */ {  0,  0, -1,  -1, 0,  0,   0, 1,  0,  0.82f }
    };

    // Signe des 4 coins d'un quad, dans l'ordre d'emission
    const int kCornerA[4] = { -1,  1,  1, -1 };
    const int kCornerB[4] = { -1, -1,  1,  1 };

    // Coordonnees de texture correspondantes (en fractions de tuile)
    const float kCornerU[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
    const float kCornerV[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

    // Facteurs d'assombrissement pour les 4 niveaux d'occlusion ambiante
    const float kAoShade[4] = { 0.45f, 0.66f, 0.84f, 1.00f };

    inline int VertexAO(bool side1, bool side2, bool corner)
    {
        if (side1 && side2) return 0;
        return 3 - ((side1 ? 1 : 0) + (side2 ? 1 : 0) + (corner ? 1 : 0));
    }

    inline int16_t TexCoord(float t)
    {
        float v = t * 32767.0f;
        if (v > 32767.0f) v = 32767.0f;
        if (v < 0.0f) v = 0.0f;
        return (int16_t)(v + 0.5f);
    }

    inline uint8_t ClampByte(float v)
    {
        int i = (int)(v + 0.5f);
        if (i < 0) i = 0;
        if (i > 255) i = 255;
        return (uint8_t)i;
    }

    struct CornerLight
    {
        int ao;
        int sky;
        int blk;
    };

    // Occlusion ambiante + lumiere lissee pour un coin de face
    CornerLight SampleCorner(const MeshSnapshot& s, int bx, int by, int bz,
                             const FaceGeom& f, int sa, int sb)
    {
        const int px = bx + f.nx, py = by + f.ny, pz = bz + f.nz;

        const int s1x = px + f.ax * sa, s1y = py + f.ay * sa, s1z = pz + f.az * sa;
        const int s2x = px + f.bx * sb, s2y = py + f.by * sb, s2z = pz + f.bz * sb;
        const int cx  = s1x + f.bx * sb, cy = s1y + f.by * sb, cz = s1z + f.bz * sb;

        const bool o1 = Blocks::IsOpaque(s.B(s1x, s1y, s1z));
        const bool o2 = Blocks::IsOpaque(s.B(s2x, s2y, s2z));
        const bool oc = Blocks::IsOpaque(s.B(cx, cy, cz));

        CornerLight r;
        r.ao = VertexAO(o1, o2, oc);

        // Moyenne des cellules non opaques qui touchent le coin
        int sky = s.Sky(px, py, pz);
        int blk = s.Blk(px, py, pz);
        int n = 1;

        if (!o1) { sky += s.Sky(s1x, s1y, s1z); blk += s.Blk(s1x, s1y, s1z); ++n; }
        if (!o2) { sky += s.Sky(s2x, s2y, s2z); blk += s.Blk(s2x, s2y, s2z); ++n; }
        if (!(o1 && o2) && !oc) { sky += s.Sky(cx, cy, cz); blk += s.Blk(cx, cy, cz); ++n; }

        r.sky = sky / n;
        r.blk = blk / n;
        return r;
    }

    // Emet un quad, en tournant les sommets si necessaire pour que la
    // diagonale du triangle suive le gradient d'occlusion (evite l'artefact
    // classique en "escalier" dans les coins).
    void EmitQuad(std::vector<VertexBuffer::VertexData>& out,
                  const VertexBuffer::VertexData v[4],
                  const int ao[4])
    {
        if (ao[0] + ao[2] > ao[1] + ao[3])
        {
            out.push_back(v[1]);
            out.push_back(v[2]);
            out.push_back(v[3]);
            out.push_back(v[0]);
        }
        else
        {
            out.push_back(v[0]);
            out.push_back(v[1]);
            out.push_back(v[2]);
            out.push_back(v[3]);
        }
    }

    // ---------------------------------------------------------------
    //  Cube (et liquide) : une face
    // ---------------------------------------------------------------
    // botH / topH delimitent la tranche verticale occupee par le bloc, en
    // huitiemes. Un cube plein va de 0 a 8; un liquide s'arrete plus bas, et
    // une face de liquide qui donne sur un liquide moins haut ne dessine que
    // la partie qui depasse.
    void AddCubeFace(const MeshSnapshot& s, std::vector<VertexBuffer::VertexData>& out,
                     int x, int y, int z, BlockType bt, int face, int wave,
                     int botH, int topH)
    {
        const FaceGeom& f = kFaces[face];
        const BlockInfo& info = Blocks::Get(bt);
        const float u = info.uv[face][0], v = info.uv[face][1];
        const float w = info.uv[face][2], h = info.uv[face][3];

        // Centre du bloc en 1/8 de bloc
        const int cx8 = x * 8 + 4;
        const int cy8 = y * 8 + 4;
        const int cz8 = z * 8 + 4;

        VertexBuffer::VertexData vert[4];
        int aoLevels[4];

        for (int c = 0; c < 4; ++c)
        {
            const int sa = kCornerA[c];
            const int sb = kCornerB[c];

            int px8 = cx8 + 4 * f.nx + 4 * sa * f.ax + 4 * sb * f.bx;
            int py8 = cy8 + 4 * f.ny + 4 * sa * f.ay + 4 * sb * f.by;
            int pz8 = cz8 + 4 * f.nz + 4 * sa * f.az + 4 * sb * f.bz;

            // Repositionnement vertical sur la tranche demandee
            float vFrac = kCornerV[c];
            if (f.ny > 0)
            {
                py8 = y * 8 + topH;
            }
            else if (f.ny < 0)
            {
                py8 = y * 8 + botH;
            }
            else if (botH != 0 || topH != 8)
            {
                const bool high = (py8 == y * 8 + 8);
                py8 = y * 8 + (high ? topH : botH);
                vFrac = (high ? (float)topH : (float)botH) / 8.0f;
            }

            CornerLight cl = SampleCorner(s, x, y, z, f, sa, sb);
            aoLevels[c] = cl.ao;

            float shade = kAoShade[cl.ao] * f.shade;

            vert[c] = VertexBuffer::VertexData(
                (int16_t)px8, (int16_t)py8, (int16_t)pz8,
                (uint8_t)(cl.sky * 17), (uint8_t)(cl.blk * 17),
                ClampByte(shade * 255.0f),
                VertexBuffer::MakeCode(face, wave),
                TexCoord(u + kCornerU[c] * w),
                TexCoord(v + vFrac * h));
        }

        EmitQuad(out, vert, aoLevels);
    }

    // ---------------------------------------------------------------
    //  Plantes en croix : deux quads doubles faces
    // ---------------------------------------------------------------
    void AddCross(const MeshSnapshot& s, std::vector<VertexBuffer::VertexData>& out,
                  int x, int y, int z, BlockType bt)
    {
        const BlockInfo& info = Blocks::Get(bt);
        const float u = info.uv[FACE_FRONT][0], v = info.uv[FACE_FRONT][1];
        const float w = info.uv[FACE_FRONT][2], h = info.uv[FACE_FRONT][3];

        const int sky = s.Sky(x, y, z) > s.Sky(x, y + 1, z) ? s.Sky(x, y, z) : s.Sky(x, y + 1, z);
        const int blk = s.Blk(x, y, z) > s.Blk(x, y + 1, z) ? s.Blk(x, y, z) : s.Blk(x, y + 1, z);
        const uint8_t shade = ClampByte(0.92f * 255.0f);
        const uint8_t code = VertexBuffer::MakeCode(FACE_TOP, VertexBuffer::WAVE_FOLIAGE);

        const int x0 = x * 8 + 1, x1 = x * 8 + 7;
        const int z0 = z * 8 + 1, z1 = z * 8 + 7;
        const int y0 = y * 8, y1 = y * 8 + 8;

        // 2 diagonales x 2 orientations (pour etre visibles des deux cotes)
        const int quads[4][4][3] =
        {
            { { x0, y0, z0 }, { x1, y0, z1 }, { x1, y1, z1 }, { x0, y1, z0 } },
            { { x1, y0, z1 }, { x0, y0, z0 }, { x0, y1, z0 }, { x1, y1, z1 } },
            { { x0, y0, z1 }, { x1, y0, z0 }, { x1, y1, z0 }, { x0, y1, z1 } },
            { { x1, y0, z0 }, { x0, y0, z1 }, { x0, y1, z1 }, { x1, y1, z0 } }
        };

        for (int q = 0; q < 4; ++q)
        {
            for (int c = 0; c < 4; ++c)
            {
                out.push_back(VertexBuffer::VertexData(
                    (int16_t)quads[q][c][0], (int16_t)quads[q][c][1], (int16_t)quads[q][c][2],
                    (uint8_t)(sky * 17), (uint8_t)(blk * 17), shade, code,
                    TexCoord(u + kCornerU[c] * w),
                    TexCoord(v + kCornerV[c] * h)));
            }
        }
    }

    // ---------------------------------------------------------------
    //  Torche : petit prisme centre, avec des UV restreints a la partie
    //  utile de la tuile.
    // ---------------------------------------------------------------
    void AddTorch(const MeshSnapshot& s, std::vector<VertexBuffer::VertexData>& out,
                  int x, int y, int z, BlockType bt)
    {
        const BlockInfo& info = Blocks::Get(bt);
        const float u = info.uv[FACE_FRONT][0], v = info.uv[FACE_FRONT][1];
        const float w = info.uv[FACE_FRONT][2], h = info.uv[FACE_FRONT][3];

        const int sky = s.Sky(x, y, z);
        const int blk = s.Blk(x, y, z) > 12 ? s.Blk(x, y, z) : 12;
        const uint8_t code = VertexBuffer::MakeCode(FACE_TOP, VertexBuffer::WAVE_NONE);

        const int x0 = x * 8 + 3, x1 = x * 8 + 5;
        const int z0 = z * 8 + 3, z1 = z * 8 + 5;
        const int y0 = y * 8,     y1 = y * 8 + 5;

        // Sous-rectangle de la tuile occupe par le baton et la flamme
        const float su0 = u + w * (6.0f / 16.0f), su1 = u + w * (10.0f / 16.0f);
        const float sv0 = v,                      sv1 = v + h * (10.0f / 16.0f);
        const float tv0 = v + h * (8.0f / 16.0f), tv1 = v + h * (12.0f / 16.0f);

        struct Quad { int p[4][3]; float uu[4]; float vv[4]; };

        const Quad quads[5] =
        {
            // faces laterales
            { { { x0, y0, z1 }, { x1, y0, z1 }, { x1, y1, z1 }, { x0, y1, z1 } },
              { su0, su1, su1, su0 }, { sv0, sv0, sv1, sv1 } },
            { { { x1, y0, z0 }, { x0, y0, z0 }, { x0, y1, z0 }, { x1, y1, z0 } },
              { su0, su1, su1, su0 }, { sv0, sv0, sv1, sv1 } },
            { { { x0, y0, z0 }, { x0, y0, z1 }, { x0, y1, z1 }, { x0, y1, z0 } },
              { su0, su1, su1, su0 }, { sv0, sv0, sv1, sv1 } },
            { { { x1, y0, z1 }, { x1, y0, z0 }, { x1, y1, z0 }, { x1, y1, z1 } },
              { su0, su1, su1, su0 }, { sv0, sv0, sv1, sv1 } },
            // dessus (la flamme)
            { { { x0, y1, z1 }, { x1, y1, z1 }, { x1, y1, z0 }, { x0, y1, z0 } },
              { su0, su1, su1, su0 }, { tv0, tv0, tv1, tv1 } }
        };

        for (int q = 0; q < 5; ++q)
            for (int c = 0; c < 4; ++c)
            {
                out.push_back(VertexBuffer::VertexData(
                    (int16_t)quads[q].p[c][0], (int16_t)quads[q].p[c][1], (int16_t)quads[q].p[c][2],
                    (uint8_t)(sky * 17), (uint8_t)(blk * 17), 255, code,
                    TexCoord(quads[q].uu[c]), TexCoord(quads[q].vv[c])));
            }
    }
}

namespace ChunkMesher
{

void Build(const MeshSnapshot& snap, ChunkMeshData& out)
{
    out.solid.clear();
    out.blend.clear();

    int minY = CHUNK_SIZE_Y;
    int maxY = 0;

    for (int y = 0; y < CHUNK_SIZE_Y; ++y)
    {
        for (int z = 0; z < CHUNK_SIZE_Z; ++z)
        {
            for (int x = 0; x < CHUNK_SIZE_X; ++x)
            {
                const BlockType bt = snap.B(x, y, z);
                if (bt == BTYPE_AIR)
                    continue;

                const BlockInfo& info = Blocks::Get(bt);
                if (info.render == RENDER_NONE)
                    continue;

                std::vector<VertexBuffer::VertexData>& target =
                    (info.transparency == TRANSP_BLEND) ? out.blend : out.solid;

                const size_t before = target.size();

                if (info.render == RENDER_CROSS)
                {
                    AddCross(snap, target, x, y, z, bt);
                }
                else if (info.render == RENDER_TORCH)
                {
                    AddTorch(snap, target, x, y, z, bt);
                }
                else
                {
                    const bool liquid = info.liquid;
                    const int wave = liquid ? VertexBuffer::WAVE_WATER
                                   : (info.transparency == TRANSP_CUTOUT && info.render == RENDER_CUBE
                                      && (bt == BTYPE_LEAVES || bt == BTYPE_BIRCH_LEAVES || bt == BTYPE_SPRUCE_LEAVES)
                                      ? VertexBuffer::WAVE_FOLIAGE : VertexBuffer::WAVE_NONE);

                    const int topH = liquid ? snap.FluidHeight(x, y, z) : 8;

                    static const int kOffsets[6][3] =
                    {
                        {  0,  1,  0 }, {  0, -1,  0 },
                        { -1,  0,  0 }, {  1,  0,  0 },
                        {  0,  0,  1 }, {  0,  0, -1 }
                    };

                    for (int face = 0; face < 6; ++face)
                    {
                        const int nxp = x + kOffsets[face][0];
                        const int nyp = y + kOffsets[face][1];
                        const int nzp = z + kOffsets[face][2];
                        const BlockType nb = snap.B(nxp, nyp, nzp);

                        int botH = 0;

                        if (liquid && nb == bt)
                        {
                            // Deux liquides identiques: seule la marche entre
                            // deux hauteurs de surface doit apparaitre.
                            if (face == FACE_TOP || face == FACE_BOTTOM)
                                continue;

                            const int hn = snap.FluidHeight(nxp, nyp, nzp);
                            if (hn >= topH)
                                continue;
                            botH = hn;
                        }
                        else if (!Blocks::ShouldRenderFace(bt, nb))
                        {
                            continue;
                        }

                        AddCubeFace(snap, target, x, y, z, bt, face, wave, botH, topH);
                    }
                }

                if (target.size() != before)
                {
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        }
    }

    if (minY > maxY) { minY = 0; maxY = 0; }
    out.minY = minY;
    out.maxY = maxY + 1;
}

} // namespace ChunkMesher
