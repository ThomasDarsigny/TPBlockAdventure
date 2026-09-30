#ifndef SHADER_H__
#define SHADER_H__

#include <string>
#include <map>
#include "define.h"

class Shader
{
public:
    Shader();
    ~Shader();

    bool Load(const std::string& vertFile, const std::string& fragFile, bool verbose = false);
    void Destroy();

    bool IsValid() const { return m_valid; }

    void Use() const;
    static void Disable();

    // Les emplacements d'uniformes sont mis en cache: on peut donc les
    // adresser par leur nom a chaque frame sans surcout notable.
    void SetInt(const std::string& name, int v) const;
    void SetFloat(const std::string& name, float v) const;
    void SetVec2(const std::string& name, float x, float y) const;
    void SetVec3(const std::string& name, float x, float y, float z) const;
    void SetVec4(const std::string& name, float x, float y, float z, float w) const;

    GLint Uniform(const std::string& name) const;

    // Ancienne interface, conservee pour compatibilite
    GLint BindIntUniform(const std::string& name) const { return Uniform(name); }
    void UpdateIntUniform(GLint loc, GLint value) const;
    void UpdateFloatUniform(GLint loc, GLfloat value) const;

private:
    bool CheckShaderError(GLuint shader, const std::string& what, bool verbose);
    bool CheckProgramError(GLuint program, bool verbose);

private:
    GLuint m_program;
    GLuint m_vertexShader;
    GLuint m_fragmentShader;
    bool   m_valid;

    mutable std::map<std::string, GLint> m_uniforms;
};

#endif // SHADER_H__
