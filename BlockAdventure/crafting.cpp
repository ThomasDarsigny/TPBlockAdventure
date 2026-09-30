#include "crafting.h"
#include "blockinfo.h"
#include <algorithm>

namespace
{
    std::vector<Recipe> s_recipes;

    const BlockType _ = BTYPE_AIR;   // case vide, pour rendre les motifs lisibles

    void Shaped(int w, int h, const BlockType* pattern, BlockType result, int count)
    {
        Recipe r;
        r.width = w;
        r.height = h;
        for (int i = 0; i < 9; ++i)
            r.pattern[i] = (i < w * h) ? pattern[i] : BTYPE_AIR;
        r.result = result;
        r.count = count;
        r.shapeless = false;
        s_recipes.push_back(r);
    }

    void Shapeless(const std::vector<BlockType>& items, BlockType result, int count)
    {
        Recipe r;
        r.width = (int)items.size();
        r.height = 1;
        for (int i = 0; i < 9; ++i)
            r.pattern[i] = (i < (int)items.size()) ? items[i] : BTYPE_AIR;
        r.result = result;
        r.count = count;
        r.shapeless = true;
        s_recipes.push_back(r);
    }

    // Motif commun aux quatre familles d'outils. 'M' est le materiau,
    // BTYPE_STICK le manche.
    void ToolRecipes(BlockType material, BlockType pickaxe, BlockType axe,
                     BlockType shovel, BlockType sword)
    {
        const BlockType M = material;
        const BlockType S = BTYPE_STICK;

        const BlockType pick[9]   = { M, M, M,   _, S, _,   _, S, _ };
        const BlockType axeR[9]   = { M, M, _,   M, S, _,   _, S, _ };
        const BlockType shovelR[9]= { _, M, _,   _, S, _,   _, S, _ };
        const BlockType swordR[9] = { _, M, _,   _, M, _,   _, S, _ };

        Shaped(3, 3, pick,    pickaxe, 1);
        Shaped(3, 3, axeR,    axe,     1);
        Shaped(3, 3, shovelR, shovel,  1);
        Shaped(3, 3, swordR,  sword,   1);
    }

    bool s_initialized = false;

    // --- comparaison avec forme -------------------------------------------
    bool MatchShaped(const Recipe& r, const BlockType grid[9], int gridSize)
    {
        if (r.width > gridSize || r.height > gridSize)
            return false;

        for (int oy = 0; oy + r.height <= gridSize; ++oy)
        {
            for (int ox = 0; ox + r.width <= gridSize; ++ox)
            {
                bool ok = true;

                for (int y = 0; y < gridSize && ok; ++y)
                {
                    for (int x = 0; x < gridSize && ok; ++x)
                    {
                        const BlockType inGrid = grid[y * 3 + x];

                        const bool inside = (x >= ox && x < ox + r.width &&
                                             y >= oy && y < oy + r.height);
                        const BlockType wanted = inside
                            ? r.pattern[(y - oy) * r.width + (x - ox)]
                            : BTYPE_AIR;

                        if (inGrid != wanted)
                            ok = false;
                    }
                }

                if (ok)
                    return true;
            }
        }

        return false;
    }

    // --- comparaison sans forme -------------------------------------------
    bool MatchShapeless(const Recipe& r, const BlockType grid[9], int gridSize)
    {
        std::vector<BlockType> have;
        for (int y = 0; y < gridSize; ++y)
            for (int x = 0; x < gridSize; ++x)
                if (grid[y * 3 + x] != BTYPE_AIR)
                    have.push_back(grid[y * 3 + x]);

        std::vector<BlockType> want(r.pattern, r.pattern + r.width);

        if (have.size() != want.size())
            return false;

        std::sort(have.begin(), have.end());
        std::sort(want.begin(), want.end());
        return have == want;
    }
}

namespace Crafting
{

void Init()
{
    if (s_initialized)
        return;
    s_initialized = true;

    // --- bases ---
    Shapeless(std::vector<BlockType>(1, BTYPE_WOOD),       BTYPE_WOODPLANK, 4);
    Shapeless(std::vector<BlockType>(1, BTYPE_BIRCH_LOG),  BTYPE_WOODPLANK, 4);
    Shapeless(std::vector<BlockType>(1, BTYPE_SPRUCE_LOG), BTYPE_WOODPLANK, 4);

    {
        const BlockType sticks[2] = { BTYPE_WOODPLANK, BTYPE_WOODPLANK };
        Shaped(1, 2, sticks, BTYPE_STICK, 4);
    }
    {
        const BlockType table[4] = { BTYPE_WOODPLANK, BTYPE_WOODPLANK,
                                     BTYPE_WOODPLANK, BTYPE_WOODPLANK };
        Shaped(2, 2, table, BTYPE_CRAFTING_TABLE, 1);
    }
    {
        // Charbon au-dessus d'un baton
        const BlockType torch[2] = { BTYPE_COAL, BTYPE_STICK };
        Shaped(1, 2, torch, BTYPE_TORCH, 4);
    }
    {
        const BlockType sandstone[4] = { BTYPE_SAND, BTYPE_SAND, BTYPE_SAND, BTYPE_SAND };
        Shaped(2, 2, sandstone, BTYPE_SANDSTONE, 1);
    }
    {
        const BlockType bricks[4] = { BTYPE_CLAY, BTYPE_CLAY, BTYPE_CLAY, BTYPE_CLAY };
        Shaped(2, 2, bricks, BTYPE_BRICK, 1);
    }
    {
        // La mousse pousse sur la pierre au contact d'une plante
        const BlockType mossy[2] = { BTYPE_COBBLESTONE, BTYPE_TALLGRASS };
        Shapeless(std::vector<BlockType>(mossy, mossy + 2), BTYPE_MOSSY_COBBLE, 1);
    }

    // --- outils ---
    // Faute de four, les outils sont fabriques directement a partir du
    // minerai brut plutot que d'un lingot.
    ToolRecipes(BTYPE_WOODPLANK,   BTYPE_WOOD_PICKAXE,  BTYPE_WOOD_AXE,  BTYPE_WOOD_SHOVEL,  BTYPE_WOOD_SWORD);
    ToolRecipes(BTYPE_COBBLESTONE, BTYPE_STONE_PICKAXE, BTYPE_STONE_AXE, BTYPE_STONE_SHOVEL, BTYPE_STONE_SWORD);
    ToolRecipes(BTYPE_IRON,        BTYPE_IRON_PICKAXE,  BTYPE_IRON_AXE,  BTYPE_IRON_SHOVEL,  BTYPE_IRON_SWORD);
    ToolRecipes(BTYPE_DIAMOND,     BTYPE_DIAM_PICKAXE,  BTYPE_DIAM_AXE,  BTYPE_DIAM_SHOVEL,  BTYPE_DIAM_SWORD);
}

bool Match(const BlockType grid[9], int gridSize, BlockType& result, int& count)
{
    Init();

    // Grille vide: rien a faire
    bool empty = true;
    for (int y = 0; y < gridSize && empty; ++y)
        for (int x = 0; x < gridSize && empty; ++x)
            if (grid[y * 3 + x] != BTYPE_AIR)
                empty = false;

    if (empty)
        return false;

    for (size_t i = 0; i < s_recipes.size(); ++i)
    {
        const Recipe& r = s_recipes[i];

        const bool ok = r.shapeless ? MatchShapeless(r, grid, gridSize)
                                    : MatchShaped(r, grid, gridSize);
        if (ok)
        {
            result = r.result;
            count = r.count;
            return true;
        }
    }

    return false;
}

const std::vector<Recipe>& All()
{
    Init();
    return s_recipes;
}

} // namespace Crafting
