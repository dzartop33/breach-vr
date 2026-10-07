#pragma once
// Breach VR - OpenGL ES 3.0 Shaders

#include <GLES3/gl3.h>
#include "math_types.h"

namespace breachvr {

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    bool Compile(const char* vertSource, const char* fragSource);
    void Use() const { glUseProgram(program_); }
    GLuint GetProgram() const { return program_; }

    void SetMat4(const char* name, const Mat4& mat) const;
    void SetVec3(const char* name, const Vec3& v) const;
    void SetVec4(const char* name, const Vec4& v) const;
    void SetFloat(const char* name, float v) const;
    void SetInt(const char* name, int v) const;

private:
    GLuint program_ = 0;
    static GLuint CompileShader(GLenum type, const char* source);
};

class ShaderLibrary {
public:
    static ShaderLibrary& Instance();
    bool Init();
    void Teardown();

    const ShaderProgram& Lit() const { return litShader_; }
    const ShaderProgram& Wireframe() const { return wireframeShader_; }
    const ShaderProgram& Particle() const { return particleShader_; }
    const ShaderProgram& HUD() const { return hudShader_; }

private:
    ShaderLibrary() = default;
    ShaderProgram litShader_;
    ShaderProgram wireframeShader_;
    ShaderProgram particleShader_;
    ShaderProgram hudShader_;
    bool initialized_ = false;
};

} // namespace breachvr
