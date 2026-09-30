#ifndef DEFINE_H__
#define DEFINE_H__

#include <GL/glew.h>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#ifdef _WIN32
#include <windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#else

#endif

#include <cstdint>

// ---------------------------------------------------------------------------
//  Dimensions du monde
// ---------------------------------------------------------------------------
#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 160
#define CHUNK_SIZE_Z 16

#define SEA_LEVEL 62

// Distance d'affichage exprimee en chunks (rayon). Reglable en jeu par le
// curseur du menu Options (Echap > Options).
#define DEFAULT_RENDER_DISTANCE 10
#define MIN_RENDER_DISTANCE 2
#define MAX_RENDER_DISTANCE 24

#define MAX_SELECTION_DISTANCE 6.0f

#define TEXTURE_PATH        "../BlockAdventure/media/textures/"
#define SHADER_PATH         "../BlockAdventure/media/shaders/"
#define SAVE_PATH           "../BlockAdventure/media/saves/"

// ---------------------------------------------------------------------------
//  Types de blocs
// ---------------------------------------------------------------------------
typedef uint8_t BlockType;

enum BLOCK_TYPE
{
    BTYPE_AIR = 0,
    BTYPE_DIRT,
    BTYPE_GRASS,
    BTYPE_WOOD,             // tronc de chene
    BTYPE_STONE,
    BTYPE_GOLD,             // minerai d'or
    BTYPE_BEDROCK,
    BTYPE_COAL,             // minerai de charbon
    BTYPE_DIAMOND,          // minerai de diamant
    BTYPE_IRON,             // minerai de fer
    BTYPE_SAND,
    BTYPE_SLIME,
    BTYPE_WOODPLANK,

    // --- ajouts ---
    BTYPE_LEAVES,           // feuilles de chene
    BTYPE_BIRCH_LOG,
    BTYPE_BIRCH_LEAVES,
    BTYPE_SPRUCE_LOG,
    BTYPE_SPRUCE_LEAVES,
    BTYPE_WATER,
    BTYPE_LAVA,
    BTYPE_GRAVEL,
    BTYPE_COBBLESTONE,
    BTYPE_MOSSY_COBBLE,
    BTYPE_SANDSTONE,
    BTYPE_SNOW,
    BTYPE_SNOW_GRASS,
    BTYPE_ICE,
    BTYPE_CLAY,
    BTYPE_GLASS,
    BTYPE_TORCH,
    BTYPE_GLOWSTONE,
    BTYPE_OBSIDIAN,
    BTYPE_REDSTONE,         // minerai de redstone
    BTYPE_EMERALD,          // minerai d'emeraude
    BTYPE_CACTUS,
    BTYPE_TALLGRASS,
    BTYPE_FLOWER_RED,
    BTYPE_FLOWER_YELLOW,
    BTYPE_DEADBUSH,
    BTYPE_MUSHROOM_RED,
    BTYPE_MUSHROOM_BROWN,
    BTYPE_PUMPKIN,
    BTYPE_BRICK,
    BTYPE_PODZOL,
    BTYPE_CRAFTING_TABLE,

    // --- objets: ils ne se posent pas dans le monde ---
    BTYPE_STICK,
    BTYPE_PORKCHOP,
    BTYPE_BEEF,

    BTYPE_WOOD_PICKAXE,  BTYPE_WOOD_AXE,  BTYPE_WOOD_SHOVEL,  BTYPE_WOOD_SWORD,
    BTYPE_STONE_PICKAXE, BTYPE_STONE_AXE, BTYPE_STONE_SHOVEL, BTYPE_STONE_SWORD,
    BTYPE_IRON_PICKAXE,  BTYPE_IRON_AXE,  BTYPE_IRON_SHOVEL,  BTYPE_IRON_SWORD,
    BTYPE_DIAM_PICKAXE,  BTYPE_DIAM_AXE,  BTYPE_DIAM_SHOVEL,  BTYPE_DIAM_SWORD,

    BTYPE_FIN
};

// Categorie d'outil, et palier de materiau (bois < pierre < fer < diamant)
enum ToolType
{
    TOOL_NONE = 0,
    TOOL_PICKAXE,
    TOOL_AXE,
    TOOL_SHOVEL,
    TOOL_SWORD
};

enum ToolTier
{
    TIER_NONE = 0,
    TIER_WOOD,
    TIER_STONE,
    TIER_IRON,
    TIER_DIAMOND
};

// Comment un bloc est transforme en geometrie
enum BlockRenderType
{
    RENDER_NONE = 0,    // air
    RENDER_CUBE,        // cube plein classique
    RENDER_CROSS,       // deux quads en croix (fleurs, herbe haute)
    RENDER_LIQUID,      // surface abaissee + animation de vagues
    RENDER_TORCH        // petit prisme
};

// Ordre des faces utilise partout dans le moteur
enum BlockFace
{
    FACE_TOP = 0,
    FACE_BOTTOM,
    FACE_LEFT,      // -X
    FACE_RIGHT,     // +X
    FACE_FRONT,     // +Z
    FACE_BACK       // -Z
};

// Niveau de lumiere maximum (comme Minecraft: 0..15)
#define MAX_LIGHT 15

// ---------------------------------------------------------------------------
//  Metadonnees d'un bloc (un octet par bloc)
//
//  Pour les liquides:
//    bits 0-2 : niveau, 0 = source (bloc plein) jusqu'a 7 = filet le plus mince
//    bit 3    : le liquide tombe depuis le bloc du dessus (il reste plein)
// ---------------------------------------------------------------------------
#define FLUID_LEVEL_MASK  0x07
#define FLUID_FALLING     0x08

#define WATER_RANGE 7      // portee horizontale de l'eau
#define LAVA_RANGE  3      // la lave s'etale beaucoup moins loin

// ---------------------------------------------------------------------------
//  Metadonnees d'un bloc (un octet par bloc)
//
//  Pour les liquides:
//    bits 0-2 : niveau, 0 = source (bloc plein) jusqu'a 7 = filet le plus mince
//    bit 3    : le liquide tombe depuis le bloc du dessus (il reste plein)
// ---------------------------------------------------------------------------
#define FLUID_LEVEL_MASK  0x07
#define FLUID_FALLING     0x08

#define WATER_RANGE 7      // portee horizontale de l'eau
#define LAVA_RANGE  3      // la lave s'etale beaucoup moins loin

#endif // DEFINE_H__
