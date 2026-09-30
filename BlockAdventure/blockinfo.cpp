#include "blockinfo.h"
#include "textureatlas.h"
#include "proctex.h"
#include <iostream>

BlockInfo::BlockInfo()
    : type(BTYPE_AIR), name("air"), render(RENDER_NONE), transparency(TRANSP_CUTOUT),
      solid(false), opaque(false), liquid(false), breakable(true),
      replaceable(true), gravity(false), isItem(false),
      tool(TOOL_NONE), requiredTier(TIER_NONE),
      toolType(TOOL_NONE), toolTier(TIER_NONE), maxDurability(0), foodValue(0.0f),
      emission(0), opacity(0), hardness(0.0f), drop(BTYPE_AIR)
{
    for (int f = 0; f < 6; ++f)
        uv[f][0] = uv[f][1] = uv[f][2] = uv[f][3] = 0.0f;
}

namespace
{
    // Index d'atlas retenus entre RegisterTextures() et ComputeUVs()
    TextureAtlas::TextureIndex s_texIndex[BTYPE_FIN][6];
    std::vector<BlockType> s_palette;

    // "quelquechose.png" -> fichier ; sinon -> texture generee proceduralement
    TextureAtlas::TextureIndex Resolve(TextureAtlas& atlas, const std::string& texName)
    {
        if (texName.size() > 4 && texName.compare(texName.size() - 4, 4, ".png") == 0)
            return atlas.AddTexture(std::string(TEXTURE_PATH) + texName);

        std::vector<unsigned char> pixels;
        if (!ProcTex::Generate(texName, pixels))
        {
            std::cerr << "[Blocks] Texture inconnue: " << texName << std::endl;
            // damier magenta pour rendre l'oubli evident a l'ecran
            pixels.assign(ProcTex::SIZE * ProcTex::SIZE * 4, 0);
            for (int y = 0; y < ProcTex::SIZE; ++y)
                for (int x = 0; x < ProcTex::SIZE; ++x)
                {
                    unsigned char* p = pixels.data() + (y * ProcTex::SIZE + x) * 4;
                    bool c = ((x / 4) + (y / 4)) % 2 == 0;
                    p[0] = c ? 255 : 0; p[1] = 0; p[2] = c ? 255 : 0; p[3] = 255;
                }
        }
        return atlas.AddTextureFromMemory(texName, ProcTex::SIZE, ProcTex::SIZE, pixels.data());
    }

    struct Def
    {
        BlockType type;
        const char* name;
        const char* texTop;
        const char* texBottom;
        const char* texSide;
        BlockRenderType render;
        BlockTransparency transparency;
        bool solid;
        bool opaque;
        bool liquid;
        unsigned char emission;
        unsigned char opacity;
        float hardness;
        BlockType drop;
        bool inPalette;
    };

    // Raccourcis de lisibilite
    #define CUBE  RENDER_CUBE
    #define CROSS RENDER_CROSS
    #define LIQ   RENDER_LIQUID
    #define TORCH RENDER_TORCH

    const Def kDefs[] =
    {
        //  type                 nom              top                bottom             cote              render  transp        solid opaq liq emi opa hard  drop                palette
        { BTYPE_DIRT,           "Terre",         "dirt.png",        "dirt.png",        "dirt.png",        CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 0.75f, BTYPE_DIRT,        true  },
        { BTYPE_GRASS,          "Herbe",         "topgrass.png",    "dirt.png",        "sidegrass.png",   CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 0.90f, BTYPE_DIRT,        true  },
        { BTYPE_PODZOL,         "Podzol",        "podzol_top",      "dirt.png",        "podzol_side",     CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 0.90f, BTYPE_DIRT,        true  },
        { BTYPE_STONE,          "Pierre",        "stone.png",       "stone.png",       "stone.png",       CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 2.25f, BTYPE_COBBLESTONE, true  },
        { BTYPE_COBBLESTONE,    "Pierre taillee","cobblestone",     "cobblestone",     "cobblestone",     CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 2.50f, BTYPE_COBBLESTONE, true  },
        { BTYPE_MOSSY_COBBLE,   "Pierre moussue","mossy_cobble",    "mossy_cobble",    "mossy_cobble",    CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 2.50f, BTYPE_MOSSY_COBBLE,true  },
        { BTYPE_BRICK,          "Briques",       "brick",           "brick",           "brick",           CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 2.75f, BTYPE_BRICK,       true  },
        { BTYPE_BEDROCK,        "Bedrock",       "bedrock.png",     "bedrock.png",     "bedrock.png",     CUBE,  TRANSP_NONE,   true, true, false, 0, 15, -1.0f, BTYPE_BEDROCK,     false },
        { BTYPE_SAND,           "Sable",         "sand.png",        "sand.png",        "sand.png",        CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 0.60f, BTYPE_SAND,        true  },
        { BTYPE_SANDSTONE,      "Gres",          "sandstone_top",   "sandstone_top",   "sandstone_side",  CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 1.60f, BTYPE_SANDSTONE,   true  },
        { BTYPE_GRAVEL,         "Gravier",       "gravel",          "gravel",          "gravel",          CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 0.75f, BTYPE_GRAVEL,      true  },
        { BTYPE_CLAY,           "Argile",        "clay",            "clay",            "clay",            CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 0.90f, BTYPE_CLAY,        true  },
        { BTYPE_SNOW,           "Neige",         "snow",            "snow",            "snow",            CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 0.35f, BTYPE_SNOW,        true  },
        { BTYPE_SNOW_GRASS,     "Herbe enneigee","snow",            "dirt.png",        "snow_grass_side", CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 0.90f, BTYPE_DIRT,        true  },
        { BTYPE_OBSIDIAN,       "Obsidienne",    "obsidian",        "obsidian",        "obsidian",        CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 9.00f, BTYPE_OBSIDIAN,    true  },

        { BTYPE_WOOD,           "Tronc de chene","topwood.png",     "topwood.png",     "sidewood.png",    CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 1.50f, BTYPE_WOOD,        true  },
        { BTYPE_BIRCH_LOG,      "Tronc bouleau", "birch_log_top",   "birch_log_top",   "birch_log_side",  CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 1.50f, BTYPE_BIRCH_LOG,   true  },
        { BTYPE_SPRUCE_LOG,     "Tronc sapin",   "topwood.png",     "topwood.png",     "spruce_log_side", CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 1.50f, BTYPE_SPRUCE_LOG,  true  },
        { BTYPE_WOODPLANK,      "Planches",      "woodplank.png",   "woodplank.png",   "woodplank.png",   CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 1.30f, BTYPE_WOODPLANK,   true  },
        { BTYPE_CRAFTING_TABLE, "Etabli",        "crafting_top",    "woodplank.png",   "crafting_side",   CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 1.60f, BTYPE_CRAFTING_TABLE, true },
        { BTYPE_LEAVES,         "Feuilles",      "leaves",          "leaves",          "leaves",          CUBE,  TRANSP_CUTOUT, true, false,false, 0,  1, 0.25f, BTYPE_LEAVES,      true  },
        { BTYPE_BIRCH_LEAVES,   "Feuilles bouleau","birch_leaves",  "birch_leaves",    "birch_leaves",    CUBE,  TRANSP_CUTOUT, true, false,false, 0,  1, 0.25f, BTYPE_BIRCH_LEAVES,true  },
        { BTYPE_SPRUCE_LEAVES,  "Feuilles sapin","spruce_leaves",   "spruce_leaves",   "spruce_leaves",   CUBE,  TRANSP_CUTOUT, true, false,false, 0,  1, 0.25f, BTYPE_SPRUCE_LEAVES,true },
        { BTYPE_CACTUS,         "Cactus",        "cactus_top",      "cactus_top",      "cactus_side",     CUBE,  TRANSP_CUTOUT, true, false,false, 0,  1, 0.55f, BTYPE_CACTUS,      true  },
        { BTYPE_PUMPKIN,        "Citrouille",    "pumpkin_top",     "pumpkin_top",     "pumpkin_side",    CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 1.00f, BTYPE_PUMPKIN,     true  },

        { BTYPE_COAL,           "Charbon",       "coal.png",        "coal.png",        "coal.png",        CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 3.00f, BTYPE_COAL,        true  },
        { BTYPE_IRON,           "Fer",           "iron.png",        "iron.png",        "iron.png",        CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 3.00f, BTYPE_IRON,        true  },
        { BTYPE_GOLD,           "Or",            "gold.png",        "gold.png",        "gold.png",        CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 3.00f, BTYPE_GOLD,        true  },
        { BTYPE_DIAMOND,        "Diamant",       "diamond.png",     "diamond.png",     "diamond.png",     CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 3.50f, BTYPE_DIAMOND,     true  },
        { BTYPE_REDSTONE,       "Redstone",      "redstone_ore",    "redstone_ore",    "redstone_ore",    CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 3.00f, BTYPE_REDSTONE,    true  },
        { BTYPE_EMERALD,        "Emeraude",      "emerald_ore",     "emerald_ore",     "emerald_ore",     CUBE,  TRANSP_NONE,   true, true, false, 0, 15, 3.50f, BTYPE_EMERALD,     true  },

        { BTYPE_GLASS,          "Verre",         "glass",           "glass",           "glass",           CUBE,  TRANSP_BLEND,  true, false,false, 0,  0, 0.30f, BTYPE_GLASS,       true  },
        { BTYPE_ICE,            "Glace",         "ice",             "ice",             "ice",             CUBE,  TRANSP_BLEND,  true, false,false, 0,  2, 0.50f, BTYPE_ICE,         true  },
        { BTYPE_SLIME,          "Slime",         "slime.png",       "slime.png",       "slime.png",       CUBE,  TRANSP_BLEND,  true, false,false, 0,  1, 0.40f, BTYPE_SLIME,       true  },
        { BTYPE_GLOWSTONE,      "Pierre lumineuse","glowstone",     "glowstone",       "glowstone",       CUBE,  TRANSP_NONE,   true, true, false,15, 15, 0.35f, BTYPE_GLOWSTONE,   true  },
        { BTYPE_TORCH,          "Torche",        "torch",           "torch",           "torch",           TORCH, TRANSP_CUTOUT, false,false,false,14,  0, 0.05f, BTYPE_TORCH,       true  },

        { BTYPE_WATER,          "Eau",           "water",           "water",           "water",           LIQ,   TRANSP_BLEND,  false,false,true,  0,  2, -1.0f, BTYPE_AIR,         true  },
        { BTYPE_LAVA,           "Lave",          "lava",            "lava",            "lava",            LIQ,   TRANSP_BLEND,  false,false,true, 15,  1, -1.0f, BTYPE_AIR,         true  },

        { BTYPE_TALLGRASS,      "Herbe haute",   "tallgrass",       "tallgrass",       "tallgrass",       CROSS, TRANSP_CUTOUT, false,false,false, 0,  0, 0.05f, BTYPE_TALLGRASS,   true  },
        { BTYPE_DEADBUSH,       "Buisson mort",  "deadbush",        "deadbush",        "deadbush",        CROSS, TRANSP_CUTOUT, false,false,false, 0,  0, 0.05f, BTYPE_DEADBUSH,    true  },
        { BTYPE_FLOWER_RED,     "Coquelicot",    "flower_red",      "flower_red",      "flower_red",      CROSS, TRANSP_CUTOUT, false,false,false, 0,  0, 0.05f, BTYPE_FLOWER_RED,  true  },
        { BTYPE_FLOWER_YELLOW,  "Pissenlit",     "flower_yellow",   "flower_yellow",   "flower_yellow",   CROSS, TRANSP_CUTOUT, false,false,false, 0,  0, 0.05f, BTYPE_FLOWER_YELLOW,true },
        { BTYPE_MUSHROOM_RED,   "Champignon rouge","mushroom_red",  "mushroom_red",    "mushroom_red",    CROSS, TRANSP_CUTOUT, false,false,false, 1,  0, 0.05f, BTYPE_MUSHROOM_RED,true  },
        { BTYPE_MUSHROOM_BROWN, "Champignon brun","mushroom_brown", "mushroom_brown",  "mushroom_brown",  CROSS, TRANSP_CUTOUT, false,false,false, 1,  0, 0.05f, BTYPE_MUSHROOM_BROWN,true},
    };

    #undef CUBE
    #undef CROSS
    #undef LIQ
    #undef TORCH

    const int kDefCount = (int)(sizeof(kDefs) / sizeof(kDefs[0]));

    // ------------------------------------------------------------------
    //  Objets: ils occupent une case d'inventaire mais ne se posent pas.
    // ------------------------------------------------------------------
    struct ItemDef
    {
        BlockType   type;
        const char* name;
        const char* tex;
        ToolType    toolType;
        int         tier;
        int         durability;
    };

    const ItemDef kItems[] =
    {
        { BTYPE_STICK,         "Baton",             "stick",                 TOOL_NONE,    TIER_NONE,      0 },
        { BTYPE_PORKCHOP,      "Porc",              "porkchop",              TOOL_NONE,    TIER_NONE,      0 },
        { BTYPE_BEEF,          "Boeuf",             "beef",                  TOOL_NONE,    TIER_NONE,      0 },

        { BTYPE_WOOD_PICKAXE,  "Pioche en bois",    "tool_pickaxe_wood",     TOOL_PICKAXE, TIER_WOOD,     60 },
        { BTYPE_WOOD_AXE,      "Hache en bois",     "tool_axe_wood",         TOOL_AXE,     TIER_WOOD,     60 },
        { BTYPE_WOOD_SHOVEL,   "Pelle en bois",     "tool_shovel_wood",      TOOL_SHOVEL,  TIER_WOOD,     60 },
        { BTYPE_WOOD_SWORD,    "Epee en bois",      "tool_sword_wood",       TOOL_SWORD,   TIER_WOOD,     60 },

        { BTYPE_STONE_PICKAXE, "Pioche en pierre",  "tool_pickaxe_stone",    TOOL_PICKAXE, TIER_STONE,   132 },
        { BTYPE_STONE_AXE,     "Hache en pierre",   "tool_axe_stone",        TOOL_AXE,     TIER_STONE,   132 },
        { BTYPE_STONE_SHOVEL,  "Pelle en pierre",   "tool_shovel_stone",     TOOL_SHOVEL,  TIER_STONE,   132 },
        { BTYPE_STONE_SWORD,   "Epee en pierre",    "tool_sword_stone",      TOOL_SWORD,   TIER_STONE,   132 },

        { BTYPE_IRON_PICKAXE,  "Pioche en fer",     "tool_pickaxe_iron",     TOOL_PICKAXE, TIER_IRON,    251 },
        { BTYPE_IRON_AXE,      "Hache en fer",      "tool_axe_iron",         TOOL_AXE,     TIER_IRON,    251 },
        { BTYPE_IRON_SHOVEL,   "Pelle en fer",      "tool_shovel_iron",      TOOL_SHOVEL,  TIER_IRON,    251 },
        { BTYPE_IRON_SWORD,    "Epee en fer",       "tool_sword_iron",       TOOL_SWORD,   TIER_IRON,    251 },

        { BTYPE_DIAM_PICKAXE,  "Pioche en diamant", "tool_pickaxe_diamond",  TOOL_PICKAXE, TIER_DIAMOND, 1562 },
        { BTYPE_DIAM_AXE,      "Hache en diamant",  "tool_axe_diamond",      TOOL_AXE,     TIER_DIAMOND, 1562 },
        { BTYPE_DIAM_SHOVEL,   "Pelle en diamant",  "tool_shovel_diamond",   TOOL_SHOVEL,  TIER_DIAMOND, 1562 },
        { BTYPE_DIAM_SWORD,    "Epee en diamant",   "tool_sword_diamond",    TOOL_SWORD,   TIER_DIAMOND, 1562 }
    };

    const int kItemCount = (int)(sizeof(kItems) / sizeof(kItems[0]));

    // ------------------------------------------------------------------
    //  Outil adapte a chaque bloc, et palier minimal pour en recuperer
    //  quelque chose. Tout ce qui n'est pas liste se casse a mains nues.
    // ------------------------------------------------------------------
    struct MineDef { BlockType type; ToolType tool; int tier; };

    const MineDef kMining[] =
    {
        { BTYPE_STONE,        TOOL_PICKAXE, TIER_WOOD },
        { BTYPE_COBBLESTONE,  TOOL_PICKAXE, TIER_WOOD },
        { BTYPE_MOSSY_COBBLE, TOOL_PICKAXE, TIER_WOOD },
        { BTYPE_BRICK,        TOOL_PICKAXE, TIER_WOOD },
        { BTYPE_SANDSTONE,    TOOL_PICKAXE, TIER_WOOD },
        { BTYPE_COAL,         TOOL_PICKAXE, TIER_WOOD },
        { BTYPE_IRON,         TOOL_PICKAXE, TIER_STONE },
        { BTYPE_GOLD,         TOOL_PICKAXE, TIER_IRON },
        { BTYPE_DIAMOND,      TOOL_PICKAXE, TIER_IRON },
        { BTYPE_REDSTONE,     TOOL_PICKAXE, TIER_IRON },
        { BTYPE_EMERALD,      TOOL_PICKAXE, TIER_IRON },
        { BTYPE_OBSIDIAN,     TOOL_PICKAXE, TIER_DIAMOND },
        { BTYPE_ICE,          TOOL_PICKAXE, TIER_NONE },
        { BTYPE_GLOWSTONE,    TOOL_PICKAXE, TIER_NONE },

        { BTYPE_DIRT,         TOOL_SHOVEL,  TIER_NONE },
        { BTYPE_GRASS,        TOOL_SHOVEL,  TIER_NONE },
        { BTYPE_PODZOL,       TOOL_SHOVEL,  TIER_NONE },
        { BTYPE_SAND,         TOOL_SHOVEL,  TIER_NONE },
        { BTYPE_GRAVEL,       TOOL_SHOVEL,  TIER_NONE },
        { BTYPE_CLAY,         TOOL_SHOVEL,  TIER_NONE },
        { BTYPE_SNOW,         TOOL_SHOVEL,  TIER_NONE },
        { BTYPE_SNOW_GRASS,   TOOL_SHOVEL,  TIER_NONE },

        { BTYPE_WOOD,           TOOL_AXE, TIER_NONE },
        { BTYPE_BIRCH_LOG,      TOOL_AXE, TIER_NONE },
        { BTYPE_SPRUCE_LOG,     TOOL_AXE, TIER_NONE },
        { BTYPE_WOODPLANK,      TOOL_AXE, TIER_NONE },
        { BTYPE_CRAFTING_TABLE, TOOL_AXE, TIER_NONE },
        { BTYPE_PUMPKIN,        TOOL_AXE, TIER_NONE }
    };

    const int kMiningCount = (int)(sizeof(kMining) / sizeof(kMining[0]));

    // Vitesse apportee par chaque palier d'outil
    const float kTierSpeed[5] = { 1.0f, 2.0f, 4.0f, 6.5f, 9.0f };
}

namespace Blocks
{

BlockInfo Info[BTYPE_FIN];

unsigned char OpacityTable[BTYPE_FIN];
unsigned char EmissionTable[BTYPE_FIN];
unsigned char LiquidTable[BTYPE_FIN];
unsigned char ReplaceableTable[BTYPE_FIN];

void BuildFastTables()
{
    for (int i = 0; i < BTYPE_FIN; ++i)
    {
        OpacityTable[i]     = Info[i].opacity;
        EmissionTable[i]    = Info[i].emission;
        LiquidTable[i]      = Info[i].liquid ? 1 : 0;
        ReplaceableTable[i] = Info[i].replaceable ? 1 : 0;
    }
}

void RegisterTextures(TextureAtlas& atlas)
{
    for (int i = 0; i < BTYPE_FIN; ++i)
    {
        Info[i] = BlockInfo();
        Info[i].type = (BlockType)i;
        for (int f = 0; f < 6; ++f)
            s_texIndex[i][f] = 0;
    }

    // L'air: rien a dessiner, ne bloque ni la vue ni la lumiere
    Info[BTYPE_AIR].name = "Air";
    Info[BTYPE_AIR].render = RENDER_NONE;
    Info[BTYPE_AIR].breakable = false;

    s_palette.clear();

    for (int d = 0; d < kDefCount; ++d)
    {
        const Def& def = kDefs[d];
        BlockInfo& bi = Info[def.type];

        bi.type = def.type;
        bi.name = def.name;
        bi.render = def.render;
        bi.transparency = def.transparency;
        bi.solid = def.solid;
        bi.opaque = def.opaque;
        bi.liquid = def.liquid;
        bi.emission = def.emission;
        bi.opacity = def.opacity;
        bi.hardness = def.hardness;
        bi.breakable = (def.hardness >= 0.0f);
        bi.drop = def.drop;

        // Un liquide peut envahir l'air, les liquides et la petite vegetation
        bi.replaceable = (def.render == RENDER_NONE) || (def.render == RENDER_CROSS) || def.liquid;
        bi.gravity = (def.type == BTYPE_SAND) || (def.type == BTYPE_GRAVEL);

        TextureAtlas::TextureIndex top = Resolve(atlas, def.texTop);
        TextureAtlas::TextureIndex bottom = Resolve(atlas, def.texBottom);
        TextureAtlas::TextureIndex side = Resolve(atlas, def.texSide);

        s_texIndex[def.type][FACE_TOP] = top;
        s_texIndex[def.type][FACE_BOTTOM] = bottom;
        s_texIndex[def.type][FACE_LEFT] = side;
        s_texIndex[def.type][FACE_RIGHT] = side;
        s_texIndex[def.type][FACE_FRONT] = side;
        s_texIndex[def.type][FACE_BACK] = side;

        if (def.inPalette)
            s_palette.push_back(def.type);
    }

    // --- objets ---
    for (int i = 0; i < kItemCount; ++i)
    {
        const ItemDef& def = kItems[i];
        BlockInfo& bi = Info[def.type];

        bi.type = def.type;
        bi.name = def.name;
        bi.render = RENDER_NONE;
        bi.transparency = TRANSP_CUTOUT;
        bi.solid = false;
        bi.opaque = false;
        bi.replaceable = true;
        bi.isItem = true;
        bi.toolType = def.toolType;
        bi.toolTier = def.tier;
        bi.maxDurability = def.durability;
        bi.hardness = 0.0f;
        bi.drop = def.type;

        const TextureAtlas::TextureIndex tex = Resolve(atlas, def.tex);
        for (int f = 0; f < 6; ++f)
            s_texIndex[def.type][f] = tex;

        s_palette.push_back(def.type);
    }

    Info[BTYPE_PORKCHOP].foodValue = 5.0f;
    Info[BTYPE_BEEF].foodValue = 6.0f;

    // --- outil conseille par bloc ---
    for (int i = 0; i < kMiningCount; ++i)
    {
        Info[kMining[i].type].tool = kMining[i].tool;
        Info[kMining[i].type].requiredTier = kMining[i].tier;
    }

    // Info est complet: on en tire les tables compactes. ComputeUVs, appele
    // plus tard, ne touche qu'aux coordonnees de texture.
    BuildFastTables();
}

float MiningSpeed(BlockType block, BlockType held)
{
    const BlockInfo& bi = Info[block];
    const BlockInfo& hi = Info[held];

    if (bi.tool == TOOL_NONE || hi.toolType == TOOL_NONE)
        return 1.0f;
    if (hi.toolType != bi.tool)
        return 1.0f;

    return kTierSpeed[hi.toolTier];
}

bool CanHarvest(BlockType block, BlockType held)
{
    const BlockInfo& bi = Info[block];
    if (bi.requiredTier == TIER_NONE)
        return true;

    const BlockInfo& hi = Info[held];
    return (hi.toolType == bi.tool) && (hi.toolTier >= bi.requiredTier);
}

void ComputeUVs(const TextureAtlas& atlas)
{
    for (int i = 0; i < BTYPE_FIN; ++i)
        for (int f = 0; f < 6; ++f)
            atlas.TextureIndexToCoord(s_texIndex[i][f],
                                      Info[i].uv[f][0], Info[i].uv[f][1],
                                      Info[i].uv[f][2], Info[i].uv[f][3]);
}

bool ShouldRenderFace(BlockType self, BlockType neighbour)
{
    if (neighbour == self)
    {
        // Les blocs transparents du meme type fusionnent (verre, eau, glace):
        // on evite ainsi des milliers de faces internes invisibles.
        const BlockInfo& bi = Info[self];
        if (bi.transparency == TRANSP_BLEND || bi.liquid)
            return false;
        return !bi.opaque ? (bi.transparency == TRANSP_CUTOUT) : false;
    }
    return !Info[neighbour].opaque;
}

const std::vector<BlockType>& Palette()
{
    return s_palette;
}

} // namespace Blocks
