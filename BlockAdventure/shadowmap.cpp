#include "shadowmap.h"
#include <cmath>
#include <cstring>
#include <iostream>

namespace
{
    // Produit de deux matrices 4x4 rangees en colonnes, comme OpenGL
    void MultiplyColumnMajor(const float* a, const float* b, float* out)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                out[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0]
                               + a[1 * 4 + r] * b[c * 4 + 1]
                               + a[2 * 4 + r] * b[c * 4 + 2]
                               + a[3 * 4 + r] * b[c * 4 + 3];
    }
}

ShadowMap::ShadowMap()
    : m_ok(false), m_enabled(true), m_resolution(0), m_radius(52.0f),
      m_fbo(0), m_depthTex(0)
{
    memset(m_lightMatrix, 0, sizeof(m_lightMatrix));
    for (int i = 0; i < 4; ++i)
        m_lightMatrix[i * 4 + i] = 1.0f;
}

ShadowMap::~ShadowMap()
{
    Destroy();
}

bool ShadowMap::Init(int resolution)
{
    Destroy();

    if (!GLEW_ARB_framebuffer_object || !GLEW_ARB_depth_texture)
    {
        std::cout << "[Ombres] Textures de profondeur indisponibles" << std::endl;
        return false;
    }

    if (!m_depth.Load(SHADER_PATH "shadow.vert", SHADER_PATH "shadow.frag", true))
    {
        std::cout << "[Ombres] Shader indisponible" << std::endl;
        return false;
    }

    m_resolution = resolution;

    glGenTextures(1, &m_depthTex);
    glBindTexture(GL_TEXTURE_2D, m_depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, resolution, resolution, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    // En dehors de la carte, tout est considere comme eclaire
    const GLfloat border[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    const bool complete = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!complete)
    {
        std::cerr << "[Ombres] FBO incomplet" << std::endl;
        Destroy();
        return false;
    }

    m_ok = true;
    std::cout << "[Ombres] carte de " << resolution << "x" << resolution << std::endl;
    return true;
}

void ShadowMap::Destroy()
{
    if (m_fbo)      { glDeleteFramebuffers(1, &m_fbo);  m_fbo = 0; }
    if (m_depthTex) { glDeleteTextures(1, &m_depthTex); m_depthTex = 0; }
    m_depth.Destroy();
    m_ok = false;
}

void ShadowMap::Begin(const Vector3f& center, const Vector3f& sunDir)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_resolution, m_resolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    Vector3f dir = sunDir;
    dir.Normalize();

    // Soleil trop bas: la carte serait rasante et inutilisable
    if (dir.y < 0.12f)
    {
        dir.y = 0.12f;
        dir.Normalize();
    }

    const float dist = 140.0f;
    const Vector3f eye = center + dir * dist;

    // Alignement du centre sur la grille des texels pour eviter le
    // scintillement des bords quand le joueur se deplace.
    const float texelWorld = (m_radius * 2.0f) / (float)m_resolution;
    Vector3f target = center;
    target.x = floorf(target.x / texelWorld) * texelWorld;
    target.z = floorf(target.z / texelWorld) * texelWorld;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-m_radius, m_radius, -m_radius, m_radius, 1.0f, dist * 2.0f);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Vecteur "haut" non colineaire a la direction du soleil
    const float upY = (fabsf(dir.y) > 0.95f) ? 0.0f : 1.0f;
    const float upZ = (fabsf(dir.y) > 0.95f) ? 1.0f : 0.0f;
    gluLookAt(eye.x, eye.y, eye.z, target.x, target.y, target.z, 0.0f, upY, upZ);

    // On relit les matrices telles qu'OpenGL les a construites: aucune
    // convention a deviner, le shader recevra exactement la meme chose.
    float proj[16], view[16];
    glGetFloatv(GL_PROJECTION_MATRIX, proj);
    glGetFloatv(GL_MODELVIEW_MATRIX, view);
    MultiplyColumnMajor(proj, view, m_lightMatrix);

    // On dessine les faces arrieres: l'acne d'ombrage se retrouve alors
    // cachee a l'interieur des blocs.
    glCullFace(GL_FRONT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
    glDisable(GL_BLEND);
}

void ShadowMap::End(int viewportWidth, int viewportHeight)
{
    glDisable(GL_POLYGON_OFFSET_FILL);
    glCullFace(GL_BACK);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, viewportWidth, viewportHeight);
}

void ShadowMap::BindTexture(GLenum unit) const
{
    glActiveTexture(unit);
    glBindTexture(GL_TEXTURE_2D, m_depthTex);
    glActiveTexture(GL_TEXTURE0);
}
