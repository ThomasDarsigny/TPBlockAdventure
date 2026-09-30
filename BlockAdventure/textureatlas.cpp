#include "textureatlas.h"
#include <cmath>
#include <iostream>
#include <cassert>
#include "tool.h"

const float TextureAtlas::TILE_INSET = 0.040f;

TextureAtlas::TextureAtlas(unsigned int nbTexture)
    : m_currentTextureIndex(0), m_textureId(0), m_isValid(false), m_tileSize(0)
{
    if (nbTexture < 4)
        nbTexture = 4;

    // Arrondir sur la puissance de 2 superieure
    m_nbTexturePerSide = (unsigned int)sqrt((float)nbTexture);
    if (m_nbTexturePerSide * m_nbTexturePerSide < nbTexture)
        m_nbTexturePerSide++;
    while (!IsPowerOfTwo(m_nbTexturePerSide))
        m_nbTexturePerSide++;
}

TextureAtlas::~TextureAtlas()
{
    if (IsValid())
        glDeleteTextures(1, &m_textureId);
}

TextureAtlas::TextureIndex TextureAtlas::AddTexture(const std::string& fname)
{
    TextureList::iterator it = m_textureList.find(fname);
    if (it != m_textureList.end())
        return it->second.texIdx;

    TextureIndex id = m_currentTextureIndex++;
    m_textureList.insert(std::make_pair(fname, TextureInfo((ILuint)-1, id)));
    return id;
}

TextureAtlas::TextureIndex TextureAtlas::AddTextureFromMemory(const std::string& key, int width,
                                                              int height, const unsigned char* rgba)
{
    TextureList::iterator it = m_textureList.find(key);
    if (it != m_textureList.end())
        return it->second.texIdx;

    TextureIndex id = m_currentTextureIndex++;
    TextureInfo info((ILuint)-1, id);
    info.fromMemory = true;
    info.memWidth = width;
    info.memHeight = height;
    info.memData.assign(rgba, rgba + (size_t)width * height * 4);
    m_textureList.insert(std::make_pair(key, info));
    return id;
}

bool TextureAtlas::Generate(int textureSize, bool mipmap)
{
    if (!IsPowerOfTwo((unsigned int)textureSize))
        return false;

    if (m_currentTextureIndex > m_nbTexturePerSide * m_nbTexturePerSide)
    {
        std::cerr << "[TextureAtlas] Trop de textures (" << m_currentTextureIndex
                  << ") pour un atlas de " << m_nbTexturePerSide << "x" << m_nbTexturePerSide
                  << std::endl;
        return false;
    }

    m_tileSize = textureSize;

    // DevIL n'est initialise qu'une seule fois
    static bool alreadyInitialized = false;
    if (!alreadyInitialized)
    {
        ilInit();
        iluInit();
        alreadyInitialized = true;
    }

    // On veut du pixel art bien net: pas d'interpolation lors du redimensionnement
    iluImageParameter(ILU_FILTER, ILU_NEAREST);

    for (TextureList::iterator it = m_textureList.begin(); it != m_textureList.end(); ++it)
    {
        ILuint texid = it->second.texId;
        if (texid != (ILuint)-1)
            continue;

        ilGenImages(1, &texid);
        ilBindImage(texid);
        ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
        ilEnable(IL_ORIGIN_SET);

        if (it->second.fromMemory)
        {
            ilTexImage(it->second.memWidth, it->second.memHeight, 1, 4,
                       IL_RGBA, IL_UNSIGNED_BYTE, (void*)it->second.memData.data());
        }
        else
        {
            std::cout << "Loading " << it->first << " (id=" << it->second.texIdx << ")..." << std::endl;
            if (!ilLoadImage((const ILstring)it->first.c_str()))
            {
                std::cerr << "[TextureAtlas] Echec du chargement de " << it->first << std::endl;
                return false;
            }
        }

        if (!ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE))
            return false;

        iluScale(textureSize, textureSize, 1);
        it->second.texId = texid;
    }

    glGenTextures(1, &m_textureId);
    glBindTexture(GL_TEXTURE_2D, m_textureId);

    // Magnification en NEAREST: on garde l'esthetique "gros pixels".
    // Minification mipmappee: supprime le scintillement des blocs lointains.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    mipmap ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Pas de filtrage anisotrope: aux angles rasants il irait chercher des
    // texels bien au-dela de la tuile courante et ferait baver les couleurs
    // des blocs voisins de l'atlas.

    int level = textureSize;
    int oglLevel = 0;
    int mipmapSize = textureSize * (int)m_nbTexturePerSide;

    while (level >= 1)
    {
        ILuint atlasTex;
        ilGenImages(1, &atlasTex);
        ilBindImage(atlasTex);
        ilTexImage(mipmapSize, mipmapSize, 1, 4, IL_RGBA, IL_UNSIGNED_BYTE, 0);
        ilClearColour(0, 0, 0, 0);
        ilClearImage();

        std::vector<unsigned char> data((size_t)level * level * 4);

        for (TextureList::iterator it = m_textureList.begin(); it != m_textureList.end(); ++it)
        {
            ILuint tmpImg;
            ilGenImages(1, &tmpImg);
            ilBindImage(tmpImg);
            ilCopyImage(it->second.texId);

            // Chaque tuile est reduite independamment: aucune couleur ne peut
            // deborder sur la tuile voisine dans les niveaux de mipmap.
            iluImageParameter(ILU_FILTER, level > 4 ? ILU_BILINEAR : ILU_NEAREST);
            if (level != textureSize)
                iluScale(level, level, 1);

            ilCopyPixels(0, 0, 0, level, level, 1, IL_RGBA, IL_UNSIGNED_BYTE, data.data());

            int imgIdx = (int)it->second.texIdx;
            int x = imgIdx % (int)m_nbTexturePerSide;
            int y = (int)m_nbTexturePerSide - 1 - imgIdx / (int)m_nbTexturePerSide;

            ilBindImage(atlasTex);
            ilSetPixels(x * level, y * level, 0, level, level, 1, IL_RGBA, IL_UNSIGNED_BYTE, data.data());

            ilDeleteImages(1, &tmpImg);
        }

        glTexImage2D(GL_TEXTURE_2D, oglLevel, GL_RGBA, mipmapSize, mipmapSize, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, ilGetData());
        CHECK_GL_ERROR();

        ilDeleteImages(1, &atlasTex);

        if (!mipmap)
            break;

        ++oglLevel;
        level /= 2;
        mipmapSize /= 2;
    }

    // oglLevel a ete incremente apres le dernier niveau ecrit: le plus haut
    // niveau reellement defini est donc oglLevel - 1. Annoncer un niveau de
    // plus rendrait la texture "incomplete" et l'echantillonnage renverrait
    // du noir.
    // On s'arrete a MAX_MIP_LEVEL: au-dela les tuiles deviennent si petites
    // que le moindre arrondi de coordonnee mord sur la tuile voisine.
    int lastLevel = mipmap ? (oglLevel > 0 ? oglLevel - 1 : 0) : 0;
    if (lastLevel > MAX_MIP_LEVEL) lastLevel = MAX_MIP_LEVEL;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, lastLevel);

    // Les images sources ne servent plus a rien une fois l'atlas construit
    for (TextureList::iterator it = m_textureList.begin(); it != m_textureList.end(); ++it)
    {
        if (it->second.texId != (ILuint)-1)
            ilDeleteImages(1, &it->second.texId);
        it->second.texId = (ILuint)-1;
        it->second.memData.clear();
        it->second.memData.shrink_to_fit();
    }

    m_isValid = true;
    return true;
}

bool TextureAtlas::IsValid() const
{
    return m_isValid;
}

void TextureAtlas::Bind() const
{
    assert(IsValid());
    glBindTexture(GL_TEXTURE_2D, m_textureId);
}

void TextureAtlas::TextureIndexToCoord(TextureIndex idx, float& u, float& v, float& w, float& h) const
{
    const float tile = 1.f / (float)m_nbTexturePerSide;

    u = (float)((unsigned int)idx % m_nbTexturePerSide) * tile;
    v = (float)(m_nbTexturePerSide - 1 - (unsigned int)idx / m_nbTexturePerSide) * tile;
    w = tile;
    h = tile;

    // Retrait de quelques texels sur chaque bord. En magnification NEAREST
    // cela ne change rien a l'image (on reste dans le meme pixel source),
    // mais cela empeche les tuiles voisines de baver dans les mipmaps.
    const float inset = tile * TILE_INSET;
    u += inset;
    v += inset;
    w -= 2.0f * inset;
    h -= 2.0f * inset;
}
