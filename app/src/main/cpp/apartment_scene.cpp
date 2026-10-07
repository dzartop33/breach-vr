#include "apartment_scene.h"
#include "audio_engine.h"
#include <android/log.h>
#include <cmath>

#define LOG_TAG "BreachVR_Scene"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace breachvr {

AABB TacticalDoor::GetAABB() const {
    Vec3 half(width * 0.5f, height * 0.5f, thickness * 0.5f);
    return AABB(position - half, position + half);
}

void TacticalDoor::BreachExplode() {
    state = DoorState::BlownOffHinges;
    health = 0.0f;
    openAngle = PI * 0.55f;
    isC4Attached = false;
    AudioEngine::Instance().PlayBreachExplosion();
}

void TacticalDoor::Kick() {
    if (state == DoorState::BlownOffHinges) return;
    health -= 50.0f;
    AudioEngine::Instance().PlayDoorKick();
    if (health <= 0.0f) {
        state = DoorState::Open;
        openAngle = PI * 0.5f;
    }
}

AABB CoverObstacle::GetAABB() const {
    Vec3 half = size * 0.5f;
    return AABB(position - half, position + half);
}

bool ApartmentScene::Init(XrInstance instance, XrSession session) {
    if (!QuerySpatialEntities(instance, session)) {
        LOGI("Room scan spatial entities not active; generating procedural tactical apartment layout");
        BuildProceduralApartment();
    }

    BuildGeometryMeshes();
    LOGI("ApartmentScene initialized successfully (%zu rooms, %zu doors, %zu cover obstacles)",
         rooms_.size(), doors_.size(), obstacles_.size());
    return true;
}

bool ApartmentScene::QuerySpatialEntities(XrInstance /*instance*/, XrSession /*session*/) {
    // Spatial entity query hook (XR_FB_scene / spatial entities).
    // On Quest 3S, when the user performs Room Setup, spatial anchors define
    // walls, floors, and furniture.
    usedScannedApartment_ = false;
    return false;
}

void ApartmentScene::BuildProceduralApartment() {
    doors_.clear();
    obstacles_.clear();
    rooms_.clear();

    // 1. Room Zones
    // Room 0: Entry Landing (Breach Starting Point)
    RoomZone r0;
    r0.id = 0;
    r0.name = "Stairwell Landing";
    r0.bounds = AABB(Vec3(-2.0f, 0, -4.5f), Vec3(2.0f, 2.8f, -1.5f));
    r0.isCleared = true;
    rooms_.push_back(r0);

    // Room 1: Living Room / CQB Main Area
    RoomZone r1;
    r1.id = 1;
    r1.name = "Living Room";
    r1.bounds = AABB(Vec3(-3.5f, 0, -1.5f), Vec3(3.5f, 2.8f, 3.5f));
    r1.isCleared = false;
    rooms_.push_back(r1);

    // Room 2: Kitchen & Utility
    RoomZone r2;
    r2.id = 2;
    r2.name = "Kitchen";
    r2.bounds = AABB(Vec3(3.5f, 0, -1.5f), Vec3(7.0f, 2.8f, 3.5f));
    r2.isCleared = false;
    rooms_.push_back(r2);

    // Room 3: Stronghold / Bedroom
    RoomZone r3;
    r3.id = 3;
    r3.name = "Hostile Stronghold";
    r3.bounds = AABB(Vec3(-3.5f, 0, 3.5f), Vec3(2.5f, 2.8f, 8.0f));
    r3.isCleared = false;
    rooms_.push_back(r3);

    // 2. Tactical Doors
    // Door 0: Entry Apartment Door (Breach point)
    TacticalDoor d0;
    d0.id = 0;
    d0.name = "Apartment Entrance";
    d0.position = Vec3(0.0f, 1.075f, -1.5f);
    d0.state = DoorState::Locked;
    doors_.push_back(d0);

    // Door 1: Kitchen Doorway
    TacticalDoor d1;
    d1.id = 1;
    d1.name = "Kitchen Access";
    d1.position = Vec3(3.5f, 1.075f, 1.0f);
    d1.orientation = Quat::FromAxisAngle(Vec3(0, 1, 0), PI * 0.5f);
    d1.state = DoorState::Locked;
    doors_.push_back(d1);

    // Door 2: Stronghold Bedroom Door
    TacticalDoor d2;
    d2.id = 2;
    d2.name = "Stronghold Door";
    d2.position = Vec3(-0.5f, 1.075f, 3.5f);
    d2.state = DoorState::Locked;
    doors_.push_back(d2);

    // 3. Furniture Cover Obstacles
    // Living Room Sofa (tactical crouch cover)
    CoverObstacle sofa;
    sofa.name = "Tactical Sofa";
    sofa.position = Vec3(-1.8f, 0.45f, 0.8f);
    sofa.size = Vec3(1.8f, 0.9f, 0.85f);
    sofa.color = Vec4(0.35f, 0.30f, 0.25f, 1.0f);
    obstacles_.push_back(sofa);

    // Living Room Coffee Table
    CoverObstacle table;
    table.name = "Coffee Table";
    table.position = Vec3(-1.8f, 0.25f, 1.8f);
    table.size = Vec3(1.2f, 0.5f, 0.6f);
    table.color = Vec4(0.25f, 0.18f, 0.12f, 1.0f);
    obstacles_.push_back(table);

    // Media Console / TV Unit
    CoverObstacle mediaUnit;
    mediaUnit.name = "Media Cabinet";
    mediaUnit.position = Vec3(1.8f, 0.35f, 0.0f);
    mediaUnit.size = Vec3(0.5f, 0.7f, 2.0f);
    mediaUnit.color = Vec4(0.18f, 0.18f, 0.20f, 1.0f);
    obstacles_.push_back(mediaUnit);

    // Kitchen Island Counter
    CoverObstacle kitchenIsland;
    kitchenIsland.name = "Kitchen Island";
    kitchenIsland.position = Vec3(5.2f, 0.5f, 1.0f);
    kitchenIsland.size = Vec3(1.4f, 1.0f, 2.2f);
    kitchenIsland.color = Vec4(0.70f, 0.70f, 0.72f, 1.0f);
    obstacles_.push_back(kitchenIsland);

    // Stronghold Barricade / Bed
    CoverObstacle strongholdBed;
    strongholdBed.name = "Barricaded Bed";
    strongholdBed.position = Vec3(-1.5f, 0.4f, 6.0f);
    strongholdBed.size = Vec3(2.0f, 0.8f, 1.8f);
    strongholdBed.color = Vec4(0.28f, 0.32f, 0.38f, 1.0f);
    obstacles_.push_back(strongholdBed);

    // Stronghold Weapon Crates
    CoverObstacle crates;
    crates.name = "Weapon Crates";
    crates.position = Vec3(1.2f, 0.45f, 5.5f);
    crates.size = Vec3(1.0f, 0.9f, 0.8f);
    crates.color = Vec4(0.22f, 0.30f, 0.20f, 1.0f);
    obstacles_.push_back(crates);
}

void ApartmentScene::BuildGeometryMeshes() {
    // 1. Apartment Walls Mesh
    {
        std::vector<Vertex> verts;
        std::vector<uint16_t> indices;
        Vec4 wallColor(0.82f, 0.80f, 0.76f, 1.0f);

        auto AddWall = [&](const Vec3& center, const Vec3& size) {
            float hx = size.x * 0.5f, hy = size.y * 0.5f, hz = size.z * 0.5f;
            uint16_t start = static_cast<uint16_t>(verts.size());

            // 6 faces
            struct Face { Vec3 n; Vec3 p[4]; } f[6] = {
                { Vec3(0,0,1), { Vec3(-hx,-hy, hz), Vec3( hx,-hy, hz), Vec3( hx, hy, hz), Vec3(-hx, hy, hz) } },
                { Vec3(0,0,-1),{ Vec3( hx,-hy,-hz), Vec3(-hx,-hy,-hz), Vec3(-hx, hy,-hz), Vec3( hx, hy,-hz) } },
                { Vec3(0,1,0), { Vec3(-hx, hy, hz), Vec3( hx, hy, hz), Vec3( hx, hy,-hz), Vec3(-hx, hy,-hz) } },
                { Vec3(0,-1,0),{ Vec3(-hx,-hy,-hz), Vec3( hx,-hy,-hz), Vec3( hx,-hy, hz), Vec3(-hx,-hy, hz) } },
                { Vec3(1,0,0), { Vec3( hx,-hy, hz), Vec3( hx,-hy,-hz), Vec3( hx, hy,-hz), Vec3( hx, hy, hz) } },
                { Vec3(-1,0,0),{ Vec3(-hx,-hy,-hz), Vec3(-hx,-hy, hz), Vec3(-hx, hy, hz), Vec3(-hx, hy,-hz) } }
            };

            for (int i = 0; i < 6; ++i) {
                uint16_t base = static_cast<uint16_t>(verts.size());
                verts.emplace_back(center + f[i].p[0], f[i].n, wallColor, Vec2(0,0));
                verts.emplace_back(center + f[i].p[1], f[i].n, wallColor, Vec2(1,0));
                verts.emplace_back(center + f[i].p[2], f[i].n, wallColor, Vec2(1,1));
                verts.emplace_back(center + f[i].p[3], f[i].n, wallColor, Vec2(0,1));

                indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
                indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
            }
        };

        float wallH = 2.8f;
        float wallY = wallH * 0.5f;

        // Outer boundaries
        AddWall(Vec3(-3.5f, wallY, 1.0f),  Vec3(0.2f, wallH, 5.0f)); // West Wall Living Room
        AddWall(Vec3( 3.5f, wallY, -0.2f), Vec3(0.2f, wallH, 2.6f)); // East Wall Part 1
        AddWall(Vec3( 7.0f, wallY, 1.0f),  Vec3(0.2f, wallH, 5.0f)); // East Wall Kitchen
        AddWall(Vec3( 5.25f, wallY, -1.5f),Vec3(3.5f, wallH, 0.2f)); // South Kitchen Wall
        AddWall(Vec3( 5.25f, wallY, 3.5f), Vec3(3.5f, wallH, 0.2f)); // North Kitchen Wall

        // Entry Wall with Doorway at (0, 0, -1.5)
        AddWall(Vec3(-2.25f, wallY, -1.5f), Vec3(2.5f, wallH, 0.2f));
        AddWall(Vec3( 2.25f, wallY, -1.5f), Vec3(2.5f, wallH, 0.2f));
        AddWall(Vec3( 0.0f, wallY + 1.15f, -1.5f), Vec3(1.0f, 0.65f, 0.2f)); // Lintel over door

        // Stronghold Wall with Doorway at (-0.5, 0, 3.5)
        AddWall(Vec3(-2.5f, wallY, 3.5f), Vec3(2.0f, wallH, 0.2f));
        AddWall(Vec3( 1.5f, wallY, 3.5f), Vec3(2.0f, wallH, 0.2f));
        AddWall(Vec3(-0.5f, wallY + 1.15f, 3.5f), Vec3(1.0f, 0.65f, 0.2f));

        // Stronghold Perimeter
        AddWall(Vec3(-3.5f, wallY, 5.75f), Vec3(0.2f, wallH, 4.5f)); // West Stronghold
        AddWall(Vec3( 2.5f, wallY, 5.75f), Vec3(0.2f, wallH, 4.5f)); // East Stronghold
        AddWall(Vec3(-0.5f, wallY, 8.0f),  Vec3(6.0f, wallH, 0.2f)); // North Stronghold

        apartmentWallsMesh_ = std::make_unique<Mesh>();
        apartmentWallsMesh_->Upload(verts, indices);
    }

    // 2. Floor & Ceiling Mesh
    {
        std::vector<Vertex> verts;
        std::vector<uint16_t> indices;
        Vec4 floorColor(0.25f, 0.24f, 0.22f, 1.0f); // tactical concrete / dark hardwood
        Vec4 ceilingColor(0.75f, 0.75f, 0.77f, 1.0f);

        auto AddPlane = [&](const Vec3& minP, const Vec3& maxP, float y, bool isCeiling) {
            uint16_t b = static_cast<uint16_t>(verts.size());
            Vec3 n = isCeiling ? Vec3(0, -1, 0) : Vec3(0, 1, 0);
            Vec4 col = isCeiling ? ceilingColor : floorColor;

            verts.emplace_back(Vec3(minP.x, y, minP.z), n, col, Vec2(0,0));
            verts.emplace_back(Vec3(maxP.x, y, minP.z), n, col, Vec2(1,0));
            verts.emplace_back(Vec3(maxP.x, y, maxP.z), n, col, Vec2(1,1));
            verts.emplace_back(Vec3(minP.x, y, maxP.z), n, col, Vec2(0,1));

            if (!isCeiling) {
                indices.push_back(b + 0); indices.push_back(b + 1); indices.push_back(b + 2);
                indices.push_back(b + 0); indices.push_back(b + 2); indices.push_back(b + 3);
            } else {
                indices.push_back(b + 0); indices.push_back(b + 2); indices.push_back(b + 1);
                indices.push_back(b + 0); indices.push_back(b + 3); indices.push_back(b + 2);
            }
        };

        AddPlane(Vec3(-3.5f, 0, -4.5f), Vec3(7.0f, 0, 8.0f), 0.0f, false); // Floor
        AddPlane(Vec3(-3.5f, 0, -4.5f), Vec3(7.0f, 0, 8.0f), 2.8f, true);  // Ceiling

        apartmentFloorCeilingMesh_ = std::make_unique<Mesh>();
        apartmentFloorCeilingMesh_->Upload(verts, indices);
    }

    // 3. Furniture Cover Mesh
    {
        std::vector<Vertex> verts;
        std::vector<uint16_t> indices;

        for (const auto& obs : obstacles_) {
            Vec3 half = obs.size * 0.5f;
            uint16_t b = static_cast<uint16_t>(verts.size());
            // 6 faces for each obstacle
            struct Face { Vec3 n; Vec3 p[4]; } f[6] = {
                { Vec3(0,0,1), { Vec3(-half.x,-half.y, half.z), Vec3( half.x,-half.y, half.z), Vec3( half.x, half.y, half.z), Vec3(-half.x, half.y, half.z) } },
                { Vec3(0,0,-1),{ Vec3( half.x,-half.y,-half.z), Vec3(-half.x,-half.y,-half.z), Vec3(-half.x, half.y,-half.z), Vec3( half.x, half.y,-half.z) } },
                { Vec3(0,1,0), { Vec3(-half.x, half.y, half.z), Vec3( half.x, half.y, half.z), Vec3( half.x, half.y,-half.z), Vec3(-half.x, half.y,-half.z) } },
                { Vec3(0,-1,0),{ Vec3(-half.x,-half.y,-half.z), Vec3( half.x,-half.y,-half.z), Vec3( half.x,-half.y, half.z), Vec3(-half.x,-half.y, half.z) } },
                { Vec3(1,0,0), { Vec3( half.x,-half.y, half.z), Vec3( half.x,-half.y,-half.z), Vec3( half.x, half.y,-half.z), Vec3( half.x, half.y, half.z) } },
                { Vec3(-1,0,0),{ Vec3(-half.x,-half.y,-half.z), Vec3(-half.x,-half.y, half.z), Vec3(-half.x, half.y, half.z), Vec3(-half.x, half.y,-half.z) } }
            };

            for (int i = 0; i < 6; ++i) {
                uint16_t start = static_cast<uint16_t>(verts.size());
                verts.emplace_back(obs.position + f[i].p[0], f[i].n, obs.color, Vec2(0,0));
                verts.emplace_back(obs.position + f[i].p[1], f[i].n, obs.color, Vec2(1,0));
                verts.emplace_back(obs.position + f[i].p[2], f[i].n, obs.color, Vec2(1,1));
                verts.emplace_back(obs.position + f[i].p[3], f[i].n, obs.color, Vec2(0,1));

                indices.push_back(start + 0); indices.push_back(start + 1); indices.push_back(start + 2);
                indices.push_back(start + 0); indices.push_back(start + 2); indices.push_back(start + 3);
            }
        }

        furnitureMesh_ = std::make_unique<Mesh>();
        furnitureMesh_->Upload(verts, indices);
    }

    // 4. Door Slab & Frame
    Vec4 woodDoor(0.40f, 0.25f, 0.15f, 1.0f);
    Vec4 steelFrame(0.20f, 0.20f, 0.22f, 1.0f);
    doorSlabMesh_ = MeshBuilder::CreateBox(0.95f, 2.15f, 0.06f, woodDoor);
    doorFrameMesh_ = MeshBuilder::CreateWireBox(1.0f, 2.2f, 0.12f, steelFrame);

    // 5. Tactical Wireframe scan mesh
    Vec4 scanGreen(0.1f, 0.9f, 0.3f, 0.7f);
    wireframeApartmentMesh_ = MeshBuilder::CreateWireBox(10.0f, 2.8f, 12.0f, scanGreen);
}

void ApartmentScene::Update(float dt) {
    // Smooth door opening rotation when breached
    for (auto& door : doors_) {
        if (door.state == DoorState::Open || door.state == DoorState::BlownOffHinges) {
            float targetAngle = (door.state == DoorState::BlownOffHinges) ? (PI * 0.55f) : (PI * 0.5f);
            door.openAngle = std::min(targetAngle, door.openAngle + dt * 6.0f);
        }
    }
}

void ApartmentScene::Render(const Mat4& view, const Mat4& proj, bool tacticalVisorActive) const {
    const auto& litShader = ShaderLibrary::Instance().Lit();
    litShader.Use();
    litShader.SetMat4("uView", view);
    litShader.SetMat4("uProj", proj);

    Mat4 identity = Mat4::Identity();
    litShader.SetMat4("uModel", identity);
    litShader.SetVec4("uTint", Vec4(1, 1, 1, 1));

    // Draw Walls, Floor, Ceiling, Furniture
    if (apartmentWallsMesh_) apartmentWallsMesh_->Draw();
    if (apartmentFloorCeilingMesh_) apartmentFloorCeilingMesh_->Draw();
    if (furnitureMesh_) furnitureMesh_->Draw();

    // Draw Tactical Doors
    for (const auto& door : doors_) {
        Mat4 doorTransform;
        if (door.state == DoorState::BlownOffHinges) {
            // Door blown off on the floor
            Vec3 blownPos = door.position + Vec3(0, -0.95f, 0.8f);
            Quat tilt = Quat::FromAxisAngle(Vec3(1, 0, 0), PI * 0.48f);
            doorTransform = Mat4::TRS(blownPos, tilt, Vec3(1, 1, 1));
        } else {
            // Pivot around hinge (left edge)
            Vec3 hingeOffset(-door.width * 0.5f, 0, 0);
            Quat hingeRot = Quat::FromAxisAngle(Vec3(0, 1, 0), door.openAngle);
            doorTransform = Mat4::TRS(door.position, door.orientation, Vec3(1, 1, 1)) *
                            Mat4::Translation(-hingeOffset) *
                            Mat4::FromRotation(hingeRot) *
                            Mat4::Translation(hingeOffset);
        }

        litShader.SetMat4("uModel", doorTransform);
        doorSlabMesh_->Draw();
    }

    // Tactical Visor Overlay
    if (tacticalVisorActive && wireframeApartmentMesh_) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        const auto& wireShader = ShaderLibrary::Instance().Wireframe();
        wireShader.Use();
        wireShader.SetMat4("uView", view);
        wireShader.SetMat4("uProj", proj);
        wireShader.SetMat4("uModel", identity);
        wireShader.SetVec4("uColor", Vec4(0.1f, 0.85f, 0.4f, 0.6f));
        wireShader.SetFloat("uPulseTime", 1.0f);

        wireframeApartmentMesh_->Draw();

        glDisable(GL_BLEND);
    }
}

bool ApartmentScene::Raycast(const Ray& ray, float maxDist, float& hitDist, Vec3& hitNorm, TacticalDoor** hitDoorOut) {
    bool hitAny = false;
    float closestT = maxDist;
    Vec3 bestNorm(0, 1, 0);
    TacticalDoor* bestDoor = nullptr;

    // Check doors
    for (auto& door : doors_) {
        if (door.state == DoorState::BlownOffHinges) continue;
        AABB box = door.GetAABB();
        float t = 0.0f;
        if (box.IntersectRay(ray, t, closestT)) {
            if (t < closestT) {
                closestT = t;
                hitAny = true;
                bestDoor = &door;
                bestNorm = Vec3(0, 0, -1);
            }
        }
    }

    // Check furniture covers
    for (const auto& obs : obstacles_) {
        AABB box = obs.GetAABB();
        float t = 0.0f;
        if (box.IntersectRay(ray, t, closestT)) {
            if (t < closestT) {
                closestT = t;
                hitAny = true;
                bestDoor = nullptr;
                bestNorm = Vec3(0, 1, 0);
            }
        }
    }

    // Check floor
    if (ray.direction.y < -1e-4f) {
        float t = -ray.origin.y / ray.direction.y;
        if (t > 0 && t < closestT) {
            closestT = t;
            hitAny = true;
            bestDoor = nullptr;
            bestNorm = Vec3(0, 1, 0);
        }
    }

    if (hitAny) {
        hitDist = closestT;
        hitNorm = bestNorm;
        if (hitDoorOut) *hitDoorOut = bestDoor;
        return true;
    }
    return false;
}

void ApartmentScene::ResolveCollision(Vec3& position, float radius) {
    // Floor clamp
    if (position.y < 0.0f) position.y = 0.0f;

    // Collide against doors if closed
    for (const auto& door : doors_) {
        if (door.state == DoorState::Locked || door.state == DoorState::Unlocked) {
            AABB b = door.GetAABB();
            b.minBounds -= Vec3(radius, radius, radius);
            b.maxBounds += Vec3(radius, radius, radius);
            if (position.x >= b.minBounds.x && position.x <= b.maxBounds.x &&
                position.z >= b.minBounds.z && position.z <= b.maxBounds.z) {
                // Push out along z
                if (position.z < door.position.z) position.z = b.minBounds.z;
                else position.z = b.maxBounds.z;
            }
        }
    }
}

int ApartmentScene::GetCurrentRoomIndex(const Vec3& pos) const {
    for (size_t i = 0; i < rooms_.size(); ++i) {
        const auto& b = rooms_[i].bounds;
        if (pos.x >= b.minBounds.x && pos.x <= b.maxBounds.x &&
            pos.z >= b.minBounds.z && pos.z <= b.maxBounds.z) {
            return static_cast<int>(i);
        }
    }
    return 0;
}

} // namespace breachvr
