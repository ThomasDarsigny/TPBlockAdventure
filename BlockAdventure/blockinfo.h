#ifndef BLOCKINFO_H
#define BLOCKINFO_H

#include <string>
#include <vector>
#include "define.h"

class TextureAtlas;

// Comment un bloc laisse passer la lumiere et le regard
enum BlockTransparency
{
    TRANSP_NONE = 0,    // opaque
    TRANSP_CUTOUT,      // alpha test (feuilles, plantes, torche)
    TRANSP_BLEND        // alpha blending (verre, glace, eau)
};

// ---------------------------------------------------------------------------
//  Description statique d'un type de bloc.
//  Toutes les donnees sont remplies une seule fois au demarrage puis lues en
//  lecture seule, y compris depuis les threads de maillage.
// ---------------------------------------------------------------------------
struct BlockInfo
{
    BlockType        type;
    std::string      name;

    BlockRenderType  render;
    BlockTransparency transparency;

    bool  solid;        // bloque le joueur
    bool  opaque;       // cache les faces voisines et arrete la lumiere
    bool  liquid;       // eau / lave
    bool  breakable;    // false pour la bedrock
    bool  replaceable;  // un liquide peut couler a travers (air, plantes)
    bool  gravity;      // tombe si rien ne le soutient (sable, gravier)
    bool  isItem;       // objet d'inventaire, ne se pose pas dans le monde

    // Minage
    ToolType tool;          // outil qui accelere la casse
    int      requiredTier;  // palier minimum pour recuperer quelque chose

    // Si ce type EST un outil
    ToolType toolType;
    int      toolTier;
    int      maxDurability;

    float    foodValue;     // points de faim rendus (0 = pas comestible)

    unsigned char emission;   // lumiere emise      (0..15)
    unsigned char opacity;    // lumiere absorbee   (0..15)

    float hardness;     // secondes de minage a mains nues

    BlockType drop;     // ce que l'on recupere en cassant le bloc

    // Coordonnees dans l'atlas, precalculees par face (u, v, w, h)
    float uv[6][4];

    BlockInfo();
};

namespace Blocks
{
    extern BlockInfo Info[BTYPE_FIN];

    // Enregistre les textures dans l'atlas puis remplit la table Info.
    // A appeler AVANT TextureAtlas::Generate() pour la partie enregistrement,
    // et APRES pour le calcul des UV: Register() fait les deux via un
    // callback, voir blockinfo.cpp.
    void RegisterTextures(TextureAtlas& atlas);
    void ComputeUVs(const TextureAtlas& atlas);

    // Tables compactes pour les boucles chaudes (lumiere, maillage, fluides).
    // BlockInfo pese plus de cent octets: y lire un seul champ par bloc fait
    // sauter le cache d'une ligne a l'autre. Ces tableaux d'octets tiennent,
    // eux, dans quelques lignes de cache.
    extern unsigned char OpacityTable[BTYPE_FIN];
    extern unsigned char EmissionTable[BTYPE_FIN];
    extern unsigned char LiquidTable[BTYPE_FIN];
    extern unsigned char ReplaceableTable[BTYPE_FIN];

    // Remplit les tables ci-dessus a partir de Info (appele par RegisterTextures)
    void BuildFastTables();

    inline const BlockInfo& Get(BlockType t)   { return Info[t]; }
    inline bool IsAir(BlockType t)             { return t == BTYPE_AIR; }
    inline bool IsOpaque(BlockType t)          { return Info[t].opaque; }
    inline bool IsSolid(BlockType t)           { return Info[t].solid; }
    inline bool IsLiquid(BlockType t)          { return LiquidTable[t] != 0; }
    inline bool IsReplaceable(BlockType t)      { return ReplaceableTable[t] != 0; }
    inline bool HasGravity(BlockType t)         { return Info[t].gravity; }
    inline bool IsItem(BlockType t)             { return Info[t].isItem; }
    inline bool IsTool(BlockType t)             { return Info[t].toolType != TOOL_NONE; }
    inline bool IsFood(BlockType t)             { return Info[t].foodValue > 0.0f; }

    // Multiplicateur de vitesse de minage pour un bloc casse avec un objet
    float MiningSpeed(BlockType block, BlockType held);

    // Le bloc laisse-t-il tomber quelque chose avec cet objet en main ?
    bool CanHarvest(BlockType block, BlockType held);

    // Portee horizontale d'un liquide (0 si ce n'est pas un liquide)
    inline int FluidRange(BlockType t)
    {
        if (t == BTYPE_WATER) return WATER_RANGE;
        if (t == BTYPE_LAVA)  return LAVA_RANGE;
        return 0;
    }
    inline unsigned char Emission(BlockType t) { return EmissionTable[t]; }
    inline unsigned char Opacity(BlockType t)  { return OpacityTable[t]; }

    // Une face entre 'self' et 'neighbour' doit-elle etre generee ?
    bool ShouldRenderFace(BlockType self, BlockType neighbour);

    // Liste des blocs proposes dans l'inventaire creatif
    const std::vector<BlockType>& Palette();
}

#endif // BLOCKINFO_H
