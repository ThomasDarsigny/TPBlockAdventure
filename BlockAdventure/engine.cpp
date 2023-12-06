#include <algorithm>
#include <cmath>
#include <iostream>
#include "engine.h"
#include "shader.h"
#include"transformation.h"
#include "textureatlas.h"

Engine::Engine() : m_player(Vector3f(50.0f, 82.0f, 100.0f)), m_textureAtlas(20), m_chunks(GetMaxChunk(), GetMaxChunk())
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

	glClearColor(135.0 / 255.0, 206.0 / 255.0, 250.0 / 255.0, 1.0); // Couleur du ciel
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

		// Lumière
		GLfloat light0Pos[4] = { 0.0f, 8.0f, 0.0f, 1.0f };
		GLfloat light0Amb[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
		GLfloat light0Diff[4] = { 0.8f, 0.8f, 0.8f, 1.0f };
		GLfloat light0Spec[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

		glEnable(GL_LIGHT0);
		glLightfv(GL_LIGHT0, GL_POSITION, light0Pos);
		glLightfv(GL_LIGHT0, GL_AMBIENT, light0Amb);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light0Diff);
		glLightfv(GL_LIGHT0, GL_SPECULAR, light0Spec);

		// Matériaux
		GLfloat materialAmbDiff[4] = { 0.8f, 0.8f, 0.8f, 1.0f };
		GLfloat materialSpecular[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
		GLfloat materialShininess = 10.0f;

		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, materialAmbDiff);
		glMaterialfv(GL_FRONT, GL_SPECULAR, materialSpecular);
		glMaterialf(GL_FRONT, GL_SHININESS, materialShininess);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(45.0f, (float)Width() / (float)Height(), 0.0001f, 1000.0f);
		glEnable(GL_DEPTH_TEST);
		glShadeModel(GL_SMOOTH);
		glEnable(GL_LIGHTING);
		glEnable(GL_LINE_SMOOTH);
	
	

	CenterMouse();
	HideCursor();
	
	const int m_maxChunk = GetMaxChunk();
	Perlin perlin(16.f, 10.f, 0.5f, 95.f);

	for (int y = -m_maxChunk; y <= m_maxChunk; ++y) {
		for (int x = -m_maxChunk; x <= m_maxChunk; ++x) {
			Chunk* nouveauchunk = new Chunk();
			m_chunks.Set(x, y, nouveauchunk);

			for (int blockX = 0; blockX < CHUNK_SIZE_X; ++blockX) {
				for (int blockZ = 0; blockZ < CHUNK_SIZE_Z; ++blockZ) {
					for (int blockY = 0; blockY <= CHUNK_SIZE_Y; ++blockY) {
						// Calcul de la hauteur du terrain en utilisant le bruit de Perlin
						int terrainHeight = static_cast<int>((perlin.Get((float)(x * CHUNK_SIZE_X + blockX) / 2000.f, (float)(blockY + CHUNK_SIZE_Y / 2000), (float)(y * CHUNK_SIZE_Z + blockZ) / 2000.f) + 1.0f) * 0.5f * CHUNK_SIZE_Y);

						// Définition des blocs en fonction de la hauteur du terrain
						if (blockY == terrainHeight - 1)
							nouveauchunk->SetBlock(blockX, terrainHeight, blockZ, BTYPE_DIRT);
						else if (blockY > terrainHeight - 6)
							nouveauchunk->SetBlock(blockX, terrainHeight, blockZ, BTYPE_GRASS);
					}
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
	LoadBlockType(BTYPE_DIRT, "dirt.png", 6);
	LoadBlockType(BTYPE_GRASS, "topgrass.png", 1);
	LoadBlockType(BTYPE_GRASS, "sidegrass.png", 4);
	LoadBlockType(BTYPE_GRASS, "dirt.png", 1);
	LoadBlockType(BTYPE_STONE, "stone.png", 6);
	LoadBlockType(BTYPE_WOOD, "topwood.png", 1);
	LoadBlockType(BTYPE_WOOD, "sidewood.png", 4);
	LoadBlockType(BTYPE_WOOD, "topwood.png", 1);
	LoadBlockType(BTYPE_BEDROCK, "bedrock.png", 6);
	LoadBlockType(BTYPE_GOLD, "gold.png", 6);
	LoadBlockType(BTYPE_COAL, "coal.png", 6);
	LoadBlockType(BTYPE_DIAMOND, "diamond.png", 6);
	LoadBlockType(BTYPE_IRON, "iron.png", 6);
	LoadBlockType(BTYPE_SAND, "sand.png", 6);
	LoadBlockType(BTYPE_SLIME, "slime.png", 6);
	LoadBlockType(BTYPE_WOODPLANK, "woodplank.png", 6);
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
	LoadTexture(m_textureOptionsButton, TEXTURE_PATH "OptionsButton.png");
	LoadTexture(m_textureQuitButton, TEXTURE_PATH "QuitButton.png");
	LoadTexture(m_textureBacktoGameButton, TEXTURE_PATH "BacktoGameButton.png");
	LoadTexture(m_textureBackButton, TEXTURE_PATH "BackButton.png");
	LoadTexture(m_textureLeftArrow, TEXTURE_PATH "LeftArrow.png");
	LoadTexture(m_textureRightArrow, TEXTURE_PATH "RightArrow.png");
	LoadTexture(m_texture30Fps, TEXTURE_PATH "30Fps.png");
	LoadTexture(m_texture60Fps, TEXTURE_PATH "60Fps.png");
	LoadTexture(m_texture120Fps, TEXTURE_PATH "120Fps.png");
	LoadTexture(m_texture240Fps, TEXTURE_PATH "240Fps.png");
	LoadTexture(m_textureFullScreenON, TEXTURE_PATH "FullscreenON.png");
	LoadTexture(m_textureFullScreenOFF, TEXTURE_PATH "FullscreenOFF.png");
	LoadTexture(m_textureLogo, TEXTURE_PATH "logo.png");
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

			if (!finiUpdate || chunk->IsDirty() && !m_keyESC)
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

BlockType Engine::BlockAt(int x, int y, int z)
{
	int chunkposy = static_cast<int>(y / CHUNK_SIZE_Y);
	int chunkposx = static_cast<int>(x / CHUNK_SIZE_X);

	if (x < 0 && z < 0)
	{
		chunkposx = static_cast<int>((x / CHUNK_SIZE_X) - 1);
		chunkposy = static_cast<int>((z / CHUNK_SIZE_Z) - 1);
	}
	else if (x < 0)
	{
		chunkposy = static_cast<int>(z / CHUNK_SIZE_Z);
		chunkposx = static_cast<int>((x / CHUNK_SIZE_X) - 1);
	}
	else if (z < 0)
	{
		chunkposy = static_cast<int>((z / CHUNK_SIZE_Z) - 1);
		chunkposx = static_cast<int>(x / CHUNK_SIZE_X);
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

		return chunk->GetBlock(x - blockPositionX, y, z - blockPositionZ);
	}
	else
		return BTYPE_AIR;
}

void Engine::CollisionPlayer(float elapsedTime)
{
	Vector3f pos = m_player.GetPositon();
	int chunkposy = static_cast<int>(pos.z / CHUNK_SIZE_Z);
	int chunkposx = static_cast<int>(pos.x / CHUNK_SIZE_X);

	if (pos.x < 0 && pos.z < 0)
	{
		chunkposx = static_cast<int>((pos.x / CHUNK_SIZE_X) - 1);
		chunkposy = static_cast<int>((pos.z / CHUNK_SIZE_Z) - 1);
	}
	else if (pos.x < 0)
	{
		chunkposy = static_cast<int>(pos.z / CHUNK_SIZE_Z);
		chunkposx = static_cast<int>((pos.x / CHUNK_SIZE_X) - 1);
	}
	else if (pos.z < 0)
	{
		chunkposy = static_cast<int>((pos.z / CHUNK_SIZE_Z) - 1);
		chunkposx = static_cast<int>(pos.x / CHUNK_SIZE_X);
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

		Vector3f delta = m_player.SimulateMove(m_keyW, m_keyS, m_keyA, m_keyD, m_keyJump, m_keyFly, elapsedTime);


		int getblockx = static_cast<int>(pos.x + delta.x - blockPositionX);
		int getblockz = static_cast<int>(pos.z + delta.z - blockPositionZ);

		checkCollisionX(chunk, pos, delta, getblockx, blockPositionZ);      // Collision dans l'axe x
		checkCollisionY(chunk, pos, delta, blockPositionX, blockPositionZ); // Collision dans l'axe y		
		checkCollisionZ(chunk, pos, delta, blockPositionX, getblockz);      // Collision dans l'axe z

		pos += delta;
		m_player.SetPosition(pos);

		SafetyNet(chunk, pos, blockPositionX, blockPositionZ);
	}
	else
	{
		Vector3f delta = m_player.SimulateMove(m_keyW, m_keyS, m_keyA, m_keyD, m_keyJump, m_keyFly, elapsedTime);
		pos += delta;
		m_player.SetPosition(pos);
	}
}

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
void Engine::SafetyNet(Chunk* chunk, Vector3f& pos, int blockPositionX, int blockPositionZ)
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
	ss << " Fps: " << Fps; // Fps
	PrintText(3, Height() - 15, ss.str());
	ss.str("");
	ss << "Game Time: " << gameTime; // Temps de jeu
	PrintText(9, Height() - 30, ss.str());
	ss.str("");
	ss << "Days: " << gameTime / 1200; // Nombre de jours écoulés
	PrintText(8, Height() - 45, ss.str());
	ss.str("");

	//Boussole
	if (m_player.GetRotationY() >= 337.5 || m_player.GetRotationY() < 22.5) // Directtion du joueur Nord
	{
		ss.str("");
		ss << "Direction: " << m_player.GetRotationY() << "(Nord)";
		PrintText(8, Height() - 60, ss.str());
	}
	else if (m_player.GetRotationY() >= 22.5 && m_player.GetRotationY() < 67.5) // Direction du joueur Nord - Est
	{
		ss.str("");
		ss << "Direction: " << m_player.GetRotationY() << "(Nord-Est)";
		PrintText(8, Height() - 60, ss.str());
	}
	else if (m_player.GetRotationY() >= 67.5 && m_player.GetRotationY() < 112.5) // Direction du joueur Est
	{
		ss.str("");
		ss << "Direction: " << m_player.GetRotationY() << "(Est)";
		PrintText(8, Height() - 60, ss.str());
	}
	else if (m_player.GetRotationY() >= 112.5 && m_player.GetRotationY() < 157.5) // Direction du joueur Sud-Est
	{
		ss.str("");
		ss << "Direction: " << m_player.GetRotationY() << "(Sud-Est)";
		PrintText(8, Height() - 60, ss.str());
	}
	else if (m_player.GetRotationY() >= 157.5 && m_player.GetRotationY() < 202.5) // Direction du joueur Sud
	{
		ss.str("");
		ss << "Direction: " << m_player.GetRotationY() << "(Sud)";
		PrintText(8, Height() - 60, ss.str());
	}
	else if (m_player.GetRotationY() >= 202.5 && m_player.GetRotationY() < 247.5) // Direction du joueur Sud-Ouest
	{
		ss.str("");
		ss << "Direction: " << m_player.GetRotationY() << "(Sud-Ouest)";
		PrintText(8, Height() - 60, ss.str());
	}
	else if (m_player.GetRotationY() >= 247.5 && m_player.GetRotationY() < 292.5) // Direction du joueur Ouest
	{
		ss.str("");
		ss << "Direction: " << m_player.GetRotationY() << "(Ouest)";
		PrintText(8, Height() - 60, ss.str());
	}
	else if (m_player.GetRotationY() >= 292.5 && m_player.GetRotationY() < 337.5) // Direction du joueur Nord-Ouest
	{
		ss.str("");
		ss << "Direction: " << m_player.GetRotationY() << "(Nord-Ouest)";
		PrintText(8, Height() - 60, ss.str());
	}

	ss.str("");
	ss << " Position: " << m_player.GetPositon(); // Position du joueur
	PrintText(0, Height() - 90, ss.str());

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
	glTranslated(Width() / 2 - 175, 30, 0); //Location
	int itemBarWidth = 370; // Largeur
	int itemBarHeight = 55; // Hauteur
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

	if (m_keyESC && !m_Settings)
	{
		const int BacktoGameButtony = 0.6 * Height();
		const int SettingsButtony = 0.5 * Height();
		const int QuitButtony = 0.4 * Height();
		const int Buttonx = Width() / 2 - buttonWidth / 2;

		//Button Quit
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		m_textureQuitButton.Bind();
		glLoadIdentity();
		glTranslated(Buttonx, QuitButtony, 0); //Location
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(0, 0);
		glTexCoord2f(1, 0);
		glVertex2i(buttonWidth, 0);
		glTexCoord2f(1, 1);
		glVertex2i(buttonWidth, buttonHeight);
		glTexCoord2f(0, 1);
		glVertex2i(0, buttonHeight);
		glEnable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glEnd();

		//Button Options
		m_textureOptionsButton.Bind();
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glLoadIdentity();
		glTranslated(Buttonx, SettingsButtony, 0); //Location
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(0, 0);
		glTexCoord2f(1, 0);
		glVertex2i(buttonWidth, 0);
		glTexCoord2f(1, 1);
		glVertex2i(buttonWidth, buttonHeight);
		glTexCoord2f(0, 1);
		glVertex2i(0, buttonHeight);
		glEnd();

		//Button Back to Game
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		m_textureBacktoGameButton.Bind();
		glLoadIdentity();
		glTranslated(Buttonx, BacktoGameButtony, 0); //Location
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(0, 0);
		glTexCoord2f(1, 0);
		glVertex2i(buttonWidth, 0);
		glTexCoord2f(1, 1);
		glVertex2i(buttonWidth, buttonHeight);
		glTexCoord2f(0, 1);
		glVertex2i(0, buttonHeight);
		glEnable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glEnd();
	}
	else if (m_keyESC && m_Settings)
	{
		const int LeftArrowButtonx = Width() / 2 - 150;
		const int LeftArrowButtony = 0.6 * Height();

		const int RightArrowButtonx = Width() / 2 + 100;
		const int RightArrowButtony = 0.6 * Height();

		const int Buttonx = Width() / 2 - buttonWidth / 2;
		const int FpsButtony = 0.6 * Height();
		const int FullscreenButtony = 0.5 * Height();
		const int BacktoGameButtony = 0.4 * Height();

		//Button decrease Max Fps
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		m_textureLeftArrow.Bind();
		glLoadIdentity();
		glTranslated(LeftArrowButtonx, LeftArrowButtony, 0); //Location
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(0, 0);
		glTexCoord2f(1, 0);
		glVertex2i(arrowbuttonWidth, 0);
		glTexCoord2f(1, 1);
		glVertex2i(arrowbuttonWidth, buttonHeight);
		glTexCoord2f(0, 1);
		glVertex2i(0, buttonHeight);
		glEnable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glEnd();

		//Button Fps
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		m_texture30Fps.Bind();
		glLoadIdentity();
		glTranslated(Buttonx, FpsButtony, 0); //Location
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(0, 0);
		glTexCoord2f(1, 0);
		glVertex2i(buttonWidth, 0);
		glTexCoord2f(1, 1);
		glVertex2i(buttonWidth, buttonHeight);
		glTexCoord2f(0, 1);
		glVertex2i(0, buttonHeight);
		glEnable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glEnd();

		//Button increase Max Fps
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		m_textureRightArrow.Bind();
		glLoadIdentity();
		glTranslated(RightArrowButtonx, RightArrowButtony, 0); //Location
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(0, 0);
		glTexCoord2f(1, 0);
		glVertex2i(arrowbuttonWidth, 0);
		glTexCoord2f(1, 1);
		glVertex2i(arrowbuttonWidth, buttonHeight);
		glTexCoord2f(0, 1);
		glVertex2i(0, buttonHeight);
		glEnable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glEnd();

		//Button Fullscreen
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		m_textureFullScreenON.Bind();
		glLoadIdentity();
		glTranslated(Buttonx, FullscreenButtony, 0); //Location
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(0, 0);
		glTexCoord2f(1, 0);
		glVertex2i(buttonWidth, 0);
		glTexCoord2f(1, 1);
		glVertex2i(buttonWidth, buttonHeight);
		glTexCoord2f(0, 1);
		glVertex2i(0, buttonHeight);
		glEnable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glEnd();

		//Button Back to Game
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		m_textureBackButton.Bind();
		glLoadIdentity();
		glTranslated(Buttonx, BacktoGameButtony, 0); //Location
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2i(0, 0);
		glTexCoord2f(1, 0);
		glVertex2i(buttonWidth, 0);
		glTexCoord2f(1, 1);
		glVertex2i(buttonWidth, buttonHeight);
		glTexCoord2f(0, 1);
		glVertex2i(0, buttonHeight);
		glEnable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glEnd();
	}

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
		case 15: // P
		
			break;
	case 36: // ESC
		if (!m_keyESC)
		{
			m_keyESC = true;
			ShowCursor();
		}
		else
		{
			m_keyESC = false;
			HideCursor();
		}
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
	if (!m_keyESC) {
		if (x == (Width() / 2) && y == (Height() / 2))
			return;
		float m_sensitivity = 0.08f;
		MakeRelativeToCenter(x, y);
		m_player.TurnLeftRight(x * m_sensitivity);
		m_player.TurnTopBottom(y * m_sensitivity);
		CenterMouse();
	}
	else
	{
		mousex = x;
		mousey = -(y - Height());
	}
}

void Engine::MousePressEvent(const MOUSE_BUTTON& button, int x, int y)
{
	y = -(y - Height()); // Invert the y axis to be consistent with the 2D drawing system
	if (button == MOUSE_BUTTON_LEFT && m_keyESC)
	{
		SetMaxFps(m_MaxFps);
		const int BacktoGameButtony = 0.6 * Height();
		const int SettingsButtony = 0.5 * Height();
		const int QuitButtony = 0.4 * Height();
		const int FullscreenButtony = 0.5 * Height();
		const int BackButtony = 0.4 * Height();
		const int LeftArrowButtony = 0.6 * Height();
		const int LeftArrowButtonx = Width() / 2 - 150;
		const int RightArrowButtony = 0.6 * Height();
		const int RightArrowButtonx = Width() / 2 + 100;
		const int Buttonx = Width() / 2 - buttonWidth / 2;

		//Options
		if (x >= Buttonx && x <= Buttonx + buttonWidth && y >= SettingsButtony && y <= SettingsButtony + buttonHeight || m_Settings)
		{
			if (m_Settings)
			{
				//Button decrease Max Fps
				if (x >= LeftArrowButtonx && x <= LeftArrowButtonx + arrowbuttonWidth && y >= LeftArrowButtony && y <= LeftArrowButtony + buttonHeight)
				{
					if (m_MaxFps >= 30 && m_MaxFps <= 240 && m_MaxFps != 30)
					{

						if (m_MaxFps == 30)
						{
							m_textureFPS = m_texture30Fps;
							m_textureFPS.Bind();
						}
						else if (m_MaxFps == 60)
						{
							m_textureFPS = m_texture60Fps;
							m_texture60Fps.Bind();
						}
						else if (m_MaxFps == 120)
						{
							m_textureFPS = m_texture120Fps;
							m_texture120Fps.Bind();
						}
						else if (m_MaxFps == 240)
						{
							m_textureFPS = m_texture240Fps;
							m_texture240Fps.Bind();
						}
					}
				}
				//Button increase Max Fps
				else if (x >= RightArrowButtonx && x <= RightArrowButtonx + arrowbuttonWidth && y >= RightArrowButtony && y <= RightArrowButtony + buttonHeight)
				{
					if (m_MaxFps >= 30 && m_MaxFps <= 240 && m_MaxFps != 240)
					{

						if (m_MaxFps == 30)
						{
							m_textureFPS = m_texture30Fps;
							m_texture30Fps.Bind();
						}
						else if (m_MaxFps == 60)
						{
							m_textureFPS = m_texture60Fps;
							m_texture60Fps.Bind();
						}
						else if (m_MaxFps == 120)
						{
							m_textureFPS = m_texture120Fps;
							m_texture120Fps.Bind();
						}
						else if (m_MaxFps == 240)
						{
							m_textureFPS = m_texture240Fps;
							m_texture240Fps.Bind();
						}
					}
				}
				//Button Fullscreen
				else if (x >= Buttonx && x <= Buttonx + buttonWidth && y >= FullscreenButtony && y <= FullscreenButtony + buttonHeight)
				{
					if (IsFullscreen())
					{
						m_textureFullScreenON = m_textureFullScreenOFF;
						m_textureFullScreenOFF.Bind();
					}
					else
					{
						m_textureFullScreenOFF = m_textureFullScreenON;
						m_textureFullScreenON.Bind();
					}
					SetFullscreen(!IsFullscreen());
				}
				
				
				
				//Back
				else if (x >= Buttonx && x <= Buttonx + buttonWidth && y >= BackButtony && y <= BackButtony + buttonHeight)
				{
					m_Settings = false;
					m_keyESC = false;
				}
			}
			else
			{
				m_Settings = true;
			}
		}
		// Back to game
		else if (x >= Buttonx && x <= Buttonx + buttonWidth && y >= BacktoGameButtony && y <= BacktoGameButtony + buttonHeight)
		{
			m_keyESC = false;
			HideCursor();
			CenterMouse();
		}
		// Quit 
		else if (x >= Buttonx && x <= Buttonx + buttonWidth && y >= QuitButtony && y <= QuitButtony + buttonHeight)
			Stop();
	}

	if (button == MOUSE_BUTTON_LEFT && !m_keyESC)
	{
		if (m_currentBlock.x != -1)
		{
			int* x = new int(m_player.GetPositon().x);
			int* y = new int(m_player.GetPositon().y);
			int* z = new int(m_player.GetPositon().z);

			GetBlockAtCursor(*x, *y, *z);
			// On detruit le bloc
			SetBlockAt(m_currentBlock.x, m_currentBlock.y, m_currentBlock.z, BTYPE_AIR);
			// On met a jour les chunks
			UpdateChunks(m_currentBlock.x, m_currentBlock.y, m_currentBlock.z);
		}
	}

	else	if (button == MOUSE_BUTTON_RIGHT && !m_keyESC)
	{
		if (m_currentBlock.x != -1)
		{
			int* x = new int(m_player.GetPositon().x);
			int* y = new int(m_player.GetPositon().y);
			int* z = new int(m_player.GetPositon().z);
			GetBlockAtCursor(*x, *y, *z);
			// On place le bloc
			SetBlockAt(m_currentBlock.x + (int)m_currentFaceNormal.x, m_currentBlock.y + (int)m_currentFaceNormal.y, m_currentBlock.z + (int)m_currentFaceNormal.z, BTYPE_GRASS);
			// On met a jour les chunks
			UpdateChunks(m_currentBlock.x + (int)m_currentFaceNormal.x, m_currentBlock.y + (int)m_currentFaceNormal.y, m_currentBlock.z + (int)m_currentFaceNormal.z);
		}
	}
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

void Engine::GetBlockAtCursor(int& x, int& y, int& z)
{
	x = Width() / 2;
	y = Height() / 2;

	GLint viewport[4];
	GLdouble modelview[16];
	GLdouble projection[16];
	GLfloat winX, winY, winZ;
	GLdouble posX, posY, posZ;

	glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
	glGetDoublev(GL_PROJECTION_MATRIX, projection);
	glGetIntegerv(GL_VIEWPORT, viewport);

	winX = (float)x;
	winY = (float)viewport[3] - (float)y;
	glReadPixels(x, int(winY), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &winZ);

	gluUnProject(winX, winY, winZ, modelview, projection, viewport, &posX, &posY, &posZ);

	posX += .5f;
	posY += .5f;
	posZ += .5f;

	// Le cast vers int marche juste pour les valeurs entiere, utiliser une fonction de la libc si besoin
	// de valeurs negatives
	int px = (int)(posX);
	int py = (int)(posY);
	int pz = (int)(posZ);

	bool found = false;
	float value = (m_player.GetPositon() - Vector3f((float)posX, (float)posY, (float)posZ)).Length();

	if (value <= MAX_SELECTION_DISTANCE)
	{
		// Apres avoir determine la position du bloc en utilisant la partie entiere du hit
		// point retourne par opengl, on doit verifier de chaque cote du bloc trouve pour trouver
		// le vrai bloc. Le vrai bloc peut etre different a cause d'erreurs de precision de nos
		// nombres flottants (si z = 14.999 par exemple, et qu'il n'y a pas de blocs a la position
		// 14 (apres arrondi vers l'entier) on doit trouver et retourner le bloc en position 15 s'il existe
		// A cause des erreurs de precisions, ils arrive que le cote d'un bloc qui doit pourtant etre a la
		// position 15 par exemple nous retourne plutot la position 15.0001
		for (int x = px - 1; !found && x <= px + 1; ++x)
		{
			for (int y = py - 1; !found && x >= 0 && y <= py + 1; ++y)
			{
				for (int z = pz - 1; !found && y >= 0 && z <= pz + 1; ++z)
				{
					if (z >= 0)
					{
						BlockType bt = BlockAt((float)x, (float)y, (float)z);
						if (bt == BTYPE_AIR || bt ==BTYPE_BEDROCK)
							continue;
						m_currentBlock.x = x;
						m_currentBlock.y = y;
						m_currentBlock.z = z;

						if (InRangeWithEpsilon<float>((float)posX, (float)x, (float)x + 1.f, 0.05f) && InRangeWithEpsilon<float>((float)posY, (float)y, (float)y + 1.f, 0.05f) && InRangeWithEpsilon<float>((float)posZ, (float)z, (float)z + 1.f, 0.05f))
						{
							found = true;
						}
					}
				}
			}
		}
	}

	if (!found)
	{
		m_currentBlock.x = -1;
	}
	else
	{
		// Find on which face of the bloc we got an hit
		m_currentFaceNormal.Zero();

		const float epsilon = 0.005f;

		// Front et back:
		if (EqualWithEpsilon<float>((float)posZ, (float)m_currentBlock.z, epsilon))
			m_currentFaceNormal.z = -1;
		else if (EqualWithEpsilon<float>((float)posZ, (float)m_currentBlock.z + 1.f, epsilon))
			m_currentFaceNormal.z = 1;
		else if (EqualWithEpsilon<float>((float)posX, (float)m_currentBlock.x, epsilon))
			m_currentFaceNormal.x = -1;
		else if (EqualWithEpsilon<float>((float)posX, (float)m_currentBlock.x + 1.f, epsilon))
			m_currentFaceNormal.x = 1;
		else if (EqualWithEpsilon<float>((float)posY, (float)m_currentBlock.y, epsilon))
			m_currentFaceNormal.y = -1;
		else if (EqualWithEpsilon<float>((float)posY, (float)m_currentBlock.y + 1.f, epsilon))
			m_currentFaceNormal.y = 1;
	}
}

void Engine::SetBlockAt(int x, int y, int z, BlockType bt)
{
	int chunkposy = static_cast<int>(z / CHUNK_SIZE_Z);
	int chunkposx = static_cast<int>(x / CHUNK_SIZE_X);

	if (x < 0 && z < 0)
	{
		chunkposx = static_cast<int>((x / CHUNK_SIZE_X) - 1);
		chunkposy = static_cast<int>((z / CHUNK_SIZE_Z) - 1);
	}
	else if (x < 0)
	{
		chunkposy = static_cast<int>(z / CHUNK_SIZE_Z);
		chunkposx = static_cast<int>((x / CHUNK_SIZE_X) - 1);
	}
	else if (z < 0)
	{
		chunkposy = static_cast<int>((z / CHUNK_SIZE_Z) - 1);
		chunkposx = static_cast<int>(x / CHUNK_SIZE_X);
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

		chunk->SetBlock(x - blockPositionX, y, z - blockPositionZ, bt);
	}
}


void Engine::UpdateChunks(int x, int y, int z)
{
	int chunkposy = static_cast<int>(z / CHUNK_SIZE_Z);
	int chunkposx = static_cast<int>(x / CHUNK_SIZE_X);

	if (x < 0 && z < 0)
	{
		chunkposx = static_cast<int>((x / CHUNK_SIZE_X) - 1);
		chunkposy = static_cast<int>((z / CHUNK_SIZE_Z) - 1);
	}
	else if (x < 0)
	{
		chunkposy = static_cast<int>(z / CHUNK_SIZE_Z);
		chunkposx = static_cast<int>((x / CHUNK_SIZE_X) - 1);
	}
	else if (z < 0)
	{
		chunkposy = static_cast<int>((z / CHUNK_SIZE_Z) - 1);
		chunkposx = static_cast<int>(x / CHUNK_SIZE_X);
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

		chunk->Update(chunkposx, chunkposy);
	}
}

