#include "BombWeapon.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Enemy/Enemy.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "KHEngine/Input/Input.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>

namespace {
    constexpr const char* kBombFrameModel = "resources/3dModels/bom/bom_1/bom_1.obj";
    constexpr const char* kBombCoreModel  = "resources/3dModels/bom/bom_core/bom_core.obj";
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

    // 事前オブジェクトプールの初期化（最大3発分をあらかじめ生成して動的確保/解放を根絶）
    bombPool_.clear();
    bombPool_.resize(kMaxBombPool);

    for (size_t i = 0; i < kMaxBombPool; ++i) {
        auto& slot = bombPool_[i];
        slot.isActive = false;
        slot.isExploding = false;

        // 外枠ケージ（bom_1.obj）：視認性の高いエレクトリックブルー・ホログラムフレーム
        slot.bombObject = std::make_unique<Object3d>();
        slot.bombObject->Initialize(object3dCommon_);
        slot.bombObject->SetModel(kBombFrameModel);
        slot.bombObject->SetEnableLighting(false);
        slot.bombObject->SetEnvironmentCoefficient(0.0f);
        slot.bombObject->SetSelectLightings(12);
        slot.bombObject->SetColor({ 0.20f, 0.75f, 1.0f, 0.92f });
        slot.bombObject->SetScale({ 0.14f, 0.14f, 0.14f });

        // 内側多面体コア（bom_core.obj）：外枠と別色の明るいホログラム・ディープパープル
        slot.bombCoreObject = std::make_unique<Object3d>();
        slot.bombCoreObject->Initialize(object3dCommon_);
        slot.bombCoreObject->SetModel(kBombCoreModel);
        slot.bombCoreObject->SetEnableLighting(false);
        slot.bombCoreObject->SetEnvironmentCoefficient(0.0f);
        slot.bombCoreObject->SetSelectLightings(13);
        slot.bombCoreObject->SetColor({ 0.72f, 0.16f, 1.0f, 0.82f });
        slot.bombCoreObject->SetScale({ 1.75f, 1.75f, 1.75f });
    }

    // 飛行トレイル＆爆発用ボクセルブロックプールの初期化
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
}

void BombWeapon::Reset() {
    ammo_ = maxAmmo_;
    cooldownTimer_ = 0.0f;
    for (auto& slot : bombPool_) {
        slot.isActive = false;
        slot.isExploding = false;
        slot.explodeTimer = 0.0f;
        slot.flightDistance = 0.0f;
    }
    for (auto& vp : voxelPool_) {
        vp.active = false;
    }
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

void BombWeapon::SpawnVoxel(const Vector3& pos, const Vector3& vel, const Vector3& rotSpeed, const Vector3& scale, const Vector4& color, float maxLife, float drag) {
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
    target->color = color;
    target->drag = drag;
    target->life = 0.0f;
    target->maxLife = maxLife;

    target->obj->SetTranslate(pos);
    target->obj->SetRotation(target->rotation);
    target->obj->SetScale(scale);
    target->obj->SetColor(color);
    target->obj->Update();
}

void BombWeapon::TriggerVoxelExplosion(const Vector3& center) {
    // プレイヤー弾・仮想空間と調和しつつ背景シアンに埋もれない重厚なバイオレット＆マゼンタ系パレット
    auto PickBombVoxelColor = [](int idx) -> Vector4 {
        int p = idx % 4;
        if (p == 0) return { 0.62f, 0.08f, 1.0f, 1.0f };  // ディープネオンパープル
        if (p == 1) return { 0.95f, 0.12f, 0.58f, 1.0f }; // サイバーマゼンタ
        if (p == 2) return { 0.38f, 0.05f, 0.85f, 1.0f }; // ダークバイオレット
        return { 0.90f, 0.85f, 1.0f, 1.0f };              // クールペールホワイト
    };

    // 1. ボクセル衝撃波リング（水平リング＋斜め交差リングで立体的なサイバー爆風リングを形成）
    const int ringCount = 24;
    for (int i = 0; i < ringCount; ++i) {
        float theta = (6.2831853f * static_cast<float>(i)) / static_cast<float>(ringCount);
        Vector3 dir = { std::cosf(theta), std::sinf(theta) * 0.25f, std::sinf(theta) };
        float spd = 38.0f;
        float sz = 1.6f;
        SpawnVoxel(
            { center.x + dir.x * 1.5f, center.y + dir.y * 1.5f, center.z + dir.z * 1.5f },
            { dir.x * spd, dir.y * spd, dir.z * spd },
            { 6.0f, 8.0f, 6.0f },
            { sz, sz, sz },
            PickBombVoxelColor(i),
            0.55f,
            2.2f
        );
    }

    // 2. 全方向へ弾け飛ぶ大量のサイバーボクセルブロック
    const int burstCount = 84;
    for (int i = 0; i < burstCount; ++i) {
        float phi = static_cast<float>(rand() % 628) * 0.01f;
        float cosT = (static_cast<float>(rand() % 200) - 100.0f) * 0.01f;
        float sinT = std::sqrtf((std::max)(0.0f, 1.0f - cosT * cosT));
        Vector3 dir = { sinT * std::cosf(phi), cosT, sinT * std::sinf(phi) };

        float spd = 14.0f + (static_cast<float>(rand() % 100) / 100.0f) * 42.0f;
        float sz = 0.85f + (static_cast<float>(rand() % 100) / 100.0f) * 1.95f;
        Vector3 rotSpd = {
            ((rand() % 100) - 50) * 0.16f,
            ((rand() % 100) - 50) * 0.16f,
            ((rand() % 100) - 50) * 0.16f
        };
        float maxLife = 0.45f + (static_cast<float>(rand() % 10)) * 0.045f;

        SpawnVoxel(
            { center.x + dir.x * 0.8f, center.y + dir.y * 0.8f, center.z + dir.z * 0.8f },
            { dir.x * spd, dir.y * spd, dir.z * spd },
            rotSpd,
            { sz, sz, sz },
            PickBombVoxelColor(rand()),
            maxLife,
            2.6f
        );
    }
}

void BombWeapon::UpdateVoxels(float gameSpeed) {
    float dt = (1.0f / 60.0f) * gameSpeed;
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
            vp.obj->SetTranslate(vp.position);
            vp.obj->SetRotation(vp.rotation);
            vp.obj->SetScale({ vp.scale.x * scaleFade, vp.scale.y * scaleFade, vp.scale.z * scaleFade });
            vp.obj->SetColor(vp.color);
            vp.obj->Update();
        }
    }
}

void BombWeapon::Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& enemies, ArmoredTrainBoss* boss, Object3d* /*parentCamera*/) {
    if (cooldownTimer_ > 0.0f) {
        cooldownTimer_ -= gameSpeed;
    }

    UpdateVoxels(gameSpeed);

    for (auto& b : bombPool_) {
        if (!b.isActive) continue;

        if (b.armingTimer > 0.0f) {
            b.armingTimer -= gameSpeed;
        }

        if (!b.isExploding) {
            // 前方へのロケット推進飛行
            b.position.x += b.velocity.x * gameSpeed;
            b.position.y += b.velocity.y * gameSpeed;
            b.position.z += b.velocity.z * gameSpeed;

            float moveStep = std::sqrtf(b.velocity.x * b.velocity.x + b.velocity.y * b.velocity.y + b.velocity.z * b.velocity.z) * gameSpeed;
            b.flightDistance += moveStep;

            // 外枠ケージと内側コアの回転＆脈動アニメーション
            b.frameRotation.y += 0.045f * gameSpeed;
            b.frameRotation.x += 0.025f * gameSpeed;
            b.coreRotation.y -= 0.11f * gameSpeed;
            b.coreRotation.z += 0.075f * gameSpeed;
            b.pulseTimer += 0.25f * gameSpeed;

            // 飛行中のボクセルブロックトレイル放出
            b.trailTimer += gameSpeed;
            if (b.trailTimer >= 1.5f) {
                b.trailTimer = 0.0f;
                Vector3 jitter = {
                    ((rand() % 100) - 50) * 0.008f,
                    ((rand() % 100) - 50) * 0.008f,
                    ((rand() % 100) - 50) * 0.008f
                };
                Vector4 trailCol = (rand() % 2 == 0)
                    ? Vector4{ 0.62f, 0.08f, 1.0f, 1.0f }
                    : Vector4{ 0.95f, 0.12f, 0.58f, 1.0f };
                SpawnVoxel(
                    { b.position.x + jitter.x, b.position.y + jitter.y, b.position.z + jitter.z },
                    { jitter.x * 12.0f, jitter.y * 12.0f, -6.0f },
                    { 4.0f, 5.0f, 4.0f },
                    { 0.45f, 0.45f, 0.45f },
                    trailCol,
                    0.28f,
                    1.5f
                );
            }

            // 敵との直接接触判定（衝突で即起爆）
            bool hitEnemy = false;
            for (const auto& enemy : enemies) {
                if (enemy && !enemy->IsDead()) {
                    Vector3 ePos = enemy->GetVisualPosition();
                    float dx = ePos.x - b.position.x;
                    float dy = ePos.y - b.position.y;
                    float dz = ePos.z - b.position.z;
                    if (dx * dx + dy * dy + dz * dz <= 25.0f) { // 半径5.0m以内
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

            // 最大飛行距離または敵接触で起爆へ移行
            if (hitEnemy || b.flightDistance >= b.maxFlightDistance) {
                b.isExploding = true;
                b.explodeTimer = 0.0f;
                TriggerVoxelExplosion(b.position);
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
            // 爆発中（ダメージ範囲の拡大と判定のみ継続、視覚効果はボクセルプールで描画）
            b.explodeTimer += gameSpeed;
            float t = std::clamp(b.explodeTimer / b.maxExplodeTime, 0.0f, 1.0f);
            float currentDamageR = b.explodeDamageRadius * std::sinf(t * 1.5707963f);

            // 半径内の敵をすべて殲滅（即死判定）
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

            // ボスへの範囲ダメージ（各車両へ大ダメージ）
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

            // 爆発完了で非アクティブ化（プールスロットを再利用可能に戻す）
            if (b.explodeTimer >= b.maxExplodeTime) {
                b.isActive = false;
                b.isExploding = false;
            }
        }
    }
}

void BombWeapon::Attack(Input* input,
                        std::list<std::unique_ptr<PlayerBullet>>& /*bullets*/,
                        std::list<std::unique_ptr<PlayerMissile>>& /*missiles*/,
                        const std::list<std::unique_ptr<Enemy>>& /*enemies*/,
                        ArmoredTrainBoss* /*boss*/,
                        Object3d* /*parentCamera*/,
                        float /*gameSpeed*/) {
    if (!input || !owner_) return;

    // スペースキーまたはBキーでボム操作
    bool triggerBomb = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_B);

    if (triggerBomb) {
        // 1. すでに飛行中でセーフティ期間（投下直後約0.4秒）を過ぎた未起爆ボムがあれば、手動で任意起爆！
        for (auto& b : bombPool_) {
            if (b.isActive && !b.isExploding && b.armingTimer <= 0.0f) {
                b.isExploding = true;
                b.explodeTimer = 0.0f;
                TriggerVoxelExplosion(b.position);
                SoundManager::GetInstance()->PlaySE("explosion.mp3", 1.0f);
                return;
            }
        }

        // 2. 残弾がありクールダウンが終了していれば新規投下
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

            // 外枠ケージと内側コアの初期姿勢を即時確定
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
}

void BombWeapon::DrawUI() {
}

std::string BombWeapon::GetAmmoText() const {
    return "BOMB: " + std::to_string(ammo_) + " / " + std::to_string(maxAmmo_);
}
