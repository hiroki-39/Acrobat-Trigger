#pragma once
#include "IWeapon.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <memory>
#include <vector>

/// <summary>
/// 爆弾兵器（スマートボム / サイバー・ディスラプター）
/// 前方へ射出され、高輝度プラズマ爆発で画面内の広範囲敵を殲滅＆敵弾消去
/// （動的確保を排したオブジェクトプール管理により、描画フリッカーおよび解放時クラッシュを根絶）
/// </summary>
class BombWeapon : public IWeapon {
public:
    static constexpr size_t kMaxBombPool = 3;

    struct BombSlot {
        bool isActive = false;
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };
        float flightDistance = 0.0f;
        float maxFlightDistance = 80.0f;
        float armingTimer = 0.0f; // 誤爆防止セーフティタイマー
        bool isExploding = false;
        float explodeTimer = 0.0f;
        float maxExplodeTime = 40.0f;
        float explodeDamageRadius = 45.0f; // ゲームプレイ上の敵・敵弾殲滅判定半径（広大）
        float maxVisualRadius = 15.0f;      // カメラ干渉・画面チカチカを防ぐ安全な視覚エフェクト最大半径

        std::unique_ptr<Object3d> bombObject = nullptr;
        std::unique_ptr<Object3d> shockwaveObject = nullptr;
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
    bool IsDepleted() const override;
    MemoryType GetType() const override { return MemoryType::BOMB; }
    float GetRemainingRatio() const override {
        return maxAmmo_ > 0 ? (static_cast<float>(ammo_) / static_cast<float>(maxAmmo_)) : 0.0f;
    }
    std::string GetAmmoText() const override;
    int GetAmmo() const override { return ammo_; }
    int GetMaxAmmo() const override { return maxAmmo_; }

    // 現在爆発中のボムがあるか判定（敵弾消去等の判定に使用）
    bool HasActiveExplosion(Vector3* outPos = nullptr, float* outRadius = nullptr) const;

private:
    Player* owner_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;

    int ammo_ = 3;
    int maxAmmo_ = 3;
    float cooldownTimer_ = 0.0f;

    // 事前生成済み固定プール（動的生成・破棄による D3D12 リソース解放クラッシュを完全防止）
    std::vector<BombSlot> bombPool_;
};
