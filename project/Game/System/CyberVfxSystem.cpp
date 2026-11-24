#define NOMINMAX
#include "CyberVfxSystem.h"
#include "CyberVfxConfig.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Graphics/Billboard/Billboard.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
    struct CyberMatCB {
        Vector4 color;
        int32_t selectLightings;
        int32_t enableLightingAsInt;
        float padding;
        float alphaReference;
        Matrix4x4 uvTransform;
    };

    Vector4 LerpVec4(const Vector4& a, const Vector4& b, float t) {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t
        };
    }
}

Vector4 CyberVfxSystem::EvaluateThreeStageColor(float ratio, float alphaScale) {
    return CyberVfxConfig::GetInstance()->EvaluateThreeStageColor(ratio, alphaScale);
}

void CyberVfxSystem::Initialize(Object3dCommon* object3dCommon) {
    CyberVfxConfig::GetInstance()->EnsureLoaded();
    object3dCommon_ = object3dCommon;
    ModelManager::GetInstance()->LoadModel("cube.obj");

    auto texMgr = TextureManager::GetInstance();
    if (texMgr) {
        texMgr->LoadTexture("star_blink.png");
        texMgr->LoadTexture("digital_ring.png");
        texMgr->LoadTexture("soft_fade.png");
        texMgr->LoadTexture("line_glow.png");
    }

    auto services = EngineServices::GetInstance();
    auto dxCommon = services ? services->GetDirectXCommon() : nullptr;
    auto srvManager = services ? services->GetSrvManager() : nullptr;
    if (dxCommon && srvManager) {
        std::vector<VertexData> quadVerts = {
            { { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
            { {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
            { { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
            { {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
            { {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
            { { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
        };
        CyberMatCB mat{};
        mat.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        mat.selectLightings = 2;
        mat.enableLightingAsInt = 0;
        mat.alphaReference = 0.0f;
        mat.uvTransform = Matrix4x4::Identity();

        auto setup = [&](ParticleRenderer& r) {
            r.Initialize(dxCommon, srvManager, static_cast<uint32_t>(kMaxBillboardPool));
            r.CreateVertexBuffer(quadVerts.data(), static_cast<uint32_t>(quadVerts.size()), sizeof(VertexData));
            r.CreateMaterialBuffer(sizeof(CyberMatCB), &mat);
        };
        setup(starBlinkRenderer_);
        setup(digitalRingRenderer_);
        setup(softFadeRenderer_);
        setup(lineGlowRenderer_);
    }

    voxelPool_.clear();
    voxelPool_.resize(kMaxVoxelPool);
    for (auto& vp : voxelPool_) {
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
    billboardPool_.resize(kMaxBillboardPool);
    Clear();
}

void CyberVfxSystem::Clear() {
    for (auto& vp : voxelPool_) vp.active = false;
    for (auto& bp : billboardPool_) bp.active = false;
    starBlinkCount_ = 0;
    digitalRingCount_ = 0;
    softFadeCount_ = 0;
    lineGlowCount_ = 0;
}

void CyberVfxSystem::SpawnBillboard(const BillboardParticle& desc) {
    BillboardParticle* target = nullptr;
    float oldestRatio = -1.0f;
    for (auto& bp : billboardPool_) {
        if (!bp.active) { target = &bp; break; }
        float r = bp.life / (std::max)(0.001f, bp.maxLife);
        if (r > oldestRatio) { oldestRatio = r; target = &bp; }
    }
    if (!target) return;
    *target = desc;
    target->active = true;
}

void CyberVfxSystem::SpawnVoxel(const Vector3& pos, const Vector3& vel, const Vector3& rotSpeed, const Vector3& scale, float maxLife, float drag) {
    VoxelParticle* target = nullptr;
    float oldestRatio = -1.0f;
    for (auto& vp : voxelPool_) {
        if (!vp.active) { target = &vp; break; }
        float r = vp.life / (std::max)(0.001f, vp.maxLife);
        if (r > oldestRatio) { oldestRatio = r; target = &vp; }
    }
    if (!target || !target->obj) return;

    target->active = true;
    target->position = pos;
    target->velocity = vel;
    target->rotation = { (rand() % 628) * 0.01f, (rand() % 628) * 0.01f, (rand() % 628) * 0.01f };
    target->rotSpeed = rotSpeed;
    target->scale = scale;
    target->drag = drag;
    target->life = 0.0f;
    target->maxLife = maxLife;

    target->obj->SetTranslate(pos);
    target->obj->SetRotation(target->rotation);
    target->obj->SetScale(scale);
    target->obj->SetColor(EvaluateThreeStageColor(0.0f, 1.0f));
    target->obj->Update();
}

void CyberVfxSystem::TriggerMuzzleFlash(const Vector3& pos, float scale) {
    const auto& cfg = CyberVfxConfig::GetInstance()->Weapon().muzzle;

    // 1. 純白→青の鋭い十字閃光（緩急付き急速縮小）
    BillboardParticle flashH;
    flashH.texKind = TexKind::StarBlink;
    flashH.position = pos;
    flashH.startScale = { cfg.flashWidth * scale, cfg.flashHeight * scale, 1.0f };
    flashH.endScale = { 0.2f * scale, 0.05f * scale, 1.0f };
    flashH.rotZ = ((rand() % 100) - 50) * 0.006f;
    flashH.maxLife = cfg.flashLife;
    flashH.useCustomColor = true;
    flashH.startColor = cfg.flashStartColor;
    flashH.endColor = cfg.flashEndColor;
    flashH.rapidShrink = true;
    SpawnBillboard(flashH);

    BillboardParticle flashV = flashH;
    flashV.startScale = { cfg.flashHeight * scale, cfg.flashVHeight * scale, 1.0f };
    flashV.endScale = { 0.05f * scale, 0.15f * scale, 1.0f };
    SpawnBillboard(flashV);

    // 2. 極小デジタルリング（青→マゼンタ）
    BillboardParticle ring;
    ring.texKind = TexKind::DigitalRing;
    ring.position = pos;
    ring.startScale = { cfg.ringStartScale * scale, cfg.ringStartScale * scale, 1.0f };
    ring.endScale = { cfg.ringEndScale * scale, cfg.ringEndScale * scale, 1.0f };
    ring.rotZ = (rand() % 628) * 0.01f;
    ring.rotSpeed = cfg.ringRotSpeed;
    ring.maxLife = cfg.ringLife;
    ring.useCustomColor = true;
    ring.startColor = cfg.ringStartColor;
    ring.endColor = cfg.ringEndColor;
    SpawnBillboard(ring);

    // 3. 極小サイバーボクセル粒を散らす
    for (int i = 0; i < cfg.voxelCount; ++i) {
        Vector3 vel = {
            ((rand() % 100) - 50) * 0.14f,
            ((rand() % 100) - 50) * 0.14f,
            cfg.voxelSpeedZ + (rand() % 6)
        };
        float sz = cfg.voxelSize * scale;
        SpawnVoxel(pos, vel, { 6.0f, 8.0f, 6.0f }, { sz, sz, sz }, cfg.voxelLife, 2.5f);
    }
}

void CyberVfxSystem::TriggerSpreadMuzzle(const Vector3& pos) {
    const auto& cfg = CyberVfxConfig::GetInstance()->Weapon().spread;
    const auto& mCfg = CyberVfxConfig::GetInstance()->Weapon().muzzle;

    // 1. 拡散用のコンパクトな十字フラッシュ（重複を避け1組のみ）
    BillboardParticle flashH;
    flashH.texKind = TexKind::StarBlink;
    flashH.position = pos;
    flashH.startScale = { cfg.coreScaleX, cfg.coreScaleY * 0.35f, 1.0f };
    flashH.endScale = { 0.2f, 0.05f, 1.0f };
    flashH.maxLife = cfg.coreLife;
    flashH.useCustomColor = true;
    flashH.startColor = mCfg.flashStartColor;
    flashH.endColor = mCfg.flashEndColor;
    flashH.rapidShrink = true;
    SpawnBillboard(flashH);

    BillboardParticle flashV = flashH;
    flashV.startScale = { cfg.coreScaleY * 0.35f, cfg.coreScaleY, 1.0f };
    flashV.endScale = { 0.05f, 0.15f, 1.0f };
    SpawnBillboard(flashV);

    // 2. 3D傾斜デジタルリング 1枚（青→マゼンタ）
    BillboardParticle ring;
    ring.texKind = TexKind::DigitalRing;
    ring.position = pos;
    ring.useBillboard = false;
    ring.rotEuler = { cfg.ringTiltX, 0.0f, 0.0f };
    ring.startScale = { cfg.ringStartScale, cfg.ringStartScale, 1.0f };
    ring.endScale = { cfg.ringEndScale, cfg.ringEndScale, 1.0f };
    ring.rotSpeed = cfg.ringRotSpeed;
    ring.maxLife = cfg.ringLife;
    ring.useCustomColor = true;
    ring.startColor = cfg.ringStartColor;
    ring.endColor = cfg.ringEndColor;
    SpawnBillboard(ring);

    // 3. 控えめなグリッチ線
    for (int i = 0; i < cfg.glitchCount; ++i) {
        float dirSign = (i % 2 == 0) ? 1.0f : -1.0f;
        BillboardParticle glitch;
        glitch.texKind = TexKind::LineGlow;
        glitch.useBillboard = false;
        glitch.rotEuler = { ((rand() % 100) - 50) * 0.004f, ((rand() % 100) - 50) * 0.006f, 0.0f };
        glitch.position = { pos.x + dirSign * 0.4f, pos.y + ((rand() % 100) - 50) * 0.01f, pos.z };
        glitch.velocity = { dirSign * 14.0f, 0.0f, 4.0f };
        glitch.startScale = { cfg.glitchLength, cfg.glitchThickness, 1.0f };
        glitch.endScale = { cfg.glitchEndLength, 0.03f, 1.0f };
        glitch.maxLife = cfg.glitchLife;
        glitch.useCustomColor = false;
        glitch.alphaScale = 0.85f;
        SpawnBillboard(glitch);
    }

    // 4. 小型ボクセルブロック（少数）
    for (int i = 0; i < cfg.voxelCount; ++i) {
        float angle = (cfg.voxelCount > 1)
            ? (-0.28f + 0.56f * (static_cast<float>(i) / static_cast<float>(cfg.voxelCount - 1)))
            : 0.0f;
        Vector3 vel = { std::sinf(angle) * cfg.voxelSpeed, ((rand() % 100) - 50) * 0.06f, std::cosf(angle) * cfg.voxelSpeed };
        float sz = cfg.voxelSize + (rand() % 10) * 0.01f;
        SpawnVoxel(pos, vel, { 5.0f, 7.0f, 5.0f }, { sz, sz, sz }, cfg.voxelLife, 2.5f);
    }
}

void CyberVfxSystem::SpawnTrailVoxel(const Vector3& pos, const Vector3& vel, float size, float maxLife) {
    SpawnVoxel(pos, vel, { 3.5f, 4.5f, 3.5f }, { size, size, size }, maxLife, 1.2f);
}

void CyberVfxSystem::TriggerBulletHit(const Vector3& pos, const Vector3& vel) {
    Vector3 backDir = { 0.0f, 0.0f, -1.0f };
    float velLen = vel.Length();
    if (velLen > 0.001f) {
        backDir = (vel / velLen) * (-1.0f);
    }

    // 瞬間閃光コアボクセル
    SpawnVoxel(pos + backDir * 0.6f, { 0.0f, 0.0f, 0.0f }, { 8.0f, 10.0f, 6.0f }, { 3.10f, 3.10f, 3.10f }, 0.20f, 0.0f);

    // 飛散する正方形ボクセルブロック 8個
    for (int i = 0; i < 8; ++i) {
        float rx = ((rand() % 200) - 100) * 0.01f;
        float ry = ((rand() % 200) - 100) * 0.01f;
        float rz = ((rand() % 200) - 100) * 0.01f;
        Vector3 spread = { rx + backDir.x * 1.1f, ry + backDir.y * 1.1f, rz + backDir.z * 1.1f };
        float sLen = spread.Length();
        if (sLen > 0.001f) spread = spread / sLen;

        float speed = 28.0f + (static_cast<float>(rand() % 100) / 100.0f) * 24.0f;
        float size = 1.35f + (static_cast<float>(rand() % 100) / 100.0f) * 0.90f;
        Vector3 rotSpd = {
            ((rand() % 100) - 50) * 0.45f,
            ((rand() % 100) - 50) * 0.45f,
            ((rand() % 100) - 50) * 0.45f
        };
        float maxLife = 0.36f + (static_cast<float>(rand() % 100) / 100.0f) * 0.12f;
        SpawnVoxel(pos + spread * 0.85f, spread * speed, rotSpd, { size, size, size }, maxLife, 2.6f);
    }
}

void CyberVfxSystem::Update(float gameSpeed) {
    float dt = (1.0f / 60.0f) * gameSpeed;

    // 1. ボクセル更新
    for (auto& vp : voxelPool_) {
        if (!vp.active) continue;
        vp.life += dt;
        if (vp.life >= vp.maxLife) { vp.active = false; continue; }

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
            Vector4 col = EvaluateThreeStageColor(ratio, 1.0f);
            col.w = 1.0f;
            vp.obj->SetTranslate(vp.position);
            vp.obj->SetRotation(vp.rotation);
            vp.obj->SetScale({ vp.scale.x * scaleFade, vp.scale.y * scaleFade, vp.scale.z * scaleFade });
            vp.obj->SetColor(col);
            vp.obj->Update();
        }
    }

    // 2. ビルボード・3D幾何学パーティクル更新
    starBlinkCount_ = 0;
    digitalRingCount_ = 0;
    softFadeCount_ = 0;
    lineGlowCount_ = 0;

    Camera* cam = object3dCommon_ ? object3dCommon_->GetDefaultCamera() : nullptr;
    if (!cam) return;

    Matrix4x4 vpMatrix = cam->GetViewMatrix() * cam->GetProjectionMatrix();
    Matrix4x4 billboardMatrix = Billboard::CreateFromCamera(cam, true);

    ParticleForGPU* starBuf = starBlinkRenderer_.GetInstancingData();
    ParticleForGPU* ringBuf = digitalRingRenderer_.GetInstancingData();
    ParticleForGPU* fadeBuf = softFadeRenderer_.GetInstancingData();
    ParticleForGPU* lineBuf = lineGlowRenderer_.GetInstancingData();

    for (auto& bp : billboardPool_) {
        if (!bp.active) continue;
        if (bp.delay > 0.0f) { bp.delay -= dt; continue; }
        bp.life += dt;
        if (bp.life >= bp.maxLife) { bp.active = false; continue; }

        bp.position.x += bp.velocity.x * dt;
        bp.position.y += bp.velocity.y * dt;
        bp.position.z += bp.velocity.z * dt;
        bp.rotZ += bp.rotSpeed * dt;

        float ratio = std::clamp(bp.life / bp.maxLife, 0.0f, 1.0f);
        float scaleT = 0.0f;
        if (bp.rapidShrink) {
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

        Vector4 col;
        if (bp.useCustomColor) {
            float colorT = std::clamp((ratio - 0.20f) / 0.60f, 0.0f, 1.0f);
            float alphaT = (ratio < 0.60f) ? 1.0f : (1.0f - (ratio - 0.60f) / 0.40f);
            col = LerpVec4(bp.startColor, bp.endColor, colorT);
            col.w = bp.startColor.w * alphaT;
        } else {
            col = EvaluateThreeStageColor(ratio, bp.alphaScale);
        }

        Matrix4x4 rotMat = bp.useBillboard
            ? (Matrix4x4::RotateZ(bp.rotZ) * billboardMatrix)
            : (Matrix4x4::RotateZ(bp.rotZ) * Matrix4x4::RotateX(bp.rotEuler.x) * Matrix4x4::RotateY(bp.rotEuler.y));
        Matrix4x4 world = Matrix4x4::Scale(sc) * rotMat * Matrix4x4::Translation(bp.position);
        Matrix4x4 wvp = world * vpMatrix;

        auto push = [&](ParticleForGPU* buf, uint32_t& count) {
            if (!buf || count >= kMaxBillboardPool) return;
            buf[count].World = world;
            buf[count].WVP = wvp;
            buf[count].color = col;
            ++count;
        };

        switch (bp.texKind) {
        case TexKind::StarBlink:   push(starBuf, starBlinkCount_); break;
        case TexKind::DigitalRing: push(ringBuf, digitalRingCount_); break;
        case TexKind::SoftFade:    push(fadeBuf, softFadeCount_); break;
        case TexKind::LineGlow:    push(lineBuf, lineGlowCount_); break;
        }
    }
}

void CyberVfxSystem::Draw() {
    for (const auto& vp : voxelPool_) {
        if (vp.active && vp.obj) {
            vp.obj->Draw();
        }
    }

    auto texMgr = TextureManager::GetInstance();
    if (!texMgr) return;

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
