#pragma once
#include "BaseBullet.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <memory>

class Player;

class EnemyBullet : public BaseBullet {
public:
    void Initialize(Object3dCommon* object3dCommon, const Vector3& position, const Vector3& velocity, bool isHoming = false, Player* targetPlayer = nullptr);

    void Update(float gameSpeed = 1.0f) override;
    void Update3DObjectOnly() override { if (object_) object_->Update(); }
    void Draw() override;
    void DrawCollider() override;

    bool IsDead() const override { return isDead_; }
    const Vector3& GetPosition() const override { return object_ ? object_->GetTranslate() : velocity_; }
    const Vector3& GetPreviousPosition() const override { return previousPosition_; }
    void OnCollision() override { isDead_ = true; }

    // 迎撃（撃墜）可能ミサイル関連の仮想インターフェース
    virtual bool IsDestructible() const { return false; }
    virtual bool IsMissile() const { return false; }
    virtual void OnDamaged(int damage = 1) { (void)damage; }
    virtual float GetRadius() const { return 1.2f; }

protected:
    Object3d* GetObject3d() const { return object_.get(); }
    void SetObjects(std::unique_ptr<Object3d> obj, std::unique_ptr<Object3d> colObj) {
        object_ = std::move(obj);
        colliderObject_ = std::move(colObj);
    }
    const Vector3& GetVelocity() const { return velocity_; }
    void SetVelocity(const Vector3& v) { velocity_ = v; }
    const Vector3& GetRotation() const { return rotation_; }
    void SetRotation(const Vector3& r) { rotation_ = r; }
    void SetPreviousPosition(const Vector3& p) { previousPosition_ = p; }
    void SetDead(bool dead) { isDead_ = dead; }
    float GetDeathTimer() const { return deathTimer_; }
    void SetDeathTimer(float t) { deathTimer_ = t; }
    void DecrementDeathTimer(float amount) { deathTimer_ -= amount; }
    Player* GetTargetPlayer() const { return targetPlayer_; }
    void SetTargetPlayer(Player* p) { targetPlayer_ = p; }

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
};

/// <summary>
/// 撃墜可能な敵誘導ミサイル（EnemyBulletの派生クラスとしてポリモーフィズムで統一管理）
/// </summary>
class EnemyMissile : public EnemyBullet {
public:
    void Initialize(Object3dCommon* object3dCommon, const Vector3& position, const Vector3& launchVel, Player* targetPlayer, int salvoIndex = 0);
    void Update(float gameSpeed = 1.0f) override;

    bool IsDestructible() const override { return true; }
    bool IsMissile() const override { return true; }
    void OnDamaged(int damage = 1) override;
    float GetRadius() const override { return 2.5f; }

private:
    int hp_ = 1;
    int missilePhase_ = 0; // 0: 上昇, 1: 追尾突進
    float missilePhaseTimer_ = 0.0f;
    int salvoIndex_ = 0;
    bool hasPassedPlayer_ = false;
    float prevDistToPlayer_ = 99999.0f;
};
