#include "proctex.h"
#include "noise.h"
#include <cmath>
#include <cstring>
#include <string>

namespace
{
    const int S = ProcTex::SIZE;

    struct Tile
    {
        unsigned char* p;
        explicit Tile(std::vector<unsigned char>& v) : p(v.data()) {}

        void Set(int x, int y, int r, int g, int b, int a = 255)
        {
            if (x < 0 || y < 0 || x >= S || y >= S) return;
            unsigned char* d = p + (y * S + x) * 4;
            d[0] = (unsigned char)(r < 0 ? 0 : (r > 255 ? 255 : r));
            d[1] = (unsigned char)(g < 0 ? 0 : (g > 255 ? 255 : g));
            d[2] = (unsigned char)(b < 0 ? 0 : (b > 255 ? 255 : b));
            d[3] = (unsigned char)a;
        }
    };

    uint32_t NameSeed(const std::string& n)
    {
        uint32_t h = 2166136261u;
        for (size_t i = 0; i < n.size(); ++i)
        {
            h ^= (unsigned char)n[i];
            h *= 16777619u;
        }
        return h;
    }

    // Bruit blanc par pixel, dans [-1,1]
    float N(int x, int y, uint32_t seed)
    {
        return Noise::Rand2(x, y, seed) * 2.0f - 1.0f;
    }

    // Bruit grumeleux (taches douces)
    float Blob(int x, int y, uint32_t seed, float scale)
    {
        return Noise::Fbm2((float)x / scale, (float)y / scale, 3, 2.0f, 0.5f, seed);
    }

    // Remplit toute la tuile avec une couleur de base bruitee
    void Grain(Tile& t, uint32_t seed, int r, int g, int b, int amp, int a = 255)
    {
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                int n = (int)(N(x, y, seed) * amp);
                t.Set(x, y, r + n, g + n, b + n, a);
            }
    }

    void Clear(std::vector<unsigned char>& v)
    {
        std::memset(v.data(), 0, v.size());
    }

    // Fond de pierre, reutilise par tous les minerais
    void StoneBase(Tile& t, uint32_t seed)
    {
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                float b = Blob(x, y, seed, 4.0f) * 18.0f;
                int n = (int)(N(x, y, seed ^ 0x51u) * 8.0f + b);
                t.Set(x, y, 128 + n, 128 + n, 130 + n);
            }
    }

    // Depose des pepites de minerai
    void Ore(Tile& t, uint32_t seed, int r, int g, int b, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            int cx = (int)(Noise::Rand2(i, 0, seed) * S);
            int cy = (int)(Noise::Rand2(0, i, seed) * S);
            int size = 1 + (int)(Noise::Rand2(i, i, seed) * 2.0f);
            for (int dy = -size; dy <= size; ++dy)
                for (int dx = -size; dx <= size; ++dx)
                {
                    if (dx * dx + dy * dy > size * size) continue;
                    int n = (int)(N(cx + dx, cy + dy, seed) * 20.0f);
                    t.Set((cx + dx + S) % S, (cy + dy + S) % S, r + n, g + n, b + n);
                }
        }
    }

    // Feuillage: teinte verte bruitee + trous transparents
    void Leaves(Tile& t, uint32_t seed, int r, int g, int b)
    {
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                float f = Blob(x, y, seed, 2.5f);
                int n = (int)(f * 34.0f + N(x, y, seed ^ 0x9au) * 16.0f);
                bool hole = Noise::Rand2(x, y, seed ^ 0xbeefu) > 0.86f;
                t.Set(x, y, r + n, g + n, b + n, hole ? 0 : 255);
            }
    }

    // Pierres arrondies separees par un joint sombre
    void Cobble(Tile& t, uint32_t seed, int r, int g, int b)
    {
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                float d = Noise::Cellular2((float)x / 5.0f, (float)y / 5.0f, seed);
                int n = (int)(N(x, y, seed) * 10.0f);
                if (d > 0.62f)
                {
                    t.Set(x, y, r - 55 + n, g - 55 + n, b - 55 + n);
                }
                else
                {
                    int k = (int)((0.6f - d) * 40.0f);
                    t.Set(x, y, r + n + k, g + n + k, b + n + k);
                }
            }
    }

    // Manche d'outil en diagonale, du coin bas-droit vers le centre
    void ToolHandle(Tile& t, uint32_t seed)
    {
        for (int i = 0; i < 10; ++i)
        {
            const int x = 12 - i;
            const int y = 3 + i;
            const int n = (int)(N(x, y, seed) * 14.0f);
            t.Set(x, y, 132 + n, 96 + n, 52 + n);
            t.Set(x + 1, y, 112 + n, 80 + n, 42 + n);
        }
    }

    // Tete d'outil, dessinee dans le quart haut-gauche de la tuile
    void ToolHead(Tile& t, uint32_t seed, int kind, int r, int g, int b)
    {
        // kind: 0 pioche, 1 hache, 2 pelle, 3 epee
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                bool on = false;

                if (kind == 0)          // pioche: barre large et pointes
                    on = (y >= 11 && y <= 12 && x >= 2 && x <= 11)
                      || (y == 13 && (x == 2 || x == 3 || x == 10 || x == 11))
                      || (y == 10 && x >= 6 && x <= 8);
                else if (kind == 1)     // hache: masse d'un seul cote
                    on = (x >= 3 && x <= 7 && y >= 9 && y <= 13)
                      || (x == 8 && y >= 10 && y <= 12);
                else if (kind == 2)     // pelle: petit godet
                    on = (x >= 4 && x <= 8 && y >= 10 && y <= 13);
                else                    // epee: lame en diagonale
                    on = (x + 14 - y >= 13 && x + 14 - y <= 15 && y >= 5 && x <= 11);

                if (!on) continue;

                const int n = (int)(N(x, y, seed) * 18.0f);
                t.Set(x, y, r + n, g + n, b + n);
            }

        if (kind == 3)   // garde de l'epee
            for (int i = -2; i <= 2; ++i)
                t.Set(10 + i, 4 - i, 120, 90, 50);
    }

    // Plante (tige + touffe) sur fond transparent
    void Plant(Tile& t, uint32_t seed, int sr, int sg, int sb, bool tall)
    {
        int top = tall ? 1 : 6;
        for (int y = top; y < S; ++y)
        {
            int spread = tall ? 5 : 3;
            for (int dx = -spread; dx <= spread; ++dx)
            {
                int x = S / 2 + dx;
                int adx = dx < 0 ? -dx : dx;
                float chance = 1.0f - (float)adx / (float)(spread + 1);
                chance *= (float)(y - top) / (float)(S - top) + 0.25f;
                if (Noise::Rand2(x, y, seed) < chance * 0.9f)
                {
                    int n = (int)(N(x, y, seed ^ 7u) * 22.0f);
                    t.Set(x, y, sr + n, sg + n, sb + n);
                }
            }
        }
    }
}

namespace ProcTex
{

bool Generate(const std::string& name, std::vector<unsigned char>& rgba)
{
    rgba.assign(S * S * 4, 0);
    Tile t(rgba);
    const uint32_t seed = NameSeed(name);

    if (name == "leaves")        { Leaves(t, seed,  52, 118,  40); return true; }
    if (name == "birch_leaves")  { Leaves(t, seed,  96, 150,  60); return true; }
    if (name == "spruce_leaves") { Leaves(t, seed,  34,  84,  54); return true; }

    if (name == "birch_log_side")
    {
        Grain(t, seed, 218, 214, 200, 10);
        for (int i = 0; i < 7; ++i)
        {
            int y = (int)(Noise::Rand2(i, 3, seed) * S);
            int x = (int)(Noise::Rand2(i, 9, seed) * S);
            int len = 2 + (int)(Noise::Rand2(i, i, seed) * 3.0f);
            for (int k = 0; k < len; ++k) t.Set((x + k) % S, y, 60, 55, 50);
        }
        return true;
    }

    if (name == "birch_log_top")
    {
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                float dx = (float)x - 7.5f, dy = (float)y - 7.5f;
                float d = sqrtf(dx * dx + dy * dy);
                int ring = ((int)(d * 1.6f) % 2) ? 18 : 0;
                int n = (int)(N(x, y, seed) * 8.0f);
                if (d > 7.0f) t.Set(x, y, 214 + n, 210 + n, 196 + n);
                else          t.Set(x, y, 200 - ring + n, 176 - ring + n, 128 - ring + n);
            }
        return true;
    }

    if (name == "spruce_log_side")
    {
        Grain(t, seed, 72, 50, 30, 12);
        for (int x = 0; x < S; ++x)
        {
            if (Noise::Rand2(x, 1, seed) > 0.6f) continue;
            for (int y = 0; y < S; ++y)
            {
                int n = (int)(N(x, y, seed) * 8.0f);
                t.Set(x, y, 56 + n, 38 + n, 22 + n);
            }
        }
        return true;
    }

    if (name == "water")
    {
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                int w = (int)(Blob(x, y, seed, 5.0f) * 22.0f);
                t.Set(x, y, 40 + w, 105 + w, 200 + w, 190);
            }
        return true;
    }

    if (name == "lava")
    {
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                int n = (int)(Blob(x, y, seed, 3.5f) * 60.0f);
                t.Set(x, y, 220 + n / 2, 90 + n, 20 + n / 3);
            }
        return true;
    }

    if (name == "gravel")
    {
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                float d = Noise::Cellular2((float)x / 2.6f, (float)y / 2.6f, seed);
                int v = 110 + (int)((0.7f - d) * 70.0f) + (int)(N(x, y, seed) * 10.0f);
                t.Set(x, y, v, v - 4, v - 10);
            }
        return true;
    }

    if (name == "cobblestone") { Cobble(t, seed, 140, 140, 142); return true; }

    if (name == "mossy_cobble")
    {
        Cobble(t, seed, 132, 134, 130);
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
                if (Blob(x, y, seed ^ 0x2211u, 4.0f) > 0.05f)
                {
                    int n = (int)(N(x, y, seed) * 14.0f);
                    t.Set(x, y, 70 + n, 105 + n, 55 + n);
                }
        return true;
    }

    if (name == "brick")
    {
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                int row = y / 4;
                int off = (row % 2) ? 4 : 0;
                bool mortar = (y % 4 == 3) || ((x + off) % 8 == 7);
                int n = (int)(N(x, y, seed) * 12.0f);
                if (mortar) t.Set(x, y, 168 + n / 2, 160 + n / 2, 155 + n / 2);
                else        t.Set(x, y, 150 + n, 76 + n, 62 + n);
            }
        return true;
    }

    if (name == "sandstone_top") { Grain(t, seed, 222, 208, 160, 10); return true; }

    if (name == "sandstone_side")
    {
        Grain(t, seed, 220, 206, 158, 8);
        for (int y = 0; y < S; ++y)
            if (y % 5 == 0)
                for (int x = 0; x < S; ++x)
                {
                    int n = (int)(N(x, y, seed) * 6.0f);
                    t.Set(x, y, 198 + n, 184 + n, 138 + n);
                }
        return true;
    }

    if (name == "snow") { Grain(t, seed, 244, 248, 255, 8); return true; }

    if (name == "snow_grass_side")
    {
        Grain(t, seed, 122, 88, 60, 12);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < S; ++x)
            {
                int n = (int)(N(x, y, seed) * 8.0f);
                t.Set(x, y, 244 + n, 248 + n, 255 + n);
            }
        return true;
    }

    if (name == "ice")
    {
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                int n = (int)(Blob(x, y, seed, 4.0f) * 26.0f);
                t.Set(x, y, 158 + n, 198 + n, 244 + n, 200);
            }
        return true;
    }

    if (name == "clay") { Grain(t, seed, 160, 166, 178, 10); return true; }

    if (name == "podzol_top")
    {
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                int n = (int)(Blob(x, y, seed, 3.0f) * 26.0f + N(x, y, seed) * 8.0f);
                t.Set(x, y, 92 + n, 62 + n, 30 + n);
            }
        return true;
    }

    if (name == "podzol_side")
    {
        Grain(t, seed, 122, 88, 60, 12);
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < S; ++x)
            {
                int n = (int)(N(x, y, seed) * 10.0f);
                t.Set(x, y, 88 + n, 58 + n, 28 + n);
            }
        return true;
    }

    if (name == "glass")
    {
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                bool border = (x == 0 || y == 0 || x == S - 1 || y == S - 1);
                bool glint  = (x + y == 5) || (x + y == 6);
                if (border)     t.Set(x, y, 205, 225, 235, 210);
                else if (glint) t.Set(x, y, 235, 245, 250, 90);
                else            t.Set(x, y, 220, 235, 245, 24);
            }
        return true;
    }

    if (name == "glowstone")
    {
        Grain(t, seed, 190, 150, 80, 14);
        for (int i = 0; i < 14; ++i)
        {
            int x = (int)(Noise::Rand2(i, 1, seed) * S);
            int y = (int)(Noise::Rand2(i, 2, seed) * S);
            t.Set(x, y, 255, 240, 170);
            t.Set(x + 1, y, 250, 225, 150);
            t.Set(x, y + 1, 250, 225, 150);
        }
        return true;
    }

    if (name == "obsidian")
    {
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                int n = (int)(N(x, y, seed) * 10.0f);
                bool spark = Noise::Rand2(x, y, seed ^ 0x77u) > 0.94f;
                if (spark) t.Set(x, y, 90 + n, 60 + n, 130 + n);
                else       t.Set(x, y, 22 + n, 16 + n, 34 + n);
            }
        return true;
    }

    if (name == "torch")
    {
        for (int y = 0; y < 10; ++y)
            for (int x = 6; x <= 9; ++x)
            {
                int n = (int)(N(x, y, seed) * 10.0f);
                t.Set(x, y, 108 + n, 76 + n, 40 + n);
            }
        for (int y = 10; y < 14; ++y)
            for (int x = 6; x <= 9; ++x)
                t.Set(x, y, 255, 210 - (13 - y) * 10, 90);
        t.Set(7, 14, 255, 246, 190);
        t.Set(8, 14, 255, 246, 190);
        return true;
    }

    if (name == "redstone_ore") { StoneBase(t, seed); Ore(t, seed, 190,  30,  30, 7); return true; }
    if (name == "emerald_ore")  { StoneBase(t, seed); Ore(t, seed,  50, 200, 110, 5); return true; }

    if (name == "cactus_top")
    {
        Grain(t, seed, 60, 130, 60, 10);
        for (int y = 3; y < 13; ++y)
            for (int x = 3; x < 13; ++x)
                if (x == 3 || y == 3 || x == 12 || y == 12)
                    t.Set(x, y, 42, 100, 42);
        return true;
    }

    if (name == "cactus_side")
    {
        Grain(t, seed, 54, 120, 54, 10);
        for (int y = 0; y < S; ++y)
        {
            t.Set(0, y, 38, 92, 38);
            t.Set(S - 1, y, 38, 92, 38);
            if (Noise::Rand2(0, y, seed) > 0.75f)
            {
                t.Set(5, y, 210, 220, 190);
                t.Set(11, y, 210, 220, 190);
            }
        }
        return true;
    }

    if (name == "tallgrass") { Clear(rgba); Plant(t, seed,  70, 140,  50, true); return true; }
    if (name == "deadbush")  { Clear(rgba); Plant(t, seed, 130,  95,  45, true); return true; }

    if (name == "flower_red" || name == "flower_yellow")
    {
        bool red = (name == "flower_red");
        Clear(rgba);
        Plant(t, seed, 60, 130, 50, false);
        for (int y = 2; y < 7; ++y)
            for (int x = 5; x < 11; ++x)
                if ((x != 5 && x != 10) || (y != 2 && y != 6))
                {
                    if (red) t.Set(x, y, 205, 55, 50);
                    else     t.Set(x, y, 235, 210, 60);
                }
        if (red) { t.Set(7, 4, 250, 220,  90); t.Set(8, 4, 250, 220,  90); }
        else     { t.Set(7, 4, 180, 130,  30); t.Set(8, 4, 180, 130,  30); }
        return true;
    }

    if (name == "mushroom_red" || name == "mushroom_brown")
    {
        bool red = (name == "mushroom_red");
        Clear(rgba);
        for (int y = 2; y < 6; ++y)
            for (int x = 6; x < 10; ++x)
                t.Set(x, y, 225, 220, 205);
        for (int y = 6; y < 11; ++y)
        {
            int w = 6 - (y - 6);
            for (int x = 8 - w; x <= 7 + w; ++x)
            {
                if (red) t.Set(x, y, 200, 45, 40);
                else     t.Set(x, y, 150, 110, 80);
            }
        }
        if (red)
        {
            t.Set(5, 8, 235, 230, 220);
            t.Set(10, 9, 235, 230, 220);
            t.Set(8, 7, 235, 230, 220);
        }
        return true;
    }

    if (name == "pumpkin_top")
    {
        Grain(t, seed, 205, 125, 30, 12);
        for (int y = 6; y < 10; ++y)
            for (int x = 6; x < 10; ++x)
                t.Set(x, y, 110, 90, 40);
        return true;
    }

    if (name == "pumpkin_side")
    {
        Grain(t, seed, 208, 128, 32, 10);
        for (int x = 0; x < S; x += 4)
            for (int y = 0; y < S; ++y)
                t.Set(x, y, 168, 96, 22);
        return true;
    }

    if (name == "crafting_top")
    {
        Grain(t, seed, 168, 126, 76, 10);
        for (int i = 0; i < S; ++i)
        {
            t.Set(i, 5, 96, 70, 40);  t.Set(i, 10, 96, 70, 40);
            t.Set(5, i, 96, 70, 40);  t.Set(10, i, 96, 70, 40);
        }
        return true;
    }

    if (name == "crafting_side")
    {
        Grain(t, seed, 156, 116, 70, 10);
        for (int y = 11; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                const int n = (int)(N(x, y, seed) * 10.0f);
                t.Set(x, y, 128 + n, 94 + n, 56 + n);
            }
        // Une scie et un marteau stylises sur le flanc
        for (int i = 0; i < 7; ++i) t.Set(2 + i, 8 - (i / 3), 90, 66, 38);
        for (int i = 0; i < 4; ++i) t.Set(11, 4 + i, 90, 66, 38);
        return true;
    }

    if (name == "porkchop" || name == "beef")
    {
        Clear(rgba);
        const bool pork = (name == "porkchop");
        const int r = pork ? 226 : 168, g = pork ? 138 : 74, b = pork ? 124 : 58;

        for (int y = 4; y < 13; ++y)
            for (int x = 3; x < 13; ++x)
            {
                const float d = Blob(x, y, seed, 3.0f);
                if (d < -0.18f) continue;
                const int n = (int)(N(x, y, seed) * 22.0f);
                t.Set(x, y, r + n, g + n, b + n);
            }

        // Filet de gras plus clair
        for (int x = 4; x < 12; ++x)
            if (Noise::Rand2(x, 9, seed) > 0.45f)
                t.Set(x, 9, 244, 226, 214);
        return true;
    }

    if (name == "stick")
    {
        Clear(rgba);
        ToolHandle(t, seed);
        return true;
    }

    // Outils: "tool_<categorie>_<materiau>"
    if (name.compare(0, 5, "tool_") == 0)
    {
        Clear(rgba);

        int kind = 0;
        if      (name.find("_pickaxe") != std::string::npos) kind = 0;
        else if (name.find("_axe")     != std::string::npos) kind = 1;
        else if (name.find("_shovel")  != std::string::npos) kind = 2;
        else                                                 kind = 3;

        int r = 150, g = 110, b = 60;                                  // bois
        if      (name.find("_stone")   != std::string::npos) { r = 132; g = 132; b = 136; }
        else if (name.find("_iron")    != std::string::npos) { r = 214; g = 214; b = 218; }
        else if (name.find("_diamond") != std::string::npos) { r =  85; g = 216; b = 210; }

        ToolHandle(t, seed);
        ToolHead(t, seed, kind, r, g, b);
        return true;
    }

    return false;
}

void GenerateBreakStrip(std::vector<unsigned char>& rgba, int& width, int& height)
{
    const int frames = 8;
    width = S * frames;
    height = S;
    rgba.assign((size_t)width * height * 4, 0);

    for (int f = 0; f < frames; ++f)
    {
        int cracks = 1 + f;
        for (int c = 0; c < cracks; ++c)
        {
            uint32_t s = 0xC0FFEEu + (uint32_t)c * 131u;
            float x = Noise::Rand2(c, 0, s) * S;
            float y = Noise::Rand2(0, c, s) * S;
            float a = Noise::Rand2(c, c, s) * 6.2831853f;
            int len = 4 + (int)(Noise::Rand2(c, 5, s) * 8.0f) + f;
            for (int k = 0; k < len; ++k)
            {
                a += (Noise::Rand2(c, k, s) - 0.5f) * 1.1f;
                x += cosf(a);
                y += sinf(a);
                int ix = (int)x, iy = (int)y;
                if (ix < 0 || iy < 0 || ix >= S || iy >= S) break;
                unsigned char* p = rgba.data() + ((size_t)iy * width + (size_t)f * S + ix) * 4;
                p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 190;
            }
        }
    }
}

} // namespace ProcTex
