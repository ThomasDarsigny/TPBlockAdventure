#ifndef SHADOWMAP_H__
#define SHADOWMAP_H__

#include "define.h"
#include "shader.h"
#include "vector3.h"

// ---------------------------------------------------------------------------
//  Ombres portees du soleil.
//
//  Une seule carte de profondeur orthographique, recentree sur le joueur a
//  chaque frame. Le terrain est redessine du point de vue du soleil dans une
//  texture de profondeur, puis le shader de terrain compare la distance au
//  soleil de chaque pixel avec celle enregistree.
//
//  Le centre est aligne sur la grille des texels: sans cela les bords des
//  ombres scintillent des que la camera bouge.
// ---------------------------------------------------------------------------
class ShadowMap
{
public:
    ShadowMap();
    ~ShadowMap();

    bool Init(int resolution);
    void Destroy();

    bool IsAvailable() const { return m_ok; }
    bool Enabled() const { return m_ok && m_enabled; }
    void SetEnabled(bool v) { m_enabled = v; }
    void Toggle() { m_enabled = !m_enabled; }

    // Prepare les matrices et commence la passe de profondeur
    void Begin(const Vector3f& center, const Vector3f& sunDir);
    void End(int viewportWidth, int viewportHeight);

    const Shader& DepthShader() const { return m_depth; }

    // Matrice monde -> espace lumiere (projection * vue)
    const float* LightMatrix() const { return m_lightMatrix; }

    void BindTexture(GLenum unit) const;

    float Radius() const { return m_radius; }
    int   Resolution() const { return m_resolution; }

private:
    bool   m_ok;
    bool   m_enabled;
    int    m_resolution;
    float  m_radius;

    GLuint m_fbo;
    GLuint m_depthTex;

    Shader m_depth;

    float  m_lightMatrix[16];
};

#endif // SHADOWMAP_H__
