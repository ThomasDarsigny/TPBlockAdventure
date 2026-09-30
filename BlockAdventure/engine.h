#ifndef ENGINE_H__
#define ENGINE_H__

#include "define.h"
#include "openglcontext.h"
#include "texture.h"
#include "textureatlas.h"
#include "shader.h"
#include "player.h"
#include "world.h"
#include "inventory.h"
#include "crafting.h"
#include "entity.h"
#include "postfx.h"
#include "vector3.h"

#include <string>
#include <memory>

class Engine : public OpenglContext
{
public:
    Engine();
    virtual ~Engine();

    virtual void Init() override;
    virtual void DeInit() override;
    virtual void LoadResource() override;
    virtual void UnloadResource() override;
    virtual void Render(float elapsedTime) override;

    virtual void KeyPressEvent(int key) override;
    virtual void KeyReleaseEvent(int key) override;
    virtual void MouseMoveEvent(int x, int y) override;
    virtual void MousePressEvent(const MOUSE_BUTTON& button, int x, int y) override;
    virtual void MouseReleaseEvent(const MOUSE_BUTTON& button, int x, int y) override;
    virtual void MouseWheelEvent(int delta) override;
    virtual void ResizeEvent(int width, int height) override;

    // Options de mise au point (voir main.cpp): capture automatique,
    // teleportation, orientation et heure imposees.
    void SetAutoScreenshot(float delaySeconds) { m_autoShot = delaySeconds; }
    void SetStartPosition(float x, float y, float z) { m_startPos = Vector3f(x, y, z); m_hasStartPos = true; }
    void SetStartLook(float yaw, float pitch) { m_startYaw = yaw; m_startPitch = pitch; m_hasStartLook = true; }
    void SetStartTime(float t) { m_startTime = t; m_hasStartTime = true; }
    void SetCreativeStart() { m_startCreative = true; }
    void SetDebugOverlay() { m_showDebug = true; }
    void SetDumpMap() { m_dumpMap = true; }
    void SetFullscreenTest(float delay) { m_fsTest = delay; }
    void SetFluidTest() { m_fluidTest = true; }
    void SetCraftTest() { m_craftTest = true; }
    void SetMobTest() { m_mobTest = true; }

private:
    // ---- ressources ----
    bool LoadTexture(Texture& texture, const std::string& filename, bool stopOnError = true);
    void LoadShaders();
    void BuildTextureAtlas();
    void LoadHudTextures();

    // ---- simulation ----
    void UpdateGame(float elapsed);
    void UpdateTimeOfDay(float elapsed);
    void UpdateTargetBlock();
    void UpdateBreaking(float elapsed);
    void PlaceBlock();
    void PickBlock();
    void AttackEntity();
    bool EatHeldFood();
    void Respawn();

    // ---- rendu ----
    void ComputeSkyColors();
    void RenderSky();
    void RenderWorld();
    void RenderChunkPass(bool blendPass, GLint chunkOriginLoc);
    void RenderSelection();

    // ---- interface ----
    void Begin2D();
    void End2D();
    void PrintText(float x, float y, const std::string& text, float scale = 1.0f);
    void DrawQuad(float x, float y, float w, float h) const;
    void DrawTexturedQuad(float x, float y, float w, float h) const;
    void DrawBlockIcon(BlockType type, float x, float y, float size) const;
    void DrawColorQuad(float x, float y, float w, float h, float r, float g, float b, float a) const;

    void DrawHud(float elapsed);
    void DrawCrosshair();
    void DrawHotbar();
    void DrawStatusBars();
    void DrawDebugOverlay(float elapsed);
    void DrawInfoOverlay();
    void DrawInventoryScreen();
    void DrawItemSlot(float x, float y, float size, const ItemStack& st);

    // --- fabrication ---
    void OpenInventory(int gridSize);
    void CloseInventory();
    bool CraftResult(BlockType& type, int& count) const;
    void TakeCraftResult(bool wholeStack);
    void ClickSlot(int id, bool rightClick);
    ItemStack* SlotFromId(int id);

    struct SlotRect { int id; float x, y, size; };
    void BuildInventoryLayout(std::vector<SlotRect>& out) const;
    void DrawPauseMenu();
    void DrawDeathScreen();

    // --- curseur de distance d'affichage (menu Options) ---
    void RenderDistanceSliderRect(float& x, float& y, float& w, float& h) const;
    void DrawRenderDistanceSlider();
    bool RenderDistanceSliderHit(int mx, int my) const;
    void SetRenderDistanceFromMouse(int mx);

    bool MouseInRect(int x, int y, float rx, float ry, float rw, float rh) const;
    int  InventorySlotAt(int mx, int my) const;

    // ---- sauvegarde ----
    std::string SavePath(const char* file) const;
    void SaveGame();
    bool LoadGame();
    bool TakeScreenshot();
    int  m_shotRetries;

private:
    // ---- monde et joueur ----
    std::unique_ptr<World> m_world;
    Player        m_player;
    Inventory     m_inventory;
    EntityManager m_entities;

    // ---- ressources graphiques ----
    std::unique_ptr<TextureAtlas> m_atlas;

    Shader m_terrainShader;
    Shader m_skyShader;
    PostFX m_postfx;

    Texture m_textureFont;
    Texture m_textureCrosshair;
    Texture m_textureBreak;
    Texture m_textureLogo;
    Texture m_textureOptionsButton;
    Texture m_textureQuitButton;
    Texture m_textureBackButton;
    Texture m_textureBacktoGameButton;
    Texture m_textureFPS;
    Texture m_textureFullScreenON;
    Texture m_textureFullScreenOFF;
    Texture m_texture30Fps;
    Texture m_texture60Fps;
    Texture m_texture120Fps;
    Texture m_texture240Fps;

    // ---- cycle jour / nuit ----
    float m_timeOfDay;        // 0 = minuit, 0.5 = midi
    bool  m_timeFrozen;
    float m_gameTime;

    Vector3f m_sunDir;
    Vector3f m_sunColor;
    Vector3f m_zenithColor;
    Vector3f m_horizonColor;
    Vector3f m_groundColor;
    Vector3f m_ambient;
    float    m_dayFactor;

    // ---- interaction ----
    bool     m_hasTarget;
    Vector3i m_targetBlock;
    Vector3i m_targetNormal;
    float    m_breakProgress;   // 0..1
    float    m_breakCooldown;   // delai avant d'attaquer le bloc suivant
    Vector3i m_breakingBlock;
    bool     m_leftDown;
    bool     m_rightDown;
    float    m_placeCooldown;
    float    m_attackCooldown;

    // Instants du dernier appui, pour detecter les doubles appuis
    float    m_lastForwardTap;  // W W  -> course
    float    m_lastJumpTap;     // Espace Espace -> vol

    // ---- etats d'interface ----
    bool m_paused;
    bool m_settingsMenu;
    bool m_fpsMenu;
    bool m_inventoryOpen;
    bool m_showHud;
    bool m_showDebug;
    bool m_wireframe;
    bool m_draggingSlider;      // curseur de distance d'affichage en cours de glissement

    int  m_mouseX, m_mouseY;

    // Pile "en main" dans l'ecran d'inventaire, et grille de fabrication
    ItemStack m_cursor;
    ItemStack m_craft[9];
    int       m_craftSize;      // 2 (inventaire) ou 3 (etabli)
    int       m_palettePage;

    // ---- divers ----
    float m_fpsAccum;
    int   m_fpsFrames;
    int   m_fps;
    bool  m_resourcesLoaded;
    bool  m_underwater;
    float m_autoSaveTimer;
    float m_autoShot;      // <= 0 : desactive
    int   m_shotCounter;

    Vector3f m_startPos;
    bool     m_hasStartPos;
    float    m_startYaw, m_startPitch;
    bool     m_hasStartLook;
    bool     m_startCreative;
    bool     m_dumpMap;
    float    m_fsTest;     // bascule le plein ecran une fois, pour tester
    bool     m_playerPlaced;
    bool     m_fluidTest;
    bool     m_craftTest;
    bool     m_mobTest;
    float    m_startTime;
    bool     m_hasStartTime;

    static const int HOTBAR_SLOT = 52;
    static const int BUTTON_W = 200;
    static const int BUTTON_H = 55;
};

#endif // ENGINE_H__
