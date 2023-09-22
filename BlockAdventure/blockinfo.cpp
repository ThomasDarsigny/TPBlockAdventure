#include "blockinfo.h"
#include <iostream>

BlockInfo::BlockInfo(BlockType type, const std::string& name) : m_type(type), m_name(name), m_durability(1)
{
}

BlockInfo::~BlockInfo()
{
}

BlockType BlockInfo::GetType() const
{
    return m_type;
}

void BlockInfo::SetDurability(int durability)
{
    m_durability = durability;
}

int BlockInfo::GetDurability() const
{
    return m_durability;
}

void BlockInfo::Show() const
{
    std::cout << "Type: " << m_type << std::endl;
    std::cout << "Nom: " << m_name << std::endl;
    std::cout << "Durabilite: " << m_durability << std::endl;
}


