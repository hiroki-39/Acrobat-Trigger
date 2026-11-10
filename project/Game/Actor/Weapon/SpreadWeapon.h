#pragma once
#include "IWeapon.h"
#include <memory>
#include <vector>

/// <summary>
/// 拡散弾兵器（スプレッドショット / 5-Wayマルチスプレッド）
/// 扇状に5発の高速プラズマ弾を同時発射し、敵編隊の一掃や至近距離での大ダメージを狙う
/// </summary>
class SpreadWeapon : public IWeapon {
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
    bool IsDepleted() const override { return ammo_ <= 0; }
    MemoryType GetType() const override { return MemoryType::SPREAD; }
    float GetRemainingRatio() const override {
        return maxAmmo_ > 0 ? (static_cast<float>(ammo_) / static_cast<float>(maxAmmo_)) : 0.0f;
    }
    std::string GetAmmoText() const override;

private:
    Player* owner_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;

    int ammo_ = 30;
    int maxAmmo_ = 30;
    float attackTimer_ = 0.0f;
    float attackInterval_ = 14.0f;
    float bulletSpeed_ = 8.5f;
};
