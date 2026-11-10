#pragma once
#include "IWeapon.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <memory>
#include <vector>

/// <summary>
/// 爆弾兵器（スマートボム / サイバー・ディスラプター）
/// 前方へ射出され、巨大なサイバーグリッド爆発で画面内の広範囲敵を殲滅＆敵弾消去
/// </summary>
class BombWeapon : public IWeapon {
public:
    struct ActiveBomb {
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };
        float flightDistance = 0.0f;
        float maxFlightDistance = 75.0f;
        float armingTimer = 0.0f; // 誤爆防止セーフティタイマー
        bool isExploding = false;
        float explodeTimer = 0.0f;
        float maxExplodeTime = 45.0f;
        float explodeRadius = 42.0f;
        std::unique_ptr<Object3d> bombObject = nullptr;
        std::unique_ptr<Object3d> shockwaveObject = nullptr;
        std::unique_ptr<Object3d> innerCoreObject = nullptr;
    };

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
    bool IsDepleted() const override { return ammo_ <= 0 && activeBombs_.empty(); }
    MemoryType GetType() const override { return MemoryType::BOMB; }
    float GetRemainingRatio() const override {
        return maxAmmo_ > 0 ? (static_cast<float>(ammo_) / static_cast<float>(maxAmmo_)) : 0.0f;
    }
    std::string GetAmmoText() const override;
    int GetAmmo() const override { return ammo_; }
    int GetMaxAmmo() const override { return maxAmmo_; }

    // 現在爆発中のボムがあるか判定
    bool HasActiveExplosion(Vector3* outPos = nullptr, float* outRadius = nullptr) const;

private:
    Player* owner_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;

    int ammo_ = 3;
    int maxAmmo_ = 3;
    float cooldownTimer_ = 0.0f;

    std::vector<ActiveBomb> activeBombs_;
};
