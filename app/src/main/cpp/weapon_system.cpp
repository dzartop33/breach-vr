#include "weapon_system.h"
#include "enemy_ai.h"
#include "audio_engine.h"
#include <android/log.h>
#include <cmath>

#define LOG_TAG "BreachVR_Weapons"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

namespace breachvr {

bool WeaponSystem::Init() {
    carbineMesh_ = MeshBuilder::CreateCarbineMesh();
    pistolMesh_ = MeshBuilder::CreatePistolMesh();
    c4Mesh_ = MeshBuilder::CreateBreachChargeMesh();
    flashbangMesh_ = MeshBuilder::CreateFlashbangMesh();
    laserBeamMesh_ = MeshBuilder::CreateLaserBeamMesh(15.0f);
    particleQuadMesh_ = MeshBuilder::CreateQuad(0.08f, 0.08f, Vec4(1, 0.8f, 0.3f, 1.0f));
    handJointSphereMesh_ = MeshBuilder::CreateHandJointMesh();

    LOGI("WeaponSystem initialized successfully");
    return true;
}

void WeaponSystem::Reload() {
    AudioEngine::Instance().PlayReload();
    if (currentWeapon_ == WeaponType::CarbineMK18) {
        currentAmmo_ = 30;
        magCapacity_ = 30;
    } else if (currentWeapon_ == WeaponType::PistolSuppressed) {
        currentAmmo_ = 15;
        magCapacity_ = 15;
    }
}

void WeaponSystem::SwitchWeapon() {
    AudioEngine::Instance().PlayRadioChirp();
    if (currentWeapon_ == WeaponType::CarbineMK18) {
        currentWeapon_ = WeaponType::PistolSuppressed;
        magCapacity_ = 15;
        currentAmmo_ = 15;
        fireRate_ = 0.16f;
    } else if (currentWeapon_ == WeaponType::PistolSuppressed) {
        currentWeapon_ = WeaponType::BreachChargeC4;
        magCapacity_ = 3;
        currentAmmo_ = 3;
    } else if (currentWeapon_ == WeaponType::BreachChargeC4) {
        currentWeapon_ = WeaponType::FlashbangGrenade;
        magCapacity_ = 3;
        currentAmmo_ = 3;
    } else {
        currentWeapon_ = WeaponType::CarbineMK18;
        magCapacity_ = 30;
        currentAmmo_ = 30;
        fireRate_ = 0.11f;
    }
}

void WeaponSystem::FireCurrentWeapon(const HandState& hand, ApartmentScene& scene, EnemyManager& enemyManager) {
    if (fireCooldown_ > 0.0f) return;

    if (currentWeapon_ == WeaponType::CarbineMK18 || currentWeapon_ == WeaponType::PistolSuppressed) {
        if (currentAmmo_ <= 0) {
            AudioEngine::Instance().PlayDryFire();
            fireCooldown_ = 0.25f;
            return;
        }

        currentAmmo_--;
        fireCooldown_ = fireRate_;
        recoilAngle_ = (currentWeapon_ == WeaponType::CarbineMK18) ? 0.09f : 0.05f;
        muzzleFlashIntensity_ = 1.0f;

        // Forward direction from weapon aim pose
        Vec3 forward = hand.aimOrientation.Rotate(Vec3(0, 0, 1)).Normalized();
        Vec3 muzzlePos = hand.aimPosition + forward * 0.35f;
        muzzleFlashPos_ = muzzlePos;

        if (currentWeapon_ == WeaponType::CarbineMK18) {
            AudioEngine::Instance().PlayCarbineShot();
        } else {
            AudioEngine::Instance().PlayPistolSuppressed();
        }

        // Alert enemies nearby if weapon is unsuppressed
        if (currentWeapon_ == WeaponType::CarbineMK18) {
            int room = scene.GetCurrentRoomIndex(muzzlePos);
            enemyManager.AlertEnemiesInRoom(room, muzzlePos);
        }

        Ray ray(muzzlePos, forward);
        float enemyHitDist = 100.0f;
        bool killed = false;
        float damage = (currentWeapon_ == WeaponType::CarbineMK18) ? 45.0f : 28.0f;

        bool hitEnemy = enemyManager.CheckBulletHit(ray, 50.0f, damage, enemyHitDist, killed);

        float sceneHitDist = 100.0f;
        Vec3 sceneHitNorm;
        TacticalDoor* hitDoor = nullptr;
        bool hitScene = scene.Raycast(ray, 50.0f, sceneHitDist, sceneHitNorm, &hitDoor);

        if (hitEnemy && (!hitScene || enemyHitDist < sceneHitDist)) {
            // Bullet hit an enemy!
            AudioEngine::Instance().PlayHitMarker();
            Vec3 hitP = ray.PointAt(enemyHitDist);

            // Spawn hit sparks/particles
            for (int i = 0; i < 5; ++i) {
                BulletHitParticle p;
                p.position = hitP;
                p.velocity = Vec3((float)(rand()%10-5)*0.3f, (float)(rand()%10-5)*0.3f, (float)(rand()%10-5)*0.3f);
                p.color = Vec4(0.9f, 0.1f, 0.1f, 1.0f);
                p.life = 0.3f;
                p.maxLife = 0.3f;
                particles_.push_back(p);
            }
        } else if (hitScene) {
            // Hit wall or door
            Vec3 hitP = ray.PointAt(sceneHitDist);
            if (hitDoor && hitDoor->state != DoorState::BlownOffHinges) {
                hitDoor->health -= 25.0f;
                if (hitDoor->health <= 0.0f) {
                    hitDoor->state = DoorState::Open;
                    hitDoor->openAngle = PI * 0.5f;
                }
            }

            for (int i = 0; i < 4; ++i) {
                BulletHitParticle p;
                p.position = hitP;
                p.velocity = sceneHitNorm * 0.8f + Vec3((float)(rand()%10-5)*0.2f, (float)(rand()%10-5)*0.2f, (float)(rand()%10-5)*0.2f);
                p.color = Vec4(1.0f, 0.85f, 0.4f, 1.0f);
                p.life = 0.25f;
                p.maxLife = 0.25f;
                particles_.push_back(p);
            }
        }
    } else if (currentWeapon_ == WeaponType::BreachChargeC4) {
        PlaceBreachCharge(scene, hand);
    } else if (currentWeapon_ == WeaponType::FlashbangGrenade) {
        Vec3 forward = hand.aimOrientation.Rotate(Vec3(0, 0, 1)).Normalized();
        ThrowFlashbang(hand.aimPosition, forward);
    }
}

void WeaponSystem::PlaceBreachCharge(ApartmentScene& scene, const HandState& hand) {
    if (currentAmmo_ <= 0) return;

    Vec3 forward = hand.aimOrientation.Rotate(Vec3(0, 0, 1)).Normalized();
    Ray ray(hand.aimPosition, forward);
    float hitDist = 2.0f;
    Vec3 hitNorm;
    TacticalDoor* hitDoor = nullptr;

    if (scene.Raycast(ray, 2.5f, hitDist, hitNorm, &hitDoor) && hitDoor != nullptr) {
        currentAmmo_--;
        fireCooldown_ = 0.6f;

        ActiveC4 c4;
        c4.position = ray.PointAt(hitDist) + hitNorm * 0.02f;
        c4.orientation = hand.aimOrientation;
        c4.attachedDoor = hitDoor;
        c4.timer = 2.5f;
        c4.detonated = false;
        hitDoor->isC4Attached = true;
        deployedC4_.push_back(c4);

        AudioEngine::Instance().PlayRadioChirp();
    }
}

void WeaponSystem::ThrowFlashbang(const Vec3& spawnPos, const Vec3& throwDir) {
    if (currentAmmo_ <= 0) return;

    currentAmmo_--;
    fireCooldown_ = 0.8f;

    ActiveFlashbang fb;
    fb.position = spawnPos + throwDir * 0.2f;
    fb.velocity = throwDir * 9.5f + Vec3(0, 2.2f, 0); // parabolic arc
    fb.timer = 1.8f;
    fb.detonated = false;
    activeFlashbangs_.push_back(fb);

    AudioEngine::Instance().PlayRadioChirp();
}

void WeaponSystem::Update(float dt, ApartmentScene& scene, EnemyManager& enemyManager) {
    if (fireCooldown_ > 0.0f) fireCooldown_ -= dt;
    recoilAngle_ = std::max(0.0f, recoilAngle_ - dt * 1.8f);
    muzzleFlashIntensity_ = std::max(0.0f, muzzleFlashIntensity_ - dt * 12.0f);
    flashbangScreenBlind_ = std::max(0.0f, flashbangScreenBlind_ - dt * 0.35f);

    // Update C4 countdowns
    for (auto& c4 : deployedC4_) {
        if (c4.detonated) continue;
        c4.timer -= dt;
        if (c4.timer <= 0.0f) {
            c4.detonated = true;
            if (c4.attachedDoor) {
                c4.attachedDoor->BreachExplode();
                int room = scene.GetCurrentRoomIndex(c4.position);
                enemyManager.AlertEnemiesInRoom(room, c4.position);
                enemyManager.StunEnemiesInRadius(c4.position, 6.0f);
            }
        }
    }

    // Update Flashbang physics and detonation
    for (auto& fb : activeFlashbangs_) {
        if (fb.detonated) continue;

        fb.timer -= dt;
        fb.velocity.y -= 9.81f * dt; // gravity
        fb.position += fb.velocity * dt;

        // Bounce on floor
        if (fb.position.y <= 0.08f) {
            fb.position.y = 0.08f;
            fb.velocity.y = -fb.velocity.y * 0.45f;
            fb.velocity.x *= 0.65f;
            fb.velocity.z *= 0.65f;
        }

        if (fb.timer <= 0.0f) {
            fb.detonated = true;
            AudioEngine::Instance().PlayFlashbang();
            flashbangScreenBlind_ = 1.0f; // blind effect
            enemyManager.StunEnemiesInRadius(fb.position, 8.0f);
        }
    }

    // Update particles
    for (auto& p : particles_) {
        p.position += p.velocity * dt;
        p.life -= dt;
    }
    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(), [](const BulletHitParticle& p) {
            return p.life <= 0.0f;
        }),
        particles_.end()
    );
}

void WeaponSystem::Render(const Mat4& view, const Mat4& proj, const HandState& rightHand, const HandState& leftHand) const {
    const auto& litShader = ShaderLibrary::Instance().Lit();
    litShader.Use();
    litShader.SetMat4("uView", view);
    litShader.SetMat4("uProj", proj);
    litShader.SetVec3("uMuzzleFlashPos", muzzleFlashPos_);
    litShader.SetFloat("uMuzzleIntensity", muzzleFlashIntensity_);

    // 1. Render Active Weapon in Right Hand
    if (rightHand.isTracked) {
        Quat recoilQuat = Quat::FromAxisAngle(Vec3(1, 0, 0), -recoilAngle_);
        Mat4 weaponTransform = Mat4::TRS(rightHand.aimPosition, rightHand.aimOrientation * recoilQuat, Vec3(1, 1, 1));
        litShader.SetMat4("uModel", weaponTransform);
        litShader.SetVec4("uTint", Vec4(1, 1, 1, 1));

        if (currentWeapon_ == WeaponType::CarbineMK18) {
            if (carbineMesh_) carbineMesh_->Draw();
        } else if (currentWeapon_ == WeaponType::PistolSuppressed) {
            if (pistolMesh_) pistolMesh_->Draw();

            // Render laser beam
            if (laserBeamMesh_) {
                const auto& wireShader = ShaderLibrary::Instance().Wireframe();
                wireShader.Use();
                wireShader.SetMat4("uView", view);
                wireShader.SetMat4("uProj", proj);
                wireShader.SetMat4("uModel", weaponTransform * Mat4::Translation(Vec3(0, -0.008f, 0.12f)));
                wireShader.SetVec4("uColor", Vec4(1.0f, 0.05f, 0.05f, 0.85f));
                wireShader.SetFloat("uPulseTime", 0.0f);
                laserBeamMesh_->Draw();
                litShader.Use();
            }
        } else if (currentWeapon_ == WeaponType::BreachChargeC4) {
            if (c4Mesh_) c4Mesh_->Draw();
        } else if (currentWeapon_ == WeaponType::FlashbangGrenade) {
            if (flashbangMesh_) flashbangMesh_->Draw();
        }
    }

    // 2. Render Deployed C4 on doors
    for (const auto& c4 : deployedC4_) {
        if (c4.detonated) continue;
        Mat4 c4Tr = Mat4::TRS(c4.position, c4.orientation, Vec3(1, 1, 1));
        litShader.SetMat4("uModel", c4Tr);
        if (c4Mesh_) c4Mesh_->Draw();
    }

    // 3. Render Active Thrown Flashbangs
    for (const auto& fb : activeFlashbangs_) {
        if (fb.detonated) continue;
        Mat4 fbTr = Mat4::TRS(fb.position, Quat::Identity(), Vec3(1, 1, 1));
        litShader.SetMat4("uModel", fbTr);
        if (flashbangMesh_) flashbangMesh_->Draw();
    }

    // 4. Render Hand Tracking Joints (if hand tracking active)
    auto RenderJoints = [&](const HandState& hand) {
        if (hand.isHandTrackingActive) {
            for (size_t j = 0; j < hand.joints.size(); ++j) {
                if (hand.joints[j].isValid) {
                    Mat4 jTr = Mat4::TRS(hand.joints[j].position, Quat::Identity(), Vec3(1, 1, 1));
                    litShader.SetMat4("uModel", jTr);
                    if (handJointSphereMesh_) handJointSphereMesh_->Draw();
                }
            }
        }
    };
    RenderJoints(leftHand);
    RenderJoints(rightHand);

    // 5. Render Particle Sparks with Additive Blending
    if (!particles_.empty() && particleQuadMesh_) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);

        const auto& pShader = ShaderLibrary::Instance().Particle();
        pShader.Use();
        pShader.SetMat4("uView", view);
        pShader.SetMat4("uProj", proj);

        for (const auto& p : particles_) {
            Mat4 pTr = Mat4::TRS(p.position, Quat::Identity(), Vec3(1, 1, 1));
            pShader.SetMat4("uModel", pTr);
            float alpha = p.life / p.maxLife;
            pShader.SetVec4("uColor", Vec4(p.color.x, p.color.y, p.color.z, alpha));
            particleQuadMesh_->Draw();
        }

        glDisable(GL_BLEND);
    }
}

} // namespace breachvr
