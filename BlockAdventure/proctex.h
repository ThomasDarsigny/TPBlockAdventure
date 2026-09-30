#ifndef PROCTEX_H__
#define PROCTEX_H__

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
//  Generateur de textures procedurales 16x16 (RGBA).
//
//  Le projet ne possede des PNG que pour une poignee de blocs. Plutot que de
//  reutiliser une texture existante au hasard, on synthetise ici des tuiles
//  "pixel art" a partir de bruit hache: c'est deterministe, ca ne demande
//  aucun fichier, et ca reste facile a remplacer plus tard par de vrais
//  assets (il suffit de deposer le PNG et de changer l'enregistrement dans
//  blockinfo.cpp).
// ---------------------------------------------------------------------------
namespace ProcTex
{
    const int SIZE = 16;

    // Remplit 'rgba' (SIZE*SIZE*4 octets) avec la texture nommee.
    // Retourne false si le nom est inconnu.
    bool Generate(const std::string& name, std::vector<unsigned char>& rgba);

    // Bandeau horizontal de 8 etapes de cassure (128x16), fond transparent.
    void GenerateBreakStrip(std::vector<unsigned char>& rgba, int& width, int& height);
}

#endif // PROCTEX_H__
