#include "inventory.h"
#include "blockinfo.h"

int Inventory::MaxStack(BlockType type)
{
    return Blocks::IsTool(type) ? 1 : MAX_STACK;
}

Inventory::Inventory() : m_selected(0)
{
}

void Inventory::Clear()
{
    for (int i = 0; i < TOTAL_SIZE; ++i)
        m_slots[i].Clear();
    m_selected = 0;
}

void Inventory::GiveStarterKit()
{
    Clear();

    static const BlockType kStarter[] =
    {
        BTYPE_WOODPLANK, BTYPE_COBBLESTONE, BTYPE_TORCH, BTYPE_GLASS,
        BTYPE_WOOD, BTYPE_SAND, BTYPE_GLOWSTONE
    };

    for (int i = 0; i < (int)(sizeof(kStarter) / sizeof(kStarter[0])); ++i)
        Add(kStarter[i], 32);
}

int Inventory::Add(BlockType type, int count)
{
    if (type == BTYPE_AIR || count <= 0)
        return 0;

    const int maxStack = MaxStack(type);
    int remaining = count;

    // On complete d'abord les piles existantes, barre rapide en premier
    for (int i = 0; i < TOTAL_SIZE && remaining > 0; ++i)
    {
        if (m_slots[i].type != type || m_slots[i].count >= maxStack)
            continue;

        const int room = maxStack - m_slots[i].count;
        const int take = (remaining < room) ? remaining : room;
        m_slots[i].count += take;
        remaining -= take;
    }

    for (int i = 0; i < TOTAL_SIZE && remaining > 0; ++i)
    {
        if (!m_slots[i].Empty())
            continue;

        const int take = (remaining < maxStack) ? remaining : maxStack;
        m_slots[i].type = type;
        m_slots[i].count = take;
        m_slots[i].durability = Blocks::Get(type).maxDurability;
        remaining -= take;
    }

    return count - remaining;
}

bool Inventory::Consume(int slot, int count)
{
    if (slot < 0 || slot >= TOTAL_SIZE)
        return false;
    if (m_slots[slot].count < count)
        return false;

    m_slots[slot].count -= count;
    if (m_slots[slot].count <= 0)
        m_slots[slot].Clear();

    return true;
}

void Inventory::Select(int index)
{
    if (index < 0) index = 0;
    if (index >= HOTBAR_SIZE) index = HOTBAR_SIZE - 1;
    m_selected = index;
}

void Inventory::Scroll(int direction)
{
    m_selected = (m_selected + direction) % HOTBAR_SIZE;
    if (m_selected < 0) m_selected += HOTBAR_SIZE;
}

void Inventory::PickBlock(BlockType type, bool creative)
{
    if (type == BTYPE_AIR)
        return;

    // Deja dans la barre rapide ? on selectionne simplement la case
    for (int i = 0; i < HOTBAR_SIZE; ++i)
        if (m_slots[i].type == type)
        {
            m_selected = i;
            return;
        }

    if (!creative)
    {
        // En survie, on ne peut prendre que ce que l'on possede
        for (int i = HOTBAR_SIZE; i < TOTAL_SIZE; ++i)
            if (m_slots[i].type == type)
            {
                Swap(i, m_selected);
                return;
            }
        return;
    }

    m_slots[m_selected].type = type;
    m_slots[m_selected].count = MaxStack(type);
    m_slots[m_selected].durability = Blocks::Get(type).maxDurability;
}

void Inventory::Swap(int a, int b)
{
    if (a < 0 || b < 0 || a >= TOTAL_SIZE || b >= TOTAL_SIZE)
        return;

    const ItemStack tmp = m_slots[a];
    m_slots[a] = m_slots[b];
    m_slots[b] = tmp;
}

bool Inventory::DamageSelected()
{
    ItemStack& st = m_slots[m_selected];
    if (st.Empty() || !Blocks::IsTool(st.type))
        return false;

    if (--st.durability > 0)
        return false;

    st.Clear();
    return true;
}

int Inventory::FirstFree() const
{
    for (int i = 0; i < TOTAL_SIZE; ++i)
        if (m_slots[i].Empty())
            return i;
    return -1;
}

int Inventory::Count(BlockType type) const
{
    int total = 0;
    for (int i = 0; i < TOTAL_SIZE; ++i)
        if (m_slots[i].type == type)
            total += m_slots[i].count;
    return total;
}
