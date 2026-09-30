#ifndef NOISE_H__
#define NOISE_H__

#include <cstdint>

// ---------------------------------------------------------------------------
//  Bruit coherent deterministe et thread-safe.
//
//  Contrairement a l'ancienne classe Perlin (qui utilisait rand() et un etat
//  interne initialise paresseusement), tout est calcule a partir de fonctions
//  de hachage pures: on peut donc generer des chunks depuis plusieurs threads
//  en parallele et obtenir exactement le meme monde a chaque execution.
// ---------------------------------------------------------------------------
namespace Noise
{
    // Hachage entier -> entier (base sur le mix de MurmurHash3)
    uint32_t Hash(uint32_t x);
    uint32_t Hash2(int x, int y, uint32_t seed);
    uint32_t Hash3(int x, int y, int z, uint32_t seed);

    // Valeur pseudo-aleatoire dans [0,1] pour une coordonnee donnee
    float Rand2(int x, int y, uint32_t seed);
    float Rand3(int x, int y, int z, uint32_t seed);

    // Bruit de gradient (style Perlin) dans [-1,1]
    float Gradient2(float x, float y, uint32_t seed);
    float Gradient3(float x, float y, float z, uint32_t seed);

    // Somme d'octaves (fractal brownian motion), resultat approx. dans [-1,1]
    float Fbm2(float x, float y, int octaves, float lacunarity, float gain, uint32_t seed);
    float Fbm3(float x, float y, float z, int octaves, float lacunarity, float gain, uint32_t seed);

    // Bruit "ridged": cretes acerees, ideal pour les montagnes, dans [0,1]
    float Ridged2(float x, float y, int octaves, float lacunarity, float gain, uint32_t seed);

    // Bruit cellulaire (Worley), retourne la distance au point le plus proche
    float Cellular2(float x, float y, uint32_t seed);
}

#endif // NOISE_H__
