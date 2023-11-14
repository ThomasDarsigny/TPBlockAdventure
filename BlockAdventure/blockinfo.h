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
    int GetDurability() const;

    void Show() const;

    private:
    BlockType m_type;
    std::string m_name;
    int m_durability;

};

#endif // BLOCKINFO_H__
