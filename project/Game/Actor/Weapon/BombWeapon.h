#pragma once
#include "IWeapon.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Graphics/3d/Particle/ParticleRenderer.h"
#include <memory>
#include <vector>
#include <deque>

/// <summary>
/// 爆弾兵器（スマートボム / サイバー・ディスラプター）
/// 外枠ケージ（bom_1）＋多面体コア（bom_core）の二重構造と、
/// 白→シアン→マゼンタの3色グラデーション×多層幾何学VFX（加算合成）を備える
/// </summary>
class BombWeapon : public IAmmoWeapon {
public:
    static constexpr size_t kMaxBombPool = 3;
    static constexpr size_t kMaxVoxelPool = 180;
    static constexpr size_t kMaxBillboardVfx = 128;
    static constexpr uint32_t kMaxTrailPoints = 32;
    static constexpr uint32_t kMaxRibbonVertices = 1024;

    struct TrailPoint {
        Vector3 position;
        float age = 0.0f;
        float maxLife = 0.28f;
    };

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
        float explodeDamageRadius = 45.0f; // ゲームプレイ上の敵・敵弾殲滅判定半径

        std::unique_ptr<Object3d> bombObject = nullptr;     // 外枠ケージ（bom_1.obj）
        std::unique_ptr<Object3d> bombCoreObject = nullptr; // 内側多面体コア（bom_core.obj）
        std::deque<TrailPoint> trailPoints;                 // 弾道リボントレイル履歴
    };

    struct BombVoxelParticle {
        bool active = false;
        std::unique_ptr<Object3d> obj = nullptr;
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };
        Vector3 rotation = { 0.0f, 0.0f, 0.0f };
        Vector3 rotSpeed = { 0.0f, 0.0f, 0.0f };
        Vector3 scale = { 1.0f, 1.0f, 1.0f };
        float drag = 2.5f;
        float life = 0.0f;
        float maxLife = 0.6f;
        bool useThreeStageColor = true;
        Vector4 fixedColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    enum class VfxTexKind {
        StarBlink = 0,
        DigitalRing,
        SoftFade,
        LineGlow
    };

    struct BillboardVfxParticle {
        bool active = false;
        VfxTexKind texKind = VfxTexKind::StarBlink;
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };
        Vector3 startScale = { 1.0f, 1.0f, 1.0f };
        Vector3 endScale = { 1.0f, 1.0f, 1.0f };
        bool useBillboard = true;
        Vector3 rotEuler = { 0.0f, 0.0f, 0.0f }; // 3D空間配置用の傾き (X, Y, Z)
        float rotZ = 0.0f;
        float rotSpeed = 0.0f;
        float delay = 0.0f;
        float life = 0.0f;
        float maxLife = 0.3f;
        bool useCustomColor = false;
        Vector4 startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        Vector4 endColor = { 1.0f, 1.0f, 1.0f, 0.0f };
        float alphaScale = 1.0f;
        bool rapidShrink = false; // コア閃光用：最大輝度点灯後に急速縮小
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
    struct RibbonVertex {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    void InitBillboardRenderers();
    void InitTrailRenderers();

    void SpawnVoxel(const Vector3& pos, const Vector3& vel, const Vector3& rotSpeed, const Vector3& scale, float maxLife, float drag, bool useThreeStage = true, const Vector4& fixedColor = { 1.0f, 1.0f, 1.0f, 1.0f });
    void SpawnBillboardVfx(const BillboardVfxParticle& desc);
    void TriggerMuzzleFlash(const Vector3& muzzlePos);
    void TriggerExplosionVfx(const Vector3& center);

    void UpdateVoxels(float dt);
    void UpdateBillboardVfx(float dt, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const Matrix4x4& billboardMatrix);
    void UpdateTrails(float dt, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix);

    static Vector4 EvaluateThreeStageColor(float ratio, float alphaScale = 1.0f);

    Player* owner_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;

    int ammo_ = 3;
    int maxAmmo_ = 3;
    float cooldownTimer_ = 0.0f;

    std::vector<BombSlot> bombPool_;
    std::vector<BombVoxelParticle> voxelPool_;
    std::vector<BillboardVfxParticle> billboardPool_;

    // 加算合成ビルボードレンダラー（テクスチャ種別ごと）
    ParticleRenderer starBlinkRenderer_;
    ParticleRenderer digitalRingRenderer_;
    ParticleRenderer softFadeRenderer_;
    ParticleRenderer lineGlowRenderer_;
    uint32_t starBlinkCount_ = 0;
    uint32_t digitalRingCount_ = 0;
    uint32_t softFadeCount_ = 0;
    uint32_t lineGlowCount_ = 0;

    // 弾道リボントレイル（先端: 白〜シアン / 後端: シアン〜マゼンタ）
    ParticleRenderer trailHeadRenderer_;
    ParticleRenderer trailTailRenderer_;
    uint32_t trailHeadVertexCount_ = 0;
    uint32_t trailTailVertexCount_ = 0;
};
