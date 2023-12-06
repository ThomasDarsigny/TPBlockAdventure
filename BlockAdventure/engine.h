#ifndef ENGINE_H__
#define ENGINE_H__

#include "define.h"
#include "openglcontext.h"
#include "texture.h"
#include "player.h"
#include "chunk.h"
#include "shader.h"
#include "textureatlas.h"
#include "array2d.h"
#include "blockinfo.h"
#include "perlin.h"


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

    

    virtual void KeyPressEvent(unsigned char key) override;
    virtual void KeyReleaseEvent(unsigned char key) override;
    virtual void MouseMoveEvent(int x, int y) override;
    virtual void MousePressEvent(const MOUSE_BUTTON& button, int x, int y) override;
    virtual void MouseReleaseEvent(const MOUSE_BUTTON& button, int x, int y) override;


private:
    bool LoadTexture(Texture& texture, const std::string& filename, bool stopOnError = true);
    int GetMaxChunk();
    void PrintText(unsigned int x, unsigned int y, const std::string& text);
    void LoadShaders();
    void LoadBlockTextures();
    void LoadBlockType(BlockType type, const std::string& texturePath, int count);
    void GenerateTextureAtlas();
    void LoadTextures();
    void PopulateBlockInfo();
    void CollisionPlayer(float elapsedTime);
    void checkCollisionX(Chunk* chunk, const Vector3f& pos, Vector3f& delta, int getblockx, int blockPositionZ);
    void checkCollisionY(Chunk* chunk, const Vector3f& pos, Vector3f& delta, int blockPositionX, int blockPositionZ);
    void checkCollisionZ(Chunk* chunk, const Vector3f& pos, Vector3f& delta, int blockPositionX, int getblockz);
    void SafetyNet(Chunk* chunk, Vector3f& pos, int blockPositionX, int blockPositionZ);
    void DrawHud(int fps, int gameTime, int crossSize);
    void GetBlockAtCursor(int x, int y, int z);

    void SetBlockAt(int x, int y, int z, BlockType);
    void UpdateChunks(int x, int y, int z);

    BlockType BlockAt(int x, int y, int z);

    template <class T>
    static bool EqualWithEpsilon(const T& v1, const T& v2, T epsilon = T(0.0001))
    {
        return (fabs(v2 - v1) < epsilon);
    }

    template <class T>
    static bool InRangeWithEpsilon(const T& v, const T& vinf, const T& vsup, T epsilon = T(0.0001))
    {
        return (v >= vinf - epsilon && v <= vsup + epsilon);
    }

private:
    bool m_wireframe = false;

    // TEXTURES
    Texture m_textureFont;
    Texture m_textureCrosshair;
    Texture m_textureItemBar;
    Texture m_textureOptionsButton;
    Texture m_textureQuitButton;
    Texture m_textureBackButton;
    Texture m_textureBacktoGameButton;

    Texture m_texture30Fps;
    Texture m_texture60Fps;
    Texture m_texture120Fps;
    Texture m_texture240Fps;
    Texture m_textureFPS;
    Texture m_textureFullScreenON;
    Texture m_textureFullScreenOFF;
    Texture m_textureFullScreen;
    Texture m_textureLogo;

    std::map<BlockType, std::vector<int>> m_BlockType;

    // SHADERS
    Shader m_shader01;

    Player m_player;

    Chunk m_testChunk;

    TextureAtlas m_textureAtlas;

    Array2d<Chunk*> m_chunks;

    BlockInfo* m_blockinfo[BTYPE_FIN];

    Vector3f m_currentBlock;
    Vector3f m_currentFaceNormal;
    

    const int m_crossSize = 20;
    const int buttonWidth = 200; // Largeur
    const int buttonHeight = 55; // Hauteur    

    int mousex = 0;
    int mousey = 0;     
    int m_chunkPositionX = 0;
    int m_chunkPositionY = 0;
    int m_ChunkCount = 0;
    int m_MaxFps = 0;

    bool m_Plusx = true;
    bool finiUpdate = false;
    bool m_keyW = false;
    bool m_keyA = false;
    bool m_keyS = false;
    bool m_keyD = false;
    bool m_keyJump = false;
    bool m_keyFly = false;
    bool m_keyESC = false;
    bool m_Settings = false;
    bool m_FPSSettings = false; 
};
#endif // ENGINE_H__
