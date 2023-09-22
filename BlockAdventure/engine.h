#ifndef ENGINE_H__
#define ENGINE_H__
#include "define.h"
#include "openglcontext.h"
#include "texture.h"

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
    virtual void KeyPressEvent(unsigned char key);
    virtual void KeyReleaseEvent(unsigned char key);
    virtual void MouseMoveEvent(int x, int y);
    virtual void MousePressEvent(const MOUSE_BUTTON &button, int x, int y);
    virtual void MouseReleaseEvent(const MOUSE_BUTTON &button, int x, int y);

private:
    bool LoadTexture(Texture& texture, const std::string& filename, bool stopOnError = true);

private:
    bool m_wireframe = false;

    Texture m_textureFloor;

    bool m_keyW = false;
    bool m_keyA = false;
    bool m_keyS = false;
    bool m_keyD = false;
};

#endif // ENGINE_H__
