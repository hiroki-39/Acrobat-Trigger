#pragma once
#include "IWeapon.h"

/// <summary>
/// 基本装備：通常弾
/// 弾数無限の高速連射ショット
/// </summary>
class NormalWeapon : public IWeapon {
public:
    void Initialize(Player* owner, Object3dCommon* object3dCommon) override;
    void Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& enemies, ArmoredTrainBoss* boss, Object3d* parentCamera) override;
    void Attack(Input* input,
                std::list<std::unique_ptr<PlayerBullet>>& bullets,
                std::list<std::unique_ptr<PlayerMissile>>& missiles,
                const std::list<std::unique_ptr<Enemy>>& enemies,
                ArmoredTrainBoss* boss,
                Object3d* parentCamera,
                float gameSpeed) override;
    void Draw() override;
    void DrawUI() override;
    void Reset() override;
    bool IsDepleted() const override { return false; } // 通常弾は弾数無限
    MemoryType GetType() const override { return MemoryType::NONE; }
    float GetRemainingRatio() const override { return 1.0f; }
    std::string GetAmmoText() const override { return "INF"; }

private:
    Player* owner_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;

    float attackTimer_ = 0.0f;
    float attackInterval_ = 15.0f;
    float bulletSpeed_ = 8.5f;
};
