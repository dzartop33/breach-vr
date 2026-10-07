#include "gl_shaders.h"
#include <android/log.h>
#include <vector>

#define LOG_TAG "BreachVR_Shader"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace breachvr {

ShaderProgram::~ShaderProgram() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

GLuint ShaderProgram::CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0) {
            std::vector<char> infoLog(infoLen);
            glGetShaderInfoLog(shader, infoLen, nullptr, infoLog.data());
            LOGE("Shader compile error: %s", infoLog.data());
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool ShaderProgram::Compile(const char* vertSource, const char* fragSource) {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertSource);
    if (!vs) return false;

    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragSource);
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0) {
            std::vector<char> infoLog(infoLen);
            glGetProgramInfoLog(program_, infoLen, nullptr, infoLog.data());
            LOGE("Program link error: %s", infoLog.data());
        }
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }

    return true;
}

void ShaderProgram::SetMat4(const char* name, const Mat4& mat) const {
    GLint loc = glGetUniformLocation(program_, name);
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, mat.m);
    }
}

void ShaderProgram::SetVec3(const char* name, const Vec3& v) const {
    GLint loc = glGetUniformLocation(program_, name);
    if (loc >= 0) {
        glUniform3f(loc, v.x, v.y, v.z);
    }
}

void ShaderProgram::SetVec4(const char* name, const Vec4& v) const {
    GLint loc = glGetUniformLocation(program_, name);
    if (loc >= 0) {
        glUniform4f(loc, v.x, v.y, v.z, v.w);
    }
}

void ShaderProgram::SetFloat(const char* name, float v) const {
    GLint loc = glGetUniformLocation(program_, name);
    if (loc >= 0) {
        glUniform1f(loc, v);
    }
}

void ShaderProgram::SetInt(const char* name, int v) const {
    GLint loc = glGetUniformLocation(program_, name);
    if (loc >= 0) {
        glUniform1i(loc, v);
    }
}

ShaderLibrary& ShaderLibrary::Instance() {
    static ShaderLibrary instance;
    return instance;
}

bool ShaderLibrary::Init() {
    if (initialized_) return true;

    // 1. Lit Shader (PBR / Blinn-Phong CQB lighting with tactical flashlight & muzzle flash)
    const char* litVS = R"glsl(#version 300 es
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec4 aColor;
        layout(location = 3) in vec2 aUV;

        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProj;

        out vec3 vWorldPos;
        out vec3 vNormal;
        out vec4 vColor;
        out vec2 vUV;

        void main() {
            vec4 worldPos = uModel * vec4(aPosition, 1.0);
            vWorldPos = worldPos.xyz;
            vNormal = mat3(uModel) * aNormal;
            vColor = aColor;
            vUV = aUV;
            gl_Position = uProj * uView * worldPos;
        }
    )glsl";

    const char* litFS = R"glsl(#version 300 es
        precision mediump float;

        in vec3 vWorldPos;
        in vec3 vNormal;
        in vec4 vColor;
        in vec2 vUV;

        uniform vec4 uTint;
        uniform vec3 uFlashlightPos;
        uniform vec3 uFlashlightDir;
        uniform vec3 uMuzzleFlashPos;
        uniform float uMuzzleIntensity;

        out vec4 fragColor;

        void main() {
            vec3 N = normalize(vNormal);

            // Ambient tactical gloom
            vec3 ambient = vec3(0.18, 0.20, 0.25);

            // Subtle ceiling / ambient bounce
            vec3 skyDir = vec3(0.0, 1.0, 0.0);
            float skyDiff = max(dot(N, skyDir), 0.0) * 0.15;
            ambient += vec3(skyDiff);

            // Tactical weapon flashlight (Spotlight)
            vec3 lightToPos = vWorldPos - uFlashlightPos;
            float dist = length(lightToPos);
            vec3 L = -normalize(lightToPos);
            float spotCos = dot(L, -normalize(uFlashlightDir));
            float spotFactor = smoothstep(0.85, 0.96, spotCos);
            float atten = 1.0 / (1.0 + 0.15 * dist + 0.08 * dist * dist);
            float diffFlash = max(dot(N, L), 0.0);
            vec3 flashlightColor = vec3(1.0, 0.98, 0.92) * (diffFlash * spotFactor * atten * 3.5);

            // Muzzle flash omni light
            vec3 muzzleToPos = vWorldPos - uMuzzleFlashPos;
            float mDist = length(muzzleToPos);
            vec3 mL = normalize(-muzzleToPos);
            float mAtten = 1.0 / (1.0 + 0.8 * mDist + 1.5 * mDist * mDist);
            float mDiff = max(dot(N, mL), 0.0);
            vec3 muzzleColor = vec3(1.0, 0.7, 0.3) * (mDiff * mAtten * uMuzzleIntensity * 4.0);

            vec3 baseColor = vColor.rgb * uTint.rgb;
            vec3 finalColor = baseColor * (ambient + flashlightColor + muzzleColor);

            fragColor = vec4(finalColor, vColor.a * uTint.a);
        }
    )glsl";

    if (!litShader_.Compile(litVS, litFS)) {
        LOGE("Failed to compile Lit Shader");
        return false;
    }

    // 2. Wireframe / Holographic Tactical Visor Shader
    const char* wireVS = R"glsl(#version 300 es
        layout(location = 0) in vec3 aPosition;
        layout(location = 1) in vec3 aNormal;
        layout(location = 2) in vec4 aColor;

        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProj;

        out vec4 vColor;
        out vec3 vPos;

        void main() {
            vec4 worldPos = uModel * vec4(aPosition, 1.0);
            vPos = aPosition;
            vColor = aColor;
            gl_Position = uProj * uView * worldPos;
        }
    )glsl";

    const char* wireFS = R"glsl(#version 300 es
        precision mediump float;

        in vec4 vColor;
        in vec3 vPos;
        uniform vec4 uColor;
        uniform float uPulseTime;

        out vec4 fragColor;

        void main() {
            float pulse = 0.85 + 0.15 * sin(uPulseTime * 5.0 + vPos.y * 10.0);
            fragColor = vec4(uColor.rgb * vColor.rgb * pulse, uColor.a * vColor.a);
        }
    )glsl";

    if (!wireframeShader_.Compile(wireVS, wireFS)) {
        LOGE("Failed to compile Wireframe Shader");
        return false;
    }

    // 3. Additive Particle Shader (muzzle sparks, breach dust/fire)
    const char* partVS = R"glsl(#version 300 es
        layout(location = 0) in vec3 aPosition;
        layout(location = 2) in vec4 aColor;
        layout(location = 3) in vec2 aUV;

        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProj;

        out vec4 vColor;
        out vec2 vUV;

        void main() {
            vColor = aColor;
            vUV = aUV;
            gl_Position = uProj * uView * uModel * vec4(aPosition, 1.0);
        }
    )glsl";

    const char* partFS = R"glsl(#version 300 es
        precision mediump float;

        in vec4 vColor;
        in vec2 vUV;
        uniform vec4 uColor;

        out vec4 fragColor;

        void main() {
            // Soft circular falloff
            float d = length(vUV - vec2(0.5));
            float alpha = smoothstep(0.5, 0.0, d);
            fragColor = vec4(vColor.rgb * uColor.rgb, vColor.a * uColor.a * alpha);
        }
    )glsl";

    if (!particleShader_.Compile(partVS, partFS)) {
        LOGE("Failed to compile Particle Shader");
        return false;
    }

    // 4. Flat HUD Shader
    const char* hudVS = R"glsl(#version 300 es
        layout(location = 0) in vec3 aPosition;
        layout(location = 2) in vec4 aColor;
        layout(location = 3) in vec2 aUV;

        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProj;

        out vec4 vColor;
        out vec2 vUV;

        void main() {
            vColor = aColor;
            vUV = aUV;
            gl_Position = uProj * uView * uModel * vec4(aPosition, 1.0);
        }
    )glsl";

    const char* hudFS = R"glsl(#version 300 es
        precision mediump float;

        in vec4 vColor;
        in vec2 vUV;
        uniform vec4 uColor;

        out vec4 fragColor;

        void main() {
            fragColor = vColor * uColor;
        }
    )glsl";

    if (!hudShader_.Compile(hudVS, hudFS)) {
        LOGE("Failed to compile HUD Shader");
        return false;
    }

    initialized_ = true;
    LOGI("ShaderLibrary initialized successfully");
    return true;
}

void ShaderLibrary::Teardown() {
    initialized_ = false;
}

} // namespace breachvr
