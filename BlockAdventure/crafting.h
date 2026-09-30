#ifndef CRAFTING_H__
#define CRAFTING_H__

#include "define.h"
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
//  Recettes de fabrication.
//
//  Une recette est soit "avec forme" (le motif compte, et il peut etre place
//  n'importe ou dans la grille), soit "sans forme" (seule la liste des
//  ingredients compte). Le motif est stocke ligne par ligne en partant du
//  haut, BTYPE_AIR marquant une case vide.
// ---------------------------------------------------------------------------
struct Recipe
{
    int       width;
    int       height;
    BlockType pattern[9];
    BlockType result;
    int       count;
    bool      shapeless;
};

namespace Crafting
{
    // grid contient 3x3 cases (ligne 0 en haut). gridSize vaut 2 ou 3 et
    // indique la partie reellement utilisable.
    bool Match(const BlockType grid[9], int gridSize, BlockType& result, int& count);

    const std::vector<Recipe>& All();

    // Recettes que le joueur peut fabriquer avec ce qu'il possede, pour
    // l'aide-memoire affiche dans l'inventaire.
    void Init();
}

#endif // CRAFTING_H__
