#include "blockinfo.h"
#include <iostream>

BlockInfo::BlockInfo(BlockType type, const std::string& name, int num)
: m_type(type), m_name(name), m_durability(num) {}

BlockInfo::~BlockInfo() {}

BlockType BlockInfo::GetType() const
{
	return m_type;
}

void BlockInfo::SetDurability(int durability) //Set the durability of the blocks
{
	m_durability = durability;		
}


int BlockInfo::GetDurability() const //To know the durability of the block
{
	return m_durability;
}

void BlockInfo::Show() const
{
	std::cout << "Type: " << m_type << ", Name: " << m_name << ", Durability: " << m_durability << std::endl;
}
