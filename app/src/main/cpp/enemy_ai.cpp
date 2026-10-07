#include "enemy_ai.h"
#include "audio_engine.h"
#include <android/log.h>
#include <cmath>

#define LOG_TAG "BreachVR_AI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

namespace breachvr {

AABB CQBEnemy::GetHeadAABB() const {
    Vec3 p = position + Vec3(0, 1.55f, 0);
    return AABB(p - Vec3(0.12f, 0.12f, 0.12f), p + Vec3(0.12f, 0.12f, 0.12f));
}

AABB CQBEnemy::GetTorsoAABB() const {
    Vec3 p = position + Vec3(0, 1.15f, 0);
    return AABB(p - Vec3(0.22f, 0.28f, 0.16f), p + Vec3(0.22f, 0.28f, 0.16f));
}

AABB CQBEnemy::GetLegsAABB() const {
    Vec3 p = position + Vec3(0, 0.45f, 0);
    return AABB(p - Vec3(0.18f, 0.45f, 0.16f), p + Vec3(0.18f, 0.45f, 0.16f));
}

bool CQBEnemy::ApplyDamage(float amount, bool isHeadshot) {
    if (state == EnemyState::Dead) return false;

    float actualDmg = isHeadshot ? (amount * 2.5f) : amount;
    health -= actualDmg;

    if (health <= 0.0f) {
        state = EnemyState::Dead;
        health = 0.0f;
        AudioEngine::Instance().PlayHitMarker();
        return true; // killed
    }
    return false;
}

void CQBEnemy::Stun(float duration) {
    if (state == EnemyState::Dead) return;
    state = EnemyState::Stunned;
    stunTimer = duration;
}

bool EnemyManager::Init(const ApartmentScene& /*scene*/) {
    enemies_.clear();

    // Living Room Enemy 1 (near sofa)
    CQBEnemy e1;
    e1.id = 1;
    e1.callsign = "Hostile 1";
    e1.position = Vec3(-1.8f, 0.0f, 1.2f);
    e1.orientation = Quat::FromAxisAngle(Vec3(0, 1, 0), PI); // facing breach door
    e1.roomIndex = 1;
    e1.state = EnemyState::Guard;
    enemies_.push_back(e1);

    // Living Room Enemy 2 (near media cabinet)
    CQBEnemy e2;
    e2.id = 2;
    e2.callsign = "Hostile 2";
    e2.position = Vec3(1.5f, 0.0f, 0.5f);
    e2.orientation = Quat::FromAxisAngle(Vec3(0, 1, 0), PI);
    e2.roomIndex = 1;
    e2.state = EnemyState::Guard;
    enemies_.push_back(e2);

    // Kitchen Enemy 3 (behind island)
    CQBEnemy e3;
    e3.id = 3;
    e3.callsign = "Kitchen Guard";
    e3.position = Vec3(5.5f, 0.0f, 1.2f);
    e3.orientation = Quat::FromAxisAngle(Vec3(0, 1, 0), -PI * 0.5f);
    e3.roomIndex = 2;
    e3.state = EnemyState::Guard;
    enemies_.push_back(e3);

    // Stronghold Elite 4 (behind barricaded bed)
    CQBEnemy e4;
    e4.id = 4;
    e4.callsign = "Stronghold Leader";
    e4.position = Vec3(-1.5f, 0.0f, 6.5f);
    e4.orientation = Quat::FromAxisAngle(Vec3(0, 1, 0), PI);
    e4.roomIndex = 3;
    e4.health = 150.0f;
    e4.maxHealth = 150.0f;
    e4.state = EnemyState::Guard;
    enemies_.push_back(e4);

    // Stronghold Gunner 5 (behind crates)
    CQBEnemy e5;
    e5.id = 5;
    e5.callsign = "Stronghold Gunner";
    e5.position = Vec3(1.2f, 0.0f, 6.2f);
    e5.orientation = Quat::FromAxisAngle(Vec3(0, 1, 0), PI);
    e5.roomIndex = 3;
    e5.state = EnemyState::Guard;
    enemies_.push_back(e5);

    enemyMesh_ = MeshBuilder::CreateEnemyMesh();
    LOGI("EnemyManager initialized with %zu CQB hostiles", enemies_.size());
    return true;
}

void EnemyManager::Update(float dt, const Vec3& playerHeadPos, ApartmentScene& scene) {
    int playerRoom = scene.GetCurrentRoomIndex(playerHeadPos);

    for (auto& e : enemies_) {
        if (e.state == EnemyState::Dead) continue;

        e.muzzleFlash = std::max(0.0f, e.muzzleFlash - dt * 10.0f);

        // Stun update
        if (e.state == EnemyState::Stunned) {
            e.stunTimer -= dt;
            if (e.stunTimer <= 0.0f) {
                e.state = EnemyState::Engaging;
            }
            continue;
        }

        // Perception: if player is in the same room or door is breached
        bool canSeePlayer = false;
        if (e.roomIndex == playerRoom) {
            canSeePlayer = true;
        } else {
            // Check if door connecting to room is open/breached
            for (const auto& d : scene.GetDoors()) {
                if (d.state == DoorState::Open || d.state == DoorState::BlownOffHinges) {
                    float distToDoor = Vec3::Distance(e.position, d.position);
                    if (distToDoor < 4.0f) {
                        canSeePlayer = true;
                        break;
                    }
                }
            }
        }

        if (canSeePlayer) {
            e.state = EnemyState::Engaging;

            // Turn towards player
            Vec3 toPlayer = (playerHeadPos - e.position);
            toPlayer.y = 0.0f;
            if (toPlayer.LengthSq() > 0.01f) {
                float yaw = std::atan2(toPlayer.x, toPlayer.z);
                e.orientation = Quat::FromAxisAngle(Vec3(0, 1, 0), yaw);
            }

            // Attack logic: 3-round burst
            e.attackCooldown -= dt;
            if (e.attackCooldown <= 0.0f) {
                if (e.burstShotsRemaining == 0) {
                    e.burstShotsRemaining = 3;
                    e.burstTimer = 0.0f;
                }
            }

            if (e.burstShotsRemaining > 0) {
                e.burstTimer -= dt;
                if (e.burstTimer <= 0.0f) {
                    e.burstShotsRemaining--;
                    e.burstTimer = 0.12f;
                    e.muzzleFlash = 1.0f;

                    // Play hostile gunshot sound
                    AudioEngine::Instance().PlayCarbineShot();

                    if (e.burstShotsRemaining == 0) {
                        e.attackCooldown = 1.5f + (float)(rand() % 10) * 0.1f;
                    }
                }
            }
        }
    }
}

void EnemyManager::AlertEnemiesInRoom(int roomIndex, const Vec3& soundPos) {
    for (auto& e : enemies_) {
        if (e.state == EnemyState::Dead) continue;
        if (e.roomIndex == roomIndex || Vec3::Distance(e.position, soundPos) < 7.0f) {
            if (e.state == EnemyState::Guard) {
                e.state = EnemyState::Suspicious;
                Vec3 toSound = (soundPos - e.position);
                toSound.y = 0.0f;
                if (toSound.LengthSq() > 0.01f) {
                    float yaw = std::atan2(toSound.x, toSound.z);
                    e.orientation = Quat::FromAxisAngle(Vec3(0, 1, 0), yaw);
                }
            }
        }
    }
}

void EnemyManager::StunEnemiesInRadius(const Vec3& explosionPos, float radius) {
    for (auto& e : enemies_) {
        if (e.state == EnemyState::Dead) continue;
        float dist = Vec3::Distance(e.position, explosionPos);
        if (dist <= radius) {
            float stunDuration = 4.5f * (1.0f - dist / radius) + 1.0f;
            e.Stun(stunDuration);
        }
    }
}

bool EnemyManager::CheckBulletHit(const Ray& ray, float maxDist, float damage, float& hitDistOut, bool& killedOut) {
    float closestT = maxDist;
    CQBEnemy* hitEnemy = nullptr;
    bool isHeadshot = false;

    for (auto& e : enemies_) {
        if (e.state == EnemyState::Dead) continue;

        // Check head
        AABB headBox = e.GetHeadAABB();
        float tHead = 0.0f;
        if (headBox.IntersectRay(ray, tHead, closestT)) {
            if (tHead < closestT) {
                closestT = tHead;
                hitEnemy = &e;
                isHeadshot = true;
            }
        }

        // Check torso
        AABB torsoBox = e.GetTorsoAABB();
        float tTorso = 0.0f;
        if (torsoBox.IntersectRay(ray, tTorso, closestT)) {
            if (tTorso < closestT) {
                closestT = tTorso;
                hitEnemy = &e;
                isHeadshot = false;
            }
        }
    }

    if (hitEnemy) {
        hitDistOut = closestT;
        killedOut = hitEnemy->ApplyDamage(damage, isHeadshot);
        return true;
    }
    return false;
}

int EnemyManager::GetAliveCount() const {
    int count = 0;
    for (const auto& e : enemies_) {
        if (e.state != EnemyState::Dead) count++;
    }
    return count;
}

void EnemyManager::Render(const Mat4& view, const Mat4& proj) const {
    if (!enemyMesh_) return;

    const auto& litShader = ShaderLibrary::Instance().Lit();
    litShader.Use();
    litShader.SetMat4("uView", view);
    litShader.SetMat4("uProj", proj);

    for (const auto& e : enemies_) {
        Mat4 tr;
        if (e.state == EnemyState::Dead) {
            // Fallen on the floor
            Vec3 deadPos = e.position + Vec3(0, 0.12f, 0);
            Quat tilt = Quat::FromAxisAngle(Vec3(1, 0, 0), PI * 0.5f);
            tr = Mat4::TRS(deadPos, e.orientation * tilt, Vec3(1, 1, 1));
            litShader.SetVec4("uTint", Vec4(0.5f, 0.5f, 0.5f, 1.0f));
        } else if (e.state == EnemyState::Stunned) {
            // Stunned posture (disoriented tilt)
            Quat wobble = Quat::FromAxisAngle(Vec3(0, 0, 1), 0.15f * std::sin(e.stunTimer * 8.0f));
            tr = Mat4::TRS(e.position, e.orientation * wobble, Vec3(1, 1, 1));
            litShader.SetVec4("uTint", Vec4(1.0f, 0.9f, 0.6f, 1.0f));
        } else {
            tr = Mat4::TRS(e.position, e.orientation, Vec3(1, 1, 1));
            litShader.SetVec4("uTint", Vec4(1, 1, 1, 1));
        }

        litShader.SetMat4("uModel", tr);
        enemyMesh_->Draw();
    }
}

} // namespace breachvr
