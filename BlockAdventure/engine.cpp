#include "engine.h"
#include "frustum.h"
#include "entity.h"
#include <vector>
#include "proctex.h"
#include "tool.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <vector>
#include <IL/il.h>

namespace
{
    // Graine du monde. La changer donne un monde completement different.
    const uint32_t WORLD_SEED = 20231211u;

    const float FIELD_OF_VIEW = 70.0f;
    const float DAY_LENGTH    = 900.0f;   // secondes pour un cycle complet
    const float MOUSE_SENS    = 0.09f;
    const float AUTOSAVE_EVERY = 120.0f;

    // Delai maximum entre deux appuis pour former un "double appui"
    const float DOUBLE_TAP_DELAY = 0.30f;

    // Temps de repos apres la destruction d'un bloc. Sans lui, un clic
    // maintenu casse un bloc par image, donc toute une colonne d'un coup.
    const float BREAK_COOLDOWN = 0.25f;

    inline float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    inline float SmoothStep(float a, float b, float x)
    {
        const float t = Clamp((x - a) / (b - a), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    inline Vector3f Mix(const Vector3f& a, const Vector3f& b, float t)
    {
        return Vector3f(a.x + (b.x - a.x) * t,
                        a.y + (b.y - a.y) * t,
                        a.z + (b.z - a.z) * t);
    }
}

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------
Engine::Engine()
    : m_timeOfDay(0.32f), m_timeFrozen(false), m_gameTime(0.0f),
      m_sunDir(0.0f, 1.0f, 0.0f), m_sunColor(1.0f, 1.0f, 1.0f),
      m_zenithColor(0.2f, 0.4f, 0.9f), m_horizonColor(0.6f, 0.8f, 1.0f),
      m_groundColor(0.2f, 0.2f, 0.25f), m_ambient(0.05f, 0.05f, 0.06f), m_dayFactor(1.0f),
      m_hasTarget(false), m_targetBlock(0, 0, 0), m_targetNormal(0, 0, 0),
      m_breakProgress(0.0f), m_breakCooldown(0.0f), m_breakingBlock(0, 0, 0),
      m_leftDown(false), m_rightDown(false), m_placeCooldown(0.0f), m_attackCooldown(0.0f),
      m_lastForwardTap(-10.0f), m_lastJumpTap(-10.0f),
      m_paused(false), m_settingsMenu(false), m_fpsMenu(false), m_inventoryOpen(false),
      m_showHud(true), m_showDebug(false), m_wireframe(false), m_draggingSlider(false),
      m_mouseX(0), m_mouseY(0), m_craftSize(2), m_palettePage(0),
      m_fpsAccum(0.0f), m_fpsFrames(0), m_fps(0),
      m_resourcesLoaded(false), m_underwater(false), m_autoSaveTimer(0.0f),
      m_autoShot(-1.0f), m_shotCounter(0), m_shotRetries(0),
      m_startPos(0.0f, 0.0f, 0.0f), m_hasStartPos(false),
      m_startYaw(0.0f), m_startPitch(0.0f), m_hasStartLook(false), m_startCreative(false), m_dumpMap(false), m_fsTest(-1.0f), m_playerPlaced(false), m_fluidTest(false), m_craftTest(false), m_mobTest(false),
      m_startTime(0.35f), m_hasStartTime(false)
{
}

Engine::~Engine()
{
}

// ---------------------------------------------------------------------------
//  Initialisation
// ---------------------------------------------------------------------------
void Engine::Init()
{
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK)
    {
        std::cerr << "ERREUR GLEW: " << glewGetErrorString(glewErr) << std::endl;
        abort();
    }

    std::cout << "OpenGL " << (const char*)glGetString(GL_VERSION)
              << " / " << (const char*)glGetString(GL_RENDERER) << std::endl;

    glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Le monde n'est cree qu'une fois: Init() est rappele apres un
    // changement de mode plein ecran, mais le terrain, lui, ne bouge pas.
    if (!m_world)
    {
        m_world = std::unique_ptr<World>(new World(WORLD_SEED));

        if (!LoadGame())
        {
            float sx, sy, sz;
            m_world->Generator().FindSpawn(sx, sy, sz);
            m_player.Reset(Vector3f(sx, sy, sz));
            m_inventory.GiveStarterKit();
        }

        // Les options de ligne de commande priment sur la sauvegarde
        if (m_hasStartTime) m_timeOfDay = m_startTime;
        if (m_hasStartPos)  m_player.Reset(m_startPos);
        if (m_hasStartLook) m_player.SetRotation(m_startPitch, m_startYaw);
        if (m_startCreative) { m_player.SetCreative(true); m_player.ToggleFly(); }

        std::cout << "[Engine] Apparition en " << m_player.GetPosition() << std::endl;

        if (m_dumpMap)
        {
            const WorldGenerator& g = m_world->Generator();
            // Proportion de blocs creuses sous terre. On genere quelques
            // chunks a part: a cet instant le monde n'a encore rien charge.
            {
                int air = 0, total = 0;
                for (int c = 0; c < 4; ++c)
                {
                    Chunk probe(c * 7, c * 5);
                    g.Generate(probe);
                    for (int lz = 0; lz < CHUNK_SIZE_Z; ++lz)
                        for (int lx = 0; lx < CHUNK_SIZE_X; ++lx)
                        {
                            const int h = g.SurfaceHeight(c * 7 * CHUNK_SIZE_X + lx,
                                                          c * 5 * CHUNK_SIZE_Z + lz);
                            for (int y = 6; y < h - 3 && y < 60; ++y)
                            {
                                ++total;
                                if (probe.GetBlock(lx, y, lz) == BTYPE_AIR) ++air;
                            }
                        }
                }
                std::cout << "--- creusement souterrain: "
                          << (total ? (100.0 * air / total) : 0.0) << " % ---" << std::endl;

            }

            std::cout << "--- carte (hauteur / biome), pas de 24 blocs ---" << std::endl;
            for (int z = -12; z <= 12; ++z)
            {
                for (int x = -12; x <= 12; ++x)
                {
                    const int h = g.SurfaceHeight(x * 24, z * 24);
                    const Biome b = g.BiomeAt(x * 24, z * 24);
                    char c = '?';
                    switch (b)
                    {
                    case BIOME_OCEAN: c = '~'; break;
                    case BIOME_BEACH: c = '.'; break;
                    case BIOME_PLAINS: c = ','; break;
                    case BIOME_FOREST: c = 'T'; break;
                    case BIOME_TAIGA: c = 't'; break;
                    case BIOME_SNOWY: c = 'S'; break;
                    case BIOME_DESERT: c = 'D'; break;
                    case BIOME_SWAMP: c = 'm'; break;
                    case BIOME_MOUNTAINS: c = 'M'; break;
                    default: break;
                    }
                    std::cout << c;
                }
                std::cout << "   ";
                for (int x = -12; x <= 12; x += 6)
                    std::cout << g.SurfaceHeight(x * 24, z * 24) << " ";
                std::cout << std::endl;
            }
        }
    }

    CenterMouse();
    HideCursor();
}

void Engine::DeInit()
{
    // Appele a la fermeture de la fenetre comme au changement de mode plein
    // ecran: dans les deux cas on ne veut pas perdre la partie en cours.
    if (m_world)
        SaveGame();
}

// ---------------------------------------------------------------------------
//  Ressources
// ---------------------------------------------------------------------------
void Engine::LoadResource()
{
    BuildTextureAtlas();
    LoadShaders();
    LoadHudTextures();

    m_postfx.Init(Width(), Height());

    // Le contexte OpenGL peut avoir ete recree: tous les VBO sont perdus
    if (m_world)
        m_world->InvalidateAllMeshes();

    m_resourcesLoaded = true;
}

void Engine::UnloadResource()
{
    if (m_world)
        m_world->InvalidateAllMeshes();

    VertexBuffer::ReleaseSharedIndices();

    m_postfx.Destroy();
    m_terrainShader.Destroy();
    m_skyShader.Destroy();

    m_atlas.reset();

    m_textureFont.Destroy();
    m_textureCrosshair.Destroy();
    m_textureBreak.Destroy();
    m_textureLogo.Destroy();
    m_textureOptionsButton.Destroy();
    m_textureQuitButton.Destroy();
    m_textureBackButton.Destroy();
    m_textureBacktoGameButton.Destroy();
    m_textureFPS.Destroy();
    m_textureFullScreenON.Destroy();
    m_textureFullScreenOFF.Destroy();
    m_texture30Fps.Destroy();
    m_texture60Fps.Destroy();
    m_texture120Fps.Destroy();
    m_texture240Fps.Destroy();

    m_resourcesLoaded = false;
}

void Engine::BuildTextureAtlas()
{
    // 256 emplacements: blocs, objets et outils tiennent largement
    m_atlas = std::unique_ptr<TextureAtlas>(new TextureAtlas(256));

    Blocks::RegisterTextures(*m_atlas);

    if (!m_atlas->Generate(64, true))
    {
        std::cerr << "[Engine] Impossible de generer l'atlas de textures" << std::endl;
        Stop();
        return;
    }

    Blocks::ComputeUVs(*m_atlas);

    std::cout << "[Engine] Atlas: " << m_atlas->TextureCount() << " textures" << std::endl;
}

void Engine::LoadShaders()
{
    std::cout << "Compilation des shaders..." << std::endl;

    if (!m_terrainShader.Load(SHADER_PATH "terrain.vert", SHADER_PATH "terrain.frag", true))
    {
        std::cerr << "[Engine] Shader de terrain indisponible" << std::endl;
        Stop();
        return;
    }

    if (!m_skyShader.Load(SHADER_PATH "sky.vert", SHADER_PATH "sky.frag", true))
        std::cerr << "[Engine] Shader de ciel indisponible (ciel uni)" << std::endl;
}

void Engine::LoadHudTextures()
{
    LoadTexture(m_textureFont, TEXTURE_PATH "font.png");
    LoadTexture(m_textureCrosshair, TEXTURE_PATH "crosshair.png");
    LoadTexture(m_textureLogo, TEXTURE_PATH "logo.png", false);
    LoadTexture(m_textureOptionsButton, TEXTURE_PATH "OptionsButton.png", false);
    LoadTexture(m_textureQuitButton, TEXTURE_PATH "QuitButton.png", false);
    LoadTexture(m_textureBackButton, TEXTURE_PATH "BackButton.png", false);
    LoadTexture(m_textureBacktoGameButton, TEXTURE_PATH "BacktoGameButton.png", false);
    LoadTexture(m_textureFPS, TEXTURE_PATH "FPSSETTINGS.png", false);
    LoadTexture(m_textureFullScreenON, TEXTURE_PATH "FullscreenON.png", false);
    LoadTexture(m_textureFullScreenOFF, TEXTURE_PATH "FullscreenOFF.png", false);
    LoadTexture(m_texture30Fps, TEXTURE_PATH "30Fps.png", false);
    LoadTexture(m_texture60Fps, TEXTURE_PATH "60Fps.png", false);
    LoadTexture(m_texture120Fps, TEXTURE_PATH "120Fps.png", false);
    LoadTexture(m_texture240Fps, TEXTURE_PATH "240Fps.png", false);

    // Etapes de cassure, generees proceduralement
    std::vector<unsigned char> strip;
    int bw = 0, bh = 0;
    ProcTex::GenerateBreakStrip(strip, bw, bh);
    m_textureBreak.LoadFromMemory(bw, bh, strip.data());
}

bool Engine::LoadTexture(Texture& texture, const std::string& filename, bool stopOnError)
{
    texture.Load(filename);
    if (!texture.IsValid())
    {
        std::cerr << "Texture introuvable (" << filename << ")" << std::endl;
        if (stopOnError)
            Stop();
        return false;
    }
    return true;
}

void Engine::ResizeEvent(int width, int height)
{
    if (width < 1 || height < 1)
        return;

    glViewport(0, 0, width, height);
    if (m_resourcesLoaded)
        m_postfx.Resize(width, height);
}

// ---------------------------------------------------------------------------
//  Boucle principale
// ---------------------------------------------------------------------------
void Engine::Render(float elapsedTime)
{
    if (elapsedTime <= 0.0f) elapsedTime = 1.0f / 60.0f;
    if (elapsedTime > 0.25f) elapsedTime = 0.25f;

    m_fpsAccum += elapsedTime;
    ++m_fpsFrames;
    if (m_fpsAccum >= 0.25f)
    {
        m_fps = (int)((float)m_fpsFrames / m_fpsAccum);
        m_fpsAccum = 0.0f;
        m_fpsFrames = 0;
    }

    UpdateGame(elapsedTime);

    // --- rendu de la scene ------------------------------------------------
    m_postfx.BeginScene();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    RenderSky();
    RenderWorld();

    m_postfx.EndScene();
    m_postfx.Render(m_gameTime, m_underwater, m_player.DamageFlash());

    // --- interface --------------------------------------------------------
    DrawHud(elapsedTime);

    if (m_fsTest > 0.0f)
    {
        m_fsTest -= elapsedTime;
        if (m_fsTest <= 0.0f)
        {
            std::cout << "[Test] bascule plein ecran" << std::endl;
            SetFullscreen(!IsFullscreen());
            return;   // le contexte vient d'etre recree, on reprend a la frame suivante
        }
    }

    if (m_autoShot > 0.0f)
    {
        m_autoShot -= elapsedTime;
        if (m_autoShot <= 0.0f)
        {
            glFinish();
            if (TakeScreenshot() || ++m_shotRetries > 30)
                Stop();
            else
                m_autoShot = 0.001f;   // on retente a la frame suivante
        }
    }
}

void Engine::UpdateGame(float elapsed)
{
    const bool interactive = !m_paused && !m_inventoryOpen;

    UpdateTimeOfDay(elapsed);
    m_gameTime += elapsed;

    // Tant que le chunk sous le joueur n'est pas genere, le monde repond
    // "air" partout: appliquer la gravite le ferait traverser le sol pendant
    // le chargement.
    const Vector3f& pos = m_player.GetPosition();
    const Chunk* here = m_world->GetChunk(World::ChunkCoord((int)floorf(pos.x)),
                                          World::ChunkCoord((int)floorf(pos.z)));
    const bool worldReady = (here != 0 && here->generated);

    if (worldReady && !m_playerPlaced)
    {
        m_player.Unstuck(*m_world);
        m_playerPlaced = true;

        if (m_mobTest)
        {
            // Un troupeau et quelques zombies devant le joueur
            const float r = 0.0f;
            for (int i = 0; i < 4; ++i)
            {
                m_entities.Spawn(ENT_PIG,    Vector3f(pos.x + 3.0f + i * 1.6f, pos.y + 1.0f, pos.z - 4.0f + r));
                m_entities.Spawn(ENT_COW,    Vector3f(pos.x + 3.0f + i * 1.6f, pos.y + 1.0f, pos.z - 7.0f + r));
                m_entities.Spawn(ENT_ZOMBIE, Vector3f(pos.x + 3.0f + i * 1.6f, pos.y + 1.0f, pos.z - 10.0f + r));
            }
            std::cout << "[Test] " << m_entities.Count() << " creatures" << std::endl;
        }

        if (m_craftTest)
        {
            // Grille pre-remplie avec la recette de la pioche en bois
            OpenInventory(3);
            m_craft[0].type = BTYPE_WOODPLANK; m_craft[0].count = 3;
            m_craft[1].type = BTYPE_WOODPLANK; m_craft[1].count = 3;
            m_craft[2].type = BTYPE_WOODPLANK; m_craft[2].count = 3;
            m_craft[4].type = BTYPE_STICK;     m_craft[4].count = 2;
            m_craft[7].type = BTYPE_STICK;     m_craft[7].count = 2;

            m_inventory.Add(BTYPE_STICK, 12);
            m_inventory.Add(BTYPE_DIAM_PICKAXE, 1);
            m_inventory.Add(BTYPE_IRON_SWORD, 1);
            m_inventory.Add(BTYPE_STONE_AXE, 1);
            m_inventory.Add(BTYPE_WOOD_SHOVEL, 1);
            m_inventory.Add(BTYPE_CRAFTING_TABLE, 4);

            BlockType t; int c;
            std::cout << "[Test] recette reconnue: " << (CraftResult(t, c) ?
                          Blocks::Get(t).name : std::string("aucune")) << std::endl;
        }

        if (m_fluidTest)
        {
            // Une source d'eau lachee en l'air: elle doit tomber puis
            // s'etaler sur le sol.
            const int bx = (int)floorf(pos.x), by = (int)floorf(pos.y), bz = (int)floorf(pos.z);
            m_world->SetBlock(bx + 6, by - 4, bz, BTYPE_WATER, 0);

            // Plateforme de sable suspendue: elle doit s'ecrouler
            for (int dz = -1; dz <= 1; ++dz)
                for (int dx = -1; dx <= 1; ++dx)
                    m_world->SetBlock(bx + 8 + dx, by - 6, bz + 8 + dz, BTYPE_SAND, 0);

            std::cout << "[Test] eau en " << (bx + 6) << " " << (by - 4) << " " << bz
                      << ", sable en " << (bx + 8) << " " << (by - 6) << " " << (bz + 8) << std::endl;
        }
    }

    if (interactive && !m_player.IsDead() && worldReady)
        m_player.Update(*m_world, elapsed);
    else
        m_player.SetMoveInput(false, false, false, false);

    m_world->Update(m_player.GetPosition(), 4);
    m_world->Tick(elapsed);

    // Creatures: elles ne vivent que lorsque le terrain sous elles existe
    if (worldReady)
    {
        float damage = 0.0f;
        m_entities.Update(*m_world, m_player.GetPosition(), damage,
                          m_dayFactor, elapsed, !m_player.IsCreative());
        if (damage > 0.0f)
            m_player.ApplyDamage(damage);
    }

    const Vector3f eye = m_player.EyePosition();
    m_underwater = Blocks::IsLiquid(m_world->GetBlock((int)floorf(eye.x),
                                                      (int)floorf(eye.y),
                                                      (int)floorf(eye.z)));

    UpdateTargetBlock();

    if (m_placeCooldown > 0.0f)
        m_placeCooldown -= elapsed;
    if (m_attackCooldown > 0.0f)
        m_attackCooldown -= elapsed;

    if (interactive && !m_player.IsDead())
    {
        UpdateBreaking(elapsed);

        if (m_rightDown && m_placeCooldown <= 0.0f)
        {
            if (!EatHeldFood())
                PlaceBlock();
            m_placeCooldown = 0.30f;
        }
    }
    else
    {
        m_breakProgress = 0.0f;
    }

    m_autoSaveTimer += elapsed;
    if (m_autoSaveTimer > AUTOSAVE_EVERY)
    {
        m_autoSaveTimer = 0.0f;
        SaveGame();
    }
}

void Engine::UpdateTimeOfDay(float elapsed)
{
    if (!m_timeFrozen)
    {
        m_timeOfDay += elapsed / DAY_LENGTH;
        while (m_timeOfDay >= 1.0f) m_timeOfDay -= 1.0f;
    }
    ComputeSkyColors();
}

void Engine::ComputeSkyColors()
{
    // Le soleil decrit un arc incline: il se leve a l'est et se couche a l'ouest
    const float ang = (m_timeOfDay - 0.25f) * 6.2831853f;
    m_sunDir = Vector3f(cosf(ang) * 0.38f, sinf(ang), cosf(ang) * 0.92f);
    m_sunDir.Normalize();

    const float sy = m_sunDir.y;

    m_dayFactor = SmoothStep(-0.12f, 0.22f, sy);
    const float dusk = expf(-(sy / 0.17f) * (sy / 0.17f));

    const Vector3f nightZen(0.008f, 0.012f, 0.034f);
    const Vector3f nightHor(0.028f, 0.038f, 0.078f);
    const Vector3f dayZen(0.14f, 0.33f, 0.86f);
    const Vector3f dayHor(0.46f, 0.68f, 0.98f);
    const Vector3f duskZen(0.20f, 0.15f, 0.42f);
    const Vector3f duskHor(1.05f, 0.36f, 0.13f);

    m_zenithColor = Mix(Mix(nightZen, dayZen, m_dayFactor), duskZen, dusk * 0.75f);
    m_horizonColor = Mix(Mix(nightHor, dayHor, m_dayFactor), duskHor, dusk * 0.85f);
    m_groundColor = m_horizonColor * 0.32f;

    m_sunColor = Mix(Vector3f(1.12f, 1.08f, 0.98f), Vector3f(1.55f, 0.58f, 0.24f), dusk);

    m_ambient = Mix(Vector3f(0.012f, 0.015f, 0.028f),
                    Vector3f(0.045f, 0.047f, 0.050f), m_dayFactor);
}

// ---------------------------------------------------------------------------
//  Interaction avec les blocs
// ---------------------------------------------------------------------------
void Engine::UpdateTargetBlock()
{
    m_hasTarget = m_world->Raycast(m_player.EyePosition(), m_player.Direction(),
                                   MAX_SELECTION_DISTANCE, m_targetBlock, m_targetNormal);

    if (!m_hasTarget)
        m_breakProgress = 0.0f;
}

void Engine::UpdateBreaking(float elapsed)
{
    if (m_breakCooldown > 0.0f)
        m_breakCooldown -= elapsed;

    if (!m_leftDown || !m_hasTarget)
    {
        m_breakProgress = 0.0f;
        return;
    }

    // Un bloc vient de tomber: on marque une pause avant d'entamer le
    // suivant, meme si le rayon vise deja le bloc situe derriere.
    if (m_breakCooldown > 0.0f)
    {
        m_breakingBlock = m_targetBlock;
        m_breakProgress = 0.0f;
        return;
    }

    if (m_targetBlock != m_breakingBlock)
    {
        m_breakingBlock = m_targetBlock;
        m_breakProgress = 0.0f;
    }

    const BlockType bt = m_world->GetBlock(m_targetBlock.x, m_targetBlock.y, m_targetBlock.z);
    const BlockInfo& info = Blocks::Get(bt);

    if (!info.breakable)
    {
        m_breakProgress = 0.0f;
        return;
    }

    const BlockType held = m_inventory.SelectedType();

    if (m_player.IsCreative())
    {
        m_breakProgress = 1.0f;
    }
    else
    {
        const float hardness = (info.hardness > 0.05f) ? info.hardness : 0.05f;
        m_breakProgress += elapsed * Blocks::MiningSpeed(bt, held) / hardness;
    }

    if (m_breakProgress >= 1.0f)
    {
        m_world->SetBlock(m_targetBlock.x, m_targetBlock.y, m_targetBlock.z, BTYPE_AIR);

        if (!m_player.IsCreative())
        {
            // Sans le bon outil, le bloc part en poussiere
            if (info.drop != BTYPE_AIR && Blocks::CanHarvest(bt, held))
                m_inventory.Add(info.drop, 1);

            m_inventory.DamageSelected();
        }

        // Une plante posee sur le bloc casse tombe avec lui
        const BlockType above = m_world->GetBlock(m_targetBlock.x, m_targetBlock.y + 1, m_targetBlock.z);
        if (above != BTYPE_AIR && Blocks::Get(above).render == RENDER_CROSS)
            m_world->SetBlock(m_targetBlock.x, m_targetBlock.y + 1, m_targetBlock.z, BTYPE_AIR);

        m_breakProgress = 0.0f;
        m_breakCooldown = BREAK_COOLDOWN;
    }
}

void Engine::PlaceBlock()
{
    if (!m_hasTarget)
        return;

    // Clic droit sur un etabli: on ouvre la grille 3x3 au lieu de poser
    if (m_world->GetBlock(m_targetBlock.x, m_targetBlock.y, m_targetBlock.z) == BTYPE_CRAFTING_TABLE)
    {
        OpenInventory(3);
        return;
    }

    ItemStack& stack = m_inventory.SelectedStack();
    if (stack.Empty())
        return;

    // Un baton ou une pioche ne se pose pas dans le monde
    if (Blocks::IsItem(stack.type))
        return;

    const Vector3i p(m_targetBlock.x + m_targetNormal.x,
                     m_targetBlock.y + m_targetNormal.y,
                     m_targetBlock.z + m_targetNormal.z);

    const BlockType existing = m_world->GetBlock(p.x, p.y, p.z);
    if (existing != BTYPE_AIR && !Blocks::IsLiquid(existing))
        return;

    // On ne se laisse pas enfermer dans un bloc
    if (Blocks::IsSolid(stack.type))
    {
        const Vector3f& pos = m_player.GetPosition();
        const float hw = Player::WIDTH * 0.5f;

        const bool overlapX = (pos.x + hw > (float)p.x) && (pos.x - hw < (float)p.x + 1.0f);
        const bool overlapZ = (pos.z + hw > (float)p.z) && (pos.z - hw < (float)p.z + 1.0f);
        const bool overlapY = (pos.y + Player::HEIGHT > (float)p.y) && (pos.y < (float)p.y + 1.0f);

        if (overlapX && overlapY && overlapZ)
            return;
    }

    // Les plantes ont besoin d'un support
    if (Blocks::Get(stack.type).render == RENDER_CROSS || stack.type == BTYPE_TORCH)
    {
        if (!Blocks::IsOpaque(m_world->GetBlock(p.x, p.y - 1, p.z)))
            return;
    }

    if (m_world->SetBlock(p.x, p.y, p.z, stack.type))
    {
        if (!m_player.IsCreative())
            m_inventory.Consume(m_inventory.Selected(), 1);
    }
}

void Engine::PickBlock()
{
    if (!m_hasTarget)
        return;

    const BlockType bt = m_world->GetBlock(m_targetBlock.x, m_targetBlock.y, m_targetBlock.z);
    m_inventory.PickBlock(bt, m_player.IsCreative());
}

void Engine::AttackEntity()
{
    if (m_attackCooldown > 0.0f)
        return;

    float dist = 0.0f;
    Entity* target = m_entities.Pick(m_player.EyePosition(), m_player.Direction(), 4.0f, dist);
    if (!target)
        return;

    // Un bloc plus proche que la creature bloque le coup
    if (m_hasTarget)
    {
        const Vector3f eye = m_player.EyePosition();
        const float bx = (float)m_targetBlock.x + 0.5f - eye.x;
        const float by = (float)m_targetBlock.y + 0.5f - eye.y;
        const float bz = (float)m_targetBlock.z + 0.5f - eye.z;
        if (sqrtf(bx * bx + by * by + bz * bz) < dist)
            return;
    }

    // Une epee frappe plus fort
    const BlockType held = m_inventory.SelectedType();
    float damage = 1.0f;
    if (Blocks::Get(held).toolType == TOOL_SWORD)
        damage = 2.0f + 1.5f * (float)Blocks::Get(held).toolTier;

    BlockType drop = BTYPE_AIR;
    int dropCount = 0;
    if (m_entities.Damage(*target, damage, drop, dropCount) && drop != BTYPE_AIR)
        m_inventory.Add(drop, dropCount);

    if (!m_player.IsCreative())
        m_inventory.DamageSelected();

    m_attackCooldown = 0.35f;
    m_breakProgress = 0.0f;
}

bool Engine::EatHeldFood()
{
    ItemStack& stack = m_inventory.SelectedStack();
    if (stack.Empty() || !Blocks::IsFood(stack.type))
        return false;

    if (m_player.Hunger() >= 20.0f)
        return true;    // rassasie: on ne pose rien non plus

    m_player.Feed(Blocks::Get(stack.type).foodValue);
    m_inventory.Consume(m_inventory.Selected(), 1);
    return true;
}

void Engine::Respawn()
{
    float sx, sy, sz;
    m_world->Generator().FindSpawn(sx, sy, sz);
    m_player.Reset(Vector3f(sx, sy, sz));
    m_breakProgress = 0.0f;
}

// ---------------------------------------------------------------------------
//  Rendu du ciel
// ---------------------------------------------------------------------------
void Engine::RenderSky()
{
    if (!m_skyShader.IsValid())
        return;

    const float aspect = (float)Width() / (float)(Height() > 0 ? Height() : 1);
    const float halfH = tanf(FIELD_OF_VIEW * 0.5f * 0.017453293f);
    const float halfW = halfH * aspect;

    Vector3f forward = m_player.Direction();
    forward.Normalize();

    Vector3f right = forward.Cross(Vector3f(0.0f, 1.0f, 0.0f));
    if (right.Length() < 0.0001f)
        right = Vector3f(1.0f, 0.0f, 0.0f);
    right.Normalize();

    Vector3f up = right.Cross(forward);
    up.Normalize();

    const Vector3f bl = forward - right * halfW - up * halfH;
    const Vector3f br = forward + right * halfW - up * halfH;
    const Vector3f tl = forward - right * halfW + up * halfH;
    const Vector3f tr = forward + right * halfW + up * halfH;

    const Vector3f eye = m_player.EyePosition();

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    m_skyShader.Use();
    m_skyShader.SetVec3("uRayBL", bl.x, bl.y, bl.z);
    m_skyShader.SetVec3("uRayBR", br.x, br.y, br.z);
    m_skyShader.SetVec3("uRayTL", tl.x, tl.y, tl.z);
    m_skyShader.SetVec3("uRayTR", tr.x, tr.y, tr.z);
    m_skyShader.SetVec3("uSunDir", m_sunDir.x, m_sunDir.y, m_sunDir.z);
    m_skyShader.SetVec3("uSunColor", m_sunColor.x, m_sunColor.y, m_sunColor.z);
    m_skyShader.SetVec3("uZenith", m_zenithColor.x, m_zenithColor.y, m_zenithColor.z);
    m_skyShader.SetVec3("uHorizon", m_horizonColor.x, m_horizonColor.y, m_horizonColor.z);
    m_skyShader.SetVec3("uGround", m_groundColor.x, m_groundColor.y, m_groundColor.z);
    m_skyShader.SetVec3("uCamPos", eye.x, eye.y, eye.z);
    m_skyShader.SetFloat("uTime", m_gameTime);
    m_skyShader.SetFloat("uDayFactor", m_dayFactor);
    m_skyShader.SetFloat("uUnderwater", m_underwater ? 1.0f : 0.0f);
    m_skyShader.SetFloat("uDirectOutput", m_postfx.IsAvailable() ? 0.0f : 1.0f);

    PostFX::DrawFullscreenQuad();

    Shader::Disable();

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

// ---------------------------------------------------------------------------
//  Rendu du monde
// ---------------------------------------------------------------------------
void Engine::RenderWorld()
{
    const int w = Width(), h = (Height() > 0 ? Height() : 1);
    const float aspect = (float)w / (float)h;
    const float farPlane = (float)(m_world->RenderDistance() * CHUNK_SIZE_X) * 1.6f + 96.0f;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(FIELD_OF_VIEW, aspect, 0.08f, farPlane);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    Transformation cam;
    m_player.ApplyTransformation(cam);
    cam.Use();

    float planes[6][4];
    Frustum::Extract(planes);
    m_world->BuildVisibleList(m_player.GetPosition(), planes);

    if (m_wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    const Vector3f eye = m_player.EyePosition();
    const float fogEnd = (float)(m_world->RenderDistance() * CHUNK_SIZE_X) - 12.0f;
    const float fogStart = fogEnd * 0.55f;

    m_terrainShader.Use();
    m_terrainShader.SetInt("uAtlas", 0);
    m_terrainShader.SetVec3("uCamPos", eye.x, eye.y, eye.z);
    m_terrainShader.SetVec3("uSunDir", m_sunDir.x, m_sunDir.y, m_sunDir.z);
    m_terrainShader.SetVec3("uSunColor", m_sunColor.x, m_sunColor.y, m_sunColor.z);
    m_terrainShader.SetVec3("uSkyColor", m_horizonColor.x, m_horizonColor.y, m_horizonColor.z);
    m_terrainShader.SetVec3("uAmbient", m_ambient.x, m_ambient.y, m_ambient.z);
    m_terrainShader.SetFloat("uDayFactor", m_dayFactor);
    m_terrainShader.SetFloat("uFogStart", fogStart);
    m_terrainShader.SetFloat("uFogEnd", fogEnd);
    m_terrainShader.SetFloat("uTime", m_gameTime);
    m_terrainShader.SetFloat("uUnderwater", m_underwater ? 1.0f : 0.0f);
    m_terrainShader.SetFloat("uDirectOutput", m_postfx.IsAvailable() ? 0.0f : 1.0f);

    glActiveTexture(GL_TEXTURE0);
    if (m_atlas && m_atlas->IsValid())
        m_atlas->Bind();

    // --- passe opaque (avec test alpha pour les feuillages) ---------------
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    // L'emplacement de uChunkOrigin est resolu une fois pour toute la passe:
    // le chercher par son nom pour chacune des centaines de chunks affichees
    // revenait a une recherche dans une table de chaines par chunk.
    const GLint originLoc = m_terrainShader.Uniform("uChunkOrigin");

    m_terrainShader.SetFloat("uAlphaTest", 0.5f);
    RenderChunkPass(false, originLoc);

    // --- passe transparente (eau, verre, glace) ---------------------------
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);          // l'eau se voit des deux cotes
    glDepthMask(GL_FALSE);
    m_terrainShader.SetFloat("uAlphaTest", 0.02f);
    RenderChunkPass(true, originLoc);

    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    Shader::Disable();

    if (m_wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    m_entities.Render(*m_world, m_dayFactor);

    RenderSelection();
}

void Engine::RenderChunkPass(bool blendPass, GLint chunkOriginLoc)
{
    const std::vector<Chunk*>& chunks = m_world->VisibleChunks();

    // Les chunks sont triees du plus proche au plus loin: la passe opaque en
    // profite (rejet precoce par le test de profondeur), la passe transparente
    // les parcourt a l'envers pour melanger de l'arriere vers l'avant.
    const int n = (int)chunks.size();

    for (int i = 0; i < n; ++i)
    {
        Chunk* c = blendPass ? chunks[n - 1 - i] : chunks[i];

        if (blendPass ? !c->HasBlend() : !c->HasSolid())
            continue;

        const float ox = (float)(c->Cx() * CHUNK_SIZE_X);
        const float oz = (float)(c->Cz() * CHUNK_SIZE_Z);

        if (chunkOriginLoc >= 0)
            glUniform3f(chunkOriginLoc, ox, 0.0f, oz);

        glPushMatrix();
        glTranslatef(ox, 0.0f, oz);

        if (blendPass) c->RenderBlend();
        else           c->RenderSolid();

        glPopMatrix();
    }
}

void Engine::RenderSelection()
{
    if (!m_hasTarget || m_inventoryOpen || m_paused)
        return;

    const float x = (float)m_targetBlock.x;
    const float y = (float)m_targetBlock.y;
    const float z = (float)m_targetBlock.z;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glColor4f(0.0f, 0.0f, 0.0f, 0.45f);
    glLineWidth(2.0f);

    const float e = 0.003f;
    const float x0 = x - e, y0 = y - e, z0 = z - e;
    const float x1 = x + 1.0f + e, y1 = y + 1.0f + e, z1 = z + 1.0f + e;

    glBegin(GL_LINES);
    // bas
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1);
    glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);
    glVertex3f(x0, y0, z1); glVertex3f(x0, y0, z0);
    // haut
    glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);
    glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
    // montants
    glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0);
    glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1);
    glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1);
    glEnd();

    // --- fissures de minage ----------------------------------------------
    if (m_breakProgress > 0.0f && m_textureBreak.IsValid())
    {
        int frame = (int)(m_breakProgress * 8.0f);
        if (frame > 7) frame = 7;
        if (frame < 0) frame = 0;

        const float u0 = (float)frame / 8.0f;
        const float u1 = u0 + 1.0f / 8.0f;

        glEnable(GL_TEXTURE_2D);
        m_textureBreak.Bind();
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        const float o = 0.004f;
        const float a0 = x - o, b0 = y - o, c0 = z - o;
        const float a1 = x + 1.0f + o, b1 = y + 1.0f + o, c1 = z + 1.0f + o;

        glBegin(GL_QUADS);
        // +Z
        glTexCoord2f(u0, 0); glVertex3f(a0, b0, c1);
        glTexCoord2f(u1, 0); glVertex3f(a1, b0, c1);
        glTexCoord2f(u1, 1); glVertex3f(a1, b1, c1);
        glTexCoord2f(u0, 1); glVertex3f(a0, b1, c1);
        // -Z
        glTexCoord2f(u0, 0); glVertex3f(a1, b0, c0);
        glTexCoord2f(u1, 0); glVertex3f(a0, b0, c0);
        glTexCoord2f(u1, 1); glVertex3f(a0, b1, c0);
        glTexCoord2f(u0, 1); glVertex3f(a1, b1, c0);
        // +X
        glTexCoord2f(u0, 0); glVertex3f(a1, b0, c1);
        glTexCoord2f(u1, 0); glVertex3f(a1, b0, c0);
        glTexCoord2f(u1, 1); glVertex3f(a1, b1, c0);
        glTexCoord2f(u0, 1); glVertex3f(a1, b1, c1);
        // -X
        glTexCoord2f(u0, 0); glVertex3f(a0, b0, c0);
        glTexCoord2f(u1, 0); glVertex3f(a0, b0, c1);
        glTexCoord2f(u1, 1); glVertex3f(a0, b1, c1);
        glTexCoord2f(u0, 1); glVertex3f(a0, b1, c0);
        // +Y
        glTexCoord2f(u0, 0); glVertex3f(a0, b1, c1);
        glTexCoord2f(u1, 0); glVertex3f(a1, b1, c1);
        glTexCoord2f(u1, 1); glVertex3f(a1, b1, c0);
        glTexCoord2f(u0, 1); glVertex3f(a0, b1, c0);
        // -Y
        glTexCoord2f(u0, 0); glVertex3f(a0, b0, c0);
        glTexCoord2f(u1, 0); glVertex3f(a1, b0, c0);
        glTexCoord2f(u1, 1); glVertex3f(a1, b0, c1);
        glTexCoord2f(u0, 1); glVertex3f(a0, b0, c1);
        glEnd();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

// ---------------------------------------------------------------------------
//  Interface 2D
// ---------------------------------------------------------------------------
void Engine::Begin2D()
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Width(), 0, Height(), -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void Engine::End2D()
{
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void Engine::DrawQuad(float x, float y, float w, float h) const
{
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void Engine::DrawTexturedQuad(float x, float y, float w, float h) const
{
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(x + w, y);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(x + w, y + h);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(x, y + h);
    glEnd();
}

void Engine::DrawColorQuad(float x, float y, float w, float h, float r, float g, float b, float a) const
{
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    DrawQuad(x, y, w, h);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void Engine::DrawBlockIcon(BlockType type, float x, float y, float size) const
{
    if (type == BTYPE_AIR || !m_atlas || !m_atlas->IsValid())
        return;

    const BlockInfo& info = Blocks::Get(type);
    const int face = (info.render == RENDER_CUBE) ? FACE_FRONT : FACE_FRONT;

    const float u = info.uv[face][0], v = info.uv[face][1];
    const float w = info.uv[face][2], h = info.uv[face][3];

    // Petit retrait d'un demi-texel pour eviter de mordre sur la tuile voisine
    const float inset = w * 0.004f;

    m_atlas->Bind();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    glTexCoord2f(u + inset,     v + inset);     glVertex2f(x, y);
    glTexCoord2f(u + w - inset, v + inset);     glVertex2f(x + size, y);
    glTexCoord2f(u + w - inset, v + h - inset); glVertex2f(x + size, y + size);
    glTexCoord2f(u + inset,     v + h - inset); glVertex2f(x, y + size);
    glEnd();
}

void Engine::PrintText(float x, float y, const std::string& t, float scale)
{
    if (!m_textureFont.IsValid())
        return;

    m_textureFont.Bind();

    const float size = 12.0f * scale;
    const float advance = 8.0f * scale;

    glPushMatrix();
    glTranslatef(x, y, 0.0f);

    for (size_t i = 0; i < t.length(); ++i)
    {
        const unsigned char c = (unsigned char)t[i];
        if (c < 32) continue;

        const float left = (float)((c - 32) % 16) / 16.0f;
        float top = (float)((c - 32) / 16) / 16.0f;
        top += 0.5f;

        glBegin(GL_QUADS);
        glTexCoord2f(left, 1.0f - top - 0.0625f);           glVertex2f(0.0f, 0.0f);
        glTexCoord2f(left + 0.0625f, 1.0f - top - 0.0625f); glVertex2f(size, 0.0f);
        glTexCoord2f(left + 0.0625f, 1.0f - top);           glVertex2f(size, size);
        glTexCoord2f(left, 1.0f - top);                     glVertex2f(0.0f, size);
        glEnd();

        glTranslatef(advance, 0.0f, 0.0f);
    }

    glPopMatrix();
}

bool Engine::MouseInRect(int x, int y, float rx, float ry, float rw, float rh) const
{
    return (float)x >= rx && (float)x <= rx + rw && (float)y >= ry && (float)y <= ry + rh;
}

// ---------------------------------------------------------------------------
//  HUD
// ---------------------------------------------------------------------------
void Engine::DrawHud(float elapsed)
{
    Begin2D();

    if (m_showHud && !m_inventoryOpen && !m_paused)
    {
        DrawCrosshair();
        DrawHotbar();
        DrawStatusBars();
    }

    // Les images par seconde et la position restent affichees en permanence;
    // l'overlay de debogage (F3) les reprend avec bien plus de details.
    if (m_showHud && !m_showDebug && !m_paused)
        DrawInfoOverlay();

    if (m_showDebug)
        DrawDebugOverlay(elapsed);

    if (m_inventoryOpen)
        DrawInventoryScreen();

    if (m_paused)
        DrawPauseMenu();

    if (m_player.IsDead())
        DrawDeathScreen();

    End2D();
}

void Engine::DrawCrosshair()
{
    if (!m_textureCrosshair.IsValid())
        return;

    const float size = 22.0f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    m_textureCrosshair.Bind();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    DrawTexturedQuad((float)Width() * 0.5f - size * 0.5f,
                     (float)Height() * 0.5f - size * 0.5f, size, size);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Engine::DrawHotbar()
{
    const float slot = (float)HOTBAR_SLOT;
    const float pad = 3.0f;
    const float total = slot * Inventory::HOTBAR_SIZE + pad * (Inventory::HOTBAR_SIZE + 1);
    const float x0 = (float)Width() * 0.5f - total * 0.5f;
    const float y0 = 14.0f;

    DrawColorQuad(x0, y0, total, slot + pad * 2.0f, 0.05f, 0.05f, 0.07f, 0.62f);

    for (int i = 0; i < Inventory::HOTBAR_SIZE; ++i)
    {
        const float x = x0 + pad + (slot + pad) * i;
        const float y = y0 + pad;

        DrawColorQuad(x, y, slot, slot, 0.16f, 0.16f, 0.19f, 0.75f);

        const ItemStack& st = m_inventory.At(i);
        if (!st.Empty())
        {
            DrawBlockIcon(st.type, x + 6.0f, y + 6.0f, slot - 12.0f);

            if (st.count > 1)
            {
                std::ostringstream ss;
                ss << st.count;
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                PrintText(x + slot - 20.0f, y + 2.0f, ss.str(), 0.8f);
            }
        }

        if (i == m_inventory.Selected())
        {
            // Cadre de selection
            DrawColorQuad(x - 2.0f, y - 2.0f, slot + 4.0f, 2.0f, 1.0f, 1.0f, 1.0f, 0.95f);
            DrawColorQuad(x - 2.0f, y + slot, slot + 4.0f, 2.0f, 1.0f, 1.0f, 1.0f, 0.95f);
            DrawColorQuad(x - 2.0f, y, 2.0f, slot, 1.0f, 1.0f, 1.0f, 0.95f);
            DrawColorQuad(x + slot, y, 2.0f, slot, 1.0f, 1.0f, 1.0f, 0.95f);
        }
    }

    // Nom du bloc selectionne
    const BlockType sel = m_inventory.SelectedType();
    if (sel != BTYPE_AIR)
    {
        const std::string& name = Blocks::Get(sel).name;
        glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
        PrintText((float)Width() * 0.5f - (float)name.size() * 4.0f,
                  y0 + slot + 14.0f, name, 1.0f);
    }
}

void Engine::DrawStatusBars()
{
    if (m_player.IsCreative())
        return;

    const float slot = (float)HOTBAR_SLOT;
    const float pad = 3.0f;
    const float total = slot * Inventory::HOTBAR_SIZE + pad * (Inventory::HOTBAR_SIZE + 1);
    const float x0 = (float)Width() * 0.5f - total * 0.5f;
    const float y = 14.0f + slot + pad * 2.0f + 30.0f;

    const float icon = 14.0f;
    const float gap = 2.0f;

    // Vie
    const int hearts = 10;
    const float hp = m_player.Health() / m_player.MaxHealth() * hearts;
    for (int i = 0; i < hearts; ++i)
    {
        const float x = x0 + (icon + gap) * i;
        const float fill = Clamp(hp - (float)i, 0.0f, 1.0f);
        DrawColorQuad(x, y, icon, icon, 0.12f, 0.05f, 0.05f, 0.8f);
        if (fill > 0.0f)
            DrawColorQuad(x + 1.0f, y + 1.0f, (icon - 2.0f) * fill, icon - 2.0f, 0.88f, 0.14f, 0.14f, 1.0f);
    }

    // Faim
    const float food = m_player.Hunger() / 20.0f * hearts;
    for (int i = 0; i < hearts; ++i)
    {
        const float x = x0 + total - icon - (icon + gap) * i;
        const float fill = Clamp(food - (float)i, 0.0f, 1.0f);
        DrawColorQuad(x, y, icon, icon, 0.10f, 0.08f, 0.04f, 0.8f);
        if (fill > 0.0f)
            DrawColorQuad(x + 1.0f, y + 1.0f, (icon - 2.0f) * fill, icon - 2.0f, 0.85f, 0.55f, 0.15f, 1.0f);
    }

    // Oxygene, seulement sous l'eau
    if (m_player.Oxygen() < m_player.MaxOxygen())
    {
        const float ox = m_player.Oxygen() / m_player.MaxOxygen() * hearts;
        for (int i = 0; i < hearts; ++i)
        {
            const float x = x0 + total - icon - (icon + gap) * i;
            if ((float)i < ox)
                DrawColorQuad(x, y + icon + 4.0f, icon, icon, 0.35f, 0.72f, 1.0f, 0.95f);
        }
    }
}

void Engine::DrawInfoOverlay()
{
    const Vector3f& p = m_player.GetPosition();

    std::ostringstream fps;
    fps << m_fps << " fps";

    std::ostringstream pos;
    pos.setf(std::ios::fixed);
    pos.precision(1);
    pos << "X " << p.x << "   Y " << p.y << "   Z " << p.z;

    std::string state;
    if (m_player.IsFlying())         state = "Vol";
    else if (m_player.IsSprinting()) state = "Course";

    const float step = 15.0f;
    const float x = 6.0f;
    const float y = (float)Height() - 20.0f;

    const size_t widest = (fps.str().size() > pos.str().size()) ? fps.str().size() : pos.str().size();
    const int    lines  = state.empty() ? 2 : 3;
    const float  top    = y + 14.0f;
    const float  bottom = y - step * (float)(lines - 1) - 4.0f;

    DrawColorQuad(x - 4.0f, bottom, (float)widest * 8.0f + 10.0f, top - bottom,
                  0.0f, 0.0f, 0.0f, 0.38f);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    PrintText(x, y, fps.str());
    PrintText(x, y - step, pos.str());
    if (!state.empty())
        PrintText(x, y - step * 2.0f, state);
}

void Engine::DrawDebugOverlay(float elapsed)
{
    const Vector3f& p = m_player.GetPosition();
    const int wx = (int)floorf(p.x), wy = (int)floorf(p.y), wz = (int)floorf(p.z);

    std::ostringstream ss;
    float y = (float)Height() - 20.0f;
    const float step = 15.0f;

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    ss << "BlockAdventure  " << m_fps << " fps  (" << (int)(elapsed * 1000.0f) << " ms)";
    PrintText(6.0f, y, ss.str()); y -= step; ss.str("");

    ss << "XYZ " << (int)p.x << " / " << (int)p.y << " / " << (int)p.z;
    PrintText(6.0f, y, ss.str()); y -= step; ss.str("");

    ss << "Chunk " << World::ChunkCoord(wx) << " , " << World::ChunkCoord(wz)
       << "   Biome " << WorldGenerator::BiomeName(m_world->Generator().BiomeAt(wx, wz));
    PrintText(6.0f, y, ss.str()); y -= step; ss.str("");

    ss << "Lumiere  ciel " << (int)m_world->GetSkyLight(wx, wy + 1, wz)
       << "  bloc " << (int)m_world->GetBlockLight(wx, wy + 1, wz);
    PrintText(6.0f, y, ss.str()); y -= step; ss.str("");

    ss << "Chunks " << m_world->LoadedChunks() << " charges, "
       << m_world->DrawnChunks() << " dessines, " << m_world->PendingJobs() << " en attente";
    PrintText(6.0f, y, ss.str()); y -= step; ss.str("");

    ss << "Sommets " << m_world->DrawnVertices() << "   Distance " << m_world->RenderDistance()
       << "   Blocs en attente " << m_world->PendingBlockUpdates();
    PrintText(6.0f, y, ss.str()); y -= step; ss.str("");

    const int hours = (int)(m_timeOfDay * 24.0f);
    const int minutes = (int)((m_timeOfDay * 24.0f - (float)hours) * 60.0f);
    ss << "Heure " << (hours < 10 ? "0" : "") << hours << ":"
       << (minutes < 10 ? "0" : "") << minutes
       << (m_timeFrozen ? " (fige)" : "");
    PrintText(6.0f, y, ss.str()); y -= step; ss.str("");

    ss << "Creatures " << m_entities.Count()
       << "  (paisibles " << m_entities.CountOf(false)
       << ", hostiles " << m_entities.CountOf(true) << ")";
    PrintText(6.0f, y, ss.str()); y -= step; ss.str("");

    ss << "Mode " << (m_player.IsCreative() ? "creatif" : "survie")
       << (m_player.IsFlying() ? " (vol)" : "")
       << (m_player.InWater() ? " (dans l'eau)" : "");
    PrintText(6.0f, y, ss.str()); y -= step; ss.str("");

    if (m_hasTarget)
    {
        const BlockType bt = m_world->GetBlock(m_targetBlock.x, m_targetBlock.y, m_targetBlock.z);
        ss << "Vise " << Blocks::Get(bt).name << " en "
           << m_targetBlock.x << " / " << m_targetBlock.y << " / " << m_targetBlock.z;
        PrintText(6.0f, y, ss.str()); y -= step; ss.str("");
    }

    y -= step;
    PrintText(6.0f, y, "F1 hud   F3 debug   F5 sauver   Echap > Options: distance"); y -= step;
    PrintText(6.0f, y, "F9 fige le temps   F10 plein ecran   Y fil de fer"); y -= step;
    PrintText(6.0f, y, "F ou Espace x2 vol   Ctrl ou W x2 course"); y -= step;
    PrintText(6.0f, y, "G creatif/survie   E inventaire   R reapparaitre");
}

// Dispose les cases a l'ecran. Identifiants:
//   0..35    inventaire (0..8 = barre rapide)
//   100..108 grille de fabrication
//   200      case de resultat
//   300+     palette creative
void Engine::BuildInventoryLayout(std::vector<SlotRect>& out) const
{
    out.clear();

    const float slot = 46.0f;
    const float gap = 4.0f;
    const int   cols = 9;

    const float panelW = cols * (slot + gap) + gap;
    const float rowsH = 4 * (slot + gap) + gap;
    const float craftH = (float)m_craftSize * (slot + gap) + gap + 24.0f;
    const float panelH = rowsH + craftH + 24.0f;

    const float px = (float)Width() * 0.5f - panelW * 0.5f;
    const float py = (float)Height() * 0.5f - panelH * 0.5f;

    // --- barre rapide et rangement (ou palette en creatif) ---
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            SlotRect r;
            r.x = px + gap + (slot + gap) * col;
            r.y = py + gap + (slot + gap) * row;
            r.size = slot;

            if (row == 0)
            {
                r.id = col;                      // barre rapide, en bas
            }
            else if (m_player.IsCreative())
            {
                r.id = 300 + m_palettePage * 27 + (3 - row) * cols + col;
            }
            else
            {
                r.id = Inventory::HOTBAR_SIZE + (3 - row) * cols + col;
            }

            out.push_back(r);
        }
    }

    if (m_player.IsCreative())
        return;                                  // pas de fabrication en creatif

    // --- grille de fabrication ---
    const float craftBase = py + rowsH + 20.0f;

    for (int row = 0; row < m_craftSize; ++row)
    {
        for (int col = 0; col < m_craftSize; ++col)
        {
            SlotRect r;
            r.id = 100 + row * 3 + col;
            r.x = px + gap + (slot + gap) * col;
            r.y = craftBase + (slot + gap) * (m_craftSize - 1 - row);
            r.size = slot;
            out.push_back(r);
        }
    }

    SlotRect res;
    res.id = 200;
    res.x = px + gap + (slot + gap) * (m_craftSize + 1);
    res.y = craftBase + (slot + gap) * (float)(m_craftSize - 1) * 0.5f;
    res.size = slot;
    out.push_back(res);
}

void Engine::DrawItemSlot(float x, float y, float size, const ItemStack& st)
{
    DrawColorQuad(x, y, size, size, 0.20f, 0.20f, 0.24f, 0.92f);

    if (st.Empty())
        return;

    DrawBlockIcon(st.type, x + 6.0f, y + 6.0f, size - 12.0f);

    if (st.count > 1)
    {
        std::ostringstream ss;
        ss << st.count;
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        PrintText(x + size - 20.0f, y + 2.0f, ss.str(), 0.8f);
    }

    // Barre d'usure des outils
    const int maxDur = Blocks::Get(st.type).maxDurability;
    if (maxDur > 0 && st.durability < maxDur)
    {
        const float ratio = (float)st.durability / (float)maxDur;
        DrawColorQuad(x + 5.0f, y + 4.0f, size - 10.0f, 3.0f, 0.1f, 0.1f, 0.1f, 0.9f);
        DrawColorQuad(x + 5.0f, y + 4.0f, (size - 10.0f) * ratio, 3.0f,
                      1.0f - ratio, ratio, 0.1f, 1.0f);
    }
}

void Engine::DrawInventoryScreen()
{
    DrawColorQuad(0.0f, 0.0f, (float)Width(), (float)Height(), 0.0f, 0.0f, 0.0f, 0.55f);

    std::vector<SlotRect> layout;
    BuildInventoryLayout(layout);

    if (layout.empty())
        return;

    // Fond du panneau
    float x0 = layout[0].x, y0 = layout[0].y, x1 = x0, y1 = y0;
    for (size_t i = 0; i < layout.size(); ++i)
    {
        if (layout[i].x < x0) x0 = layout[i].x;
        if (layout[i].y < y0) y0 = layout[i].y;
        if (layout[i].x + layout[i].size > x1) x1 = layout[i].x + layout[i].size;
        if (layout[i].y + layout[i].size > y1) y1 = layout[i].y + layout[i].size;
    }
    DrawColorQuad(x0 - 8.0f, y0 - 8.0f, x1 - x0 + 16.0f, y1 - y0 + 44.0f,
                  0.10f, 0.10f, 0.13f, 0.94f);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    if (m_player.IsCreative())
    {
        std::ostringstream ss;
        ss << "Palette creative - page " << (m_palettePage + 1) << " (molette)";
        PrintText(x0, y1 + 14.0f, ss.str());
    }
    else
    {
        PrintText(x0, y1 + 14.0f,
                  m_craftSize == 3 ? "Etabli - grille 3x3" : "Inventaire - grille 2x2");
    }

    const std::vector<BlockType>& palette = Blocks::Palette();

    BlockType craftType = BTYPE_AIR;
    int craftCount = 0;
    const bool canCraft = CraftResult(craftType, craftCount);

    for (size_t i = 0; i < layout.size(); ++i)
    {
        const SlotRect& r = layout[i];

        if (r.id >= 300)
        {
            const int index = r.id - 300;
            ItemStack st;
            if (index < (int)palette.size())
            {
                st.type = palette[index];
                st.count = 1;
            }
            DrawColorQuad(r.x, r.y, r.size, r.size, 0.20f, 0.20f, 0.24f, 0.92f);
            if (!st.Empty())
                DrawBlockIcon(st.type, r.x + 6.0f, r.y + 6.0f, r.size - 12.0f);
        }
        else if (r.id == 200)
        {
            ItemStack st;
            if (canCraft) { st.type = craftType; st.count = craftCount; }
            DrawColorQuad(r.x - 3.0f, r.y - 3.0f, r.size + 6.0f, r.size + 6.0f,
                          0.35f, 0.30f, 0.15f, 0.95f);
            DrawItemSlot(r.x, r.y, r.size, st);
        }
        else if (r.id >= 100)
        {
            DrawItemSlot(r.x, r.y, r.size, m_craft[r.id - 100]);
        }
        else
        {
            DrawItemSlot(r.x, r.y, r.size, m_inventory.At(r.id));
        }
    }

    // Nom de l'objet survole
    for (size_t i = 0; i < layout.size(); ++i)
    {
        const SlotRect& r = layout[i];
        if (!MouseInRect(m_mouseX, m_mouseY, r.x, r.y, r.size, r.size))
            continue;

        BlockType type = BTYPE_AIR;
        if (r.id >= 300)      { const int k = r.id - 300; if (k < (int)palette.size()) type = palette[k]; }
        else if (r.id == 200) { if (canCraft) type = craftType; }
        else if (r.id >= 100) type = m_craft[r.id - 100].type;
        else                  type = m_inventory.At(r.id).type;

        if (type != BTYPE_AIR)
        {
            const std::string& name = Blocks::Get(type).name;
            DrawColorQuad((float)m_mouseX + 12.0f, (float)m_mouseY + 12.0f,
                          (float)name.size() * 8.0f + 8.0f, 18.0f, 0.0f, 0.0f, 0.0f, 0.8f);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            PrintText((float)m_mouseX + 16.0f, (float)m_mouseY + 15.0f, name);
        }
        break;
    }

    // Pile accrochee au curseur
    if (!m_cursor.Empty())
    {
        DrawBlockIcon(m_cursor.type, (float)m_mouseX - 16.0f, (float)m_mouseY - 16.0f, 32.0f);
        if (m_cursor.count > 1)
        {
            std::ostringstream ss;
            ss << m_cursor.count;
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            PrintText((float)m_mouseX + 2.0f, (float)m_mouseY - 16.0f, ss.str(), 0.8f);
        }
    }
}

// ---------------------------------------------------------------------------
//  Fabrication
// ---------------------------------------------------------------------------
void Engine::OpenInventory(int gridSize)
{
    m_craftSize = gridSize;
    m_inventoryOpen = true;
    m_palettePage = 0;
    ShowCursor();
}

void Engine::CloseInventory()
{
    // Ce qui restait dans la grille et sous le curseur retourne au sac
    for (int i = 0; i < 9; ++i)
    {
        if (!m_craft[i].Empty())
            m_inventory.Add(m_craft[i].type, m_craft[i].count);
        m_craft[i].Clear();
    }
    if (!m_cursor.Empty())
    {
        m_inventory.Add(m_cursor.type, m_cursor.count);
        m_cursor.Clear();
    }

    m_inventoryOpen = false;
    HideCursor();
    CenterMouse();
}

bool Engine::CraftResult(BlockType& type, int& count) const
{
    BlockType grid[9];
    for (int i = 0; i < 9; ++i)
        grid[i] = m_craft[i].Empty() ? BTYPE_AIR : m_craft[i].type;

    return Crafting::Match(grid, m_craftSize, type, count);
}

void Engine::TakeCraftResult(bool wholeStack)
{
    BlockType type;
    int count;
    if (!CraftResult(type, count))
        return;

    // On ne peut prendre le resultat que si le curseur est libre ou compatible
    if (!m_cursor.Empty() && (m_cursor.type != type ||
                              m_cursor.count + count > Inventory::MaxStack(type)))
        return;

    int repeats = 1;
    if (wholeStack)
    {
        // Combien de fois la recette peut-elle etre repetee ?
        int minStack = 64;
        for (int i = 0; i < 9; ++i)
            if (!m_craft[i].Empty() && m_craft[i].count < minStack)
                minStack = m_craft[i].count;
        repeats = minStack;

        const int room = Inventory::MaxStack(type) - (m_cursor.Empty() ? 0 : m_cursor.count);
        while (repeats > 1 && repeats * count > room)
            --repeats;
    }

    for (int r = 0; r < repeats; ++r)
    {
        if (m_cursor.Empty())
        {
            m_cursor.type = type;
            m_cursor.count = count;
            m_cursor.durability = Blocks::Get(type).maxDurability;
        }
        else
        {
            if (m_cursor.count + count > Inventory::MaxStack(type))
                break;
            m_cursor.count += count;
        }

        for (int i = 0; i < 9; ++i)
            if (!m_craft[i].Empty() && --m_craft[i].count <= 0)
                m_craft[i].Clear();

        BlockType t2; int c2;
        if (!CraftResult(t2, c2) || t2 != type)
            break;
    }
}

ItemStack* Engine::SlotFromId(int id)
{
    if (id >= 100 && id < 109) return &m_craft[id - 100];
    if (id >= 0 && id < Inventory::TOTAL_SIZE) return &m_inventory.At(id);
    return 0;
}

void Engine::ClickSlot(int id, bool rightClick)
{
    // --- palette creative ---
    if (id >= 300)
    {
        const std::vector<BlockType>& palette = Blocks::Palette();
        const int index = id - 300;
        if (index >= (int)palette.size())
            return;

        const BlockType t = palette[index];
        m_cursor.type = t;
        m_cursor.count = Inventory::MaxStack(t);
        m_cursor.durability = Blocks::Get(t).maxDurability;
        return;
    }

    // --- case de resultat ---
    if (id == 200)
    {
        TakeCraftResult(rightClick);
        return;
    }

    ItemStack* slot = SlotFromId(id);
    if (!slot)
        return;

    if (m_cursor.Empty())
    {
        if (slot->Empty())
            return;

        if (rightClick && slot->count > 1)
        {
            // Clic droit: on prend la moitie
            const int half = (slot->count + 1) / 2;
            m_cursor = *slot;
            m_cursor.count = half;
            slot->count -= half;
        }
        else
        {
            m_cursor = *slot;
            slot->Clear();
        }
        return;
    }

    if (slot->Empty())
    {
        *slot = m_cursor;
        if (rightClick)
        {
            slot->count = 1;
            if (--m_cursor.count <= 0) m_cursor.Clear();
        }
        else
        {
            m_cursor.Clear();
        }
        return;
    }

    if (slot->type == m_cursor.type)
    {
        const int maxStack = Inventory::MaxStack(slot->type);
        const int room = maxStack - slot->count;
        if (room > 0)
        {
            const int move = rightClick ? 1 : (m_cursor.count < room ? m_cursor.count : room);
            slot->count += move;
            m_cursor.count -= move;
            if (m_cursor.count <= 0) m_cursor.Clear();
        }
        return;
    }

    // Types differents: on echange
    const ItemStack tmp = *slot;
    *slot = m_cursor;
    m_cursor = tmp;
}

// ---------------------------------------------------------------------------
//  Curseur de distance d'affichage (menu Options)
// ---------------------------------------------------------------------------
void Engine::RenderDistanceSliderRect(float& x, float& y, float& w, float& h) const
{
    w = 300.0f;
    h = 14.0f;
    x = (float)Width() * 0.5f - w * 0.5f;
    y = (float)Height() * 0.22f;
}

bool Engine::RenderDistanceSliderHit(int mx, int my) const
{
    float x, y, w, h;
    RenderDistanceSliderRect(x, y, w, h);

    // Zone de saisie un peu plus large que la barre, pour attraper le curseur
    return MouseInRect(mx, my, x - 10.0f, y - 10.0f, w + 20.0f, h + 20.0f);
}

void Engine::DrawRenderDistanceSlider()
{
    float x, y, w, h;
    RenderDistanceSliderRect(x, y, w, h);

    const int   span = MAX_RENDER_DISTANCE - MIN_RENDER_DISTANCE;
    const int   d    = m_world->RenderDistance();
    const float t    = (float)(d - MIN_RENDER_DISTANCE) / (float)span;

    const float knobW = 10.0f;

    // Rail, portion parcourue, puis curseur
    DrawColorQuad(x, y, w, h, 0.07f, 0.07f, 0.09f, 0.85f);
    DrawColorQuad(x + 2.0f, y + 2.0f, (w - 4.0f) * t, h - 4.0f, 0.33f, 0.60f, 0.90f, 0.95f);
    DrawColorQuad(x + (w - knobW) * t, y - 4.0f, knobW, h + 8.0f, 0.92f, 0.92f, 0.95f, 1.0f);

    std::ostringstream ss;
    ss << "Distance d'affichage: " << d << " chunks";
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    PrintText(x, y + h + 12.0f, ss.str());

    std::ostringstream lo, hi;
    lo << MIN_RENDER_DISTANCE;
    hi << MAX_RENDER_DISTANCE;
    glColor4f(1.0f, 1.0f, 1.0f, 0.75f);
    PrintText(x - 26.0f, y, lo.str(), 0.8f);
    PrintText(x + w + 8.0f, y, hi.str(), 0.8f);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void Engine::SetRenderDistanceFromMouse(int mx)
{
    float x, y, w, h;
    RenderDistanceSliderRect(x, y, w, h);

    const float knobW = 10.0f;
    const float t = Clamp(((float)mx - x - knobW * 0.5f) / (w - knobW), 0.0f, 1.0f);

    const int span = MAX_RENDER_DISTANCE - MIN_RENDER_DISTANCE;
    const int d = MIN_RENDER_DISTANCE + (int)(t * (float)span + 0.5f);

    if (d != m_world->RenderDistance())
        m_world->SetRenderDistance(d);
}

void Engine::DrawPauseMenu()
{
    DrawColorQuad(0.0f, 0.0f, (float)Width(), (float)Height(), 0.0f, 0.0f, 0.0f, 0.6f);

    const float bx = (float)Width() * 0.5f - (float)BUTTON_W * 0.5f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    if (!m_settingsMenu)
    {
        if (m_textureLogo.IsValid())
        {
            m_textureLogo.Bind();
            DrawTexturedQuad((float)Width() * 0.5f - 200.0f, (float)Height() * 0.72f, 400.0f, 110.0f);
        }

        if (m_textureBacktoGameButton.IsValid())
        {
            m_textureBacktoGameButton.Bind();
            DrawTexturedQuad(bx, (float)Height() * 0.58f, (float)BUTTON_W, (float)BUTTON_H);
        }
        if (m_textureOptionsButton.IsValid())
        {
            m_textureOptionsButton.Bind();
            DrawTexturedQuad(bx, (float)Height() * 0.46f, (float)BUTTON_W, (float)BUTTON_H);
        }
        if (m_textureQuitButton.IsValid())
        {
            m_textureQuitButton.Bind();
            DrawTexturedQuad(bx, (float)Height() * 0.34f, (float)BUTTON_W, (float)BUTTON_H);
        }
    }
    else if (!m_fpsMenu)
    {
        if (m_textureFPS.IsValid())
        {
            m_textureFPS.Bind();
            DrawTexturedQuad(bx, (float)Height() * 0.58f, (float)BUTTON_W, (float)BUTTON_H);
        }

        const Texture& fs = IsFullscreen() ? m_textureFullScreenON : m_textureFullScreenOFF;
        if (fs.IsValid())
        {
            fs.Bind();
            DrawTexturedQuad(bx, (float)Height() * 0.46f, (float)BUTTON_W, (float)BUTTON_H);
        }
        if (m_textureBackButton.IsValid())
        {
            m_textureBackButton.Bind();
            DrawTexturedQuad(bx, (float)Height() * 0.34f, (float)BUTTON_W, (float)BUTTON_H);
        }

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        DrawRenderDistanceSlider();
    }
    else
    {
        const Texture* fps[4] = { &m_texture30Fps, &m_texture60Fps, &m_texture120Fps, &m_texture240Fps };
        for (int i = 0; i < 4; ++i)
            if (fps[i]->IsValid())
            {
                fps[i]->Bind();
                DrawTexturedQuad(bx, (float)Height() * (0.64f - 0.10f * i), (float)BUTTON_W, (float)BUTTON_H);
            }

        if (m_textureBackButton.IsValid())
        {
            m_textureBackButton.Bind();
            DrawTexturedQuad(bx, (float)Height() * 0.24f, (float)BUTTON_W, (float)BUTTON_H);
        }
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Engine::DrawDeathScreen()
{
    DrawColorQuad(0.0f, 0.0f, (float)Width(), (float)Height(), 0.45f, 0.0f, 0.0f, 0.55f);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    PrintText((float)Width() * 0.5f - 70.0f, (float)Height() * 0.55f, "VOUS ETES MORT", 1.6f);
    PrintText((float)Width() * 0.5f - 110.0f, (float)Height() * 0.45f, "Appuyez sur R pour reapparaitre");
}

// ---------------------------------------------------------------------------
//  Entrees
// ---------------------------------------------------------------------------
void Engine::KeyPressEvent(int key)
{
    switch (key)
    {
    case sf::Keyboard::Escape:
        if (m_inventoryOpen)
        {
            CloseInventory();
        }
        else if (m_paused && (m_settingsMenu || m_fpsMenu))
        {
            m_fpsMenu = false;
            m_settingsMenu = false;
            m_draggingSlider = false;
        }
        else
        {
            m_paused = !m_paused;
            if (m_paused) ShowCursor();
            else { HideCursor(); CenterMouse(); }
        }
        break;

    case sf::Keyboard::E:
        if (m_inventoryOpen) CloseInventory();
        else                 OpenInventory(2);
        break;

    case sf::Keyboard::Space:
        m_player.SetJump(true);

        // Deux appuis rapproches: on entre ou sort du vol
        if (!m_paused && !m_inventoryOpen)
        {
            if (m_gameTime - m_lastJumpTap < DOUBLE_TAP_DELAY)
            {
                m_player.ToggleFly();
                m_lastJumpTap = -10.0f;
            }
            else
            {
                m_lastJumpTap = m_gameTime;
            }
        }
        break;

    case sf::Keyboard::W:
        // Deux appuis rapproches: course (Ctrl gauche fait la meme chose)
        if (!m_paused && !m_inventoryOpen)
        {
            if (m_gameTime - m_lastForwardTap < DOUBLE_TAP_DELAY)
            {
                m_player.SetSprint(true);
                m_lastForwardTap = -10.0f;
            }
            else
            {
                m_lastForwardTap = m_gameTime;
            }
        }
        break;

    case sf::Keyboard::LShift: m_player.SetSneak(true); break;
    case sf::Keyboard::LControl: m_player.SetSprint(true); break;

    case sf::Keyboard::F: m_player.ToggleFly(); break;
    case sf::Keyboard::G: m_player.ToggleCreative(); break;
    case sf::Keyboard::R: if (m_player.IsDead()) Respawn(); break;
    case sf::Keyboard::Y:
        m_wireframe = !m_wireframe;
        break;

    case sf::Keyboard::F1: m_showHud = !m_showHud; break;
    case sf::Keyboard::F2: TakeScreenshot(); break;   // valeur de retour ignoree
    case sf::Keyboard::F3: m_showDebug = !m_showDebug; break;
    case sf::Keyboard::F5: SaveGame(); break;
    case sf::Keyboard::F6: LoadGame(); break;
    case sf::Keyboard::F9: m_timeFrozen = !m_timeFrozen; break;
    case sf::Keyboard::F10: SetFullscreen(!IsFullscreen()); break;
    case sf::Keyboard::F11: SetVerticalSync(!VerticalSync()); break;

    default:
        if (key >= sf::Keyboard::Num1 && key <= sf::Keyboard::Num9)
            m_inventory.Select(key - sf::Keyboard::Num1);
        break;
    }

    // Les touches de deplacement sont relues en continu: c'est plus fiable
    // que de suivre chaque appui/relachement individuellement.
    if (!m_paused && !m_inventoryOpen && HasFocus())
    {
        m_player.SetMoveInput(sf::Keyboard::isKeyPressed(sf::Keyboard::W),
                              sf::Keyboard::isKeyPressed(sf::Keyboard::S),
                              sf::Keyboard::isKeyPressed(sf::Keyboard::A),
                              sf::Keyboard::isKeyPressed(sf::Keyboard::D));
    }
}

void Engine::KeyReleaseEvent(int key)
{
    switch (key)
    {
    case sf::Keyboard::Space:    m_player.SetJump(false); break;
    case sf::Keyboard::LShift:   m_player.SetSneak(false); break;
    case sf::Keyboard::LControl: m_player.SetSprint(false); break;

    case sf::Keyboard::W:
        // Lacher W arrete la course, sauf si Ctrl reste enfonce
        m_player.SetSprint(sf::Keyboard::isKeyPressed(sf::Keyboard::LControl));
        break;

    default: break;
    }

    m_player.SetMoveInput(sf::Keyboard::isKeyPressed(sf::Keyboard::W),
                          sf::Keyboard::isKeyPressed(sf::Keyboard::S),
                          sf::Keyboard::isKeyPressed(sf::Keyboard::A),
                          sf::Keyboard::isKeyPressed(sf::Keyboard::D));
}

void Engine::MouseMoveEvent(int x, int y)
{
    m_mouseX = x;
    m_mouseY = Height() - y;      // repere 2D: origine en bas a gauche

    // Glissement du curseur de distance d'affichage
    if (m_paused && m_settingsMenu && !m_fpsMenu && m_draggingSlider)
    {
        SetRenderDistanceFromMouse(x);
        return;
    }

    if (m_paused || m_inventoryOpen || !HasFocus())
        return;

    if (x == Width() / 2 && y == Height() / 2)
        return;

    int dx = x, dy = y;
    MakeRelativeToCenter(dx, dy);

    m_player.TurnLeftRight((float)dx * MOUSE_SENS);
    m_player.TurnTopBottom((float)dy * MOUSE_SENS);

    CenterMouse();
}

int Engine::InventorySlotAt(int mx, int my) const
{
    std::vector<SlotRect> layout;
    BuildInventoryLayout(layout);

    for (size_t i = 0; i < layout.size(); ++i)
        if (MouseInRect(mx, my, layout[i].x, layout[i].y, layout[i].size, layout[i].size))
            return layout[i].id;

    return -1;
}

void Engine::MousePressEvent(const MOUSE_BUTTON& button, int x, int y)
{
    const int my = Height() - y;

    // ----- ecran d'inventaire -----
    if (m_inventoryOpen)
    {
        if (button != MOUSE_BUTTON_LEFT && button != MOUSE_BUTTON_RIGHT)
            return;

        const int id = InventorySlotAt(x, my);
        if (id >= 0)
            ClickSlot(id, button == MOUSE_BUTTON_RIGHT);
        return;
    }

    // ----- menu pause -----
    if (m_paused)
    {
        if (button != MOUSE_BUTTON_LEFT) return;

        const float bx = (float)Width() * 0.5f - (float)BUTTON_W * 0.5f;
        const float bw = (float)BUTTON_W, bh = (float)BUTTON_H;

        if (!m_settingsMenu)
        {
            if (MouseInRect(x, my, bx, (float)Height() * 0.58f, bw, bh))
            {
                m_paused = false;
                HideCursor();
                CenterMouse();
            }
            else if (MouseInRect(x, my, bx, (float)Height() * 0.46f, bw, bh))
            {
                m_settingsMenu = true;
            }
            else if (MouseInRect(x, my, bx, (float)Height() * 0.34f, bw, bh))
            {
                SaveGame();
                Stop();
            }
        }
        else if (!m_fpsMenu)
        {
            if (RenderDistanceSliderHit(x, my))
            {
                m_draggingSlider = true;
                SetRenderDistanceFromMouse(x);
            }
            else if (MouseInRect(x, my, bx, (float)Height() * 0.58f, bw, bh))
            {
                m_fpsMenu = true;
                m_draggingSlider = false;
            }
            else if (MouseInRect(x, my, bx, (float)Height() * 0.46f, bw, bh))
                SetFullscreen(!IsFullscreen());
            else if (MouseInRect(x, my, bx, (float)Height() * 0.34f, bw, bh))
            {
                m_settingsMenu = false;
                m_draggingSlider = false;
            }
        }
        else
        {
            static const int kFps[4] = { 30, 60, 120, 240 };
            for (int i = 0; i < 4; ++i)
                if (MouseInRect(x, my, bx, (float)Height() * (0.64f - 0.10f * i), bw, bh))
                {
                    SetMaxFps(kFps[i]);
                    return;
                }

            if (MouseInRect(x, my, bx, (float)Height() * 0.24f, bw, bh))
                m_fpsMenu = false;
        }
        return;
    }

    // ----- jeu -----
    if (m_player.IsDead())
        return;

    if (button == MOUSE_BUTTON_LEFT)
    {
        m_leftDown = true;
        m_breakProgress = 0.0f;
        m_breakCooldown = 0.0f;
        AttackEntity();
    }
    else if (button == MOUSE_BUTTON_RIGHT)
    {
        m_rightDown = true;
        m_placeCooldown = 0.0f;
    }
    else if (button == MOUSE_BUTTON_MIDDLE)
    {
        PickBlock();
    }
}

void Engine::MouseReleaseEvent(const MOUSE_BUTTON& button, int x, int y)
{
    if (button == MOUSE_BUTTON_LEFT)
    {
        m_leftDown = false;
        m_breakProgress = 0.0f;
        m_draggingSlider = false;
    }
    else if (button == MOUSE_BUTTON_RIGHT)
    {
        m_rightDown = false;
    }
}

void Engine::MouseWheelEvent(int delta)
{
    if (m_paused)
        return;

    if (m_inventoryOpen)
    {
        if (!m_player.IsCreative())
            return;

        const int pages = ((int)Blocks::Palette().size() + 26) / 27;
        m_palettePage += (delta > 0) ? -1 : 1;
        if (m_palettePage < 0) m_palettePage = pages - 1;
        if (m_palettePage >= pages) m_palettePage = 0;
        return;
    }

    m_inventory.Scroll(delta > 0 ? -1 : 1);
}

// ---------------------------------------------------------------------------
//  Sauvegarde
// ---------------------------------------------------------------------------
std::string Engine::SavePath(const char* file) const
{
    return std::string(SAVE_PATH) + file;
}

void Engine::SaveGame()
{
    if (!m_world)
        return;

    // On s'assure que le dossier existe
#ifdef _WIN32
    CreateDirectoryA(SAVE_PATH, NULL);
#endif

    m_world->Save(SavePath("world.dat"));

    FILE* f = fopen(SavePath("player.dat").c_str(), "wb");
    if (!f)
    {
        std::cerr << "[Engine] Sauvegarde du joueur impossible" << std::endl;
        return;
    }

    const Vector3f p = m_player.GetPosition();
    const float rx = m_player.GetRotationX();
    const float ry = m_player.GetRotationY();
    const int creative = m_player.IsCreative() ? 1 : 0;

    fwrite(&p.x, sizeof(float), 1, f);
    fwrite(&p.y, sizeof(float), 1, f);
    fwrite(&p.z, sizeof(float), 1, f);
    fwrite(&rx, sizeof(float), 1, f);
    fwrite(&ry, sizeof(float), 1, f);
    fwrite(&m_timeOfDay, sizeof(float), 1, f);
    fwrite(&creative, sizeof(int), 1, f);

    for (int i = 0; i < Inventory::TOTAL_SIZE; ++i)
    {
        const ItemStack& st = m_inventory.At(i);
        const int type = (int)st.type;
        const int count = st.count;
        fwrite(&type, sizeof(int), 1, f);
        fwrite(&count, sizeof(int), 1, f);
    }

    fclose(f);
    std::cout << "[Engine] Partie sauvegardee" << std::endl;
}

bool Engine::TakeScreenshot()
{
    const int w = Width(), h = Height();
    if (w <= 0 || h <= 0) return false;

    // Avant de relire le tampon d'affichage on remet le pipeline dans un
    // etat neutre et on attend la fin des commandes en cours. Sans cela le
    // pilote renvoie parfois un tampon vide au lieu de l'image affichee.
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0, w, 0, h, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_TEXTURE_2D); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glUseProgram(0);
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glFinish();

    // On lit uniquement le RGB: le canal alpha du tampon d'affichage n'a
    // aucune signification ici et rendrait l'image entierement transparente.
    std::vector<unsigned char> pixels((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);   // image tout juste dessinee, avant l'echange des tampons
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // Le pilote renvoie parfois un tampon vide au lieu de l'image affichee.
    // Une image entierement uniforme est donc consideree comme ratee: on
    // laissera l'appelant retenter a la frame suivante.
    bool uniform = true;
    for (size_t i = 3; i < pixels.size() && uniform; i += 3)
        if (pixels[i] != pixels[0] || pixels[i + 1] != pixels[1] || pixels[i + 2] != pixels[2])
            uniform = false;

    if (uniform)
        return false;

#ifdef _WIN32
    CreateDirectoryA(SAVE_PATH, NULL);
    CreateDirectoryA("../BlockAdventure/media/screenshots", NULL);
#endif

    char name[256];
    sprintf(name, "../BlockAdventure/media/screenshots/shot_%03d.png", m_shotCounter++);

    ILuint img;
    ilGenImages(1, &img);
    ilBindImage(img);
    ilTexImage(w, h, 1, 3, IL_RGB, IL_UNSIGNED_BYTE, pixels.data());
    ilEnable(IL_FILE_OVERWRITE);
    const bool ok = (ilSaveImage((const ILstring)name) != 0);
    if (ok) std::cout << "[Engine] Capture: " << name << std::endl;
    else    std::cerr << "[Engine] Capture impossible" << std::endl;
    ilDeleteImages(1, &img);
    return ok;
}

bool Engine::LoadGame()
{
    if (!m_world)
        return false;

    m_world->Load(SavePath("world.dat"));

    FILE* f = fopen(SavePath("player.dat").c_str(), "rb");
    if (!f)
        return false;

    float x = 0, y = 0, z = 0, rx = 0, ry = 0, tod = 0.3f;
    int creative = 0;

    bool ok = true;
    ok = ok && fread(&x, sizeof(float), 1, f) == 1;
    ok = ok && fread(&y, sizeof(float), 1, f) == 1;
    ok = ok && fread(&z, sizeof(float), 1, f) == 1;
    ok = ok && fread(&rx, sizeof(float), 1, f) == 1;
    ok = ok && fread(&ry, sizeof(float), 1, f) == 1;
    ok = ok && fread(&tod, sizeof(float), 1, f) == 1;
    ok = ok && fread(&creative, sizeof(int), 1, f) == 1;

    if (ok)
    {
        m_player.Reset(Vector3f(x, y, z));
        m_player.SetRotation(rx, ry);
        m_player.SetCreative(creative != 0);
        m_timeOfDay = tod;

        for (int i = 0; i < Inventory::TOTAL_SIZE; ++i)
        {
            int type = 0, count = 0;
            if (fread(&type, sizeof(int), 1, f) != 1) break;
            if (fread(&count, sizeof(int), 1, f) != 1) break;
            if (type < 0 || type >= BTYPE_FIN) { type = BTYPE_AIR; count = 0; }
            m_inventory.At(i).type = (BlockType)type;
            m_inventory.At(i).count = count;
        }

        std::cout << "[Engine] Partie chargee" << std::endl;
    }

    fclose(f);
    return ok;
}
