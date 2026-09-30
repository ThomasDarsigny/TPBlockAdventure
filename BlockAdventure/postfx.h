#ifndef POSTFX_H__
#define POSTFX_H__

#include "define.h"
#include "shader.h"

// ---------------------------------------------------------------------------
//  Chaine de post-traitement.
//
//    scene (HDR, multi-echantillonnee)
//        -> resolution MSAA
//        -> extraction des zones brillantes (demi-resolution)
//        -> flou gaussien horizontal puis vertical
//        -> composition finale: bloom + ACES + saturation + vignettage
//
//  Si le materiel ne supporte pas les FBO (ou si la creation echoue), tout est
//  desactive proprement et le rendu se fait directement a l'ecran.
// ---------------------------------------------------------------------------
class PostFX
{
public:
    PostFX();
    ~PostFX();

    bool Init(int width, int height);
    void Destroy();
    void Resize(int width, int height);

    bool IsAvailable() const { return m_ok; }

    void BeginScene();
    void EndScene();
    void Render(float time, bool underwater, float damage);

    void  SetExposure(float v)   { m_exposure = v; }
    float Exposure() const       { return m_exposure; }
    void  SetBloom(float v)      { m_bloom = v; }
    float Bloom() const          { return m_bloom; }
    void  SetSaturation(float v) { m_saturation = v; }
    float Saturation() const     { return m_saturation; }
    void  SetVignette(float v)   { m_vignette = v; }

    static void DrawFullscreenQuad();

private:
    bool CreateTargets(int width, int height);
    void DestroyTargets();

private:
    bool m_ok;
    bool m_msaa;

    int m_width, m_height;
    int m_bloomW, m_bloomH;
    int m_samples;

    GLuint m_msFbo, m_msColor, m_msDepth;
    GLuint m_sceneFbo, m_sceneTex, m_sceneDepth;
    GLuint m_blurFbo[2], m_blurTex[2];

    Shader m_bright;
    Shader m_blur;
    Shader m_post;

    float m_exposure;
    float m_bloom;
    float m_saturation;
    float m_vignette;
};

#endif // POSTFX_H__
