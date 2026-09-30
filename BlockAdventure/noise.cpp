#include "noise.h"
#include <cmath>

namespace
{
    inline float Lerp(float a, float b, float t) { return a + t * (b - a); }

    // Courbe quintique de Perlin: derivees premiere et seconde nulles aux bords
    inline float Fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

    inline int FastFloor(float v)
    {
        int i = (int)v;
        return (v < (float)i) ? i - 1 : i;
    }
}

namespace Noise
{

uint32_t Hash(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint32_t Hash2(int x, int y, uint32_t seed)
{
    uint32_t h = seed;
    h ^= Hash((uint32_t)x * 0x9e3779b1U);
    h ^= Hash((uint32_t)y * 0x85ebca6bU + 0x165667b1U);
    return Hash(h);
}

uint32_t Hash3(int x, int y, int z, uint32_t seed)
{
    uint32_t h = seed;
    h ^= Hash((uint32_t)x * 0x9e3779b1U);
    h ^= Hash((uint32_t)y * 0x85ebca6bU + 0x165667b1U);
    h ^= Hash((uint32_t)z * 0xc2b2ae35U + 0x27d4eb2fU);
    return Hash(h);
}

float Rand2(int x, int y, uint32_t seed)
{
    return (float)(Hash2(x, y, seed) & 0xffffff) / (float)0xffffff;
}

float Rand3(int x, int y, int z, uint32_t seed)
{
    return (float)(Hash3(x, y, z, seed) & 0xffffff) / (float)0xffffff;
}

// Produit scalaire avec un gradient pseudo-aleatoire unitaire
static inline float Grad2(int ix, int iy, float dx, float dy, uint32_t seed)
{
    uint32_t h = Hash2(ix, iy, seed);
    float angle = (float)(h & 0xffff) * (6.2831853f / 65536.0f);
    return dx * cosf(angle) + dy * sinf(angle);
}

static inline float Grad3(int ix, int iy, int iz, float dx, float dy, float dz, uint32_t seed)
{
    // 12 gradients classiques de Perlin (arretes d'un cube)
    static const float g[12][3] = {
        { 1, 1, 0}, {-1, 1, 0}, { 1,-1, 0}, {-1,-1, 0},
        { 1, 0, 1}, {-1, 0, 1}, { 1, 0,-1}, {-1, 0,-1},
        { 0, 1, 1}, { 0,-1, 1}, { 0, 1,-1}, { 0,-1,-1}
    };
    const float* v = g[Hash3(ix, iy, iz, seed) % 12];
    return dx * v[0] + dy * v[1] + dz * v[2];
}

float Gradient2(float x, float y, uint32_t seed)
{
    int x0 = FastFloor(x), y0 = FastFloor(y);
    float fx = x - (float)x0, fy = y - (float)y0;
    float u = Fade(fx), v = Fade(fy);

    float n00 = Grad2(x0,     y0,     fx,        fy,        seed);
    float n10 = Grad2(x0 + 1, y0,     fx - 1.0f, fy,        seed);
    float n01 = Grad2(x0,     y0 + 1, fx,        fy - 1.0f, seed);
    float n11 = Grad2(x0 + 1, y0 + 1, fx - 1.0f, fy - 1.0f, seed);

    return Lerp(Lerp(n00, n10, u), Lerp(n01, n11, u), v);
}

float Gradient3(float x, float y, float z, uint32_t seed)
{
    int x0 = FastFloor(x), y0 = FastFloor(y), z0 = FastFloor(z);
    float fx = x - (float)x0, fy = y - (float)y0, fz = z - (float)z0;
    float u = Fade(fx), v = Fade(fy), w = Fade(fz);

    float n000 = Grad3(x0,     y0,     z0,     fx,        fy,        fz,        seed);
    float n100 = Grad3(x0 + 1, y0,     z0,     fx - 1.0f, fy,        fz,        seed);
    float n010 = Grad3(x0,     y0 + 1, z0,     fx,        fy - 1.0f, fz,        seed);
    float n110 = Grad3(x0 + 1, y0 + 1, z0,     fx - 1.0f, fy - 1.0f, fz,        seed);
    float n001 = Grad3(x0,     y0,     z0 + 1, fx,        fy,        fz - 1.0f, seed);
    float n101 = Grad3(x0 + 1, y0,     z0 + 1, fx - 1.0f, fy,        fz - 1.0f, seed);
    float n011 = Grad3(x0,     y0 + 1, z0 + 1, fx,        fy - 1.0f, fz - 1.0f, seed);
    float n111 = Grad3(x0 + 1, y0 + 1, z0 + 1, fx - 1.0f, fy - 1.0f, fz - 1.0f, seed);

    float x00 = Lerp(n000, n100, u);
    float x10 = Lerp(n010, n110, u);
    float x01 = Lerp(n001, n101, u);
    float x11 = Lerp(n011, n111, u);

    return Lerp(Lerp(x00, x10, v), Lerp(x01, x11, v), w);
}

float Fbm2(float x, float y, int octaves, float lacunarity, float gain, uint32_t seed)
{
    float sum = 0.0f, amp = 1.0f, norm = 0.0f, freq = 1.0f;
    for (int i = 0; i < octaves; ++i)
    {
        sum += Gradient2(x * freq, y * freq, seed + (uint32_t)i * 7919U) * amp;
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return (norm > 0.0f) ? sum / norm : 0.0f;
}

float Fbm3(float x, float y, float z, int octaves, float lacunarity, float gain, uint32_t seed)
{
    float sum = 0.0f, amp = 1.0f, norm = 0.0f, freq = 1.0f;
    for (int i = 0; i < octaves; ++i)
    {
        sum += Gradient3(x * freq, y * freq, z * freq, seed + (uint32_t)i * 7919U) * amp;
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return (norm > 0.0f) ? sum / norm : 0.0f;
}

float Ridged2(float x, float y, int octaves, float lacunarity, float gain, uint32_t seed)
{
    float sum = 0.0f, amp = 1.0f, norm = 0.0f, freq = 1.0f;
    for (int i = 0; i < octaves; ++i)
    {
        float n = 1.0f - fabsf(Gradient2(x * freq, y * freq, seed + (uint32_t)i * 6151U));
        n *= n;
        sum += n * amp;
        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }
    return (norm > 0.0f) ? sum / norm : 0.0f;
}

float Cellular2(float x, float y, uint32_t seed)
{
    int xi = FastFloor(x), yi = FastFloor(y);
    float best = 8.0f;
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            int cx = xi + dx, cy = yi + dy;
            float px = (float)cx + Rand2(cx, cy, seed);
            float py = (float)cy + Rand2(cx, cy, seed ^ 0x5bf03635U);
            float ddx = px - x, ddy = py - y;
            float d = ddx * ddx + ddy * ddy;
            if (d < best) best = d;
        }
    }
    return sqrtf(best);
}

} // namespace Noise
