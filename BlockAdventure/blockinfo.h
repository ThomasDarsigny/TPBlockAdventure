#ifndef BLOCKINFO_H__
#define BLOCKINFO_H__

#include <string>
#include "define.h"

class BlockInfo
{
public:
	BlockInfo(BlockType type, const std::string& name);
	~BlockInfo();

	BlockType GetType() const;

	void SetDurability(int durability);
	void SetTexture(float u, float v, float h, float w);

	int GetDurability() const;

	// Change the return type to float for texture coordinates
	float GetBlockU() const;
	float GetBlockV() const;
	float GetBlockH() const;
	float GetBlockW() const;

	void Show() const;

private:
	BlockType m_type;
	std::string m_name;
	int m_durability;
	float blocku;
	float blockv;
	float blockh;
	float blockw;
};

#endif // BLOCKINFO_H__