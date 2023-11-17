#include <algorithm>
#include <cmath>
#include <iostream>
#include "engine.h"
#include "shader.h"
#include"transformation.h"
#include "textureatlas.h"


Engine::Engine() : m_player(Vector3f(0.0f, 0.0f, 0.0f)), m_textureAtlas(8), m_chunks(GetMaxChunk(), GetMaxChunk())
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

	glClearColor(135.0 / 255.0, 206.0 / 255.0, 250.0 / 255.0, 1.0); // Sky color
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	// Light
	GLfloat light0Pos[4] = { 0.0f, 8 , 0, 1.0f };
	GLfloat light0Amb[4] = { 1,1,1,1 };
	GLfloat light0Diff[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
	GLfloat light0Spec[4] = { 0.2f, 0.2f, 0.2f, 1.0f };

	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_POSITION, light0Pos);
	glLightfv(GL_LIGHT0, GL_AMBIENT, light0Amb);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0Diff);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light0Spec);


	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0f, (float)Width() / (float)Height(), 0.0001f, 1000.0f);
	glEnable(GL_DEPTH_TEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_LIGHTING);
	glEnable(GL_LINE_SMOOTH);

	CenterMouse();
	HideCursor();

	const int m_maxChunk = GetMaxChunk();
	for (int y = -m_maxChunk; y <= m_maxChunk; ++y)
	{
		for (int x = -m_maxChunk; x <= m_maxChunk; ++x)
		{
			Chunk* nouveauchunk = new Chunk();
			m_chunks.Set(x, y, nouveauchunk);
			for (int y = 0; y < 5; ++y)
				for (int x = 0; x < CHUNK_SIZE_X; ++x)
					for (int z = 0; z < CHUNK_SIZE_Z; ++z)
					{
						if (y < 2)
							nouveauchunk->SetBlock(x, y, z, BTYPE_STONE);
						if (y == 2)
							nouveauchunk->SetBlock(x, y, z, BTYPE_STONE);
						if (y == 3)
							nouveauchunk->SetBlock(x, y, z, BTYPE_DIRT);
						if (y == 4)
							nouveauchunk->SetBlock(x, y, z, BTYPE_GRASS);
					}
			// Escalier
			nouveauchunk->SetBlock(6, 5, 7, BTYPE_STONE);
			nouveauchunk->SetBlock(7, 6, 7, BTYPE_STONE);
			nouveauchunk->SetBlock(8, 7, 7, BTYPE_STONE);
			nouveauchunk->SetBlock(9, 8, 7, BTYPE_STONE);

			// Passage
			nouveauchunk->SetBlock(11, 5, 1, BTYPE_DIRT);
			nouveauchunk->SetBlock(11, 6, 1, BTYPE_DIRT);
			nouveauchunk->SetBlock(11, 7, 1, BTYPE_DIRT);
			nouveauchunk->SetBlock(11, 7, 2, BTYPE_DIRT);
			nouveauchunk->SetBlock(11, 7, 3, BTYPE_DIRT);
			nouveauchunk->SetBlock(11, 6, 3, BTYPE_DIRT);
			nouveauchunk->SetBlock(11, 5, 3, BTYPE_DIRT);
			//Mur axe des Z
			for (int i = 5; i <= 8; i++)
			{
				nouveauchunk->SetBlock(5, i, 10, BTYPE_DIRT);
				nouveauchunk->SetBlock(5, i, 11, BTYPE_DIRT);
				nouveauchunk->SetBlock(5, i, 12, BTYPE_DIRT);
				nouveauchunk->SetBlock(5, i, 13, BTYPE_DIRT);
				nouveauchunk->SetBlock(5, i, 14, BTYPE_DIRT);
			}
			//Mur axe des X
			for (int i = 5; i <= 8; i++) {
				nouveauchunk->SetBlock(14, i, 14, BTYPE_STONE);
				nouveauchunk->SetBlock(13, i, 14, BTYPE_STONE);
				nouveauchunk->SetBlock(12, i, 14, BTYPE_STONE);
				nouveauchunk->SetBlock(11, i, 14, BTYPE_STONE);
				nouveauchunk->SetBlock(10, i, 14, BTYPE_STONE);
			}
		}
	}
	m_player.SetRotationY(130);
}

void Engine::DeInit()
{
}

void Engine::LoadResource()
{
	LoadShaders();
	LoadBlockTextures();
	GenerateTextureAtlas();
	PopulateBlockInfo();
	LoadTextures();
}

void Engine::LoadShaders()
{
	std::cout << "Loading and compiling shaders..." << std::endl;
	if (!m_shader01.Load(SHADER_PATH "shader01.vert", SHADER_PATH "shader01.frag", true))
	{
		std::cout << "Failed to load shader" << std::endl;
		exit(1);
	}
}

void Engine::LoadBlockTextures()
{
	LoadBlockType(BTYPE_CHECKER, "checker.png", 6);
	LoadBlockType(BTYPE_DIRT, "dirt.png", 6);
	LoadBlockType(BTYPE_GRASS, "topgrass.png", 1);
	LoadBlockType(BTYPE_GRASS, "sidegrass.png", 4);
	LoadBlockType(BTYPE_GRASS, "dirt.png", 1);
	LoadBlockType(BTYPE_STONE, "stone.png", 6);
	LoadBlockType(BTYPE_WOOD, "topwood.png", 1);
	LoadBlockType(BTYPE_WOOD, "sidewood.png", 4);
	LoadBlockType(BTYPE_WOOD, "topwood.png", 1);
}

void Engine::LoadBlockType(BlockType type, const std::string& texturePath, int count)
{
	TextureAtlas::TextureIndex texture = m_textureAtlas.AddTexture(TEXTURE_PATH + texturePath);
	for (int j = 0; j < count; j++)
		m_BlockType[type].push_back(texture);
}

void Engine::GenerateTextureAtlas()
{
	if (!m_textureAtlas.Generate(128, false))
	{
		std::cout << " Unable to generate texture atlas..." << std::endl;
		abort();
	}
}

void Engine::PopulateBlockInfo()
{
	for (int i = 0; i < BTYPE_FIN; i++)
	{
		m_blockinfo[i] = new BlockInfo(static_cast<BlockType>(i), std::to_string(i), 1, false);
		m_blockinfo[i]->m_type = static_cast<BlockType>(i);
		m_blockinfo[i]->m_textureCount = m_BlockType[i].size();
		m_blockinfo[i]->m_textures = new TextureAtlas::TextureIndex[m_blockinfo[i]->m_textureCount];

		for (int j = 0; j < m_blockinfo[i]->m_textureCount; j++)
			m_blockinfo[i]->m_textures[j] = m_BlockType[i][j];
	}
}

void Engine::LoadTextures()
{
	LoadTexture(m_textureFont, TEXTURE_PATH "font.png");
	LoadTexture(m_textureCrosshair, TEXTURE_PATH "crosshair.png");
	LoadTexture(m_textureItemBar, TEXTURE_PATH "Itembar.png");
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
	CollisionPlayer(elapsedTime * Speed);
	Transformation cam;
	m_player.ApplyTransformation(cam);
	cam.ApplyTranslation(0.5f, 0, 0.5f);
	cam.Use();
	

	m_textureAtlas.Bind();
	for (int x = 0; x < m_chunkPositionX; x++)
	{
		for (int y = 0; y < m_chunkPositionY; y++)
		{
			Chunk* chunk = m_chunks.Get(x, y);
			for (int i = 0; i < BTYPE_FIN; i++)
				chunk->SetBlockInfo(m_blockinfo[i], i);

			if (!finiUpdate || chunk->IsDirty())
				chunk->Update(x, y);
			chunk->Render();
		}
	}

	if (m_chunkPositionX < GetMaxChunk() || m_chunkPositionY < GetMaxChunk())
	{
		if (m_Plusx)
			m_chunkPositionX++;
		else
			m_chunkPositionY++;
		m_Plusx = !m_Plusx;
	}
	else
		finiUpdate = true;

	glEnable(GL_LIGHTING);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if (m_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	if (elapsedTime > 0.00f)
		DrawHud(static_cast<int>(1.0f / elapsedTime), gameTime, m_crossSize);
	if (m_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Engine::CollisionPlayer(float elapsedTime)
{
	// Collision
	Vector3f pos = m_player.GetPositon();
	int chunkposx = static_cast<int>(pos.x / CHUNK_SIZE_X);
	int chunkposy = static_cast<int>(pos.z / CHUNK_SIZE_Z);

	// Adjust chunk positions for negative coordinates
	if (pos.x < 0 && pos.z < 0)
	{
		chunkposx = static_cast<int>((pos.x / CHUNK_SIZE_X) - 1);
		chunkposy = static_cast<int>((pos.z / CHUNK_SIZE_Z) - 1);
	}
	else if (pos.z < 0)
	{
		chunkposy = static_cast<int>((pos.z / CHUNK_SIZE_Z) - 1);
		chunkposx = static_cast<int>(pos.x / CHUNK_SIZE_X);
	}
	else if (pos.x < 0)
	{
		chunkposy = static_cast<int>(pos.z / CHUNK_SIZE_Z);
		chunkposx = static_cast<int>((pos.x / CHUNK_SIZE_X) - 1);
	}

	bool safe = true;
	int max = GetMaxChunk();
	if ((chunkposx >= max && chunkposy >= max) || chunkposx >= max || chunkposy >= max)
		safe = false;
	else if ((chunkposx < 0 && chunkposy < 0) || chunkposx < 0 || chunkposy < 0)
		safe = false;
	if (safe)
	{
		Chunk* chunk = m_chunks.Get(chunkposx, chunkposy);
		const int blockPositionX = chunkposx * CHUNK_SIZE_X;
		const int blockPositionZ = chunkposy * CHUNK_SIZE_Z;

		// Simulate player movement
		Vector3f delta = m_player.SimulateMove(m_keyW, m_keyS, m_keyA, m_keyD, m_keyJump, m_keyFly, elapsedTime);

		// Check for collision in the x-axis
		int getblockx = static_cast<int>(pos.x + delta.x - blockPositionX);
		int getblockz = static_cast<int>(pos.z + delta.z - blockPositionZ);
		checkCollisionX(chunk, pos, delta, getblockx, blockPositionZ);

		// Check for collision in the y-axis
		checkCollisionY(chunk, pos, delta, blockPositionX, blockPositionZ);

		// Check for collision in the z-axis
		checkCollisionZ(chunk, pos, delta, blockPositionX, getblockz);

		// Update player position
		pos += delta;
		m_player.SetPosition(pos);

		// Application du SafetyNet
		applySafetyNet(chunk, pos, blockPositionX, blockPositionZ);
	}
	else
	{
		Vector3f delta = m_player.SimulateMove(m_keyW, m_keyS, m_keyA, m_keyD, m_keyJump, m_keyFly, elapsedTime);
		pos += delta;
		m_player.SetPosition(pos);
	}	
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Vérifier la collision dans l'axe x
void Engine::checkCollisionX(Chunk* chunk, const Vector3f& pos, Vector3f& delta, int getblockx, int blockPositionZ)
{
	BlockType bt1 = chunk->GetBlock(getblockx, pos.y, pos.z - blockPositionZ);
	BlockType bt2 = chunk->GetBlock(getblockx, pos.y + 0.9f, pos.z - blockPositionZ);
	BlockType bt3 = chunk->GetBlock(getblockx, pos.y - 1.f, pos.z - blockPositionZ);

	if (bt1 != BTYPE_AIR || bt2 != BTYPE_AIR || bt3 != BTYPE_AIR)
		delta.x = 0;
}

// Vérifier la collision dans l'axe y
void Engine::checkCollisionY(Chunk* chunk, const Vector3f& pos, Vector3f& delta, int blockPositionX, int blockPositionZ)
{
	BlockType bt1 = chunk->GetBlock(pos.x - blockPositionX, pos.y + delta.y + 0.9f, pos.z - blockPositionZ);
	BlockType bt2 = chunk->GetBlock(pos.x - blockPositionX, pos.y + delta.y - 1.f, pos.z - blockPositionZ);

	if (bt1 != BTYPE_AIR)
	{
		delta.y = 0;
		m_player.CheckBlockUnderROver(false, true);
	}
	else if (bt2 != BTYPE_AIR)
	{
		delta.y = 0;
		m_player.CheckBlockUnderROver(true, false);
	}
	else
		m_player.CheckBlockUnderROver(false, false);
}

// Vérifier la collision dans l'axe z
void Engine::checkCollisionZ(Chunk* chunk, const Vector3f& pos, Vector3f& delta, int blockPositionX, int getblockz)
{
	BlockType bt1 = chunk->GetBlock(pos.x - blockPositionX, pos.y, getblockz);
	BlockType bt2 = chunk->GetBlock(pos.x - blockPositionX, pos.y + 0.9f, getblockz);
	BlockType bt3 = chunk->GetBlock(pos.x - blockPositionX, pos.y - 1.f, getblockz);

	if (bt1 != BTYPE_AIR || bt2 != BTYPE_AIR || bt3 != BTYPE_AIR)
		delta.z = 0;
}

// Application du SafetyNet
void Engine::applySafetyNet(Chunk* chunk, Vector3f& pos, int blockPositionX, int blockPositionZ)
{
	BlockType bt1 = chunk->GetBlock(pos.x - blockPositionX, pos.y, pos.z - blockPositionZ);
	BlockType bt2 = chunk->GetBlock(pos.x - blockPositionX, pos.y + 0.9f, pos.z - blockPositionZ);
	BlockType bt3 = chunk->GetBlock(pos.x - blockPositionX, pos.y - 1.f, pos.z - blockPositionZ);

	if (bt1 != BTYPE_AIR || bt2 != BTYPE_AIR || bt3 != BTYPE_AIR)
	{
		pos.y += 1;
		m_player.SetPosition(pos);
	}
}

void Engine::DrawHud(int Fps, const int gameTime, const int crossSize)
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
	ss << " Fps : " << Fps;
	PrintText(1, Height() - 25, ss.str());
	ss.str("");
	ss << " Position : " << m_player.GetPositon();
	PrintText(1, 10, ss.str());
	ss.str("");
	ss << "Game Time : " << gameTime;
	PrintText(7, Height() - 40, ss.str());
	ss.str("");
	ss<< "Direction " << m_player.GetRotationY();
	PrintText(6, Height() - 55, ss.str());

	//Crosshair
	m_textureCrosshair.Bind();
	glLoadIdentity();
	glTranslated(Width() / 2 - m_crossSize / 2, Height() / 2 - m_crossSize / 2, 0);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2i(0, 0);
	glTexCoord2f(1, 0);
	glVertex2i(m_crossSize, 0);
	glTexCoord2f(1, 1);
	glVertex2i(m_crossSize, m_crossSize);
	glTexCoord2f(0, 1);
	glVertex2i(0, m_crossSize);
	glEnd();

	//ItemBar
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	m_textureItemBar.Bind();
	glLoadIdentity();
	glTranslated(Width() / 2 - 175, 30, 0); 	//Déplacer vers le bas au milieu
	int itemBarWidth = 370; // Choisissez la largeur souhaitée
	int itemBarHeight = 55; // Conservez la hauteur d'origine ou ajustez au besoin
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2i(0, 0);
	glTexCoord2f(1, 0);
	glVertex2i(itemBarWidth, 0);
	glTexCoord2f(1, 1);
	glVertex2i(itemBarWidth, itemBarHeight);
	glTexCoord2f(0, 1);
	glVertex2i(0, itemBarHeight);
	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	glEnd();

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
	switch (key)
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
	case 57: //space
		m_keyJump = true;
		break;
	case 38: //MAJ
		m_keyFly = true;
		break;
	default:
		std::cout << "Unhandled key: " << (int)key << std::endl;
	}
}

void Engine::KeyReleaseEvent(unsigned char key)
{
	switch (key)
	{
	case 24: // Y
		m_wireframe = !m_wireframe;
		if (m_wireframe)
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
	case 57: //space
		m_keyJump = false;
		break;
	case 38: //MAJ
		m_keyFly = false;
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
	if (x == (Width() / 2) && y == (Height() / 2))
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
	if (!texture.IsValid())
	{
		std::cerr << "Unable to load texture (" << filename << ")" << std::endl;
		if (stopOnError)
			Stop();

		return false;
	}

	return true;
}

int Engine::GetMaxChunk()
{
	// racine de VIEW_DISTANCE
	return VIEW_DISTANCE / CHUNK_SIZE_X;
}
