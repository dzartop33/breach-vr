#pragma once
// Breach VR - Procedural 3D Mesh Generation & OpenGL ES 3.0 Geometry

#include <vector>
#include <memory>
#include <cstddef>
#include <GLES3/gl3.h>
#include "math_types.h"

namespace breachvr {

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec4 color;
    Vec2 uv;

    Vertex() = default;
    Vertex(const Vec3& p, const Vec3& n, const Vec4& c, const Vec2& u)
        : position(p), normal(n), color(c), uv(u) {}
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    void Upload(const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices, GLenum drawMode = GL_TRIANGLES);
    void Draw() const;

    bool IsValid() const { return vao_ != 0 && indexCount_ > 0; }
    size_t GetIndexCount() const { return indexCount_; }

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    GLsizei indexCount_ = 0;
    GLenum drawMode_ = GL_TRIANGLES;
};

class MeshBuilder {
public:
    static std::unique_ptr<Mesh> CreateBox(float sx, float sy, float sz, const Vec4& color);
    static std::unique_ptr<Mesh> CreateCylinder(float radius, float height, int segments, const Vec4& color);
    static std::unique_ptr<Mesh> CreateSphere(float radius, int rings, int sectors, const Vec4& color);
    static std::unique_ptr<Mesh> CreateQuad(float width, float height, const Vec4& color);
    static std::unique_ptr<Mesh> CreateWireBox(float sx, float sy, float sz, const Vec4& color);
    static std::unique_ptr<Mesh> CreateCarbineMesh();
    static std::unique_ptr<Mesh> CreatePistolMesh();
    static std::unique_ptr<Mesh> CreateBreachChargeMesh();
    static std::unique_ptr<Mesh> CreateFlashbangMesh();
    static std::unique_ptr<Mesh> CreateEnemyMesh();
    static std::unique_ptr<Mesh> CreateHandJointMesh();
    static std::unique_ptr<Mesh> CreateLaserBeamMesh(float length);
};

} // namespace breachvr
