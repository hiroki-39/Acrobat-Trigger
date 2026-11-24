#pragma once
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Graphics/3d/Particle/ParticleRenderer.h"
#include "KHEngine/Math/Vector3.h"
#include "KHEngine/Math/Vector4.h"
#include "KHEngine/Math/Matrix4x4.h"
#include <vector>
#include <memory>

class Object3dCommon;

/// <summary>
/// プレイヤー武器共通のサイバーVFXシステム
/// ボムVFXで確立した「白 → 青/シアン → マゼンタ」の3色グラデーション、
/// 加算合成多層幾何学（十字閃光・破線デジタルリング・グリッチ線）、
/// および小型サイバーボクセル粒子を一括管理する
/// </summary>
class CyberVfxSystem {
public:
    static constexpr size_t kMaxVoxelPool = 160;
    static constexpr size_t kMaxBillboardPool = 128;

    enum class TexKind {
        StarBlink = 0,
        DigitalRing,
        SoftFade,
        LineGlow
    };

    struct BillboardParticle {
        bool active = false;
        TexKind texKind = TexKind::StarBlink;
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };
        Vector3 startScale = { 1.0f, 1.0f, 1.0f };
        Vector3 endScale = { 1.0f, 1.0f, 1.0f };
        bool useBillboard = true;
        Vector3 rotEuler = { 0.0f, 0.0f, 0.0f };
        float rotZ = 0.0f;
        float rotSpeed = 0.0f;
        float delay = 0.0f;
        float life = 0.0f;
        float maxLife = 0.25f;
        bool useCustomColor = false;
        Vector4 startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        Vector4 endColor = { 1.0f, 1.0f, 1.0f, 0.0f };
        float alphaScale = 1.0f;
        bool rapidShrink = false;
    };

    struct VoxelParticle {
        bool active = false;
        std::unique_ptr<Object3d> obj = nullptr;
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };
        Vector3 rotation = { 0.0f, 0.0f, 0.0f };
        Vector3 rotSpeed = { 0.0f, 0.0f, 0.0f };
        Vector3 scale = { 1.0f, 1.0f, 1.0f };
        float drag = 2.0f;
        float life = 0.0f;
        float maxLife = 0.35f;
    };

    void Initialize(Object3dCommon* object3dCommon);
    void Clear();
    void Update(float gameSpeed);
    void Draw();

    /// <summary>
    /// 通常弾・ファンネル用のシャープなマズルフラッシュ（十字閃光＋極小破線リング＋ボクセル粒）
    /// </summary>
    void TriggerMuzzleFlash(const Vector3& pos, float scale = 1.0f);

    /// <summary>
    /// 拡散弾用のワイドなマズルフラッシュ（扇状破線リング＋グリッチ線＋ボクセル粒）
    /// </summary>
    void TriggerSpreadMuzzle(const Vector3& pos);

    /// <summary>
    /// 飛行中のデジタルブロック残像（極小キューブ）を1個発生
    /// </summary>
    void SpawnTrailVoxel(const Vector3& pos, const Vector3& vel, float size = 0.22f, float maxLife = 0.25f);

    /// <summary>
    /// メモリ武器・ファンネル体当たり等の着弾ヒット用 正方形ボクセルエフェクト
    /// </summary>
    void TriggerBulletHit(const Vector3& pos, const Vector3& vel);

    static Vector4 EvaluateThreeStageColor(float ratio, float alphaScale = 1.0f);

private:
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    void SpawnBillboard(const BillboardParticle& desc);
    void SpawnVoxel(const Vector3& pos, const Vector3& vel, const Vector3& rotSpeed, const Vector3& scale, float maxLife, float drag);

    Object3dCommon* object3dCommon_ = nullptr;
    std::vector<BillboardParticle> billboardPool_;
    std::vector<VoxelParticle> voxelPool_;

    ParticleRenderer starBlinkRenderer_;
    ParticleRenderer digitalRingRenderer_;
    ParticleRenderer softFadeRenderer_;
    ParticleRenderer lineGlowRenderer_;
    uint32_t starBlinkCount_ = 0;
    uint32_t digitalRingCount_ = 0;
    uint32_t softFadeCount_ = 0;
    uint32_t lineGlowCount_ = 0;
};
