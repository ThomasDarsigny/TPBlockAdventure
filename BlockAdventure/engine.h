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

class Engine : public OpenglContext
{
public:
    Engine();
    virtual ~Engine();
    virtual void Init();
    virtual void DeInit();
    virtual void LoadResource();
    virtual void UnloadResource();
    virtual void Render(float elapsedTime);
    void DrawHud(int Fps, const int gameTime, const int m_crossSize);
    virtual void KeyPressEvent(unsigned char key);
    virtual void KeyReleaseEvent(unsigned char key);
    virtual void MouseMoveEvent(int x, int y);
    virtual void MousePressEvent(const MOUSE_BUTTON& button, int x, int y);
    virtual void MouseReleaseEvent(const MOUSE_BUTTON& button, int x, int y);

private:
    bool LoadTexture(Texture& texture, const std::string& filename, bool stopOnError = true);
    int GetMaxChunk();
   
    void PrintText(unsigned int x, unsigned int y, const std::string& t);
private:
    bool m_wireframe = false;

    //TEXTURES
    Texture m_textureFont;
    Texture m_textureCrosshair;
    Texture m_textureItemBar;

    //SHADERS
    Shader m_shader01;

    Player m_player;

    Chunk m_testChunk;

    TextureAtlas m_textureAtlas;

    Array2d<Chunk*> m_chunks; // mettre dans un pointeur pour reprendre chunk[posx,posy] par la suite

    const int m_crossSize = 20;

    bool m_keyW = false;
    bool m_keyA = false;
    bool m_keyS = false;
    bool m_keyD = false;
    bool m_keyJump = false;
};

#endif // ENGINE_H__