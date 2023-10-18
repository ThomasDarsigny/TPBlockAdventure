#include "engine.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include"transformation.h"

Engine::Engine(): m_player(Vector3f(0.0f,0.0f,0.0f))
{
}

Engine::~Engine()
{
}

void Engine::Init()
{
    GLenum glewErr = glewInit(); if (glewErr != GLEW_OK)
    {
        std::cerr << "ERREUR GLEW: " << glewGetErrorString(glewErr) << std::endl; abort();
    }

    glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);

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

    //Mouvements du joueur
    float Speed = 7.0f;
    m_player.Move(m_keyW, m_keyS, m_keyA, m_keyD, elapsedTime * Speed);
    Transformation cam;
    cam.ApplyRotation(-m_player.GetRotationX(), 1.0f, 0, 0);
    cam.ApplyRotation(-m_player.GetRotationY(), 0, 1.0f, 0);
    cam.ApplyTranslation(-m_player.GetPositon());
    cam.Use();
   

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
    m_player.ApplyTransformation(t);
    t.ApplyTranslation(0, 0, -7.f);
    t.Use();
  
    // Crosshair
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glColor3b(0, 1, 0);                                                             //Changement de couleur du crosshair

    float crosshairSize = 0.01f;
    int numSegments = 50;
    float radius = crosshairSize / 2.0f;
    float angleIncrement = 2.0f * 3.14159265359f / numSegments;

    glDisable(GL_LIGHTING);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(0.0f, 0.0f);

    for (int i = 0; i <= numSegments; ++i) {
        float angle = i * angleIncrement;
        float x = radius * cos(angle);
        float y = radius * sin(angle);
        glVertex2f(x, y);
    }
    glEnd();
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
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
        case 0: //a
            m_keyA = true;
            break;
        case 3: //d
            m_keyD = true;
            break;
        case 22: //w
            m_keyW = true;
            break;
        case 18: //s
            m_keyS = true;
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
        case 0: //a
            m_keyA = false;
            break;
        case 3: //d
            m_keyD = false;
            break;
        case 22: //w
            m_keyW = false;
            break;
        case 18: //s
            m_keyS = false;
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
    float m_sensitivity = 0.08f;
    MakeRelativeToCenter(x, y);
    m_player.TurnLeftRight(x * m_sensitivity);
    m_player.TurnTopBottom(y * m_sensitivity);
    

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
