#pragma once
// Breach VR - Gameplay Manager & Tactical Visor HUD

#include "math_types.h"
#include "mesh_builder.h"
#include "apartment_scene.h"
#include "enemy_ai.h"
#include "weapon_system.h"
#include "input_system.h"
#include <string>
#include <memory>

namespace breachvr {

enum class MissionPhase {
    BreachEntry,
    RoomClearing,
    MissionSecured
};

class GameplayManager {
public:
    GameplayManager() = default;

    bool Init();
    void Update(float dt, const Vec3& headPos, const Quat& headRot,
                ApartmentScene& scene, EnemyManager& enemyManager,
                WeaponSystem& weaponSystem, InputSystem& inputSystem);

    void RenderHUD(const Mat4& view, const Mat4& proj,
                   const Vec3& headPos, const Quat& headRot,
                   const WeaponSystem& weaponSystem,
                   const EnemyManager& enemyManager,
                   const ApartmentScene& scene) const;

    bool IsTacticalVisorActive() const { return tacticalVisorActive_; }
    bool IsPassthroughActive() const { return passthroughActive_; }

    float GetPlayerHealth() const { return playerHealth_; }
    MissionPhase GetMissionPhase() const { return missionPhase_; }

private:
    MissionPhase missionPhase_ = MissionPhase::BreachEntry;
    float playerHealth_ = 100.0f;
    float playerArmor_ = 100.0f;
    float missionTimer_ = 0.0f;

    bool tacticalVisorActive_ = false;
    bool passthroughActive_ = false;

    // HUD visual meshes
    std::unique_ptr<Mesh> hudPlateMesh_;
    std::unique_ptr<Mesh> hudCompassMesh_;
    std::unique_ptr<Mesh> hudHealthBarMesh_;
    std::unique_ptr<Mesh> screenBlindQuadMesh_;
};

} // namespace breachvr
