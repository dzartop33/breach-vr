#pragma once
// Breach VR - CQB Tactical Enemy AI: Perception, States, Cover, Combat

#include "math_types.h"
#include "mesh_builder.h"
#include "gl_shaders.h"
#include "apartment_scene.h"
#include <vector>
#include <memory>
#include <string>

namespace breachvr {

enum class EnemyState {
    Guard,
    Suspicious,
    Engaging,
    Stunned,
    Dead
};

struct CQBEnemy {
    int id = 0;
    std::string callsign;
    Vec3 position;
    Quat orientation = Quat::Identity();
    EnemyState state = EnemyState::Guard;

    float health = 100.0f;
    float maxHealth = 100.0f;
    int roomIndex = 1;

    float stunTimer = 0.0f;
    float attackCooldown = 1.0f;
    float burstTimer = 0.0f;
    int burstShotsRemaining = 0;

    Vec3 coverPosition;
    bool hasCover = false;

    float muzzleFlash = 0.0f;

    AABB GetHeadAABB() const;
    AABB GetTorsoAABB() const;
    AABB GetLegsAABB() const;

    bool ApplyDamage(float amount, bool isHeadshot);
    void Stun(float duration);
};

class EnemyManager {
public:
    EnemyManager() = default;

    bool Init(const ApartmentScene& scene);
    void Update(float dt, const Vec3& playerHeadPos, ApartmentScene& scene);
    void Render(const Mat4& view, const Mat4& proj) const;

    bool CheckBulletHit(const Ray& ray, float maxDist, float damage, float& hitDistOut, bool& killedOut);
    void AlertEnemiesInRoom(int roomIndex, const Vec3& soundPos);
    void StunEnemiesInRadius(const Vec3& explosionPos, float radius);

    int GetAliveCount() const;
    const std::vector<CQBEnemy>& GetEnemies() const { return enemies_; }

private:
    std::vector<CQBEnemy> enemies_;
    std::unique_ptr<Mesh> enemyMesh_;
};

} // namespace breachvr
