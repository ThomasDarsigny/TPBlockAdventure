#include "shader.h"
#include "define.h"
#include "tool.h"
#include <iostream>
#include <vector>

Shader::Shader() : m_program(0), m_vertexShader(0), m_fragmentShader(0), m_valid(false)
{
}

Shader::~Shader()
{
}

void Shader::Destroy()
{
    if (m_program)
    {
        if (m_vertexShader)   { glDetachShader(m_program, m_vertexShader);   glDeleteShader(m_vertexShader); }
        if (m_fragmentShader) { glDetachShader(m_program, m_fragmentShader); glDeleteShader(m_fragmentShader); }
        glDeleteProgram(m_program);
    }
    m_program = m_vertexShader = m_fragmentShader = 0;
    m_valid = false;
    m_uniforms.clear();
}

bool Shader::Load(const std::string& vertFile, const std::string& fragFile, bool verbose)
{
    Destroy();

    std::string vertexShader, fragmentShader;

    if (!Tool::LoadTextFile(vertFile, vertexShader))
    {
        std::cerr << "[Shader] Fichier introuvable: " << vertFile << std::endl;
        return false;
    }
    if (!Tool::LoadTextFile(fragFile, fragmentShader))
    {
        std::cerr << "[Shader] Fichier introuvable: " << fragFile << std::endl;
        return false;
    }

    const char* vsrc = vertexShader.c_str();
    const char* fsrc = fragmentShader.c_str();

    m_program = glCreateProgram();
    m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(m_vertexShader, 1, &vsrc, NULL);
    glShaderSource(m_fragmentShader, 1, &fsrc, NULL);

    glCompileShader(m_vertexShader);
    if (!CheckShaderError(m_vertexShader, vertFile, verbose))
        return false;

    glCompileShader(m_fragmentShader);
    if (!CheckShaderError(m_fragmentShader, fragFile, verbose))
        return false;

    glAttachShader(m_program, m_vertexShader);
    glAttachShader(m_program, m_fragmentShader);
    glLinkProgram(m_program);

    if (!CheckProgramError(m_program, verbose))
        return false;

    m_valid = true;
    return true;
}

void Shader::Use() const
{
    glUseProgram(m_program);
}

void Shader::Disable()
{
    glUseProgram(0);
}

GLint Shader::Uniform(const std::string& name) const
{
    std::map<std::string, GLint>::const_iterator it = m_uniforms.find(name);
    if (it != m_uniforms.end())
        return it->second;

    const GLint loc = glGetUniformLocation(m_program, name.c_str());
    m_uniforms[name] = loc;
    return loc;
}

void Shader::SetInt(const std::string& name, int v) const
{
    const GLint loc = Uniform(name);
    if (loc >= 0) glUniform1i(loc, v);
}

void Shader::SetFloat(const std::string& name, float v) const
{
    const GLint loc = Uniform(name);
    if (loc >= 0) glUniform1f(loc, v);
}

void Shader::SetVec2(const std::string& name, float x, float y) const
{
    const GLint loc = Uniform(name);
    if (loc >= 0) glUniform2f(loc, x, y);
}

void Shader::SetVec3(const std::string& name, float x, float y, float z) const
{
    const GLint loc = Uniform(name);
    if (loc >= 0) glUniform3f(loc, x, y, z);
}

void Shader::SetVec4(const std::string& name, float x, float y, float z, float w) const
{
    const GLint loc = Uniform(name);
    if (loc >= 0) glUniform4f(loc, x, y, z, w);
}

void Shader::UpdateIntUniform(GLint loc, GLint value) const
{
    if (loc >= 0) glUniform1i(loc, value);
}

void Shader::UpdateFloatUniform(GLint loc, GLfloat value) const
{
    if (loc >= 0) glUniform1f(loc, value);
}

bool Shader::CheckShaderError(GLuint shader, const std::string& what, bool verbose)
{
    GLint compileOk = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileOk);

    if (!compileOk || verbose)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        if (maxLength > 1)
        {
            std::vector<char> log(maxLength + 1, 0);
            glGetShaderInfoLog(shader, maxLength, &maxLength, log.data());
            if (!compileOk)
                std::cerr << "[Shader] Erreur de compilation (" << what << "):\n" << log.data() << std::endl;
        }
    }

    return compileOk == GL_TRUE;
}

bool Shader::CheckProgramError(GLuint program, bool verbose)
{
    GLint linkOk = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linkOk);

    if (!linkOk)
    {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<char> log(maxLength + 1, 0);
        if (maxLength > 1)
            glGetProgramInfoLog(program, maxLength, &maxLength, log.data());
        std::cerr << "[Shader] Erreur d'edition de liens:\n" << log.data() << std::endl;
    }

    return linkOk == GL_TRUE;
}
