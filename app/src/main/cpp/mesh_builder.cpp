#include "mesh_builder.h"
#include <cmath>

namespace breachvr {

Mesh::~Mesh() {
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

void Mesh::Upload(const std::vector<Vertex>& vertices, const std::vector<uint16_t>& indices, GLenum drawMode) {
    drawMode_ = drawMode;
    indexCount_ = static_cast<GLsizei>(indices.size());

    if (vao_ == 0) glGenVertexArrays(1, &vao_);
    if (vbo_ == 0) glGenBuffers(1, &vbo_);
    if (ebo_ == 0) glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint16_t), indices.data(), GL_STATIC_DRAW);

    // Layout 0: Position (Vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

    // Layout 1: Normal (Vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    // Layout 2: Color (Vec4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

    // Layout 3: UV (Vec2)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

    glBindVertexArray(0);
}

void Mesh::Draw() const {
    if (vao_ && indexCount_ > 0) {
        glBindVertexArray(vao_);
        glDrawElements(drawMode_, indexCount_, GL_UNSIGNED_SHORT, nullptr);
        glBindVertexArray(0);
    }
}

static void AppendBox(std::vector<Vertex>& verts, std::vector<uint16_t>& indices,
                      const Vec3& center, const Vec3& size, const Vec4& color) {
    uint16_t baseIdx = static_cast<uint16_t>(verts.size());
    float hx = size.x * 0.5f, hy = size.y * 0.5f, hz = size.z * 0.5f;

    struct Face {
        Vec3 n;
        Vec3 p[4];
    } faces[6] = {
        // Front (+Z)
        { Vec3(0,0,1), { Vec3(-hx,-hy, hz), Vec3( hx,-hy, hz), Vec3( hx, hy, hz), Vec3(-hx, hy, hz) } },
        // Back (-Z)
        { Vec3(0,0,-1),{ Vec3( hx,-hy,-hz), Vec3(-hx,-hy,-hz), Vec3(-hx, hy,-hz), Vec3( hx, hy,-hz) } },
        // Top (+Y)
        { Vec3(0,1,0), { Vec3(-hx, hy, hz), Vec3( hx, hy, hz), Vec3( hx, hy,-hz), Vec3(-hx, hy,-hz) } },
        // Bottom (-Y)
        { Vec3(0,-1,0),{ Vec3(-hx,-hy,-hz), Vec3( hx,-hy,-hz), Vec3( hx,-hy, hz), Vec3(-hx,-hy, hz) } },
        // Right (+X)
        { Vec3(1,0,0), { Vec3( hx,-hy, hz), Vec3( hx,-hy,-hz), Vec3( hx, hy,-hz), Vec3( hx, hy, hz) } },
        // Left (-X)
        { Vec3(-1,0,0),{ Vec3(-hx,-hy,-hz), Vec3(-hx,-hy, hz), Vec3(-hx, hy, hz), Vec3(-hx, hy,-hz) } }
    };

    for (int f = 0; f < 6; ++f) {
        uint16_t start = static_cast<uint16_t>(verts.size());
        verts.emplace_back(center + faces[f].p[0], faces[f].n, color, Vec2(0, 0));
        verts.emplace_back(center + faces[f].p[1], faces[f].n, color, Vec2(1, 0));
        verts.emplace_back(center + faces[f].p[2], faces[f].n, color, Vec2(1, 1));
        verts.emplace_back(center + faces[f].p[3], faces[f].n, color, Vec2(0, 1));

        indices.push_back(start + 0);
        indices.push_back(start + 1);
        indices.push_back(start + 2);
        indices.push_back(start + 0);
        indices.push_back(start + 2);
        indices.push_back(start + 3);
    }
}

std::unique_ptr<Mesh> MeshBuilder::CreateBox(float sx, float sy, float sz, const Vec4& color) {
    auto mesh = std::make_unique<Mesh>();
    std::vector<Vertex> verts;
    std::vector<uint16_t> indices;
    AppendBox(verts, indices, Vec3(0, 0, 0), Vec3(sx, sy, sz), color);
    mesh->Upload(verts, indices);
    return mesh;
}

std::unique_ptr<Mesh> MeshBuilder::CreateCylinder(float radius, float height, int segments, const Vec4& color) {
    auto mesh = std::make_unique<Mesh>();
    std::vector<Vertex> verts;
    std::vector<uint16_t> indices;

    float halfH = height * 0.5f;

    // Side vertices
    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / segments * 2.0f * PI;
        float cosT = std::cos(theta);
        float sinT = std::sin(theta);
        Vec3 norm(cosT, 0, sinT);

        verts.emplace_back(Vec3(radius * cosT, -halfH, radius * sinT), norm, color, Vec2((float)i / segments, 0));
        verts.emplace_back(Vec3(radius * cosT,  halfH, radius * sinT), norm, color, Vec2((float)i / segments, 1));
    }

    for (int i = 0; i < segments; ++i) {
        uint16_t i0 = i * 2;
        uint16_t i1 = i0 + 1;
        uint16_t i2 = i0 + 2;
        uint16_t i3 = i0 + 3;

        indices.push_back(i0);
        indices.push_back(i2);
        indices.push_back(i1);

        indices.push_back(i1);
        indices.push_back(i2);
        indices.push_back(i3);
    }

    // Top & Bottom caps
    uint16_t topCenterIdx = static_cast<uint16_t>(verts.size());
    verts.emplace_back(Vec3(0, halfH, 0), Vec3(0, 1, 0), color, Vec2(0.5f, 0.5f));
    uint16_t bottomCenterIdx = static_cast<uint16_t>(verts.size());
    verts.emplace_back(Vec3(0, -halfH, 0), Vec3(0, -1, 0), color, Vec2(0.5f, 0.5f));

    for (int i = 0; i < segments; ++i) {
        float t0 = (float)i / segments * 2.0f * PI;
        float t1 = (float)(i + 1) / segments * 2.0f * PI;

        uint16_t v0 = static_cast<uint16_t>(verts.size());
        verts.emplace_back(Vec3(radius * std::cos(t0), halfH, radius * std::sin(t0)), Vec3(0, 1, 0), color, Vec2(0.5f + 0.5f * std::cos(t0), 0.5f + 0.5f * std::sin(t0)));
        verts.emplace_back(Vec3(radius * std::cos(t1), halfH, radius * std::sin(t1)), Vec3(0, 1, 0), color, Vec2(0.5f + 0.5f * std::cos(t1), 0.5f + 0.5f * std::sin(t1)));
        indices.push_back(topCenterIdx);
        indices.push_back(v0);
        indices.push_back(v0 + 1);

        uint16_t b0 = static_cast<uint16_t>(verts.size());
        verts.emplace_back(Vec3(radius * std::cos(t0), -halfH, radius * std::sin(t0)), Vec3(0, -1, 0), color, Vec2(0.5f + 0.5f * std::cos(t0), 0.5f + 0.5f * std::sin(t0)));
        verts.emplace_back(Vec3(radius * std::cos(t1), -halfH, radius * std::sin(t1)), Vec3(0, -1, 0), color, Vec2(0.5f + 0.5f * std::cos(t1), 0.5f + 0.5f * std::sin(t1)));
        indices.push_back(bottomCenterIdx);
        indices.push_back(b0 + 1);
        indices.push_back(b0);
    }

    mesh->Upload(verts, indices);
    return mesh;
}

std::unique_ptr<Mesh> MeshBuilder::CreateSphere(float radius, int rings, int sectors, const Vec4& color) {
    auto mesh = std::make_unique<Mesh>();
    std::vector<Vertex> verts;
    std::vector<uint16_t> indices;

    for (int r = 0; r <= rings; ++r) {
        float phi = (float)r / rings * PI;
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (int s = 0; s <= sectors; ++s) {
            float theta = (float)s / sectors * 2.0f * PI;
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            Vec3 norm(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
            Vec3 pos = norm * radius;
            Vec2 uv((float)s / sectors, (float)r / rings);
            verts.emplace_back(pos, norm, color, uv);
        }
    }

    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            uint16_t i0 = r * (sectors + 1) + s;
            uint16_t i1 = i0 + sectors + 1;

            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i0 + 1);

            indices.push_back(i0 + 1);
            indices.push_back(i1);
            indices.push_back(i1 + 1);
        }
    }

    mesh->Upload(verts, indices);
    return mesh;
}

std::unique_ptr<Mesh> MeshBuilder::CreateQuad(float width, float height, const Vec4& color) {
    auto mesh = std::make_unique<Mesh>();
    std::vector<Vertex> verts;
    std::vector<uint16_t> indices;

    float hw = width * 0.5f;
    float hh = height * 0.5f;

    verts.emplace_back(Vec3(-hw, -hh, 0), Vec3(0, 0, 1), color, Vec2(0, 0));
    verts.emplace_back(Vec3( hw, -hh, 0), Vec3(0, 0, 1), color, Vec2(1, 0));
    verts.emplace_back(Vec3( hw,  hh, 0), Vec3(0, 0, 1), color, Vec2(1, 1));
    verts.emplace_back(Vec3(-hw,  hh, 0), Vec3(0, 0, 1), color, Vec2(0, 1));

    indices = { 0, 1, 2, 0, 2, 3 };
    mesh->Upload(verts, indices);
    return mesh;
}

std::unique_ptr<Mesh> MeshBuilder::CreateWireBox(float sx, float sy, float sz, const Vec4& color) {
    auto mesh = std::make_unique<Mesh>();
    std::vector<Vertex> verts;
    std::vector<uint16_t> indices;

    float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
    Vec3 p[8] = {
        Vec3(-hx,-hy,-hz), Vec3( hx,-hy,-hz), Vec3( hx, hy,-hz), Vec3(-hx, hy,-hz),
        Vec3(-hx,-hy, hz), Vec3( hx,-hy, hz), Vec3( hx, hy, hz), Vec3(-hx, hy, hz)
    };

    for (int i = 0; i < 8; ++i) {
        verts.emplace_back(p[i], Vec3(0, 1, 0), color, Vec2(0, 0));
    }

    indices = {
        0,1, 1,2, 2,3, 3,0, // Back
        4,5, 5,6, 6,7, 7,4, // Front
        0,4, 1,5, 2,6, 3,7  // Connectors
    };

    mesh->Upload(verts, indices, GL_LINES);
    return mesh;
}

// Tactical Carbine MK18
std::unique_ptr<Mesh> MeshBuilder::CreateCarbineMesh() {
    auto mesh = std::make_unique<Mesh>();
    std::vector<Vertex> verts;
    std::vector<uint16_t> indices;

    Vec4 metalBlack(0.12f, 0.13f, 0.15f, 1.0f);
    Vec4 darkGrey(0.20f, 0.22f, 0.24f, 1.0f);
    Vec4 tanCamo(0.38f, 0.34f, 0.26f, 1.0f);
    Vec4 redDotSight(0.9f, 0.1f, 0.1f, 1.0f);

    // Receiver (center)
    AppendBox(verts, indices, Vec3(0.0f, 0.0f, -0.05f), Vec3(0.045f, 0.06f, 0.24f), metalBlack);
    // Lower receiver & grip
    AppendBox(verts, indices, Vec3(0.0f, -0.07f, -0.08f), Vec3(0.038f, 0.10f, 0.05f), darkGrey);
    // Curved tactical magazine
    AppendBox(verts, indices, Vec3(0.0f, -0.09f, 0.02f), Vec3(0.032f, 0.14f, 0.06f), tanCamo);
    // Handguard / Quad Rail
    AppendBox(verts, indices, Vec3(0.0f, 0.005f, 0.16f), Vec3(0.048f, 0.05f, 0.20f), tanCamo);
    // Barrel extending forward
    AppendBox(verts, indices, Vec3(0.0f, 0.015f, 0.30f), Vec3(0.02f, 0.02f, 0.14f), metalBlack);
    // Muzzle brake / Flash hider
    AppendBox(verts, indices, Vec3(0.0f, 0.015f, 0.38f), Vec3(0.025f, 0.025f, 0.04f), metalBlack);
    // Tactical Stock (rear)
    AppendBox(verts, indices, Vec3(0.0f, -0.01f, -0.24f), Vec3(0.04f, 0.09f, 0.18f), darkGrey);
    // Holographic Sight Mount & Hood
    AppendBox(verts, indices, Vec3(0.0f, 0.055f, -0.02f), Vec3(0.04f, 0.045f, 0.09f), metalBlack);
    // Holographic optic glass element
    AppendBox(verts, indices, Vec3(0.0f, 0.055f, -0.02f), Vec3(0.025f, 0.028f, 0.005f), redDotSight);

    mesh->Upload(verts, indices);
    return mesh;
}

// Suppressed Tactical Pistol P10
std::unique_ptr<Mesh> MeshBuilder::CreatePistolMesh() {
    auto mesh = std::make_unique<Mesh>();
    std::vector<Vertex> verts;
    std::vector<uint16_t> indices;

    Vec4 slideBlack(0.15f, 0.16f, 0.18f, 1.0f);
    Vec4 gripPolymer(0.10f, 0.10f, 0.11f, 1.0f);
    Vec4 suppressorColor(0.08f, 0.08f, 0.09f, 1.0f);
    Vec4 laserEmitter(0.1f, 0.9f, 0.2f, 1.0f);

    // Slide
    AppendBox(verts, indices, Vec3(0.0f, 0.02f, 0.04f), Vec3(0.03f, 0.035f, 0.16f), slideBlack);
    // Pistol Grip
    AppendBox(verts, indices, Vec3(0.0f, -0.055f, 0.0f), Vec3(0.028f, 0.11f, 0.045f), gripPolymer);
    // Trigger guard
    AppendBox(verts, indices, Vec3(0.0f, -0.015f, 0.045f), Vec3(0.015f, 0.03f, 0.04f), gripPolymer);
    // Tactical Suppressor (front)
    AppendBox(verts, indices, Vec3(0.0f, 0.022f, 0.19f), Vec3(0.036f, 0.036f, 0.15f), suppressorColor);
    // Under-barrel laser module
    AppendBox(verts, indices, Vec3(0.0f, -0.008f, 0.09f), Vec3(0.024f, 0.02f, 0.05f), slideBlack);
    // Laser diode lens
    AppendBox(verts, indices, Vec3(0.0f, -0.008f, 0.116f), Vec3(0.008f, 0.008f, 0.004f), laserEmitter);

    mesh->Upload(verts, indices);
    return mesh;
}

// Breach C4 Explosive Pack
std::unique_ptr<Mesh> MeshBuilder::CreateBreachChargeMesh() {
    auto mesh = std::make_unique<Mesh>();
    std::vector<Vertex> verts;
    std::vector<uint16_t> indices;

    Vec4 c4Clay(0.72f, 0.68f, 0.58f, 1.0f);
    Vec4 tapeBlack(0.15f, 0.15f, 0.15f, 1.0f);
    Vec4 detonatorBody(0.2f, 0.45f, 0.2f, 1.0f);
    Vec4 ledRed(1.0f, 0.1f, 0.1f, 1.0f);

    // C4 main block
    AppendBox(verts, indices, Vec3(0, 0, 0), Vec3(0.18f, 0.12f, 0.04f), c4Clay);
    // Securing military tape bands
    AppendBox(verts, indices, Vec3(-0.06f, 0, 0.002f), Vec3(0.03f, 0.122f, 0.042f), tapeBlack);
    AppendBox(verts, indices, Vec3( 0.06f, 0, 0.002f), Vec3(0.03f, 0.122f, 0.042f), tapeBlack);
    // Detonator electronics
    AppendBox(verts, indices, Vec3(0, 0.01f, 0.028f), Vec3(0.07f, 0.06f, 0.02f), detonatorBody);
    // Blinking armed LED
    AppendBox(verts, indices, Vec3(0.02f, 0.025f, 0.04f), Vec3(0.012f, 0.012f, 0.01f), ledRed);

    mesh->Upload(verts, indices);
    return mesh;
}

// M84 Stun Flashbang Grenade
std::unique_ptr<Mesh> MeshBuilder::CreateFlashbangMesh() {
    auto mesh = std::make_unique<Mesh>();
    std::vector<Vertex> verts;
    std::vector<uint16_t> indices;

    Vec4 canisterSteel(0.45f, 0.48f, 0.50f, 1.0f);
    Vec4 topFuze(0.25f, 0.25f, 0.25f, 1.0f);
    Vec4 safetyLever(0.35f, 0.55f, 0.25f, 1.0f);
    Vec4 pullRing(0.75f, 0.75f, 0.75f, 1.0f);

    // Perforated cylindrical canister
    AppendBox(verts, indices, Vec3(0, 0, 0), Vec3(0.042f, 0.11f, 0.042f), canisterSteel);
    // Fuze assembly
    AppendBox(verts, indices, Vec3(0, 0.07f, 0), Vec3(0.025f, 0.035f, 0.025f), topFuze);
    // Curved spoon / lever
    AppendBox(verts, indices, Vec3(0.018f, 0.035f, 0), Vec3(0.008f, 0.09f, 0.015f), safetyLever);
    // Pull ring
    AppendBox(verts, indices, Vec3(-0.02f, 0.075f, 0), Vec3(0.022f, 0.022f, 0.005f), pullRing);

    mesh->Upload(verts, indices);
    return mesh;
}

// CQB Hostile Character
std::unique_ptr<Mesh> MeshBuilder::CreateEnemyMesh() {
    auto mesh = std::make_unique<Mesh>();
    std::vector<Vertex> verts;
    std::vector<uint16_t> indices;

    Vec4 darkFatigues(0.22f, 0.24f, 0.22f, 1.0f);
    Vec4 tacticalVest(0.15f, 0.15f, 0.16f, 1.0f);
    Vec4 balaclava(0.12f, 0.12f, 0.12f, 1.0f);
    Vec4 skinTone(0.80f, 0.65f, 0.55f, 1.0f);
    Vec4 boots(0.08f, 0.08f, 0.08f, 1.0f);

    // Torso / Body (y: 0.9m to 1.4m)
    AppendBox(verts, indices, Vec3(0, 1.15f, 0), Vec3(0.38f, 0.50f, 0.22f), darkFatigues);
    // Tactical Body Armor / Plate Carrier
    AppendBox(verts, indices, Vec3(0, 1.18f, 0.01f), Vec3(0.36f, 0.42f, 0.24f), tacticalVest);
    // Head & Balaclava (y: 1.55m)
    AppendBox(verts, indices, Vec3(0, 1.55f, 0), Vec3(0.20f, 0.24f, 0.20f), balaclava);
    // Tactical Goggles / Visor
    AppendBox(verts, indices, Vec3(0, 1.58f, 0.105f), Vec3(0.16f, 0.06f, 0.03f), Vec4(0.1f, 0.1f, 0.1f, 1.0f));
    // Left & Right Legs
    AppendBox(verts, indices, Vec3(-0.11f, 0.45f, 0), Vec3(0.14f, 0.90f, 0.16f), darkFatigues);
    AppendBox(verts, indices, Vec3( 0.11f, 0.45f, 0), Vec3(0.14f, 0.90f, 0.16f), darkFatigues);
    // Combat Boots
    AppendBox(verts, indices, Vec3(-0.11f, 0.06f, 0.03f), Vec3(0.15f, 0.12f, 0.24f), boots);
    AppendBox(verts, indices, Vec3( 0.11f, 0.06f, 0.03f), Vec3(0.15f, 0.12f, 0.24f), boots);
    // Left & Right Arms (aiming forward CQB pose)
    AppendBox(verts, indices, Vec3(-0.25f, 1.15f, 0.15f), Vec3(0.12f, 0.12f, 0.42f), darkFatigues);
    AppendBox(verts, indices, Vec3( 0.20f, 1.15f, 0.20f), Vec3(0.12f, 0.12f, 0.38f), darkFatigues);
    // Hostile AK Carbine in hands
    AppendBox(verts, indices, Vec3(0.02f, 1.15f, 0.45f), Vec3(0.04f, 0.08f, 0.50f), Vec4(0.18f, 0.12f, 0.08f, 1.0f));

    mesh->Upload(verts, indices);
    return mesh;
}

// Hand Joint Tracking Spheres
std::unique_ptr<Mesh> MeshBuilder::CreateHandJointMesh() {
    return CreateSphere(0.012f, 6, 8, Vec4(0.2f, 0.8f, 0.9f, 0.85f));
}

// Weapon Red Laser Beam
std::unique_ptr<Mesh> MeshBuilder::CreateLaserBeamMesh(float length) {
    auto mesh = std::make_unique<Mesh>();
    std::vector<Vertex> verts;
    std::vector<uint16_t> indices;

    Vec4 laserRed(1.0f, 0.05f, 0.05f, 0.9f);
    verts.emplace_back(Vec3(0, 0, 0), Vec3(0, 1, 0), laserRed, Vec2(0, 0));
    verts.emplace_back(Vec3(0, 0, length), Vec3(0, 1, 0), laserRed, Vec2(1, 1));
    indices = { 0, 1 };

    mesh->Upload(verts, indices, GL_LINES);
    return mesh;
}

} // namespace breachvr
