#ifndef INVENTORY_H__
#define INVENTORY_H__

#include "define.h"
#include <vector>

struct ItemStack
{
    BlockType type;
    int       count;
    int       durability;   // points restants, pour les outils

    ItemStack() : type(BTYPE_AIR), count(0), durability(0) {}
    bool Empty() const { return type == BTYPE_AIR || count <= 0; }
    void Clear() { type = BTYPE_AIR; count = 0; durability = 0; }
};

// ---------------------------------------------------------------------------
//  Inventaire: 9 cases de barre rapide + 27 cases de rangement.
//  Les piles montent a 64 comme dans Minecraft.
// ---------------------------------------------------------------------------
class Inventory
{
public:
    static const int HOTBAR_SIZE = 9;
    static const int STORAGE_SIZE = 27;
    static const int TOTAL_SIZE = HOTBAR_SIZE + STORAGE_SIZE;
    static const int MAX_STACK = 64;

    // Les outils ne s'empilent pas: chacun a sa propre usure.
    static int MaxStack(BlockType type);

    Inventory();

    void Clear();
    void GiveStarterKit();

    // Ajoute des blocs, retourne le nombre reellement range
    int  Add(BlockType type, int count);
    bool Consume(int slot, int count);

    ItemStack&       At(int slot)       { return m_slots[slot]; }
    const ItemStack& At(int slot) const { return m_slots[slot]; }

    int  Selected() const { return m_selected; }
    void Select(int index);
    void Scroll(int direction);

    ItemStack&       SelectedStack()       { return m_slots[m_selected]; }
    const ItemStack& SelectedStack() const { return m_slots[m_selected]; }
    BlockType SelectedType() const { return m_slots[m_selected].type; }

    // Prend le bloc vise (clic molette) et le met dans la barre rapide
    void PickBlock(BlockType type, bool creative);

    // Echange deux cases (utilise par l'ecran d'inventaire)
    void Swap(int a, int b);

    int  Count(BlockType type) const;

    // Retire un point de durabilite a l'outil de la case courante et casse
    // l'outil si elle tombe a zero. Retourne true si l'outil a casse.
    bool DamageSelected();

    // Premiere case libre, ou -1
    int  FirstFree() const;

private:
    ItemStack m_slots[TOTAL_SIZE];
    int m_selected;
};

#endif // INVENTORY_H__
