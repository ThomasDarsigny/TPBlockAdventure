#ifndef WORLDGEN_H__
#define WORLDGEN_H__

#include "define.h"
#include "chunk.h"
#include <cstdint>

enum Biome
{
    BIOME_OCEAN = 0,
    BIOME_BEACH,
    BIOME_PLAINS,
    BIOME_FOREST,
    BIOME_TAIGA,
    BIOME_SNOWY,
    BIOME_DESERT,
    BIOME_SWAMP,
    BIOME_MOUNTAINS,
    BIOME_COUNT
};

// ---------------------------------------------------------------------------
//  Generateur de monde.
//
//  Tout est fonction pure de (graine, x, z): aucun etat mutable, donc
//  plusieurs threads peuvent generer des chunks differents en parallele et
//  une meme graine redonne toujours exactement le meme monde.
// ---------------------------------------------------------------------------
class WorldGenerator
{
public:
    explicit WorldGenerator(uint32_t seed);

    uint32_t Seed() const { return m_seed; }

    // Remplit entierement les blocs du chunk (terrain, grottes, minerais,
    // eau, vegetation).
    void Generate(Chunk& chunk) const;

    // Altitude du sol (dernier bloc solide) pour une colonne du monde
    int SurfaceHeight(int wx, int wz) const;

    Biome BiomeAt(int wx, int wz) const;
    static const char* BiomeName(Biome b);

    // Hauteurs et biomes precalcules pour les 16x16 colonnes d'un chunk:
    // SurfaceHeight() est le point chaud de la generation, on evite ainsi de
    // le recalculer trois fois par colonne.
    struct ColumnData
    {
        int   height[CHUNK_SIZE_X * CHUNK_SIZE_Z];
        Biome biome[CHUNK_SIZE_X * CHUNK_SIZE_Z];
    };

    // Trouve un point d'apparition correct (sur la terre ferme, hors de l'eau)
    void FindSpawn(float& x, float& y, float& z) const;

private:
    float Continent(float x, float z) const;
    float Erosion(float x, float z) const;
    float Hills(float x, float z) const;
    float Ridges(float x, float z) const;
    float Temperature(float x, float z) const;
    float Humidity(float x, float z) const;

    bool  IsCave(int x, int y, int z, int surface) const;

    Biome BiomeFrom(int height, int wx, int wz) const;

    void  BuildTerrain(Chunk& chunk, const ColumnData& col) const;
    void  PlaceOres(Chunk& chunk) const;
    void  Decorate(Chunk& chunk, const ColumnData& col) const;

    void  PlaceTree(Chunk& chunk, int wx, int wz, Biome biome, int groundY) const;
    void  PutBlock(Chunk& chunk, int wx, int wy, int wz, BlockType t, bool replaceSolid) const;

    uint32_t m_seed;
};

#endif // WORLDGEN_H__
