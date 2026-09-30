# BlockAdventure

Clone de Minecraft en C++ / OpenGL (SFML + GLEW + DevIL).

Monde quasi infini genere en continu autour du joueur, eclairage propage
bloc par bloc, et un pipeline de rendu shader avec ciel procedural,
occlusion ambiante et post-traitement HDR.

---

## Lancer le jeu

Ouvrir `BlockAdventure.sln` dans Visual Studio 2022, configuration
**Debug | x86**, puis F5. Le repertoire de travail doit rester le dossier du
projet (`BlockAdventure\BlockAdventure`) : les chemins des ressources sont
relatifs (`../BlockAdventure/media/...`).

La configuration **Release** compile et s'execute plus vite, mais le depot ne
contient que les DLL SFML de debug. Pour l'utiliser, deposer dans `Release\`
les DLL SFML 2.3 de release (`sfml-system-2.dll`, `sfml-window-2.dll`,
`sfml-graphics-2.dll`). Les autres DLL (DevIL, GLEW) y sont deja.

Le monde est genere a partir de la graine definie dans `engine.cpp`
(`WORLD_SEED`). La changer donne un monde entierement different.

---

## Commandes

| Touche | Action |
|---|---|
| `Z Q S D` / `W A S D` | Se deplacer |
| Souris | Regarder |
| `Espace` | Sauter, monter en vol, nager vers le haut |
| `Maj gauche` | S'accroupir, descendre en vol |
| `Ctrl gauche` | Courir |
| Clic gauche (maintenu) | Casser un bloc |
| Clic droit | Poser le bloc selectionne |
| Clic molette | Copier le bloc vise dans la barre |
| Molette / `1`-`9` | Choisir la case de la barre rapide |
| `E` | Inventaire (palette creative en mode creatif) |
| `F` | Activer / desactiver le vol |
| `G` | Basculer creatif / survie |
| `R` | Reapparaitre apres la mort |
| `Echap` | Menu pause |

Touches de fonction :

| Touche | Action |
|---|---|
| `F1` | Afficher / masquer l'interface |
| `F2` | Capture d'ecran (`media/screenshots`) |
| `F3` | Informations de debogage |
| `F5` / `F6` | Sauvegarder / recharger |
| `F7` / `F8` | Reduire / augmenter la distance d'affichage |
| `F9` | Figer le cycle jour-nuit |
| `F10` | Plein ecran |
| `F11` | Synchronisation verticale |
| `Y` | Rendu en fil de fer |

La partie est sauvegardee automatiquement toutes les deux minutes, ainsi
qu'a la fermeture du jeu (`media/saves/`).

### Options de ligne de commande

Utiles pour aller directement verifier une situation precise :

```
BlockAdventure.exe --tp 0 90 0 --look 40 10 --time 0.42 --creative --debug
BlockAdventure.exe --shot 12        # capture apres 12 s puis quitte
BlockAdventure.exe --map            # carte des biomes/hauteurs sur la sortie standard
```

---

## Ce que contient le moteur

### Monde

- **Chunks de 16 x 160 x 16**, charges et decharges autour du joueur
  (distance reglable de 2 a 24 chunks). Le monde n'a pas de bord.
- **Generation multi-thread** : un pool de threads (nombre de coeurs - 1)
  produit le terrain et les maillages ; le thread principal ne fait que
  l'insertion, l'eclairage et l'envoi au GPU.
- **Relief multi-bruit** : continentalite, erosion, vallonnement et cretes
  combines. Oceans profonds, plaines, collines et montagnes jusqu'a ~150.
- **9 biomes** : ocean, plage, plaines, foret, taiga, toundra, desert,
  marais, montagnes — determines par temperature, humidite et altitude.
- **Grottes** : intersection de deux champs de bruit 3D (galeries allongees)
  plus quelques cavernes en profondeur, ~3,4 % du sous-sol creuse.
- **Minerais** par filons, avec des plages de profondeur distinctes
  (charbon, fer, or, redstone, diamant, emeraude en montagne).
- **Vegetation** : chenes, bouleaux, sapins, cactus, herbes hautes, fleurs,
  champignons. Les arbres sont places sur une grille de cellules, ce qui
  garantit un espacement correct et un rendu identique des deux cotes d'une
  frontiere de chunk.
- **Eau et lave** : mers, lacs, glace dans les biomes froids, lacs de lave
  en profondeur.
- **Sauvegarde** : seuls les chunks modifies par le joueur sont ecrits, en
  compression RLE.

### Eclairage

Deux canaux sur 4 bits, comme Minecraft :

- **lumiere du ciel**, attenuee par l'opacite traversee (l'eau assombrit
  progressivement) et propagee en largeur d'abord a travers les chunks ;
- **lumiere des blocs**, emise par les torches, la pierre lumineuse et la
  lave.

Les deux sont conservees separement jusqu'au shader : le cycle jour-nuit ne
module que la premiere, une torche eclaire donc autant a minuit qu'a midi,
sans jamais reconstruire le maillage.

### Rendu

- **Format de sommet compact de 16 octets** (position en 1/8 de bloc,
  lumieres, occlusion ambiante et code de face empaquetes). Un unique index
  buffer est partage par tous les chunks.
- **Occlusion ambiante** calculee par sommet, avec rotation du quad pour
  eviter l'artefact en escalier dans les coins.
- **Eclairage lisse** : moyenne des cellules voisines a chaque coin de face.
- **Passes separees** : opaque avec test alpha (feuillages, plantes), puis
  transparente triee de l'arriere vers l'avant (eau, verre, glace).
- **Culling par frustum** sur la boite englobante reelle de chaque chunk.
- **Ciel procedural** (`sky.frag`) : degrade zenith/horizon, soleil et lune,
  etoiles ponctuelles scintillantes, nuages animes obtenus en projetant le
  rayon de vue sur un plan d'altitude.
- **Eau** : ondulation des sommets de surface, normale perturbee, reflet
  speculaire et Fresnel.
- **Feuillages** balances par le vent dans le vertex shader.
- **Post-traitement** (`postfx.cpp`) : rendu HDR 16 bits, MSAA 4x resolu par
  blit, extraction des hautes lumieres, flou gaussien separable, puis
  composition avec tone mapping ACES, saturation, vignettage, ondulation et
  teinte sous-marines. Si le materiel ne suit pas, tout se desactive
  proprement et le rendu passe en direct.
- **Textures procedurales** : les blocs sans PNG (feuilles, eau, gravier,
  verre, torche, minerais...) sont synthetises au demarrage a partir de
  bruit hache, puis assembles dans un atlas mipmappe tuile par tuile.

### Jeu

- Boite de collision alignee sur les axes, gravite, saut, course,
  accroupissement, nage, degats de chute, noyade, brulure dans la lave,
  faim et regeneration.
- Modes survie et creatif, vol.
- Selection du bloc vise par lancer de rayon (DDA), contour noir et
  fissures de minage progressives selon la durete du bloc.
- Inventaire de 36 cases avec barre rapide, piles de 64, ramassage
  automatique de ce que l'on casse, palette creative.
- Interface : viseur, barre rapide avec icones et compteurs, coeurs, faim,
  bulles d'oxygene, ecran de mort, menu pause, superposition de debogage.

---

## Organisation des fichiers

| Fichier | Role |
|---|---|
| `noise.*` | Bruit de gradient hache, deterministe et utilisable depuis plusieurs threads |
| `worldgen.*` | Relief, biomes, grottes, minerais, arbres et decor |
| `world.*` | Carte des chunks, streaming, pool de threads, lumiere, lancer de rayon, sauvegarde |
| `chunk.*` | Stockage des blocs et des lumieres d'un chunk, tampons GPU |
| `chunkmesher.*` | Construction du maillage a partir d'un instantane (occlusion ambiante, lumiere lissee) |
| `blockinfo.*` | Table des proprietes de chaque type de bloc |
| `proctex.*` | Generation des textures manquantes |
| `textureatlas.*` | Assemblage de l'atlas et de ses mipmaps |
| `vertexbuffer.*` | Format de sommet compact et index buffer partage |
| `postfx.*` | Chaine de post-traitement |
| `frustum.h` | Extraction des plans du frustum |
| `player.*` | Physique et etat du joueur |
| `inventory.*` | Inventaire et barre rapide |
| `engine.*` | Assemblage : rendu, interface, entrees, sauvegarde |

`perlin.*` (l'ancien generateur de bruit) est conserve mais n'est plus
utilise : `noise.*` le remplace, car il fallait un bruit sans etat global
pour pouvoir generer plusieurs chunks en parallele.

---

## Limites connues et suites possibles

- L'eau et la lave sont statiques : pas d'ecoulement ni de mise a jour de
  fluide.
- Pas encore de creatures ni d'entites : le monde est vide de vie animale.
- Pas d'etabli ni de recettes : l'inventaire ne fait que stocker.
- Pas de sons (le projet lie deja `sfml-audio`, mais aucun echantillon n'est
  fourni).
- Pas d'ombres portees : l'ombrage vient de l'occlusion ambiante et de
  l'ombrage directionnel par face.
- Pas de redstone, de portails ni de dimensions.
