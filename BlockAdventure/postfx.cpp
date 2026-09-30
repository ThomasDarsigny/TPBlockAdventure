#include "postfx.h"
#include <iostream>

namespace
{
    GLenum HdrFormat()
    {
#ifdef GL_RGBA16F
        return GL_RGBA16F;
#else
        return GL_RGBA16F_ARB;
#endif
    }
}

PostFX::PostFX()
    : m_ok(false), m_msaa(false), m_width(0), m_height(0), m_bloomW(0), m_bloomH(0), m_samples(0),
      m_msFbo(0), m_msColor(0), m_msDepth(0),
      m_sceneFbo(0), m_sceneTex(0), m_sceneDepth(0),
      m_exposure(1.02f), m_bloom(0.42f), m_saturation(1.04f), m_vignette(0.50f)
{
    m_blurFbo[0] = m_blurFbo[1] = 0;
    m_blurTex[0] = m_blurTex[1] = 0;
}

PostFX::~PostFX()
{
    Destroy();
}

bool PostFX::Init(int width, int height)
{
    Destroy();

    if (!GLEW_ARB_framebuffer_object)
    {
        std::cout << "[PostFX] FBO non supportes, rendu direct" << std::endl;
        return false;
    }

    if (!m_bright.Load(SHADER_PATH "fullscreen.vert", SHADER_PATH "bright.frag", true) ||
        !m_blur.Load(SHADER_PATH "fullscreen.vert", SHADER_PATH "blur.frag", true) ||
        !m_post.Load(SHADER_PATH "fullscreen.vert", SHADER_PATH "post.frag", true))
    {
        std::cout << "[PostFX] Shaders indisponibles, rendu direct" << std::endl;
        Destroy();
        return false;
    }

    if (!CreateTargets(width, height))
    {
        Destroy();
        return false;
    }

    m_ok = true;
    std::cout << "[PostFX] actif (" << width << "x" << height
              << ", MSAA " << (m_msaa ? m_samples : 0) << "x)" << std::endl;
    return true;
}

bool PostFX::CreateTargets(int width, int height)
{
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    m_width = width;
    m_height = height;
    m_bloomW = width / 2 > 1 ? width / 2 : 1;
    m_bloomH = height / 2 > 1 ? height / 2 : 1;

    const GLenum hdr = HdrFormat();

    // --- Cible de resolution (texture, non multi-echantillonnee) ----------
    glGenTextures(1, &m_sceneTex);
    glBindTexture(GL_TEXTURE_2D, m_sceneTex);
    glTexImage2D(GL_TEXTURE_2D, 0, hdr, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &m_sceneFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_sceneTex, 0);

    glGenRenderbuffers(1, &m_sceneDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_sceneDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_sceneDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "[PostFX] FBO de scene incomplet" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    // --- Cible multi-echantillonnee (anti-aliasing) -----------------------
    m_msaa = false;
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    m_samples = (maxSamples >= 4) ? 4 : (maxSamples >= 2 ? 2 : 0);

    if (m_samples > 0)
    {
        glGenFramebuffers(1, &m_msFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_msFbo);

        glGenRenderbuffers(1, &m_msColor);
        glBindRenderbuffer(GL_RENDERBUFFER, m_msColor);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, hdr, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_msColor);

        glGenRenderbuffers(1, &m_msDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_msDepth);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_msDepth);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        {
            m_msaa = true;
        }
        else
        {
            // Pas grave: on se rabat sur un rendu sans MSAA
            glDeleteFramebuffers(1, &m_msFbo);   m_msFbo = 0;
            glDeleteRenderbuffers(1, &m_msColor); m_msColor = 0;
            glDeleteRenderbuffers(1, &m_msDepth); m_msDepth = 0;
        }
    }

    // --- Cibles de flou (demi-resolution) ---------------------------------
    for (int i = 0; i < 2; ++i)
    {
        glGenTextures(1, &m_blurTex[i]);
        glBindTexture(GL_TEXTURE_2D, m_blurTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, hdr, m_bloomW, m_bloomH, 0, GL_RGBA, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &m_blurFbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_blurFbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_blurTex[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "[PostFX] FBO de flou incomplet" << std::endl;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void PostFX::DestroyTargets()
{
    if (m_msFbo)     { glDeleteFramebuffers(1, &m_msFbo);      m_msFbo = 0; }
    if (m_msColor)   { glDeleteRenderbuffers(1, &m_msColor);   m_msColor = 0; }
    if (m_msDepth)   { glDeleteRenderbuffers(1, &m_msDepth);   m_msDepth = 0; }
    if (m_sceneFbo)  { glDeleteFramebuffers(1, &m_sceneFbo);   m_sceneFbo = 0; }
    if (m_sceneTex)  { glDeleteTextures(1, &m_sceneTex);       m_sceneTex = 0; }
    if (m_sceneDepth){ glDeleteRenderbuffers(1, &m_sceneDepth);m_sceneDepth = 0; }

    for (int i = 0; i < 2; ++i)
    {
        if (m_blurFbo[i]) { glDeleteFramebuffers(1, &m_blurFbo[i]); m_blurFbo[i] = 0; }
        if (m_blurTex[i]) { glDeleteTextures(1, &m_blurTex[i]);     m_blurTex[i] = 0; }
    }
}

void PostFX::Destroy()
{
    DestroyTargets();
    m_bright.Destroy();
    m_blur.Destroy();
    m_post.Destroy();
    m_ok = false;
}

void PostFX::Resize(int width, int height)
{
    if (!m_ok) return;
    if (width == m_width && height == m_height) return;

    DestroyTargets();
    if (!CreateTargets(width, height))
    {
        DestroyTargets();
        m_ok = false;
    }
}

void PostFX::BeginScene()
{
    if (!m_ok) return;
    glBindFramebuffer(GL_FRAMEBUFFER, m_msaa ? m_msFbo : m_sceneFbo);
    glViewport(0, 0, m_width, m_height);
}

void PostFX::EndScene()
{
    if (!m_ok) return;

    if (m_msaa)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_msFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_sceneFbo);
        glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostFX::DrawFullscreenQuad()
{
    glBegin(GL_QUADS);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f( 1.0f, -1.0f);
    glVertex2f( 1.0f,  1.0f);
    glVertex2f(-1.0f,  1.0f);
    glEnd();
}

void PostFX::Render(float time, bool underwater, float damage)
{
    if (!m_ok) return;

    const GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullWas = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendWas = glIsEnabled(GL_BLEND);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);

    // --- 1. Extraction des zones brillantes -------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, m_blurFbo[0]);
    glViewport(0, 0, m_bloomW, m_bloomH);
    m_bright.Use();
    m_bright.SetInt("uScene", 0);
    m_bright.SetFloat("uThreshold", 1.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_sceneTex);
    DrawFullscreenQuad();

    // --- 2. Flou horizontal puis vertical ---------------------------------
    m_blur.Use();
    m_blur.SetInt("uScene", 0);

    glBindFramebuffer(GL_FRAMEBUFFER, m_blurFbo[1]);
    m_blur.SetVec2("uDirection", 1.0f / (float)m_bloomW, 0.0f);
    glBindTexture(GL_TEXTURE_2D, m_blurTex[0]);
    DrawFullscreenQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, m_blurFbo[0]);
    m_blur.SetVec2("uDirection", 0.0f, 1.0f / (float)m_bloomH);
    glBindTexture(GL_TEXTURE_2D, m_blurTex[1]);
    DrawFullscreenQuad();

    // --- 3. Composition finale a l'ecran ----------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);

    m_post.Use();
    m_post.SetInt("uScene", 0);
    m_post.SetInt("uBloom", 1);
    m_post.SetFloat("uExposure", m_exposure);
    m_post.SetFloat("uBloomStrength", m_bloom);
    m_post.SetFloat("uSaturation", m_saturation);
    m_post.SetFloat("uVignette", m_vignette);
    m_post.SetFloat("uUnderwater", underwater ? 1.0f : 0.0f);
    m_post.SetFloat("uTime", time);
    m_post.SetFloat("uDamage", damage);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_blurTex[0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_sceneTex);

    DrawFullscreenQuad();

    Shader::Disable();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);

    if (depthWas) glEnable(GL_DEPTH_TEST);
    if (cullWas)  glEnable(GL_CULL_FACE);
    if (blendWas) glEnable(GL_BLEND);
}
