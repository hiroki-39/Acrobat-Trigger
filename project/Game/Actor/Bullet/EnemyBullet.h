#pragma once
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <memory>

class Player;

class EnemyBullet {
public:
    void Initialize(Object3dCommon* object3dCommon, const Vector3& position, const Vector3& velocity, bool isHoming = false, Player* targetPlayer = nullptr);
    void InitializeMissile(Object3dCommon* object3dCommon, const Vector3& position, const Vector3& launchVel, Player* targetPlayer, int salvoIndex = 0);

    void Update(float gameSpeed = 1.0f);
    void Update3DObjectOnly() { if (object_) object_->Update(); }
    void Draw();
    void DrawCollider();

    bool IsDead() const { return isDead_; }
    const Vector3& GetPosition() const { return object_ ? object_->GetTranslate() : velocity_; }
    const Vector3& GetPreviousPosition() const { return previousPosition_; }
    void OnCollision() { isDead_ = true; }

    // 迎撃（撃墜）可能ミサイル関連
    bool IsDestructible() const { return isDestructible_; }
    bool IsMissile() const { return isMissile_; }
    void OnDamaged(int damage = 1);
    float GetRadius() const { return isMissile_ ? 2.5f : 1.2f; }

private:
    std::unique_ptr<Object3d> object_ = nullptr; // 高エネルギーレーザービーム または ミサイル
    std::unique_ptr<Object3d> colliderObject_ = nullptr;
    Vector3 velocity_ = { 0.0f, 0.0f, -1.5f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 previousPosition_ = { 0.0f, 0.0f, 0.0f };
    bool isDead_ = false;
    float deathTimer_ = 180.0f;
    
    bool isHoming_ = false;
    Player* targetPlayer_ = nullptr;

    // 撃墜可能ミサイルパラメータ
    bool isMissile_ = false;
    bool isDestructible_ = false;
    int hp_ = 1;
    int missilePhase_ = 0; // 0: 上昇, 1: 追尾突進
    float missilePhaseTimer_ = 0.0f;
    int salvoIndex_ = 0;
    bool hasPassedPlayer_ = false;       // 自機を通過したか（オービット旋回防止フラグ）
    float prevDistToPlayer_ = 99999.0f;  // プレイヤーとの最短距離監視用
};
