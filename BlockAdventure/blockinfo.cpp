#include "BlockInfo.h"
#include <iostream> 

BlockInfo::BlockInfo(BlockType type, const std::string& name)
	: m_type(type), m_name(name), m_durability(100), blocku(0.0f), blockv(0.0f), blockh(1.0f), blockw(1.0f)
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

void BlockInfo::SetTexture(float u, float v, float h, float w)
{
	blocku = u;
	blockv = v;
	blockh = h;
	blockw = w;
}

float BlockInfo::GetBlockU() const
{
	return blocku;
}

float BlockInfo::GetBlockV() const
{
	return blockv;
}

float BlockInfo::GetBlockH() const
{
	return blockh;
}

float BlockInfo::GetBlockW() const
{
	return blockw;
}

int BlockInfo::GetDurability() const
{
	return m_durability;
}

void BlockInfo::Show() const
{
	std::cout << "Block Type: " << m_type << ", Name: " << m_name << ", Durability: " << m_durability << std::endl;
}
