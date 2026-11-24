#pragma once
#include "IWeapon.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <memory>
#include <vector>

/// <summary>
/// 爆弾兵器（スマートボム / サイバー・ディスラプター）
/// 外枠ケージ（bom_1）＋多面体コア（bom_core）の二重構造を持ち、
/// 飛行トレイルおよび広範囲爆発のすべてをサイバーボクセルブロック（cube.obj）で表現する
/// </summary>
class BombWeapon : public IWeapon {
public:
    static constexpr size_t kMaxBombPool = 3;
    static constexpr size_t kMaxVoxelPool = 180;

    struct BombSlot {
        bool isActive = false;
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };
        Vector3 frameRotation = { 0.0f, 0.0f, 0.0f };
        Vector3 coreRotation = { 0.0f, 0.0f, 0.0f };
        float pulseTimer = 0.0f;
        float trailTimer = 0.0f;
        float flightDistance = 0.0f;
        float maxFlightDistance = 80.0f;
        float armingTimer = 0.0f; // 誤爆防止セーフティタイマー
        bool isExploding = false;
        float explodeTimer = 0.0f;
        float maxExplodeTime = 40.0f;
        float explodeDamageRadius = 45.0f; // ゲームプレイ上の敵・敵弾殲滅判定半径（広大）

        std::unique_ptr<Object3d> bombObject = nullptr;     // 外枠ケージ（bom_1.obj）
        std::unique_ptr<Object3d> bombCoreObject = nullptr; // 内側多面体コア（bom_core.obj）
    };

    struct BombVoxelParticle {
        bool active = false;
        std::unique_ptr<Object3d> obj = nullptr;
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };
        Vector3 rotation = { 0.0f, 0.0f, 0.0f };
        Vector3 rotSpeed = { 0.0f, 0.0f, 0.0f };
        Vector3 scale = { 1.0f, 1.0f, 1.0f };
        Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float drag = 2.5f;
        float life = 0.0f;
        float maxLife = 0.6f;
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
    void SpawnVoxel(const Vector3& pos, const Vector3& vel, const Vector3& rotSpeed, const Vector3& scale, const Vector4& color, float maxLife, float drag);
    void TriggerVoxelExplosion(const Vector3& center);
    void UpdateVoxels(float gameSpeed);

    Player* owner_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;

    int ammo_ = 3;
    int maxAmmo_ = 3;
    float cooldownTimer_ = 0.0f;

    // 事前生成済み固定プール（動的生成・破棄による D3D12 リソース解放クラッシュを完全防止）
    std::vector<BombSlot> bombPool_;
    std::vector<BombVoxelParticle> voxelPool_;
};
