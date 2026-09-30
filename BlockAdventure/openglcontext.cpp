#include "openglcontext.h"
#include "define.h"
#include <iostream>

OpenglContext::OpenglContext()
    : m_maxFps(240), m_fullscreen(false), m_vsync(false), m_hasFocus(true), m_running(false),
      m_title(""), m_lastFrameTime(0.0f), m_windowedWidth(1280), m_windowedHeight(720)
{
}

OpenglContext::~OpenglContext()
{
}

void OpenglContext::InitWindow(int width, int height)
{
    // Profil de compatibilite (2.1) : le moteur utilise encore la pile de
    // matrices fixe, tout en compilant des shaders GLSL 1.20.
    sf::ContextSettings settings(24, 8, 0, 2, 1);

    if (m_fullscreen)
    {
        m_app.create(sf::VideoMode::getFullscreenModes()[0], m_title,
                     sf::Style::Fullscreen, settings);
    }
    else
    {
        m_app.create(sf::VideoMode(width, height, 32), m_title,
                     sf::Style::Resize | sf::Style::Close, settings);
    }

    m_app.setFramerateLimit(m_vsync ? 0 : m_maxFps);
    m_app.setVerticalSyncEnabled(m_vsync);
    m_app.setKeyRepeatEnabled(false);
}

bool OpenglContext::Start(const std::string& title, int width, int height, bool fullscreen)
{
    m_title = title;
    m_fullscreen = fullscreen;
    m_windowedWidth = width;
    m_windowedHeight = height;

    InitWindow(width, height);

    Init();
    LoadResource();
    ResizeEvent(Width(), Height());

    m_running = true;
    sf::Clock clock;

    while (m_app.isOpen() && m_running)
    {
        clock.restart();

        sf::Event Event;
        while (m_app.pollEvent(Event))
        {
            switch (Event.type)
            {
            case sf::Event::Closed:
                m_app.close();
                break;

            case sf::Event::Resized:
                if (!m_fullscreen)
                {
                    m_windowedWidth = (int)Event.size.width;
                    m_windowedHeight = (int)Event.size.height;
                }
                glViewport(0, 0, Event.size.width, Event.size.height);
                ResizeEvent((int)Event.size.width, (int)Event.size.height);
                break;

            case sf::Event::GainedFocus:
                m_hasFocus = true;
                break;

            case sf::Event::LostFocus:
                m_hasFocus = false;
                break;

            case sf::Event::KeyPressed:
                KeyPressEvent((int)Event.key.code);
                break;

            case sf::Event::KeyReleased:
                KeyReleaseEvent((int)Event.key.code);
                break;

            case sf::Event::MouseMoved:
                MouseMoveEvent(Event.mouseMove.x, Event.mouseMove.y);
                break;

            case sf::Event::MouseButtonPressed:
                MousePressEvent(ConvertMouseButton(Event.mouseButton.button),
                                Event.mouseButton.x, Event.mouseButton.y);
                break;

            case sf::Event::MouseButtonReleased:
                MouseReleaseEvent(ConvertMouseButton(Event.mouseButton.button),
                                  Event.mouseButton.x, Event.mouseButton.y);
                break;

            case sf::Event::MouseWheelMoved:
                MouseWheelEvent(Event.mouseWheel.delta);
                break;

            default:
                break;
            }
        }

        if (!m_app.isOpen() || !m_running)
            break;

        m_app.setActive();
        Render(m_lastFrameTime);
        m_app.display();

        m_lastFrameTime = clock.getElapsedTime().asSeconds();

        if (!m_vsync && m_maxFps > 0)
        {
            const float waitTime = (1.0f / (float)m_maxFps) - m_lastFrameTime;
            if (waitTime > 0.0f)
            {
                sf::sleep(sf::seconds(waitTime));
                m_lastFrameTime = clock.getElapsedTime().asSeconds();
            }
        }
    }

    UnloadResource();
    DeInit();

    if (m_app.isOpen())
        m_app.close();

    return true;
}

bool OpenglContext::Stop()
{
    m_running = false;
    return true;
}

void OpenglContext::CenterMouse()
{
    sf::Mouse::setPosition(sf::Vector2i(Width() / 2, Height() / 2), m_app);
}

int OpenglContext::Width() const
{
    return (int)m_app.getSize().x;
}

int OpenglContext::Height() const
{
    return (int)m_app.getSize().y;
}

void OpenglContext::SetMaxFps(int maxFps)
{
    m_maxFps = maxFps;
    m_app.setFramerateLimit(m_vsync ? 0 : maxFps);
}

int OpenglContext::GetMaxFps() const
{
    return m_maxFps;
}

void OpenglContext::SetVerticalSync(bool enabled)
{
    m_vsync = enabled;
    m_app.setVerticalSyncEnabled(enabled);
    m_app.setFramerateLimit(enabled ? 0 : m_maxFps);
}

void OpenglContext::SetFullscreen(bool fullscreen)
{
    if (m_fullscreen == fullscreen)
        return;

    // Recreer la fenetre detruit le contexte OpenGL: toutes les ressources
    // GPU (textures, shaders, VBO) doivent etre rechargees.
    UnloadResource();
    DeInit();

    m_fullscreen = fullscreen;
    InitWindow(m_windowedWidth, m_windowedHeight);

    Init();
    LoadResource();
    ResizeEvent(Width(), Height());
}

bool OpenglContext::IsFullscreen() const
{
    return m_fullscreen;
}

void OpenglContext::MakeRelativeToCenter(int& x, int& y) const
{
    x = x - (Width() / 2);
    y = y - (Height() / 2);
}

void OpenglContext::ShowCursor()
{
    m_app.setMouseCursorVisible(true);
}

void OpenglContext::HideCursor()
{
    m_app.setMouseCursorVisible(false);
}

void OpenglContext::ShowCrossCursor() const
{
}

OpenglContext::MOUSE_BUTTON OpenglContext::ConvertMouseButton(sf::Mouse::Button button) const
{
    switch (button)
    {
    case sf::Mouse::Left:   return MOUSE_BUTTON_LEFT;
    case sf::Mouse::Middle: return MOUSE_BUTTON_MIDDLE;
    case sf::Mouse::Right:  return MOUSE_BUTTON_RIGHT;
    default:                return MOUSE_BUTTON_NONE;
    }
}
