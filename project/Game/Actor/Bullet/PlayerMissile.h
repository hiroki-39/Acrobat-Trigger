#pragma once
#include "BaseBullet.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <memory>

class Enemy;
class ArmoredTrainBoss;

class PlayerMissile : public BaseBullet {
public:
    enum class Phase {
        DROP,    // 発射直後、自機から落下・分離するフェーズ
        FLIGHT   // スラスターを点火し、ターゲットに向かって飛行するフェーズ
    };

    void Initialize(Object3dCommon* object3dCommon, const Vector3& position, const Vector3& rotation, const Vector3& velocity, Object3d* parent, Enemy* targetEnemy = nullptr, ArmoredTrainBoss* targetBoss = nullptr, int targetCarriageIndex = -1);
    void Update(float gameSpeed = 1.0f) override;
    void Update3DObjectOnly() override { if (object_) object_->Update(); }
    void Draw() override;
    void DrawCollider() override;

    bool IsDead() const override { return isDead_; }
    const Vector3& GetPosition() const override { return object_ ? object_->GetTranslate() : position_; }
    const Vector3& GetPreviousPosition() const override { return previousPosition_; }
    
    // 衝突時の処理
    void OnCollision() override { isDead_ = true; }

    Phase GetCurrentPhase() const { return currentPhase_; }

private:
    std::unique_ptr<Object3d> object_ = nullptr;
    std::unique_ptr<Object3d> colliderObject_ = nullptr;
    
    Vector3 spawnPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
    Vector3 previousPosition_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };

    bool isDead_ = false;
    float deathTimer_ = 180.0f; // 寿命（フレーム: 約3秒）
    float maxFlightDistance_ = 380.0f; // 最大飛行射程（m）
    
    Enemy* targetEnemy_ = nullptr;
    ArmoredTrainBoss* targetBoss_ = nullptr;
    int targetCarriageIndex_ = -1;

    Phase currentPhase_ = Phase::DROP;
    float phaseTimer_ = 0.0f;
    const float dropDuration_ = 12.0f; // 落下フェーズの長さ (フレーム数。12.0f = 0.2秒)
};
