#define NOMINMAX
#include "BombWeapon.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Enemy/Enemy.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Graphics/Billboard/Billboard.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "KHEngine/Input/Input.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>

namespace {
    constexpr const char* kBombFrameModel = "resources/3dModels/bom/bom_1/bom_1.obj";
    constexpr const char* kBombCoreModel  = "resources/3dModels/bom/bom_core/bom_core.obj";

    struct ParticleMatCB {
        Vector4 color;
        int32_t selectLightings;
        int32_t enableLightingAsInt;
        float padding;
        float alphaReference;
        Matrix4x4 uvTransform;
    };

    Vector4 LerpColor(const Vector4& a, const Vector4& b, float t) {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t
        };
    }
}

Vector4 BombWeapon::EvaluateThreeStageColor(float ratio, float alphaScale) {
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    const Vector4 kWhite      = { 1.3f, 1.4f, 1.6f, 1.0f };
    const Vector4 kBlue       = { 0.08f, 0.48f, 1.65f, 0.98f };
    const Vector4 kMagenta    = { 1.45f, 0.0f, 0.88f, 0.88f };
    const Vector4 kMagentaEnd = { 1.30f, 0.0f, 0.80f, 0.0f };

    Vector4 result;
    if (ratio < 0.08f) {
        result = kWhite;
    } else if (ratio < 0.45f) {
        float t = (ratio - 0.08f) / 0.37f;
        result = LerpColor(kWhite, kBlue, t);
    } else if (ratio < 0.80f) {
        float t = (ratio - 0.45f) / 0.35f;
        result = LerpColor(kBlue, kMagenta, t);
    } else {
        float t = (ratio - 0.80f) / 0.20f;
        result = LerpColor(kMagenta, kMagentaEnd, t);
    }
    result.w *= alphaScale;
    return result;
}

void BombWeapon::InitBillboardRenderers() {
    auto services = EngineServices::GetInstance();
    if (!services) return;
    auto dxCommon = services->GetDirectXCommon();
    auto srvManager = services->GetSrvManager();
    if (!dxCommon || !srvManager) return;

    std::vector<RibbonVertex> quadVerts = {
        { { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
        { {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
        { { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
        { {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
        { {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
        { { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
    };

    ParticleMatCB mat{};
    mat.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    mat.selectLightings = 2;
    mat.enableLightingAsInt = 0;
    mat.alphaReference = 0.0f;
    mat.uvTransform = Matrix4x4::Identity();

    auto setup = [&](ParticleRenderer& r) {
        r.Initialize(dxCommon, srvManager, static_cast<uint32_t>(kMaxBillboardVfx));
        r.CreateVertexBuffer(quadVerts.data(), static_cast<uint32_t>(quadVerts.size()), sizeof(RibbonVertex));
        r.CreateMaterialBuffer(sizeof(ParticleMatCB), &mat);
    };

    setup(starBlinkRenderer_);
    setup(digitalRingRenderer_);
    setup(softFadeRenderer_);
    setup(lineGlowRenderer_);
}

void BombWeapon::InitTrailRenderers() {
    auto services = EngineServices::GetInstance();
    if (!services) return;
    auto dxCommon = services->GetDirectXCommon();
    auto srvManager = services->GetSrvManager();
    if (!dxCommon || !srvManager) return;

    std::vector<RibbonVertex> dummy(kMaxRibbonVertices);
    ParticleMatCB mat{};
    mat.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    mat.selectLightings = 2;
    mat.enableLightingAsInt = 0;
    mat.alphaReference = 0.0f;
    mat.uvTransform = Matrix4x4::Identity();

    trailHeadRenderer_.Initialize(dxCommon, srvManager, 1);
    trailHeadRenderer_.CreateVertexBuffer(dummy.data(), kMaxRibbonVertices, sizeof(RibbonVertex));
    trailHeadRenderer_.CreateMaterialBuffer(sizeof(ParticleMatCB), &mat);

    trailTailRenderer_.Initialize(dxCommon, srvManager, 1);
    trailTailRenderer_.CreateVertexBuffer(dummy.data(), kMaxRibbonVertices, sizeof(RibbonVertex));
    trailTailRenderer_.CreateMaterialBuffer(sizeof(ParticleMatCB), &mat);
}

void BombWeapon::Initialize(Player* owner, Object3dCommon* object3dCommon) {
    owner_ = owner;
    object3dCommon_ = object3dCommon;
    ammo_ = 3;
    maxAmmo_ = 3;
    cooldownTimer_ = 0.0f;

    ModelManager::GetInstance()->LoadModel(kBombFrameModel);
    ModelManager::GetInstance()->LoadModel(kBombCoreModel);
    ModelManager::GetInstance()->LoadModel("cube.obj");

    // 仕様書指定テクスチャのロード
    auto texMgr = TextureManager::GetInstance();
    if (texMgr) {
        texMgr->LoadTexture("star_blink.png");
        texMgr->LoadTexture("digital_ring.png");
        texMgr->LoadTexture("soft_fade.png");
        texMgr->LoadTexture("line_glow.png");
        texMgr->LoadTexture("circle2.png");
    }

    InitBillboardRenderers();
    InitTrailRenderers();

    bombPool_.clear();
    bombPool_.resize(kMaxBombPool);

    for (size_t i = 0; i < kMaxBombPool; ++i) {
        auto& slot = bombPool_[i];
        slot.isActive = false;
        slot.isExploding = false;

        // 外枠ケージ（bom_1.obj）：ホログラムシアンブルー
        slot.bombObject = std::make_unique<Object3d>();
        slot.bombObject->Initialize(object3dCommon_);
        slot.bombObject->SetModel(kBombFrameModel);
        slot.bombObject->SetEnableLighting(false);
        slot.bombObject->SetEnvironmentCoefficient(0.0f);
        slot.bombObject->SetSelectLightings(12);
        slot.bombObject->SetColor({ 0.20f, 0.75f, 1.0f, 0.92f });
        slot.bombObject->SetScale({ 0.14f, 0.14f, 0.14f });

        // 内側多面体コア（bom_core.obj）：ホログラムディープパープル
        slot.bombCoreObject = std::make_unique<Object3d>();
        slot.bombCoreObject->Initialize(object3dCommon_);
        slot.bombCoreObject->SetModel(kBombCoreModel);
        slot.bombCoreObject->SetEnableLighting(false);
        slot.bombCoreObject->SetEnvironmentCoefficient(0.0f);
        slot.bombCoreObject->SetSelectLightings(13);
        slot.bombCoreObject->SetColor({ 0.72f, 0.16f, 1.0f, 0.82f });
        slot.bombCoreObject->SetScale({ 1.75f, 1.75f, 1.75f });
    }

    // デジタルブロック残像＆小型爆風破片用ボクセルプール
    voxelPool_.clear();
    voxelPool_.resize(kMaxVoxelPool);
    for (size_t i = 0; i < kMaxVoxelPool; ++i) {
        auto& vp = voxelPool_[i];
        vp.active = false;
        vp.obj = std::make_unique<Object3d>();
        vp.obj->Initialize(object3dCommon_);
        vp.obj->SetModel("cube.obj");
        vp.obj->SetEnableLighting(false);
        vp.obj->SetEnvironmentCoefficient(0.0f);
        vp.obj->SetSelectLightings(10);
        vp.obj->SetScale({ 0.001f, 0.001f, 0.001f });
    }

    billboardPool_.clear();
    billboardPool_.resize(kMaxBillboardVfx);
}

void BombWeapon::Reset() {
    ammo_ = maxAmmo_;
    cooldownTimer_ = 0.0f;
    for (auto& slot : bombPool_) {
        slot.isActive = false;
        slot.isExploding = false;
        slot.explodeTimer = 0.0f;
        slot.flightDistance = 0.0f;
        slot.trailPoints.clear();
    }
    for (auto& vp : voxelPool_) {
        vp.active = false;
    }
    for (auto& bp : billboardPool_) {
        bp.active = false;
    }
    starBlinkCount_ = 0;
    digitalRingCount_ = 0;
    softFadeCount_ = 0;
    lineGlowCount_ = 0;
    trailHeadVertexCount_ = 0;
    trailTailVertexCount_ = 0;
}

bool BombWeapon::IsDepleted() const {
    if (ammo_ > 0) return false;
    for (const auto& slot : bombPool_) {
        if (slot.isActive) return false;
    }
    return true;
}

bool BombWeapon::HasActiveExplosion(Vector3* outPos, float* outRadius) const {
    for (const auto& b : bombPool_) {
        if (b.isActive && b.isExploding) {
            float t = std::clamp(b.explodeTimer / b.maxExplodeTime, 0.0f, 1.0f);
            float curR = b.explodeDamageRadius * std::sinf(t * 1.5707963f);
            if (outPos) *outPos = b.position;
            if (outRadius) *outRadius = curR;
            return true;
        }
    }
    return false;
}

void BombWeapon::SpawnVoxel(const Vector3& pos, const Vector3& vel, const Vector3& rotSpeed, const Vector3& scale, float maxLife, float drag, bool useThreeStage, const Vector4& fixedColor) {
    BombVoxelParticle* target = nullptr;
    float oldestRatio = -1.0f;
    for (auto& vp : voxelPool_) {
        if (!vp.active) {
            target = &vp;
            break;
        }
        float ratio = vp.life / (std::max)(0.001f, vp.maxLife);
        if (ratio > oldestRatio) {
            oldestRatio = ratio;
            target = &vp;
        }
    }
    if (!target || !target->obj) return;

    target->active = true;
    target->position = pos;
    target->velocity = vel;
    target->rotation = {
        static_cast<float>(rand() % 628) * 0.01f,
        static_cast<float>(rand() % 628) * 0.01f,
        static_cast<float>(rand() % 628) * 0.01f
    };
    target->rotSpeed = rotSpeed;
    target->scale = scale;
    target->drag = drag;
    target->life = 0.0f;
    target->maxLife = maxLife;
    target->useThreeStageColor = useThreeStage;
    target->fixedColor = fixedColor;

    Vector4 initCol = useThreeStage ? EvaluateThreeStageColor(0.0f, 1.0f) : fixedColor;
    target->obj->SetTranslate(pos);
    target->obj->SetRotation(target->rotation);
    target->obj->SetScale(scale);
    target->obj->SetColor(initCol);
    target->obj->Update();
}

void BombWeapon::SpawnBillboardVfx(const BillboardVfxParticle& desc) {
    BillboardVfxParticle* target = nullptr;
    float oldestRatio = -1.0f;
    for (auto& bp : billboardPool_) {
        if (!bp.active) {
            target = &bp;
            break;
        }
        float ratio = bp.life / (std::max)(0.001f, bp.maxLife);
        if (ratio > oldestRatio) {
            oldestRatio = ratio;
            target = &bp;
        }
    }
    if (!target) return;
    *target = desc;
    target->active = true;
}

void BombWeapon::TriggerMuzzleFlash(const Vector3& muzzlePos) {
    // ① マズルフラッシュ（0.0〜0.1s）：純白〜シアンの鋭い十字閃光 ＋ 極小破線リング
    BillboardVfxParticle crossFlash;
    crossFlash.texKind = VfxTexKind::StarBlink;
    crossFlash.position = muzzlePos;
    crossFlash.startScale = { 3.2f, 1.4f, 1.0f };
    crossFlash.endScale = { 0.2f, 0.1f, 1.0f };
    crossFlash.rotZ = 0.0f;
    crossFlash.rotSpeed = 4.0f;
    crossFlash.maxLife = 0.10f;
    crossFlash.useCustomColor = true;
    crossFlash.startColor = { 1.0f, 1.0f, 1.0f, 0.95f };
    crossFlash.endColor = { 0.0f, 0.94f, 1.0f, 0.0f };
    SpawnBillboardVfx(crossFlash);

    BillboardVfxParticle ringFlash;
    ringFlash.texKind = VfxTexKind::DigitalRing;
    ringFlash.position = muzzlePos;
    ringFlash.startScale = { 0.8f, 0.8f, 1.0f };
    ringFlash.endScale = { 2.4f, 2.4f, 1.0f };
    ringFlash.rotZ = 0.4f;
    ringFlash.rotSpeed = -8.0f;
    ringFlash.maxLife = 0.12f;
    ringFlash.useCustomColor = true;
    ringFlash.startColor = { 0.35f, 0.96f, 1.0f, 0.85f };
    ringFlash.endColor = { 0.0f, 0.94f, 1.0f, 0.0f };
    SpawnBillboardVfx(ringFlash);
}

void BombWeapon::TriggerExplosionVfx(const Vector3& center) {
    // =========================================================
    // レイヤー①：立体コア閃光（0.0s 〜 0.16s）
    // =========================================================
    BillboardVfxParticle coreGlow;
    coreGlow.texKind = VfxTexKind::SoftFade;
    coreGlow.position = center;
    coreGlow.startScale = { 12.0f, 12.0f, 1.0f };
    coreGlow.endScale = { 0.3f, 0.3f, 1.0f };
    coreGlow.maxLife = 0.16f;
    coreGlow.useCustomColor = true;
    coreGlow.startColor = { 2.2f, 2.3f, 2.5f, 1.0f };
    coreGlow.endColor = { 0.10f, 0.55f, 1.65f, 0.0f };
    coreGlow.rapidShrink = true;
    SpawnBillboardVfx(coreGlow);

    // 縦横比を強調した鋭い十字フラッシュ（ビルボード＋3D斜め交差）
    for (int k = 0; k < 2; ++k) {
        BillboardVfxParticle flareH;
        flareH.texKind = VfxTexKind::StarBlink;
        flareH.position = center;
        flareH.useBillboard = (k == 0);
        flareH.rotEuler = (k == 0) ? Vector3{ 0.0f, 0.0f, 0.0f } : Vector3{ 0.65f, 0.75f, 0.0f };
        flareH.startScale = { 18.0f, 2.8f, 1.0f };
        flareH.endScale = { 1.0f, 0.1f, 1.0f };
        flareH.maxLife = 0.16f;
        flareH.useCustomColor = true;
        flareH.startColor = { 2.0f, 2.2f, 2.4f, 1.0f };
        flareH.endColor = { 0.10f, 0.55f, 1.65f, 0.0f };
        flareH.rapidShrink = true;
        SpawnBillboardVfx(flareH);

        BillboardVfxParticle flareV = flareH;
        flareV.startScale = { 2.8f, 15.0f, 1.0f };
        flareV.endScale = { 0.1f, 0.8f, 1.0f };
        SpawnBillboardVfx(flareV);
    }

    // =========================================================
    // レイヤー②：3Dジャイロ多重衝撃波（0.0s 〜 0.50s）
    // 視界に収まる適正サイズへ抑え、爆発始め＝青 → 終わり＝マゼンタのグラデーションに統一
    // =========================================================
    const Vector3 ringTilts[4] = {
        { 1.22f,  0.15f, 0.0f },
        { 0.55f,  0.85f, 0.0f },
        { -0.60f, -0.80f, 0.0f },
        { 0.0f,   0.0f,  0.0f }
    };

    for (int i = 0; i < 4; ++i) {
        for (int layer = 0; layer < 2; ++layer) {
            float widthOffset = (layer == 0) ? 0.97f : 1.03f;
            BillboardVfxParticle ring;
            ring.texKind = VfxTexKind::DigitalRing;
            ring.position = center;
            ring.useBillboard = (i == 3);
            ring.rotEuler = ringTilts[i];
            ring.startScale = { (1.6f + 0.6f * i) * widthOffset, (1.6f + 0.6f * i) * widthOffset, 1.0f };
            ring.endScale = { (8.5f + 2.2f * i) * widthOffset, (8.5f + 2.2f * i) * widthOffset, 1.0f };
            ring.rotZ = static_cast<float>(i) * 0.85f;
            ring.rotSpeed = (i % 2 == 0) ? 5.0f : -6.0f;
            ring.delay = 0.025f * static_cast<float>(i);
            ring.maxLife = 0.45f + 0.04f * static_cast<float>(i);
            ring.useCustomColor = true;
            // 全リング統一：爆発始め＝鮮烈な青 → 終わり＝ネオンマゼンタ
            ring.startColor = { 0.08f, 0.52f, 1.75f, 1.0f };
            ring.endColor   = { 1.55f, 0.02f, 0.95f, 0.0f };
            SpawnBillboardVfx(ring);
        }
    }

    // =========================================================
    // レイヤー③：3D破片＆立体グリッチ（0.05s 〜 0.60s）
    // =========================================================
    // 小型化・高密度化した3Dキューブ破片（白→シアン→マゼンタの3段階カーブ）
    const int burstCount = 96;
    for (int i = 0; i < burstCount; ++i) {
        float phi = static_cast<float>(rand() % 628) * 0.01f;
        float cosT = (static_cast<float>(rand() % 200) - 100.0f) * 0.01f;
        float sinT = std::sqrtf((std::max)(0.0f, 1.0f - cosT * cosT));
        Vector3 dir = { sinT * std::cosf(phi), cosT, sinT * std::sinf(phi) };

        float spd = 16.0f + (static_cast<float>(rand() % 100) / 100.0f) * 46.0f;
        float sz = 0.45f + (static_cast<float>(rand() % 100) / 100.0f) * 0.75f;
        Vector3 rotSpd = {
            ((rand() % 100) - 50) * 0.18f,
            ((rand() % 100) - 50) * 0.18f,
            ((rand() % 100) - 50) * 0.18f
        };
        float maxLife = 0.45f + (static_cast<float>(rand() % 10)) * 0.02f;

        SpawnVoxel(
            { center.x + dir.x * 0.8f, center.y + dir.y * 0.8f, center.z + dir.z * 0.8f },
            { dir.x * spd, dir.y * spd, dir.z * spd },
            rotSpd,
            { sz, sz, sz },
            maxLife,
            2.5f,
            true
        );
    }

    // 立体的に交差して走るグリッチ線（X方向＋斜め・奥行き方向）
    const int glitchCount = 12;
    for (int i = 0; i < glitchCount; ++i) {
        float dirSign = (i % 2 == 0) ? 1.0f : -1.0f;
        float offsetY = ((rand() % 100) - 50) * 0.14f;
        float offsetZ = ((rand() % 100) - 50) * 0.14f;
        float yawTilt = ((rand() % 100) - 50) * 0.012f;   // 奥行き方向の傾き
        float pitchTilt = ((rand() % 100) - 50) * 0.008f; // 上下方向の傾き

        BillboardVfxParticle glitch;
        glitch.texKind = VfxTexKind::LineGlow;
        glitch.useBillboard = false;
        glitch.rotEuler = { pitchTilt, yawTilt, 0.0f };
        glitch.position = { center.x + dirSign * 1.5f, center.y + offsetY, center.z + offsetZ };
        glitch.velocity = { dirSign * (30.0f + (rand() % 22)), 0.0f, ((rand() % 100) - 50) * 0.25f };
        glitch.startScale = { 8.5f + (rand() % 6), 0.22f, 1.0f };
        glitch.endScale = { 14.0f, 0.06f, 1.0f };
        glitch.delay = 0.04f + 0.02f * static_cast<float>(i % 3);
        glitch.maxLife = 0.28f + 0.04f * static_cast<float>(i % 4);
        glitch.useCustomColor = false;
        glitch.alphaScale = 1.0f;
        SpawnBillboardVfx(glitch);
    }
}

void BombWeapon::UpdateVoxels(float dt) {
    for (auto& vp : voxelPool_) {
        if (!vp.active) continue;
        vp.life += dt;
        if (vp.life >= vp.maxLife) {
            vp.active = false;
            continue;
        }

        vp.position.x += vp.velocity.x * dt;
        vp.position.y += vp.velocity.y * dt;
        vp.position.z += vp.velocity.z * dt;
        vp.rotation.x += vp.rotSpeed.x * dt;
        vp.rotation.y += vp.rotSpeed.y * dt;
        vp.rotation.z += vp.rotSpeed.z * dt;

        float damp = (std::max)(0.0f, 1.0f - vp.drag * dt);
        vp.velocity.x *= damp;
        vp.velocity.y *= damp;
        vp.velocity.z *= damp;

        float ratio = std::clamp(vp.life / vp.maxLife, 0.0f, 1.0f);
        float scaleFade = 1.0f - ratio * ratio;

        if (vp.obj) {
            Vector4 col = vp.useThreeStageColor
                ? EvaluateThreeStageColor(ratio, 1.0f)
                : vp.fixedColor;
            col.w = 1.0f; // Object3d の alpha<=0.5 discard 対策（縮小で消滅）
            vp.obj->SetTranslate(vp.position);
            vp.obj->SetRotation(vp.rotation);
            vp.obj->SetScale({ vp.scale.x * scaleFade, vp.scale.y * scaleFade, vp.scale.z * scaleFade });
            vp.obj->SetColor(col);
            vp.obj->Update();
        }
    }
}

void BombWeapon::UpdateBillboardVfx(float dt, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const Matrix4x4& billboardMatrix) {
    starBlinkCount_ = 0;
    digitalRingCount_ = 0;
    softFadeCount_ = 0;
    lineGlowCount_ = 0;

    Matrix4x4 vpMatrix = viewMatrix * projectionMatrix;
    ParticleForGPU* starBuf = starBlinkRenderer_.GetInstancingData();
    ParticleForGPU* ringBuf = digitalRingRenderer_.GetInstancingData();
    ParticleForGPU* fadeBuf = softFadeRenderer_.GetInstancingData();
    ParticleForGPU* lineBuf = lineGlowRenderer_.GetInstancingData();

    for (auto& bp : billboardPool_) {
        if (!bp.active) continue;
        if (bp.delay > 0.0f) {
            bp.delay -= dt;
            continue;
        }
        bp.life += dt;
        if (bp.life >= bp.maxLife) {
            bp.active = false;
            continue;
        }

        bp.position.x += bp.velocity.x * dt;
        bp.position.y += bp.velocity.y * dt;
        bp.position.z += bp.velocity.z * dt;
        bp.rotZ += bp.rotSpeed * dt;

        float ratio = std::clamp(bp.life / bp.maxLife, 0.0f, 1.0f);
        float scaleT = 0.0f;
        if (bp.rapidShrink) {
            // 発生直後 (0〜20%) は最大サイズを維持・わずかに膨張し、その後一気に急速縮小（強い緩急）
            if (ratio < 0.20f) {
                scaleT = -0.10f * std::sinf((ratio / 0.20f) * 3.14159265f);
            } else {
                float u = (ratio - 0.20f) / 0.80f;
                scaleT = u * u * u;
            }
        } else {
            scaleT = 1.0f - (1.0f - ratio) * (1.0f - ratio);
        }

        Vector3 sc = {
            std::lerp(bp.startScale.x, bp.endScale.x, scaleT),
            std::lerp(bp.startScale.y, bp.endScale.y, scaleT),
            1.0f
        };

        // 開始時(0〜30%)は青をしっかり保ち、30〜85%で青→マゼンタへグラデーション、最後でフェードアウト
        Vector4 col;
        if (bp.useCustomColor) {
            float colorT = std::clamp((ratio - 0.25f) / 0.55f, 0.0f, 1.0f);
            float alphaT = (ratio < 0.65f) ? 1.0f : (1.0f - (ratio - 0.65f) / 0.35f);
            col = LerpColor(bp.startColor, bp.endColor, colorT);
            col.w = bp.startColor.w * alphaT;
        } else {
            col = EvaluateThreeStageColor(ratio, bp.alphaScale);
        }

        Matrix4x4 rotMat = bp.useBillboard
            ? (Matrix4x4::RotateZ(bp.rotZ) * billboardMatrix)
            : (Matrix4x4::RotateZ(bp.rotZ) * Matrix4x4::RotateX(bp.rotEuler.x) * Matrix4x4::RotateY(bp.rotEuler.y));
        Matrix4x4 world = Matrix4x4::Scale(sc) * rotMat * Matrix4x4::Translation(bp.position);
        Matrix4x4 wvp = world * vpMatrix;

        auto pushInstance = [&](ParticleForGPU* buf, uint32_t& count) {
            if (!buf || count >= kMaxBillboardVfx) return;
            buf[count].World = world;
            buf[count].WVP = wvp;
            buf[count].color = col;
            ++count;
        };

        switch (bp.texKind) {
        case VfxTexKind::StarBlink:   pushInstance(starBuf, starBlinkCount_); break;
        case VfxTexKind::DigitalRing: pushInstance(ringBuf, digitalRingCount_); break;
        case VfxTexKind::SoftFade:    pushInstance(fadeBuf, softFadeCount_); break;
        case VfxTexKind::LineGlow:    pushInstance(lineBuf, lineGlowCount_); break;
        }
    }
}

void BombWeapon::UpdateTrails(float dt, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix) {
    Matrix4x4 invView = Matrix4x4::Inverse(viewMatrix);
    Vector3 cameraPos = { invView.m[3][0], invView.m[3][1], invView.m[3][2] };
    Matrix4x4 vpMatrix = viewMatrix * projectionMatrix;

    std::vector<RibbonVertex> headVerts;
    std::vector<RibbonVertex> tailVerts;
    headVerts.reserve(kMaxRibbonVertices);
    tailVerts.reserve(kMaxRibbonVertices);

    for (auto& b : bombPool_) {
        for (auto& pt : b.trailPoints) {
            pt.age += dt;
        }
        while (!b.trailPoints.empty() && b.trailPoints.back().age >= b.trailPoints.back().maxLife) {
            b.trailPoints.pop_back();
        }

        if (b.isActive && !b.isExploding) {
            if (b.trailPoints.empty()) {
                b.trailPoints.push_front({ b.position, 0.0f, 0.28f });
            } else {
                Vector3 d = b.position - b.trailPoints.front().position;
                if (d.x * d.x + d.y * d.y + d.z * d.z > 0.01f) {
                    b.trailPoints.push_front({ b.position, 0.0f, 0.28f });
                    if (b.trailPoints.size() > kMaxTrailPoints) {
                        b.trailPoints.pop_back();
                    }
                }
            }
        }

        const size_t n = b.trailPoints.size();
        if (n < 2) continue;

        std::vector<Vector3> leftPts(n);
        std::vector<Vector3> rightPts(n);
        std::vector<float> ratios(n);

        for (size_t i = 0; i < n; ++i) {
            const Vector3& pos = b.trailPoints[i].position;
            Vector3 tangent = (i == 0)
                ? (b.trailPoints[0].position - b.trailPoints[1].position)
                : ((i == n - 1) ? (b.trailPoints[n - 2].position - b.trailPoints[n - 1].position)
                                : (b.trailPoints[i - 1].position - b.trailPoints[i + 1].position));
            float tLen = tangent.Length();
            if (tLen > 1e-5f) tangent = tangent / tLen;
            else tangent = { 0.0f, 0.0f, 1.0f };

            Vector3 toCam = cameraPos - pos;
            float cLen = toCam.Length();
            if (cLen > 1e-5f) toCam = toCam / cLen;
            else toCam = { 0.0f, 0.0f, -1.0f };

            Vector3 side = Vector3::Cross(tangent, toCam);
            float sLen = side.Length();
            if (sLen > 1e-5f) side = side / sLen;
            else side = { 1.0f, 0.0f, 0.0f };

            float r = std::clamp(b.trailPoints[i].age / b.trailPoints[i].maxLife, 0.0f, 1.0f);
            ratios[i] = r;
            float w = 0.36f * (1.0f - r);
            leftPts[i] = pos - side * w;
            rightPts[i] = pos + side * w;
        }

        // 先端側 (0..0.45): 白〜シアン / 後端側 (0.45..1.0): マゼンタ
        for (size_t i = 0; i + 1 < n; ++i) {
            float v0 = 0.50f - 0.48f * ratios[i];
            float v1 = 0.50f - 0.48f * ratios[i + 1];

            RibbonVertex l0 = { { leftPts[i].x,      leftPts[i].y,      leftPts[i].z,      1.0f }, { 0.20f, v0 }, { 0.0f, 1.0f, 0.0f } };
            RibbonVertex r0 = { { rightPts[i].x,     rightPts[i].y,     rightPts[i].z,     1.0f }, { 0.80f, v0 }, { 0.0f, 1.0f, 0.0f } };
            RibbonVertex l1 = { { leftPts[i + 1].x,  leftPts[i + 1].y,  leftPts[i + 1].z,  1.0f }, { 0.20f, v1 }, { 0.0f, 1.0f, 0.0f } };
            RibbonVertex r1 = { { rightPts[i + 1].x, rightPts[i + 1].y, rightPts[i + 1].z, 1.0f }, { 0.80f, v1 }, { 0.0f, 1.0f, 0.0f } };

            auto& dst = (ratios[i] < 0.35f) ? headVerts : tailVerts;
            if (dst.size() + 6 <= kMaxRibbonVertices) {
                dst.push_back(l0); dst.push_back(r0); dst.push_back(l1);
                dst.push_back(r0); dst.push_back(r1); dst.push_back(l1);
            }
        }
    }

    trailHeadVertexCount_ = static_cast<uint32_t>(headVerts.size());
    trailTailVertexCount_ = static_cast<uint32_t>(tailVerts.size());

    if (trailHeadVertexCount_ > 0) {
        trailHeadRenderer_.UpdateVertexBuffer(headVerts.data(), trailHeadVertexCount_);
        if (auto* buf = trailHeadRenderer_.GetInstancingData()) {
            buf[0].World = Matrix4x4::Identity();
            buf[0].WVP = vpMatrix;
            buf[0].color = { 0.30f, 1.0f, 1.0f, 0.95f }; // 先端：高濃度シアン〜白
        }
    }
    if (trailTailVertexCount_ > 0) {
        trailTailRenderer_.UpdateVertexBuffer(tailVerts.data(), trailTailVertexCount_);
        if (auto* buf = trailTailRenderer_.GetInstancingData()) {
            buf[0].World = Matrix4x4::Identity();
            buf[0].WVP = vpMatrix;
            buf[0].color = { 1.45f, 0.02f, 0.92f, 0.95f }; // 消え際：強調ネオン・マゼンタ
        }
    }
}

void BombWeapon::Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& enemies, ArmoredTrainBoss* boss, Object3d* /*parentCamera*/) {
    if (cooldownTimer_ > 0.0f) {
        cooldownTimer_ -= gameSpeed;
    }

    float dt = (1.0f / 60.0f) * gameSpeed;
    UpdateVoxels(dt);

    Camera* activeCam = object3dCommon_ ? object3dCommon_->GetDefaultCamera() : nullptr;
    Matrix4x4 viewMat = activeCam ? activeCam->GetViewMatrix() : Matrix4x4::Identity();
    Matrix4x4 projMat = activeCam ? activeCam->GetProjectionMatrix() : Matrix4x4::Identity();
    Matrix4x4 billMat = Billboard::CreateFromCamera(activeCam, true);

    for (auto& b : bombPool_) {
        if (!b.isActive) continue;

        if (b.armingTimer > 0.0f) {
            b.armingTimer -= gameSpeed;
        }

        if (!b.isExploding) {
            b.position.x += b.velocity.x * gameSpeed;
            b.position.y += b.velocity.y * gameSpeed;
            b.position.z += b.velocity.z * gameSpeed;

            float moveStep = std::sqrtf(b.velocity.x * b.velocity.x + b.velocity.y * b.velocity.y + b.velocity.z * b.velocity.z) * gameSpeed;
            b.flightDistance += moveStep;

            b.frameRotation.y += 0.045f * gameSpeed;
            b.frameRotation.x += 0.025f * gameSpeed;
            b.coreRotation.y -= 0.11f * gameSpeed;
            b.coreRotation.z += 0.075f * gameSpeed;
            b.pulseTimer += 0.25f * gameSpeed;

            // ③ デジタルブロック残像：リボントレイル周囲に極小キューブを1〜2個ずつパラパラ配置
            b.trailTimer += gameSpeed;
            if (b.trailTimer >= 2.0f) {
                b.trailTimer = 0.0f;
                int blockCount = 1 + (rand() % 2);
                for (int k = 0; k < blockCount; ++k) {
                    Vector3 jitter = {
                        ((rand() % 100) - 50) * 0.018f,
                        ((rand() % 100) - 50) * 0.018f,
                        ((rand() % 100) - 50) * 0.010f
                    };
                    SpawnVoxel(
                        { b.position.x + jitter.x, b.position.y + jitter.y, b.position.z + jitter.z },
                        { jitter.x * 2.0f, jitter.y * 2.0f, -2.5f },
                        { 3.5f, 4.5f, 3.5f },
                        { 0.22f, 0.22f, 0.22f },
                        0.26f,
                        1.0f,
                        true
                    );
                }
            }

            // 敵との直接接触判定
            bool hitEnemy = false;
            for (const auto& enemy : enemies) {
                if (enemy && !enemy->IsDead()) {
                    Vector3 ePos = enemy->GetVisualPosition();
                    float dx = ePos.x - b.position.x;
                    float dy = ePos.y - b.position.y;
                    float dz = ePos.z - b.position.z;
                    if (dx * dx + dy * dy + dz * dz <= 25.0f) {
                        hitEnemy = true;
                        break;
                    }
                }
            }

            if (boss && boss->IsActive()) {
                const auto& carriages = boss->GetCarriages();
                for (const auto& car : carriages) {
                    if (!car.isDestroyed) {
                        float dx = car.position.x - b.position.x;
                        float dy = car.position.y - b.position.y;
                        float dz = car.position.z - b.position.z;
                        if (dx * dx + dy * dy + dz * dz <= 36.0f) {
                            hitEnemy = true;
                            break;
                        }
                    }
                }
            }

            if (hitEnemy || b.flightDistance >= b.maxFlightDistance) {
                b.isExploding = true;
                b.explodeTimer = 0.0f;
                TriggerExplosionVfx(b.position);
                SoundManager::GetInstance()->PlaySE("explosion.mp3", 1.0f);
            }

            if (b.bombObject) {
                b.bombObject->SetTranslate(b.position);
                b.bombObject->SetRotation(b.frameRotation);
                b.bombObject->Update();
            }
            if (b.bombCoreObject) {
                float corePulse = 1.75f + 0.18f * std::sinf(b.pulseTimer);
                b.bombCoreObject->SetTranslate(b.position);
                b.bombCoreObject->SetRotation(b.coreRotation);
                b.bombCoreObject->SetScale({ corePulse, corePulse, corePulse });
                b.bombCoreObject->Update();
            }
        } else {
            b.explodeTimer += gameSpeed;
            float t = std::clamp(b.explodeTimer / b.maxExplodeTime, 0.0f, 1.0f);
            float currentDamageR = b.explodeDamageRadius * std::sinf(t * 1.5707963f);

            float rSq = currentDamageR * currentDamageR;
            for (const auto& enemy : enemies) {
                if (enemy && !enemy->IsDead()) {
                    Vector3 ePos = enemy->GetVisualPosition();
                    float dx = ePos.x - b.position.x;
                    float dy = ePos.y - b.position.y;
                    float dz = ePos.z - b.position.z;
                    if (dx * dx + dy * dy + dz * dz <= rSq) {
                        enemy->Kill();
                    }
                }
            }

            if (boss && boss->IsActive()) {
                const auto& carriages = boss->GetCarriages();
                for (size_t i = 0; i < carriages.size(); ++i) {
                    if (!carriages[i].isDestroyed) {
                        float dx = carriages[i].position.x - b.position.x;
                        float dy = carriages[i].position.y - b.position.y;
                        float dz = carriages[i].position.z - b.position.z;
                        if (dx * dx + dy * dy + dz * dz <= rSq) {
                            boss->OnDamaged(static_cast<int>(i), 3);
                        }
                    }
                }
            }

            if (b.explodeTimer >= b.maxExplodeTime) {
                b.isActive = false;
                b.isExploding = false;
            }
        }
    }

    UpdateTrails(dt, viewMat, projMat);
    UpdateBillboardVfx(dt, viewMat, projMat, billMat);
}

void BombWeapon::Attack(Input* input,
                        std::list<std::unique_ptr<PlayerBullet>>& /*bullets*/,
                        std::list<std::unique_ptr<PlayerMissile>>& /*missiles*/,
                        const std::list<std::unique_ptr<Enemy>>& /*enemies*/,
                        ArmoredTrainBoss* /*boss*/,
                        Object3d* /*parentCamera*/,
                        float /*gameSpeed*/) {
    if (!input || !owner_) return;

    bool triggerBomb = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_B);

    if (triggerBomb) {
        // 1. 飛行中の未起爆ボムがあれば任意起爆
        for (auto& b : bombPool_) {
            if (b.isActive && !b.isExploding && b.armingTimer <= 0.0f) {
                b.isExploding = true;
                b.explodeTimer = 0.0f;
                TriggerExplosionVfx(b.position);
                SoundManager::GetInstance()->PlaySE("explosion.mp3", 1.0f);
                return;
            }
        }

        // 2. 新規投下
        if (ammo_ > 0 && cooldownTimer_ <= 0.0f) {
            BombSlot* slot = nullptr;
            for (auto& b : bombPool_) {
                if (!b.isActive) {
                    slot = &b;
                    break;
                }
            }
            if (!slot) return;

            ammo_--;
            cooldownTimer_ = 25.0f;

            Object3d* playerObj = owner_->GetObject3d();
            if (!playerObj) return;

            const Matrix4x4& mat = playerObj->GetmatWorld();
            Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
            Vector3 forwardVec = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
            float fLen = std::sqrt(forwardVec.x * forwardVec.x + forwardVec.y * forwardVec.y + forwardVec.z * forwardVec.z);
            if (fLen > 0.0001f) { forwardVec.x /= fLen; forwardVec.y /= fLen; forwardVec.z /= fLen; }

            Vector3 targetPos = owner_->GetReticleWorldPosition();
            Vector3 muzzlePos = { playerPos.x + forwardVec.x * 3.5f, playerPos.y + forwardVec.y * 3.5f, playerPos.z + forwardVec.z * 3.5f };

            Vector3 dir = { targetPos.x - muzzlePos.x, targetPos.y - muzzlePos.y, targetPos.z - muzzlePos.z };
            float len = std::sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            if (len > 0.0001f) { dir.x /= len; dir.y /= len; dir.z /= len; }
            else { dir = forwardVec; }

            float bombSpeed = 7.0f;
            Vector3 vel = { dir.x * bombSpeed, dir.y * bombSpeed, dir.z * bombSpeed };

            slot->isActive = true;
            slot->position = muzzlePos;
            slot->velocity = vel;
            slot->frameRotation = { 0.0f, 0.0f, 0.0f };
            slot->coreRotation = { 0.0f, 0.0f, 0.0f };
            slot->pulseTimer = 0.0f;
            slot->trailTimer = 0.0f;
            slot->flightDistance = 0.0f;
            slot->armingTimer = 25.0f;
            slot->isExploding = false;
            slot->explodeTimer = 0.0f;
            slot->explodeDamageRadius = 45.0f;
            slot->trailPoints.clear();
            slot->trailPoints.push_front({ muzzlePos, 0.0f, 0.28f });

            if (slot->bombObject) {
                slot->bombObject->SetTranslate(muzzlePos);
                slot->bombObject->SetRotation(slot->frameRotation);
                slot->bombObject->SetScale({ 0.14f, 0.14f, 0.14f });
                slot->bombObject->SetColor({ 0.20f, 0.75f, 1.0f, 0.92f });
                slot->bombObject->Update();
            }
            if (slot->bombCoreObject) {
                slot->bombCoreObject->SetTranslate(muzzlePos);
                slot->bombCoreObject->SetRotation(slot->coreRotation);
                slot->bombCoreObject->SetScale({ 1.75f, 1.75f, 1.75f });
                slot->bombCoreObject->SetColor({ 0.72f, 0.16f, 1.0f, 0.82f });
                slot->bombCoreObject->Update();
            }

            TriggerMuzzleFlash(muzzlePos);
            SoundManager::GetInstance()->PlaySE("launcher.mp3", 0.9f);
        }
    }
}

void BombWeapon::Draw() {
    for (auto& b : bombPool_) {
        if (!b.isActive || b.isExploding) continue;

        if (b.bombCoreObject) {
            b.bombCoreObject->Draw();
        }
        if (b.bombObject) {
            b.bombObject->Draw();
        }
    }

    for (const auto& vp : voxelPool_) {
        if (vp.active && vp.obj) {
            vp.obj->Draw();
        }
    }

    auto texMgr = TextureManager::GetInstance();
    if (!texMgr) return;

    // 弾道リボントレイル（加算合成）
    uint32_t circleTex = texMgr->GetSrvIndex("circle2.png");
    if (trailTailVertexCount_ > 0) {
        trailTailRenderer_.Draw(1, circleTex, BlendMode::Additive);
    }
    if (trailHeadVertexCount_ > 0) {
        trailHeadRenderer_.Draw(1, circleTex, BlendMode::Additive);
    }

    // マズルフラッシュ・コア閃光・多重破線リング・グリッチ線（すべて加算合成）
    if (softFadeCount_ > 0) {
        softFadeRenderer_.Draw(softFadeCount_, texMgr->GetSrvIndex("soft_fade.png"), BlendMode::Additive);
    }
    if (digitalRingCount_ > 0) {
        digitalRingRenderer_.Draw(digitalRingCount_, texMgr->GetSrvIndex("digital_ring.png"), BlendMode::Additive);
    }
    if (lineGlowCount_ > 0) {
        lineGlowRenderer_.Draw(lineGlowCount_, texMgr->GetSrvIndex("line_glow.png"), BlendMode::Additive);
    }
    if (starBlinkCount_ > 0) {
        starBlinkRenderer_.Draw(starBlinkCount_, texMgr->GetSrvIndex("star_blink.png"), BlendMode::Additive);
    }

    if (object3dCommon_) {
        object3dCommon_->SetCommonDrawSetting();
    }
}

void BombWeapon::DrawUI() {
}

std::string BombWeapon::GetAmmoText() const {
    return "BOMB: " + std::to_string(ammo_) + " / " + std::to_string(maxAmmo_);
}
