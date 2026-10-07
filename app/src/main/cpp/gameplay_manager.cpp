#include "gameplay_manager.h"
#include "audio_engine.h"
#include <android/log.h>
#include <cmath>

#define LOG_TAG "BreachVR_Game"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

namespace breachvr {

bool GameplayManager::Init() {
    hudPlateMesh_ = MeshBuilder::CreateQuad(0.24f, 0.08f, Vec4(0.05f, 0.12f, 0.18f, 0.75f));
    hudCompassMesh_ = MeshBuilder::CreateQuad(0.35f, 0.04f, Vec4(0.08f, 0.22f, 0.15f, 0.80f));
    hudHealthBarMesh_ = MeshBuilder::CreateQuad(0.18f, 0.02f, Vec4(0.1f, 0.85f, 0.3f, 0.90f));
    screenBlindQuadMesh_ = MeshBuilder::CreateQuad(4.0f, 4.0f, Vec4(1.0f, 1.0f, 1.0f, 1.0f));

    LOGI("GameplayManager initialized successfully");
    return true;
}

void GameplayManager::Update(float dt, const Vec3& headPos, const Quat& /*headRot*/,
                            ApartmentScene& scene, EnemyManager& enemyManager,
                            WeaponSystem& weaponSystem, InputSystem& inputSystem) {
    missionTimer_ += dt;

    // Check Input Actions
    const auto& rightHand = inputSystem.GetRightHand();
    const auto& leftHand  = inputSystem.GetLeftHand();

    if (rightHand.isSpecialClicked || leftHand.isSpecialClicked) {
        tacticalVisorActive_ = !tacticalVisorActive_;
        AudioEngine::Instance().PlayRadioChirp();
    }

    // Phase transition checks
    if (missionPhase_ == MissionPhase::BreachEntry) {
        const auto& doors = scene.GetDoors();
        if (!doors.empty() && (doors[0].state == DoorState::Open || doors[0].state == DoorState::BlownOffHinges)) {
            missionPhase_ = MissionPhase::RoomClearing;
            AudioEngine::Instance().PlayRadioChirp();
            LOGI("Mission phase updated: ROOM CLEARING");
        }
    } else if (missionPhase_ == MissionPhase::RoomClearing) {
        if (enemyManager.GetAliveCount() == 0) {
            missionPhase_ = MissionPhase::MissionSecured;
            AudioEngine::Instance().PlayRadioChirp();
            LOGI("Mission phase updated: MISSION SECURED");
        }
    }

    // Check damage from enemies engaging player
    for (const auto& e : enemyManager.GetEnemies()) {
        if (e.state == EnemyState::Engaging && e.burstShotsRemaining > 0 && e.burstTimer >= 0.10f) {
            // Check line of sight
            Vec3 toHead = headPos - (e.position + Vec3(0, 1.4f, 0));
            float dist = toHead.Length();
            Ray los(e.position + Vec3(0, 1.4f, 0), toHead);

            float hitDist = dist;
            Vec3 hitNorm;
            TacticalDoor* doorHit = nullptr;
            bool blocked = scene.Raycast(los, dist - 0.2f, hitDist, hitNorm, &doorHit);

            if (!blocked && dist < 12.0f) {
                // Player takes damage
                float dmg = 8.0f;
                if (playerArmor_ > 0.0f) {
                    playerArmor_ = std::max(0.0f, playerArmor_ - dmg);
                } else {
                    playerHealth_ = std::max(0.0f, playerHealth_ - dmg);
                }
                AudioEngine::Instance().PlayHitMarker();
                inputSystem.TriggerHaptic(XR_NULL_HANDLE, true, 0.12f, 180.0f, 0.9f);
            }
        }
    }

    // Check room clear state
    int currentRoom = scene.GetCurrentRoomIndex(headPos);
    if (currentRoom > 0 && currentRoom < (int)scene.GetRooms().size()) {
        bool anyEnemyInRoom = false;
        for (const auto& e : enemyManager.GetEnemies()) {
            if (e.state != EnemyState::Dead && e.roomIndex == currentRoom) {
                anyEnemyInRoom = true;
                break;
            }
        }
        if (!anyEnemyInRoom) {
            scene.GetRooms()[currentRoom].isCleared = true;
        }
    }
}

void GameplayManager::RenderHUD(const Mat4& view, const Mat4& proj,
                               const Vec3& headPos, const Quat& headRot,
                               const WeaponSystem& weaponSystem,
                               const EnemyManager& enemyManager,
                               const ApartmentScene& scene) const {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    const auto& hudShader = ShaderLibrary::Instance().HUD();
    hudShader.Use();
    hudShader.SetMat4("uView", view);
    hudShader.SetMat4("uProj", proj);

    // Forward, Right, Up vectors from head orientation
    Vec3 fwd = headRot.Rotate(Vec3(0, 0, 1)).Normalized();
    Vec3 right = headRot.Rotate(Vec3(1, 0, 0)).Normalized();
    Vec3 up = headRot.Rotate(Vec3(0, 1, 0)).Normalized();

    float hudDist = 0.65f;
    Vec3 hudCenter = headPos + fwd * hudDist;

    // 1. Top HUD: Mission Objective Banner & Compass
    {
        Vec3 compassPos = hudCenter + up * 0.16f;
        Mat4 tr = Mat4::TRS(compassPos, headRot, Vec3(1, 1, 1));
        hudShader.SetMat4("uModel", tr);

        Vec4 bannerColor = (missionPhase_ == MissionPhase::MissionSecured) ?
            Vec4(0.1f, 0.85f, 0.3f, 0.85f) : Vec4(0.1f, 0.3f, 0.6f, 0.80f);
        hudShader.SetVec4("uColor", bannerColor);
        if (hudCompassMesh_) hudCompassMesh_->Draw();
    }

    // 2. Bottom-Left HUD: Health & Armor Indicators
    {
        Vec3 healthPlatePos = hudCenter - right * 0.18f - up * 0.14f;
        Mat4 tr = Mat4::TRS(healthPlatePos, headRot, Vec3(1, 1, 1));
        hudShader.SetMat4("uModel", tr);
        hudShader.SetVec4("uColor", Vec4(0.05f, 0.10f, 0.15f, 0.75f));
        if (hudPlateMesh_) hudPlateMesh_->Draw();

        // Health bar
        float healthPct = std::max(0.0f, playerHealth_ / 100.0f);
        Vec3 barPos = healthPlatePos + up * 0.015f;
        Mat4 barTr = Mat4::TRS(barPos, headRot, Vec3(healthPct, 1, 1));
        hudShader.SetMat4("uModel", barTr);
        hudShader.SetVec4("uColor", Vec4(0.2f, 0.85f, 0.3f, 0.95f));
        if (hudHealthBarMesh_) hudHealthBarMesh_->Draw();

        // Armor bar
        float armorPct = std::max(0.0f, playerArmor_ / 100.0f);
        Vec3 armorPos = healthPlatePos - up * 0.015f;
        Mat4 armTr = Mat4::TRS(armorPos, headRot, Vec3(armorPct, 0.75f, 1));
        hudShader.SetMat4("uModel", armTr);
        hudShader.SetVec4("uColor", Vec4(0.2f, 0.5f, 0.9f, 0.95f));
        if (hudHealthBarMesh_) hudHealthBarMesh_->Draw();
    }

    // 3. Bottom-Right HUD: Ammo Counter & Hostile Threat Count
    {
        Vec3 ammoPlatePos = hudCenter + right * 0.18f - up * 0.14f;
        Mat4 tr = Mat4::TRS(ammoPlatePos, headRot, Vec3(1, 1, 1));
        hudShader.SetMat4("uModel", tr);
        hudShader.SetVec4("uColor", Vec4(0.05f, 0.10f, 0.15f, 0.75f));
        if (hudPlateMesh_) hudPlateMesh_->Draw();

        // Ammo fill indicator
        float ammoPct = (float)weaponSystem.GetCurrentAmmo() / (float)weaponSystem.GetMagCapacity();
        Vec3 ammoBarPos = ammoPlatePos + up * 0.015f;
        Mat4 ammoTr = Mat4::TRS(ammoBarPos, headRot, Vec3(ammoPct, 1, 1));
        hudShader.SetMat4("uModel", ammoTr);
        hudShader.SetVec4("uColor", Vec4(0.95f, 0.75f, 0.1f, 0.95f));
        if (hudHealthBarMesh_) hudHealthBarMesh_->Draw();

        // Hostiles remaining bar indicator
        int aliveCount = enemyManager.GetAliveCount();
        float threatPct = std::min(1.0f, (float)aliveCount / 5.0f);
        Vec3 threatBarPos = ammoPlatePos - up * 0.015f;
        Mat4 threatTr = Mat4::TRS(threatBarPos, headRot, Vec3(threatPct, 0.75f, 1));
        hudShader.SetMat4("uModel", threatTr);
        hudShader.SetVec4("uColor", Vec4(0.9f, 0.2f, 0.2f, 0.95f));
        if (hudHealthBarMesh_) hudHealthBarMesh_->Draw();
    }

    // 4. Flashbang Blind Full-Screen Overlay
    float blindFactor = weaponSystem.GetFlashbangBlindFactor();
    if (blindFactor > 0.01f && screenBlindQuadMesh_) {
        Vec3 blindPos = headPos + fwd * 0.15f;
        Mat4 blindTr = Mat4::TRS(blindPos, headRot, Vec3(1, 1, 1));
        hudShader.SetMat4("uModel", blindTr);
        hudShader.SetVec4("uColor", Vec4(1.0f, 1.0f, 1.0f, blindFactor));
        screenBlindQuadMesh_->Draw();
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

} // namespace breachvr
