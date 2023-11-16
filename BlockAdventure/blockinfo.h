#ifndef BLOCKINFO_H
#define BLOCKINFO_H

#include <string>
#include "define.h"
#include "textureatlas.h"

class TextureAtlas; 

class BlockInfo
{
public:
    BlockInfo(BlockType type, const std::string& name, int durability, bool lightSource);
    ~BlockInfo();

    BlockType GetType() const;

    void SetDurability(int durability);

    int GetDurability() const;
    int GetTextureIndex(int idx) const;
    void GetTextureIndexToCoord(unsigned int idx, float& u, float& v, float& w, float& h) const;

    BlockType m_type;
    int m_textureCount;
    TextureAtlas::TextureIndex* m_textures;

private:
    TextureAtlas m_textureAtlas;
    int m_durability;
};

#endif // BLOCKINFO_H
