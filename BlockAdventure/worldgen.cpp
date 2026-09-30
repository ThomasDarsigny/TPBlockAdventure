#include "worldgen.h"
#include "noise.h"
#include <cmath>
#include <algorithm>

namespace
{
    inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    inline float SmoothStep(float edge0, float edge1, float x)
    {
        float t = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // Le bruit de gradient normalise reste concentre autour de 0 (|v| depasse
    // rarement 0.3). On l'etale sur [-1,1] avant de s'en servir, sinon le
    // monde est desesperement plat et les biomes extremes ne sortent jamais.
    inline float Spread(float v, float gain)
    {
        return Clamp(v * gain, -1.0f, 1.0f);
    }

    const int MIN_HEIGHT = 4;
    const int MAX_HEIGHT = CHUNK_SIZE_Y - 8;

    // Espacement de la grille utilisee pour repartir les arbres
    const int TREE_CELL = 6;
}

WorldGenerator::WorldGenerator(uint32_t seed) : m_seed(seed)
{
}

// ---------------------------------------------------------------------------
//  Champs de bruit
// ---------------------------------------------------------------------------
float WorldGenerator::Continent(float x, float z) const
{
    return Noise::Fbm2(x * 0.0009f, z * 0.0009f, 5, 2.0f, 0.5f, m_seed + 101u);
}

float WorldGenerator::Erosion(float x, float z) const
{
    return Noise::Fbm2(x * 0.0021f, z * 0.0021f, 4, 2.0f, 0.5f, m_seed + 211u);
}

float WorldGenerator::Hills(float x, float z) const
{
    return Noise::Fbm2(x * 0.0090f, z * 0.0090f, 4, 2.0f, 0.5f, m_seed + 307u);
}

float WorldGenerator::Ridges(float x, float z) const
{
    // On etale le bruit avant de le replier: sans cela les "cretes" sont
    // toutes a la meme altitude et le relief manque de caractere.
    float sum = 0.0f, amp = 1.0f, norm = 0.0f, freq = 1.0f;
    for (int i = 0; i < 5; ++i)
    {
        const float n = Spread(Noise::Gradient2(x * 0.0034f * freq, z * 0.0034f * freq,
                                                m_seed + 401u + (uint32_t)i * 6151u), 2.6f);
        float r = 1.0f - fabsf(n);
        r *= r;
        sum += r * amp;
        norm += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return (norm > 0.0f) ? sum / norm : 0.0f;
}

float WorldGenerator::Temperature(float x, float z) const
{
    return Spread(Noise::Fbm2(x * 0.00045f, z * 0.00045f, 3, 2.0f, 0.5f, m_seed + 503u), 3.0f);
}

float WorldGenerator::Humidity(float x, float z) const
{
    return Spread(Noise::Fbm2(x * 0.00058f, z * 0.00058f, 3, 2.0f, 0.5f, m_seed + 601u), 3.0f);
}

// ---------------------------------------------------------------------------
//  Relief
// ---------------------------------------------------------------------------
int WorldGenerator::SurfaceHeight(int wx, int wz) const
{
    const float x = (float)wx, z = (float)wz;

    const float cont = Spread(Continent(x, z), 2.9f);   // oceans <-> interieur
    const float ero  = Spread(Erosion(x, z), 2.4f);     // relief doux <-> decoupe
    const float hill = Spread(Hills(x, z), 2.6f);       // vallonnement local

    float h = (float)SEA_LEVEL + cont * 34.0f + hill * 8.0f;

    // Les montagnes n'apparaissent qu'au coeur des continents, et l'erosion
    // decide si le relief y est arrondi ou en aretes.
    const float mountain = SmoothStep(0.22f, 0.78f, cont) * (0.35f + 0.65f * (ero * 0.5f + 0.5f));
    if (mountain > 0.001f)
        h += Ridges(x, z) * 58.0f * mountain;

    int ih = (int)(h + 0.5f);
    if (ih < MIN_HEIGHT) ih = MIN_HEIGHT;
    if (ih > MAX_HEIGHT) ih = MAX_HEIGHT;
    return ih;
}

Biome WorldGenerator::BiomeAt(int wx, int wz) const
{
    return BiomeFrom(SurfaceHeight(wx, wz), wx, wz);
}

Biome WorldGenerator::BiomeFrom(int h, int wx, int wz) const
{
    const float x = (float)wx, z = (float)wz;
    const float t = Temperature(x, z);
    const float hum = Humidity(x, z);

    if (h < SEA_LEVEL - 1)       return BIOME_OCEAN;
    if (h <= SEA_LEVEL + 2)      return BIOME_BEACH;
    if (h > SEA_LEVEL + 40)      return BIOME_MOUNTAINS;

    if (t < -0.45f)              return (hum > 0.0f) ? BIOME_TAIGA : BIOME_SNOWY;
    if (t > 0.45f && hum < 0.0f) return BIOME_DESERT;
    if (hum > 0.45f && h < SEA_LEVEL + 8) return BIOME_SWAMP;
    if (hum > 0.05f)             return BIOME_FOREST;
    return BIOME_PLAINS;
}

const char* WorldGenerator::BiomeName(Biome b)
{
    switch (b)
    {
    case BIOME_OCEAN:     return "Ocean";
    case BIOME_BEACH:     return "Plage";
    case BIOME_PLAINS:    return "Plaines";
    case BIOME_FOREST:    return "Foret";
    case BIOME_TAIGA:     return "Taiga";
    case BIOME_SNOWY:     return "Toundra";
    case BIOME_DESERT:    return "Desert";
    case BIOME_SWAMP:     return "Marais";
    case BIOME_MOUNTAINS: return "Montagnes";
    default:              return "?";
    }
}

// ---------------------------------------------------------------------------
//  Grottes
// ---------------------------------------------------------------------------
bool WorldGenerator::IsCave(int x, int y, int z, int surface) const
{
    if (y < 3 || y > surface - 2 || y > 110)
        return false;

    const float fx = (float)x, fy = (float)y, fz = (float)z;

    // Deux champs de bruit qui s'annulent au meme endroit: leur intersection
    // forme des galeries allongees plutot que des poches spheriques.
    const float n1 = Spread(Noise::Fbm3(fx * 0.0125f, fy * 0.0270f, fz * 0.0125f, 2, 2.0f, 0.5f, m_seed + 911u), 3.0f);
    const float n2 = Spread(Noise::Fbm3(fx * 0.0125f, fy * 0.0270f, fz * 0.0125f, 2, 2.0f, 0.5f, m_seed + 977u), 3.0f);

    // Les galeries s'elargissent legerement en profondeur
    const float radius = 0.0022f + 0.0026f * (1.0f - Clamp((float)y / 60.0f, 0.0f, 1.0f));
    if (n1 * n1 + n2 * n2 < radius)
        return true;

    // Quelques cavernes plus larges, uniquement tres bas
    if (y < 34)
    {
        const float c = Spread(Noise::Fbm3(fx * 0.021f, fy * 0.034f, fz * 0.021f, 3, 2.0f, 0.5f, m_seed + 1013u), 3.2f);
        if (c > 0.80f)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
//  Terrain
// ---------------------------------------------------------------------------
void WorldGenerator::BuildTerrain(Chunk& chunk, const ColumnData& col) const
{
    const int ox = chunk.Cx() * CHUNK_SIZE_X;
    const int oz = chunk.Cz() * CHUNK_SIZE_Z;

    for (int lz = 0; lz < CHUNK_SIZE_Z; ++lz)
    {
        for (int lx = 0; lx < CHUNK_SIZE_X; ++lx)
        {
            const int wx = ox + lx, wz = oz + lz;
            const int h = col.height[lz * CHUNK_SIZE_X + lx];
            const Biome biome = col.biome[lz * CHUNK_SIZE_X + lx];

            // Epaisseur de la couche meuble
            const int soil = 3 + (int)(Noise::Rand2(wx, wz, m_seed + 55u) * 2.0f);

            for (int y = 0; y <= h; ++y)
            {
                BlockType t;

                if (y == 0)
                {
                    t = BTYPE_BEDROCK;
                }
                else if (y <= 2 && Noise::Rand3(wx, y, wz, m_seed + 77u) < 0.55f)
                {
                    t = BTYPE_BEDROCK;
                }
                else if (y == h)
                {
                    // Bloc de surface
                    switch (biome)
                    {
                    case BIOME_OCEAN:  t = (h < SEA_LEVEL - 6) ? BTYPE_GRAVEL : BTYPE_SAND; break;
                    case BIOME_BEACH:  t = BTYPE_SAND; break;
                    case BIOME_DESERT: t = BTYPE_SAND; break;
                    case BIOME_SNOWY:  t = BTYPE_SNOW_GRASS; break;
                    case BIOME_TAIGA:  t = BTYPE_PODZOL; break;
                    case BIOME_SWAMP:  t = BTYPE_GRASS; break;
                    case BIOME_MOUNTAINS:
                        if (h > SEA_LEVEL + 62)      t = BTYPE_SNOW;
                        else if (h > SEA_LEVEL + 50) t = BTYPE_STONE;
                        else                         t = BTYPE_GRASS;
                        break;
                    default:           t = BTYPE_GRASS; break;
                    }
                    if (h < SEA_LEVEL && t == BTYPE_GRASS)
                        t = BTYPE_DIRT;
                }
                else if (y > h - soil)
                {
                    // Sous-sol
                    switch (biome)
                    {
                    case BIOME_OCEAN:
                    case BIOME_BEACH:
                    case BIOME_DESERT: t = (y > h - 2) ? BTYPE_SAND : BTYPE_SANDSTONE; break;
                    case BIOME_MOUNTAINS: t = (h > SEA_LEVEL + 50) ? BTYPE_STONE : BTYPE_DIRT; break;
                    default:           t = BTYPE_DIRT; break;
                    }
                }
                else
                {
                    t = BTYPE_STONE;
                }

                // Creusement des grottes (jamais la bedrock)
                if (t != BTYPE_BEDROCK && IsCave(wx, y, wz, h))
                    t = BTYPE_AIR;

                chunk.SetBlock(lx, y, lz, t);
            }

            // Remplissage des oceans, lacs et rivieres
            for (int y = h + 1; y <= SEA_LEVEL; ++y)
                chunk.SetBlock(lx, y, lz, BTYPE_WATER);

            // Glace en surface dans les biomes froids
            if (h < SEA_LEVEL && (biome == BIOME_SNOWY || biome == BIOME_TAIGA))
                chunk.SetBlock(lx, SEA_LEVEL, lz, BTYPE_ICE);

            // Lave au fond des grottes profondes
            for (int y = 1; y < 11; ++y)
                if (chunk.GetBlock(lx, y, lz) == BTYPE_AIR)
                    chunk.SetBlock(lx, y, lz, BTYPE_LAVA);
        }
    }
}

// ---------------------------------------------------------------------------
//  Minerais
// ---------------------------------------------------------------------------
void WorldGenerator::PlaceOres(Chunk& chunk) const
{
    struct OreDef { BlockType type; int veins; int minY; int maxY; int size; };

    static const OreDef kOres[] =
    {
        { BTYPE_COAL,     22, 6, 118, 9 },
        { BTYPE_IRON,     16, 6,  66, 7 },
        { BTYPE_GOLD,      5, 5,  34, 6 },
        { BTYPE_REDSTONE,  6, 4,  18, 7 },
        { BTYPE_DIAMOND,   3, 3,  15, 5 },
        { BTYPE_EMERALD,   2, 5,  40, 3 }
    };

    const int ox = chunk.Cx() * CHUNK_SIZE_X;
    const int oz = chunk.Cz() * CHUNK_SIZE_Z;

    for (int o = 0; o < (int)(sizeof(kOres) / sizeof(kOres[0])); ++o)
    {
        const OreDef& ore = kOres[o];

        for (int v = 0; v < ore.veins; ++v)
        {
            const uint32_t s = m_seed + (uint32_t)o * 7717u + (uint32_t)v * 131u;
            const float r1 = Noise::Rand2(chunk.Cx() * 31 + v, chunk.Cz() * 17 + o, s);
            const float r2 = Noise::Rand2(chunk.Cz() * 13 + v, chunk.Cx() * 29 + o, s ^ 0x5a5au);
            const float r3 = Noise::Rand2(chunk.Cx() + o, chunk.Cz() + v, s ^ 0xa5a5u);

            // L'emeraude ne se trouve qu'en montagne
            if (ore.type == BTYPE_EMERALD)
            {
                if (BiomeAt(ox + 8, oz + 8) != BIOME_MOUNTAINS) continue;
            }

            int cx = (int)(r1 * CHUNK_SIZE_X);
            int cz = (int)(r2 * CHUNK_SIZE_Z);
            int cy = ore.minY + (int)(r3 * (float)(ore.maxY - ore.minY));

            const int count = 2 + (int)(r3 * (float)ore.size);
            int px = cx, py = cy, pz = cz;

            for (int k = 0; k < count; ++k)
            {
                if (chunk.GetBlock(px, py, pz) == BTYPE_STONE)
                    chunk.SetBlock(px, py, pz, ore.type);

                // Marche aleatoire courte -> filon compact
                const uint32_t ks = s + (uint32_t)k * 977u;
                px += (int)(Noise::Rand2(k, 1, ks) * 3.0f) - 1;
                py += (int)(Noise::Rand2(k, 2, ks) * 3.0f) - 1;
                pz += (int)(Noise::Rand2(k, 3, ks) * 3.0f) - 1;

                if (px < 0 || px >= CHUNK_SIZE_X || pz < 0 || pz >= CHUNK_SIZE_Z) break;
                if (py < 1 || py >= CHUNK_SIZE_Y) break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Vegetation
// ---------------------------------------------------------------------------
void WorldGenerator::PutBlock(Chunk& chunk, int wx, int wy, int wz, BlockType t, bool replaceSolid) const
{
    const int lx = wx - chunk.Cx() * CHUNK_SIZE_X;
    const int lz = wz - chunk.Cz() * CHUNK_SIZE_Z;

    if (lx < 0 || lx >= CHUNK_SIZE_X || lz < 0 || lz >= CHUNK_SIZE_Z) return;
    if (wy < 0 || wy >= CHUNK_SIZE_Y) return;

    if (!replaceSolid)
    {
        BlockType cur = chunk.GetBlock(lx, wy, lz);
        if (cur != BTYPE_AIR && cur != BTYPE_WATER && cur != BTYPE_LEAVES &&
            cur != BTYPE_BIRCH_LEAVES && cur != BTYPE_SPRUCE_LEAVES)
            return;
    }

    chunk.SetBlock(lx, wy, lz, t);
}

void WorldGenerator::PlaceTree(Chunk& chunk, int wx, int wz, Biome biome, int groundY) const
{
    const uint32_t s = Noise::Hash2(wx, wz, m_seed + 4242u);
    const float r = (float)(s & 0xffff) / 65535.0f;

    BlockType log = BTYPE_WOOD;
    BlockType leaf = BTYPE_LEAVES;
    int height = 5 + (int)(r * 3.0f);
    bool conifer = false;

    switch (biome)
    {
    case BIOME_TAIGA:
    case BIOME_SNOWY:
        log = BTYPE_SPRUCE_LOG; leaf = BTYPE_SPRUCE_LEAVES;
        height = 7 + (int)(r * 4.0f);
        conifer = true;
        break;
    case BIOME_FOREST:
        if (r > 0.62f) { log = BTYPE_BIRCH_LOG; leaf = BTYPE_BIRCH_LEAVES; height = 6 + (int)(r * 3.0f); }
        break;
    case BIOME_DESERT:
    {
        // Cactus: pas de feuillage
        const int ch = 2 + (int)(r * 3.0f);
        for (int i = 1; i <= ch; ++i)
            PutBlock(chunk, wx, groundY + i, wz, BTYPE_CACTUS, false);
        return;
    }
    case BIOME_SWAMP:
        height = 5 + (int)(r * 2.0f);
        break;
    default:
        break;
    }

    if (groundY + height + 3 >= CHUNK_SIZE_Y) return;

    // Tronc
    for (int i = 0; i < height; ++i)
        PutBlock(chunk, wx, groundY + 1 + i, wz, log, true);

    const int top = groundY + height;

    if (conifer)
    {
        // Sapin: etages de plus en plus larges vers le bas
        int radius = 0;
        for (int y = top + 1; y >= top - height + 2; --y)
        {
            for (int dz = -radius; dz <= radius; ++dz)
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    if (dx * dx + dz * dz > radius * radius + 1) continue;
                    if (dx == 0 && dz == 0 && y <= top) continue;
                    PutBlock(chunk, wx + dx, y, wz + dz, leaf, false);
                }
            radius = (radius >= 2) ? 0 : radius + 1;
        }
        PutBlock(chunk, wx, top + 1, wz, leaf, false);
    }
    else
    {
        // Chene / bouleau: couronne quasi spherique
        for (int dy = -2; dy <= 2; ++dy)
        {
            const int y = top + dy;
            const int radius = (dy >= 1) ? 1 : 2;
            for (int dz = -radius; dz <= radius; ++dz)
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    if (dx == 0 && dz == 0 && dy < 1) continue;
                    // coins arrondis, avec un peu d'aleatoire
                    if (dx * dx + dz * dz > radius * radius + 1) continue;
                    if (dx * dx + dz * dz == radius * radius + 1 &&
                        Noise::Rand3(wx + dx, y, wz + dz, m_seed) < 0.45f) continue;
                    PutBlock(chunk, wx + dx, y, wz + dz, leaf, false);
                }
        }
    }
}

void WorldGenerator::Decorate(Chunk& chunk, const ColumnData& col) const
{
    const int ox = chunk.Cx() * CHUNK_SIZE_X;
    const int oz = chunk.Cz() * CHUNK_SIZE_Z;

    // --- Arbres ---
    // Les arbres sont places sur une grille de cellules: une cellule contient
    // au plus un arbre, ce qui garantit un espacement correct. On balaie aussi
    // les cellules voisines pour que les couronnes debordant d'un chunk sur
    // l'autre soient generees de maniere coherente.
    const int cell0x = (int)floorf((float)(ox - 8) / TREE_CELL);
    const int cell1x = (int)floorf((float)(ox + CHUNK_SIZE_X + 8) / TREE_CELL);
    const int cell0z = (int)floorf((float)(oz - 8) / TREE_CELL);
    const int cell1z = (int)floorf((float)(oz + CHUNK_SIZE_Z + 8) / TREE_CELL);

    for (int cz = cell0z; cz <= cell1z; ++cz)
    {
        for (int cx = cell0x; cx <= cell1x; ++cx)
        {
            const uint32_t h = Noise::Hash2(cx, cz, m_seed + 8080u);
            const int tx = cx * TREE_CELL + (int)(h % TREE_CELL);
            const int tz = cz * TREE_CELL + (int)((h >> 8) % TREE_CELL);

            const Biome biome = BiomeAt(tx, tz);

            float density;
            switch (biome)
            {
            case BIOME_FOREST:    density = 0.34f; break;
            case BIOME_TAIGA:     density = 0.30f; break;
            case BIOME_SWAMP:     density = 0.18f; break;
            case BIOME_PLAINS:    density = 0.05f; break;
            case BIOME_SNOWY:     density = 0.10f; break;
            case BIOME_DESERT:    density = 0.06f; break;
            case BIOME_MOUNTAINS: density = 0.08f; break;
            default:              density = 0.0f;  break;
            }

            if ((float)((h >> 16) & 0xffff) / 65535.0f > density)
                continue;

            const int gy = SurfaceHeight(tx, tz);
            if (gy <= SEA_LEVEL) continue;             // pas d'arbre dans l'eau
            if (gy > SEA_LEVEL + 58) continue;         // ni sur les sommets enneiges

            PlaceTree(chunk, tx, tz, biome, gy);
        }
    }

    // --- Petite vegetation et decor de surface ---
    for (int lz = 0; lz < CHUNK_SIZE_Z; ++lz)
    {
        for (int lx = 0; lx < CHUNK_SIZE_X; ++lx)
        {
            const int wx = ox + lx, wz = oz + lz;

            // On repart du terrain reel: les grottes ont pu emporter la surface
            int gy = -1;
            for (int y = CHUNK_SIZE_Y - 1; y > 0; --y)
            {
                BlockType t = chunk.GetBlock(lx, y, lz);
                if (t != BTYPE_AIR && t != BTYPE_WATER && t != BTYPE_ICE) { gy = y; break; }
            }
            if (gy < 0 || gy + 1 >= CHUNK_SIZE_Y) continue;
            if (chunk.GetBlock(lx, gy + 1, lz) != BTYPE_AIR) continue;

            const BlockType ground = chunk.GetBlock(lx, gy, lz);
            const Biome biome = col.biome[lz * CHUNK_SIZE_X + lx];
            const float r = Noise::Rand2(wx, wz, m_seed + 3131u);

            if (ground == BTYPE_GRASS || ground == BTYPE_PODZOL)
            {
                if (r < 0.14f)      chunk.SetBlock(lx, gy + 1, lz, BTYPE_TALLGRASS);
                else if (r < 0.155f) chunk.SetBlock(lx, gy + 1, lz, BTYPE_FLOWER_RED);
                else if (r < 0.17f) chunk.SetBlock(lx, gy + 1, lz, BTYPE_FLOWER_YELLOW);
                else if (r < 0.176f && biome == BIOME_SWAMP) chunk.SetBlock(lx, gy + 1, lz, BTYPE_MUSHROOM_BROWN);
            }
            else if (ground == BTYPE_SAND && biome == BIOME_DESERT)
            {
                if (r < 0.03f) chunk.SetBlock(lx, gy + 1, lz, BTYPE_DEADBUSH);
            }
            else if (ground == BTYPE_SNOW_GRASS || ground == BTYPE_SNOW)
            {
                if (r < 0.02f) chunk.SetBlock(lx, gy + 1, lz, BTYPE_DEADBUSH);
            }

            // Champignons dans le noir des grottes proches de la surface
            if (gy < SEA_LEVEL - 8 && r > 0.985f)
                chunk.SetBlock(lx, gy + 1, lz, (r > 0.9925f) ? BTYPE_MUSHROOM_RED : BTYPE_MUSHROOM_BROWN);
        }
    }
}

// ---------------------------------------------------------------------------
void WorldGenerator::Generate(Chunk& chunk) const
{
    ColumnData col;

    const int ox = chunk.Cx() * CHUNK_SIZE_X;
    const int oz = chunk.Cz() * CHUNK_SIZE_Z;

    for (int lz = 0; lz < CHUNK_SIZE_Z; ++lz)
        for (int lx = 0; lx < CHUNK_SIZE_X; ++lx)
        {
            const int wx = ox + lx, wz = oz + lz;
            const int h = SurfaceHeight(wx, wz);
            col.height[lz * CHUNK_SIZE_X + lx] = h;
            col.biome[lz * CHUNK_SIZE_X + lx] = BiomeFrom(h, wx, wz);
        }

    BuildTerrain(chunk, col);
    PlaceOres(chunk);
    Decorate(chunk, col);
    chunk.RecomputeHighest();
    chunk.generated = true;
}

void WorldGenerator::FindSpawn(float& x, float& y, float& z) const
{
    // Spirale autour de l'origine jusqu'a trouver une plaine hors de l'eau
    for (int radius = 0; radius < 200; radius += 4)
    {
        for (int a = 0; a < 16; ++a)
        {
            const float ang = (float)a * 0.3927f;
            const int wx = (int)(cosf(ang) * radius * 12.0f);
            const int wz = (int)(sinf(ang) * radius * 12.0f);

            const int h = SurfaceHeight(wx, wz);
            if (h > SEA_LEVEL + 1 && h < SEA_LEVEL + 30 && BiomeAt(wx, wz) != BIOME_OCEAN)
            {
                x = (float)wx + 0.5f;
                y = (float)h + 2.5f;
                z = (float)wz + 0.5f;
                return;
            }
        }
    }
    x = 0.5f;
    y = (float)(SEA_LEVEL + 20);
    z = 0.5f;
}
