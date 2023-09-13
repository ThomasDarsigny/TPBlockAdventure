#include "blockinfo.h"
#include <iostream>

BlockInfo::BlockInfo(BlockType type, const std::string& name)
	: m_type(type), m_name(name), m_durability(0) {}

BlockInfo::~BlockInfo() {}

BlockType BlockInfo::GetType() const
{
	return m_type;
}

void BlockInfo::SetDurability(int durability) //Set the durability of the blocks
{
	switch (m_type)
	{
	case BTYPE_AIR:
		m_durability = 0;
		break;
	case BTYPE_DIRT:
		m_durability = 3;
		break;
	case BTYPE_GRASS:
		m_durability = 3;
		break;
	}
}


int BlockInfo::GetDurability() const //To know the durability of the block
{
	return m_durability;
}

void BlockInfo::Show() const
{
	std::cout << "Type: " << m_type << ", Name: " << m_name << ", Durability: " << m_durability << std::endl;
}
