#ifndef DEFINE_H__
#define DEFINE_H__

#include <GL/glew.h>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#ifdef _WIN32
#include <windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#else

#endif

#define CHUNK_SIZE_X 16
#define CHUNK_SIZE_Y 128
#define CHUNK_SIZE_Z 16

typedef uint8_t BlockType; enum BLOCK_TYPE { BTYPE_AIR, BTYPE_DIRT, BTYPE_GRASS, BTYPE_CHECKER };

#define TEXTURE_PATH        "../BlockAdventure/media/textures/"
#define SHADER_PATH			"../BlockAdventure/media/shaders/"
#define VIEW_DISTANCE       128

#endif // DEFINE_H__
