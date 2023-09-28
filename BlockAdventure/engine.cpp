#include "engine.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include"transformation.h"

Engine::Engine()
{
}

Engine::~Engine()
{
}

void Engine::Init()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (float)Width() / (float)Height(), 0.0001f, 1000.0f);
    glEnable(GL_DEPTH_TEST);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_LIGHTING);
    glEnable(GL_LINE_SMOOTH);

    // Light
    GLfloat light0Pos[4] = { 0.0f, CHUNK_SIZE_Y, 0.0f, 1.0f };
    GLfloat light0Amb[4] = { 0.9f, 0.9f, 0.9f, 1.0f };
    GLfloat light0Diff[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat light0Spec[4] = { 0.2f, 0.2f, 0.2f, 1.0f };

    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, light0Pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light0Amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0Diff);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light0Spec);

    CenterMouse();
    HideCursor();
}

void Engine::DeInit()
{
}

void Engine::LoadResource()
{
    LoadTexture(m_textureFloor, TEXTURE_PATH "checker.png");
    LoadTexture(m_textureSideGrass, TEXTURE_PATH "sidegrass.png");
    LoadTexture(m_textureTopGrass, TEXTURE_PATH "topgrass.png");
    LoadTexture(m_textureDirt, TEXTURE_PATH "dirt.png");
}

void Engine::UnloadResource()
{
}

void Engine::Render(float elapsedTime)
{
    static float gameTime = elapsedTime;

    gameTime += elapsedTime;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Transformations initiales
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Plancher
    // Les vertex doivent etre affiches dans le sens anti-horaire (CCW)
    m_textureTopGrass.Bind();
    float nbRep = 50.f;
    glBegin(GL_QUADS);
        glNormal3f(0, 1, 0); // Normal vector

        glTexCoord2f(0, 0);
        glVertex3f(-100.f, -2.f, 100.f);

        glTexCoord2f(nbRep, 0);
        glVertex3f(100.f, -2.f, 100.f);

        glTexCoord2f(nbRep, nbRep);
        glVertex3f(100.f, -2.f, -100.f);

        glTexCoord2f(0, nbRep);
        glVertex3f(-100.f, -2.f, -100.f);
    glEnd();

    Transformation t;
    t.ApplyTranslation(0, 0, -7.f);
    t.ApplyRotation(gameTime * 100.f, 0, 1, 0);
    t.ApplyRotation(gameTime * 100.f, 1, 0, 0);
    t.Use();
    
    //Front face
    //BIND LA TEXTURE
    m_textureSideGrass.Bind();
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);

    glTexCoord2f(0, 0);
    glVertex3f(-0.5f, -0.5f, 0.5f);

    glTexCoord2f(1, 0);
    glVertex3f(0.5f, -0.5f, 0.5f);

    glTexCoord2f(1, 1);
    glVertex3f(0.5f, 0.5f, 0.5f);

    glTexCoord2f(0, 1);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glEnd();

    // Back face
    //m_textureSideGrass.Bind();
    glBegin(GL_QUADS);
    glNormal3f(0, 0, -1);

    glTexCoord2f(1, 0);
    glVertex3f(-0.5f, 0.5f, -0.5f);

    glTexCoord2f(1, 1);
    glVertex3f(-0.5f, -0.5f, -0.5f);

    glTexCoord2f(0, 1);
    glVertex3f(0.5f, -0.5f, -0.5f);

    glTexCoord2f(0, 0);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glEnd();

    // Left side face
    m_textureSideGrass.Bind();
    glBegin(GL_QUADS);
    glNormal3f(-1, 0, 0);

    glTexCoord2f(0, 0);
    glVertex3f(-0.5f, -0.5f, -0.5f);

    glTexCoord2f(1, 0);
    glVertex3f(-0.5f, -0.5f, 0.5f);

    glTexCoord2f(1, 1);
    glVertex3f(-0.5f, 0.5f, 0.5f);

    glTexCoord2f(0, 1);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glEnd();

    // Right side face  
    m_textureSideGrass.Bind();
    glBegin(GL_QUADS);
    glNormal3f(1, 0, 0);

    glTexCoord2f(1, 0);
    glVertex3f(0.5f, -0.5f, -0.5f);

    glTexCoord2f(0, 0);
    glVertex3f(0.5f, -0.5f, 0.5f);

    glTexCoord2f(0, 1);
    glVertex3f(0.5f, 0.5f, 0.5f);

    glTexCoord2f(1, 1);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glEnd();

    //Top face
    m_textureTopGrass.Bind();
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);

    glTexCoord2f(0, 0);
    glVertex3f(-0.5f, 0.5f, -0.5f);

    glTexCoord2f(1, 0);
    glVertex3f(0.5f, 0.5f, -0.5f);

    glTexCoord2f(1, 1);
    glVertex3f(0.5f, 0.5f, 0.5f);

    glTexCoord2f(0, 1);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glEnd();

    // Bottom face
    m_textureDirt.Bind();
    glBegin(GL_QUADS);
    glNormal3f(0, -1, 0);

    glTexCoord2f(1, 1);
    glVertex3f(-0.5f, -0.5f, -0.5f);

    glTexCoord2f(0, 1);
    glVertex3f(0.5f, -0.5f, -0.5f);

    glTexCoord2f(0, 0);
    glVertex3f(0.5f, -0.5f, 0.5f);

    glTexCoord2f(1, 0);
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glEnd();

    
}

void Engine::KeyPressEvent(unsigned char key)
{
    switch(key)
    {
        case 36: // ESC
            Stop();
            break;
        case 94: // F10
            SetFullscreen(!IsFullscreen());
            break;
        default:
            std::cout << "Unhandled key: " << (int)key << std::endl;
    }
}

void Engine::KeyReleaseEvent(unsigned char key)
{
    switch(key)
    {
        case 24: // Y
            m_wireframe = !m_wireframe;
            if(m_wireframe)
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            else
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            break;
    }
}

void Engine::MouseMoveEvent(int x, int y)
{
    // Centrer la souris seulement si elle n'est pas déjà centrée
    // Il est nécessaire de faire la vérification pour éviter de tomber
    // dans une boucle infinie où l'appel à CenterMouse génère un
    // MouseMoveEvent, qui rapelle CenterMouse qui rapelle un autre
    // MouseMoveEvent, etc
    if(x == (Width() / 2) && y == (Height() / 2))
        return;

    CenterMouse();
}

void Engine::MousePressEvent(const MOUSE_BUTTON& button, int x, int y)
{
}

void Engine::MouseReleaseEvent(const MOUSE_BUTTON& button, int x, int y)
{
}

bool Engine::LoadTexture(Texture& texture, const std::string& filename, bool stopOnError)
{
    texture.Load(filename);
    if(!texture.IsValid())
    {
        std::cerr << "Unable to load texture (" << filename << ")" << std::endl;
        if(stopOnError)
            Stop();

        return false;
    }

    return true;
}
