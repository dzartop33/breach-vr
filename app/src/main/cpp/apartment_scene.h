#pragma once
// Breach VR - Apartment Scene & Room Geometry
// Reconstructs tactical map from scanned apartment (XR_FB_scene) or procedural fallback.

#ifndef XR_USE_PLATFORM_ANDROID
#define XR_USE_PLATFORM_ANDROID
#endif
#ifndef XR_USE_GRAPHICS_API_OPENGL_ES
#define XR_USE_GRAPHICS_API_OPENGL_ES
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "math_types.h"
#include "mesh_builder.h"
#include "gl_shaders.h"
#include <vector>
#include <memory>
#include <string>

namespace breachvr {

enum class DoorState {
    Locked,
    Unlocked,
    Open,
    BlownOffHinges
};

struct TacticalDoor {
    int id = 0;
    std::string name;
    Vec3 position;
    Quat orientation = Quat::Identity();
    float width = 0.95f;
    float height = 2.15f;
    float thickness = 0.06f;

    DoorState state = DoorState::Locked;
    float health = 100.0f;
    float openAngle = 0.0f; // in radians
    bool isC4Attached = false;
    Vec3 c4LocalPos{0, 1.1f, 0.04f};

    AABB GetAABB() const;
    void BreachExplode();
    void Kick();
};

struct CoverObstacle {
    std::string name;
    Vec3 position;
    Vec3 size;
    Quat orientation = Quat::Identity();
    Vec4 color;
    AABB GetAABB() const;
};

struct RoomZone {
    int id = 0;
    std::string name;
    AABB bounds;
    bool isCleared = false;
};

class ApartmentScene {
public:
    ApartmentScene() = default;

    bool Init(XrInstance instance, XrSession session);
    void Update(float dt);
    void Render(const Mat4& view, const Mat4& proj, bool tacticalVisorActive) const;

    bool Raycast(const Ray& ray, float maxDist, float& hitDist, Vec3& hitNorm, TacticalDoor** hitDoorOut);
    void ResolveCollision(Vec3& position, float radius = 0.35f);

    std::vector<TacticalDoor>& GetDoors() { return doors_; }
    const std::vector<TacticalDoor>& GetDoors() const { return doors_; }
    const std::vector<RoomZone>& GetRooms() const { return rooms_; }
    std::vector<RoomZone>& GetRooms() { return rooms_; }

    int GetCurrentRoomIndex(const Vec3& pos) const;

    bool IsScannedApartment() const { return usedScannedApartment_; }

private:
    bool QuerySpatialEntities(XrInstance instance, XrSession session);
    void BuildProceduralApartment();
    void BuildGeometryMeshes();

    bool usedScannedApartment_ = false;

    std::vector<TacticalDoor> doors_;
    std::vector<CoverObstacle> obstacles_;
    std::vector<RoomZone> rooms_;

    // Visual meshes
    std::unique_ptr<Mesh> apartmentWallsMesh_;
    std::unique_ptr<Mesh> apartmentFloorCeilingMesh_;
    std::unique_ptr<Mesh> furnitureMesh_;
    std::unique_ptr<Mesh> doorFrameMesh_;
    std::unique_ptr<Mesh> doorSlabMesh_;
    std::unique_ptr<Mesh> wireframeApartmentMesh_;
};

} // namespace breachvr
