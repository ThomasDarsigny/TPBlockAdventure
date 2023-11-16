#include "blockinfo.h"
#include <iostream>

BlockInfo::BlockInfo(BlockType type, const std::string& name, int durability, bool lightSource)
    : m_type(type), m_durability(durability), m_textureAtlas(16) {}

BlockInfo::~BlockInfo() {}

BlockType BlockInfo::GetType() const {
    return m_type;
}

void BlockInfo::SetDurability(int durability) {
    m_durability = durability;
}

int BlockInfo::GetDurability() const {
    return m_durability;
}

int BlockInfo::GetTextureIndex(int idx) const {
    if (idx >= 0 && idx < 6) {
        return m_textures[idx];
    }
    else {
        return -1;
    }
}

void BlockInfo::GetTextureIndexToCoord(unsigned int idx, float& u, float& v, float& w, float& h) const {
    m_textureAtlas.TextureIndexToCoord(idx, u, v, w, h);
}
