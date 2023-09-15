#ifndef BLOCKINFO_H
#define BLOCKINFO_H

#include "define.h"
#include <string>

class BlockInfo {
public:
    BlockInfo(BlockType type, const std::string& name);
    ~BlockInfo();

    BlockType GetType() const;

    void SetDurability(int durability);
    int GetDurability() const;

    void Show() const;
    int m_durability;

private:
    BlockType m_type;
    std::string m_name;
    
};

#endif 
