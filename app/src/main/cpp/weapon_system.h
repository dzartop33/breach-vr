#pragma once
// Breach VR - Weapon & Equipment Systems: MK18 Carbine, Suppressed P10, C4 Charge, M84 Flashbang

#include "math_types.h"
#include "mesh_builder.h"
#include "gl_shaders.h"
#include "apartment_scene.h"
#include <vector>
#include <memory>

namespace breachvr {

// Forward declaration
class EnemyManager;

enum class WeaponType {
    CarbineMK18,
    PistolSuppressed,
    BreachChargeC4,
    FlashbangGrenade
};

struct ActiveC4 {
    Vec3 position;
    Quat orientation = Quat::Identity();
    TacticalDoor* attachedDoor = nullptr;
    float timer = 2.5f; // detonation countdown
    bool detonated = false;
};

struct ActiveFlashbang {
    Vec3 position;
    Vec3 velocity;
    float timer = 1.8f;
    bool detonated = false;
    bool isBouncing = false;
};

struct BulletHitParticle {
    Vec3 position;
    Vec3 velocity;
    Vec4 color;
    float life = 0.4f;
    float maxLife = 0.4f;
};

class WeaponSystem {
public:
    WeaponSystem() = default;

    bool Init();
    void Update(float dt, ApartmentScene& scene, EnemyManager& enemyManager);
    void Render(const Mat4& view, const Mat4& proj, const HandState& rightHand, const HandState& leftHand) const;

    void FireCurrentWeapon(const HandState& hand, ApartmentScene& scene, EnemyManager& enemyManager);
    void Reload();
    void SwitchWeapon();
    void ThrowFlashbang(const Vec3& spawnPos, const Vec3& throwDir);
    void PlaceBreachCharge(ApartmentScene& scene, const HandState& hand);

    WeaponType GetCurrentWeapon() const { return currentWeapon_; }
    int GetCurrentAmmo() const { return currentAmmo_; }
    int GetMagCapacity() const { return magCapacity_; }
    float GetMuzzleIntensity() const { return muzzleFlashIntensity_; }
    Vec3 GetMuzzlePosition() const { return muzzleFlashPos_; }

    float GetFlashbangBlindFactor() const { return flashbangScreenBlind_; }

private:
    WeaponType currentWeapon_ = WeaponType::CarbineMK18;

    int currentAmmo_ = 30;
    int magCapacity_ = 30;
    float fireCooldown_ = 0.0f;
    float fireRate_ = 0.11f; // seconds between shots (~550 RPM)
    float recoilAngle_ = 0.0f;

    float muzzleFlashIntensity_ = 0.0f;
    Vec3 muzzleFlashPos_{0, 0, 0};

    float flashbangScreenBlind_ = 0.0f; // 1.0 (white screen) to 0.0 (normal)

    std::vector<ActiveC4> deployedC4_;
    std::vector<ActiveFlashbang> activeFlashbangs_;
    std::vector<BulletHitParticle> particles_;

    // Meshes
    std::unique_ptr<Mesh> carbineMesh_;
    std::unique_ptr<Mesh> pistolMesh_;
    std::unique_ptr<Mesh> c4Mesh_;
    std::unique_ptr<Mesh> flashbangMesh_;
    std::unique_ptr<Mesh> laserBeamMesh_;
    std::unique_ptr<Mesh> particleQuadMesh_;
    std::unique_ptr<Mesh> handJointSphereMesh_;
};

} // namespace breachvr
