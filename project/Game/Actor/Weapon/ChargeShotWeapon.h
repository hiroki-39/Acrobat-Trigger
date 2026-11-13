#pragma once
#include "IWeapon.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <memory>

/// <summary>
/// チャージショット兵器（高威力貫通スナイパー弾）
/// スペースキー長押しでエネルギーを収束し、離すと高威力の貫通ビームを放つ
/// </summary>
class ChargeShotWeapon : public IWeapon {
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
    MemoryType GetType() const override { return MemoryType::CHARGE_SHOT; }
    float GetRemainingRatio() const override {
        return maxAmmo_ > 0 ? (static_cast<float>(ammo_) / static_cast<float>(maxAmmo_)) : 0.0f;
    }
    std::string GetAmmoText() const override;

private:
    Player* owner_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;

    int ammo_ = 6;
    int maxAmmo_ = 6;

    float chargeTimer_ = 0.0f;
    float maxChargeTime_ = 48.0f; // 約0.8秒でフルチャージ完了（心地よいリズム）
    bool isCharging_ = false;
    bool hasPlayedMaxChargeSE_ = false;

    // チャージエフェクト用オブジェクト
    std::unique_ptr<Object3d> chargeCoreObject_ = nullptr;  // 機首の白熱コア
    std::unique_ptr<Object3d> chargeAuraObject_ = nullptr;  // 外周のエネルギーオーラ
    float coreAnimTimer_ = 0.0f;
};
