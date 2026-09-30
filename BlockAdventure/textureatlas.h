#ifndef TEXTUREATLAS_H__
#define TEXTUREATLAS_H__

#include "define.h"
#include <string>
#include <map>
#include <vector>
#include <IL/ilu.h>

// ---------------------------------------------------------------------------
//  Atlas de textures.
//
//  Deux sources possibles:
//    - AddTexture()           : un fichier image sur disque
//    - AddTextureFromMemory() : des pixels RGBA generes proceduralement
//
//  Generate() assemble tout dans une seule texture OpenGL avec une chaine de
//  mipmaps construite tuile par tuile (chaque niveau est reduit
//  independamment), ce qui evite le "bleeding" entre tuiles voisines tout en
//  supprimant le scintillement au loin.
// ---------------------------------------------------------------------------
class TextureAtlas
{
public:
    typedef unsigned int TextureIndex;

    // Niveau de mipmap maximal conserve, et retrait applique aux coordonnees
    // de chaque tuile (en fraction de tuile) pour eviter le debordement.
    static const int MAX_MIP_LEVEL = 1;
    static const float TILE_INSET;

public:
    TextureAtlas(unsigned int nbTexture);
    ~TextureAtlas();

    TextureIndex AddTexture(const std::string& fname);
    TextureIndex AddTextureFromMemory(const std::string& key, int width, int height,
                                      const unsigned char* rgba);

    bool Generate(int textureSize, bool mipmap);

    bool IsValid() const;
    void Bind() const;

    void TextureIndexToCoord(TextureIndex idx, float& u, float& v, float& w, float& h) const;

    unsigned int TilesPerSide() const { return m_nbTexturePerSide; }
    unsigned int TextureCount() const { return m_currentTextureIndex; }

private:
    static bool IsPowerOfTwo(unsigned int x)
    {
        return ((x != 0) && ((x & (~x + 1)) == x));
    }

private:
    struct TextureInfo
    {
        ILuint texId;
        TextureIndex texIdx;
        bool fromMemory;
        int memWidth;
        int memHeight;
        std::vector<unsigned char> memData;

        TextureInfo(ILuint _texId, unsigned int _texIdx)
            : texId(_texId), texIdx(_texIdx), fromMemory(false), memWidth(0), memHeight(0) {}
    };

    typedef std::map<std::string, TextureInfo> TextureList;
    TextureList m_textureList;

    TextureIndex m_currentTextureIndex;
    GLuint  m_textureId;
    bool    m_isValid;
    unsigned int m_nbTexturePerSide;
    int     m_tileSize;
};

#endif // TEXTUREATLAS_H__
