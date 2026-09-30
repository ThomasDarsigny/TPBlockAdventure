#ifndef FRUSTUM_H__
#define FRUSTUM_H__

#include "define.h"
#include <cmath>

// ---------------------------------------------------------------------------
//  Extraction des 6 plans du frustum a partir des matrices OpenGL courantes.
//  Chaque plan est stocke sous la forme (a, b, c, d) avec ax+by+cz+d >= 0
//  du cote visible.
// ---------------------------------------------------------------------------
namespace Frustum
{
    inline void Extract(float planes[6][4])
    {
        float proj[16], modl[16], clip[16];
        glGetFloatv(GL_PROJECTION_MATRIX, proj);
        glGetFloatv(GL_MODELVIEW_MATRIX, modl);

        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                clip[i * 4 + j] = modl[i * 4 + 0] * proj[0 * 4 + j]
                                + modl[i * 4 + 1] * proj[1 * 4 + j]
                                + modl[i * 4 + 2] * proj[2 * 4 + j]
                                + modl[i * 4 + 3] * proj[3 * 4 + j];

        // droite, gauche, bas, haut, loin, proche
        const int idx[6][2] = { { 0, -1 }, { 0, 1 }, { 1, -1 }, { 1, 1 }, { 2, -1 }, { 2, 1 } };

        for (int p = 0; p < 6; ++p)
        {
            const int col = idx[p][0];
            const float sign = (float)idx[p][1];
            for (int r = 0; r < 4; ++r)
                planes[p][r] = clip[r * 4 + 3] + sign * clip[r * 4 + col];

            const float len = sqrtf(planes[p][0] * planes[p][0] +
                                    planes[p][1] * planes[p][1] +
                                    planes[p][2] * planes[p][2]);
            if (len > 0.0f)
                for (int r = 0; r < 4; ++r)
                    planes[p][r] /= len;
        }
    }
}

#endif // FRUSTUM_H__
