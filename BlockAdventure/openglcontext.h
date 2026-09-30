#ifndef OPENGLCONTEXT_H__
#define OPENGLCONTEXT_H__

#include "define.h"

#include <string>
#include <SFML/Window.hpp>

// Documentation de SFML: http://www.sfml-dev.org/documentation/index-fr.php
class OpenglContext
{
public:
    enum MOUSE_BUTTON {
        MOUSE_BUTTON_NONE       = 0x00,
        MOUSE_BUTTON_LEFT       = 0x01,
        MOUSE_BUTTON_MIDDLE     = 0x02,
        MOUSE_BUTTON_RIGHT      = 0x04,
        MOUSE_BUTTON_WHEEL_UP   = 0x08,
        MOUSE_BUTTON_WHEEL_DOWN = 0x10
    };

    OpenglContext();
    virtual ~OpenglContext();

    virtual void Init() = 0;
    virtual void DeInit() = 0;
    virtual void LoadResource() = 0;
    virtual void UnloadResource() = 0;
    virtual void Render(float elapsedTime) = 0;

    // Les codes de touches sont ceux de sf::Keyboard::Key
    virtual void KeyPressEvent(int key) = 0;
    virtual void KeyReleaseEvent(int key) = 0;
    virtual void MouseMoveEvent(int x, int y) = 0;
    virtual void MousePressEvent(const MOUSE_BUTTON& button, int x, int y) = 0;
    virtual void MouseReleaseEvent(const MOUSE_BUTTON& button, int x, int y) = 0;
    virtual void MouseWheelEvent(int delta) {}
    virtual void ResizeEvent(int width, int height) {}

    bool Start(const std::string& title, int width, int height, bool fullscreen);
    bool Stop();

    int Width() const;
    int Height() const;

    void SetMaxFps(int maxFps);
    int GetMaxFps() const;

    void SetVerticalSync(bool enabled);
    bool VerticalSync() const { return m_vsync; }

    void SetFullscreen(bool fullscreen);
    bool IsFullscreen() const;

    bool HasFocus() const { return m_hasFocus; }

protected:
    void CenterMouse();
    void MakeRelativeToCenter(int& x, int& y) const;

    void ShowCursor();
    void HideCursor();
    void ShowCrossCursor() const;

private:
    void InitWindow(int width, int height);
    MOUSE_BUTTON ConvertMouseButton(sf::Mouse::Button button) const;

private:
    sf::Window  m_app;
    int         m_maxFps;
    bool        m_fullscreen;
    bool        m_vsync;
    bool        m_hasFocus;
    bool        m_running;
    std::string m_title;
    float       m_lastFrameTime;
    int         m_windowedWidth;
    int         m_windowedHeight;
};

#endif // OPENGLCONTEXT_H__
