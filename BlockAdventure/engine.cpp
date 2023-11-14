#include <algorithm>
#include <cmath>
#include <iostream>
#include"transformation.h"


Engine::Engine() : m_player(Vector3f(0.0f, 0.0f, 0.0f)), m_textureAtlas(16), m_chunks(GetMaxChunk(), GetMaxChunk())
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

	int maxchunk = 16 / 2;
	// divise par 2, à cause qu'il faut des chunks au négatif
	// exemple : max chunk = 4
	//            min : -2,  max : 2
	for (int x = -maxchunk; x <= maxchunk; x++) {
		for (int z = -maxchunk; z <= maxchunk; z++) {
			if (x != 0 || z != 0) // exclure les 0, sinon il va créer, par exemple, 5 chunks au lieu de 4
			{
				Chunk* NouveauChunk = new Chunk(); // créer un nouveau chunk
				m_chunks.Set(x, z, NouveauChunk); // mettre le nouveau chunk dans le tableau 2D
				NouveauChunk->SetBlock(0, 0, 0, BTYPE_DIRT);

				Chunk* chunk = m_chunks.Get(x, z);
				if (chunk && chunk->IsDirty()) {
					BlockType bt = chunk->GetBlock(0, 0, 0);
				}
			}
		}
	}
}
	


void Engine::DeInit()
{
}

void Engine::LoadResource()
{
    std::cout << "Loading and compiling shaders..." << std::endl; if (!m_shader01.Load(SHADER_PATH "shader01.vert", SHADER_PATH "shader01.frag", true))
    {
        std::cout << "Failed to load shader" << std::endl; exit(1);
    }

	TextureAtlas::TextureIndex texture = m_textureAtlas.AddTexture(TEXTURE_PATH "checker.png");

	texture = m_textureAtlas.AddTexture(TEXTURE_PATH "dirt.png");

	if (!m_textureAtlas.Generate(128, false))
	{
		std::cout << " Unable to generate texture atlas ..." << std::endl;
		abort();
	}

	LoadTexture(m_textureFont, TEXTURE_PATH "font.bmp");
	LoadTexture(m_textureSideGrass, TEXTURE_PATH "sidegrass.png");
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
    glColor3f(1.0f, 0.0f, 0.0f);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    float crosshairSize = 0.01f;
    int numSegments = 50;
    float radius = crosshairSize / 1.5f;
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

   
	for (int x = 0; x < CHUNK_SIZE_X; ++x)
	{
		for (int z = 0; z < CHUNK_SIZE_Z; ++z)
		{
			for (int y = 0; y < 32; ++y)
			{
				if (x % 2 == 0 && y % 2 == 0 && z % 2 == 0)
					m_testChunk.SetBlock(x, y, z, BTYPE_DIRT);
			}
		}
	}
    m_textureDirt.Bind();
	if (m_testChunk.IsDirty()) m_testChunk.Update();
	m_shader01.Use(); m_testChunk.Render();
	Shader::Disable();

	if (m_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	DrawHud(static_cast<int>(1.0f / elapsedTime));
	if (m_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}
void Engine::DrawHud(int Fps)
{
	// Setter le blend function , tout ce qui sera noir sera transparent
	glDisable(GL_LIGHTING);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, Width(), 0, Height(), -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	// Bind de la texture pour le font
	m_textureFont.Bind();
	std::ostringstream ss;
	ss << " fps : " << Fps;
	PrintText(10, Height() - 25, ss.str());
	ss.str("");
	ss << " position : " << m_player.GetPositon(); // important : on utilise l ’ operateur << pour afficher la position
	PrintText(10, 10, ss.str());

	glEnable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

void Engine::PrintText(unsigned int x, unsigned int y, const std::string& t)
{
	glLoadIdentity();
	glTranslated(x, y, 0);
	for (unsigned int i = 0; i < t.length(); ++i)
	{
		float left = (float)((t[i] - 32) % 16) / 16.0f;
		float top = (float)((t[i] - 32) / 16) / 16.0f;
		top += 0.5f;
		glBegin(GL_QUADS);
		glTexCoord2f(left, 1.0f - top - 0.0625f);
		glVertex2f(0, 0);
		glTexCoord2f(left + 0.0625f, 1.0f - top - 0.0625f);
		glVertex2f(12, 0);
		glTexCoord2f(left + 0.0625f, 1.0f - top);
		glVertex2f(12, 12);
		glTexCoord2f(left, 1.0f - top);
		glVertex2f(0, 12);
		glEnd();
		glTranslated(8, 0, 0);
	}
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
