#include "MemoryItem.h"
#include "Game/Actor/Player/Player.h"
#include "Game/System/GamePlayVfxManager.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include <cmath>
#include <algorithm>

float MemoryItem::s_ScaleMultiplier = 1.6f;
Vector4 MemoryItem::s_CustomBitColor = { 1.0f, 0.65f, 0.12f, 1.0f }; // 地形に対して最も際立つエレクトリックオレンジ/ゴールド

void MemoryItem::Initialize(Object3dCommon* object3dCommon, const Vector3& pos, MemoryType type) {
    position_ = pos;
    spawnStartPos_ = pos;
    memoryType_ = type;
    animTimer_ = 0.0f;
    spawnTimer_ = 0.0f;
    hasSpawnedBurst_ = false;
    trailAccum_ = 0.0f;
    ambientVoxelAccum_ = 0.0f;
    lifeTimer_ = 600.0f;
    isDead_ = false;
    isAttracting_ = false;
    attractSpeed_ = 0.25f;

    // メモリ種類に応じたカラー設定
    switch (type) {
    case MemoryType::CYBER_BIT:
        // 地形のシアン・ブルー系背景に対して最大の補色コントラストを発揮するエレクトリックオレンジ
        itemColor_ = s_CustomBitColor;
        break;
    case MemoryType::CHARGE_SHOT:
        itemColor_ = { 1.0f, 0.95f, 0.15f, 1.0f }; // ネオンイエロー
        break;
    case MemoryType::BOMB:
        itemColor_ = { 1.0f, 0.2f, 0.2f, 1.0f }; // ネオンレッド
        break;
    case MemoryType::SPREAD:
        itemColor_ = { 0.2f, 1.0f, 0.35f, 1.0f }; // ネオングリーン
        break;
    case MemoryType::FUNNEL:
        itemColor_ = { 0.9f, 0.2f, 1.0f, 1.0f }; // ネオンマゼンタ
        break;
    case MemoryType::LASER_BEAM:
        itemColor_ = { 0.2f, 0.6f, 1.0f, 1.0f }; // ディープブルー
        break;
    default:
        itemColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
        break;
    }

    ModelManager::GetInstance()->LoadModel("memory/memory_core/memory_core.obj");
    ModelManager::GetInstance()->LoadModel("memory/memory_1/memory_1.obj");
    ModelManager::GetInstance()->LoadModel("cube.obj");

    Vector3 initScale(baseScale_.x * s_ScaleMultiplier * 0.2f, baseScale_.y * s_ScaleMultiplier * 0.2f, baseScale_.z * s_ScaleMultiplier * 0.2f);

    // 1. memory_core：メモリ種類ごとの色を適用（サイバーホログラムシェーダー case 15）
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel("memory/memory_core/memory_core.obj");
    object_->SetColor(itemColor_);
    object_->SetScale(initScale);
    object_->SetEnableLighting(false);
    object_->SetEnvironmentCoefficient(0.0f);
    object_->SetSelectLightings(15);
    object_->SetShininess(0.0f);
    object_->SetTranslate(position_);
    object_->Update();

    // 2. memory_1：全メモリ共通のサイバープラチナカラーを適用（サイバーホログラムシェーダー case 15）
    const Vector4 kCommonFrameColor = { 0.78f, 0.88f, 0.98f, 1.0f };
    subObject_ = std::make_unique<Object3d>();
    subObject_->Initialize(object3dCommon);
    subObject_->SetModel("memory/memory_1/memory_1.obj");
    subObject_->SetColor(kCommonFrameColor);
    subObject_->SetScale(initScale);
    subObject_->SetEnableLighting(false);
    subObject_->SetEnvironmentCoefficient(0.0f);
    subObject_->SetSelectLightings(15);
    subObject_->SetShininess(0.0f);
    subObject_->SetTranslate(position_);
    subObject_->Update();

    // 周囲を公転するオービタル・データビット3個を初期化
    for (int i = 0; i < kOrbitBitCount; ++i) {
        orbitBits_[i] = std::make_unique<Object3d>();
        orbitBits_[i]->Initialize(object3dCommon);
        orbitBits_[i]->SetModel("cube.obj");
        orbitBits_[i]->SetEnableLighting(false);
        orbitBits_[i]->SetEnvironmentCoefficient(0.0f);
        orbitBits_[i]->SetSelectLightings(10);
        // 1つは白コア、2つはメモリカラーで発光させる
        Vector4 bitCol = (i == 0)
            ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }
            : itemColor_;
        orbitBits_[i]->SetColor(bitCol);
        orbitBits_[i]->SetScale({ 0.11f, 0.11f, 0.38f });
        orbitBits_[i]->SetTranslate(position_);
        orbitBits_[i]->Update();
    }
}

void MemoryItem::Update(float gameSpeed, Player* player, GamePlayVfxManager* vfxManager) {
    if (isDead_) return;

    float dt = (1.0f / 60.0f) * gameSpeed;
    animTimer_ += 0.05f * gameSpeed;
    lifeTimer_ -= gameSpeed;

    // 出現直後のバースト粒子（初回フレームで1度だけ発生）
    if (!hasSpawnedBurst_) {
        hasSpawnedBurst_ = true;
        if (vfxManager) {
            vfxManager->SpawnMemoryBurstParticles(position_, itemColor_, 10, 0.7f);
        }
    }

    // 出現ポップアップ演出（最初の0.45秒で上にふわっと跳ねてバウンススケール）
    float spawnScaleMul = 1.0f;
    if (spawnTimer_ < spawnDuration_) {
        spawnTimer_ += dt;
        float t = std::clamp(spawnTimer_ / spawnDuration_, 0.0f, 1.0f);
        // イーズアウト（1 - (1-t)^3）で上方向へ飛び出す
        float easeOut = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        if (!isAttracting_) {
            position_.y = spawnStartPos_.y + easeOut * 1.8f;
        }
        // スケールは 0.15 -> 1.18 -> 1.0 と弾む
        if (t < 0.65f) {
            float st = t / 0.65f;
            spawnScaleMul = 0.15f + (1.18f - 0.15f) * st;
        } else {
            float st = (t - 0.65f) / 0.35f;
            spawnScaleMul = 1.18f + (1.0f - 1.18f) * st;
        }
    }

    // 自転アニメーション（吸引中は高速スピン）
    float spinMul = isAttracting_ ? 3.2f : 1.0f;
    rotation_.y += 0.045f * gameSpeed * spinMul;
    rotation_.x = 0.35f + std::sinf(animTimer_ * 0.7f) * 0.15f;
    rotation_.z = std::cosf(animTimer_ * 0.5f) * 0.12f;

    // パルス明滅・スケール脈動（遠くからでもアイテムだと一目でわかるように）
    float pulse = 1.0f + 0.12f * std::sinf(animTimer_ * 3.5f);
    float currentScaleVal = baseScale_.x * s_ScaleMultiplier * pulse * spawnScaleMul;
    float orbitShrink = 1.0f;

    // プレイヤーとの相対位置・マグネット吸引処理
    if (player && !player->IsDead()) {
        Vector3 pPos = player->GetWorldPosition();
        Vector3 diff = { pPos.x - position_.x, pPos.y - position_.y, pPos.z - position_.z };
        float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        float dist = std::sqrtf(distSq);

        // 吸引範囲に入ったらマグネット吸引を開始
        if (dist < magnetRadius_) {
            isAttracting_ = true;
        }

        if (isAttracting_) {
            if (dist > 0.0001f) {
                Vector3 dir = { diff.x / dist, diff.y / dist, diff.z / dist };
                // 自機に向かって加速
                attractSpeed_ += 0.12f * gameSpeed;
                float moveStep = attractSpeed_ * gameSpeed;

                // 吸い込まれるにつれて少し縮小して自機に収束する演出
                float shrinkFactor = std::clamp(dist / magnetRadius_, 0.35f, 1.0f);
                currentScaleVal *= shrinkFactor;
                orbitShrink = std::clamp(dist / magnetRadius_, 0.25f, 1.0f);

                // 吸引中はデータ光の尾を落とす
                trailAccum_ += dt;
                if (trailAccum_ >= 0.025f && vfxManager) {
                    trailAccum_ = 0.0f;
                    vfxManager->SpawnMemoryTrailParticle(position_, itemColor_);
                }

                if (moveStep >= dist || dist < collectRadius_) {
                    // 回収完了！プレイヤーの通常弾を変化（装備）
                    player->EquipMemory(memoryType_);
                    SoundManager::GetInstance()->PlaySE("powerup.mp3", 0.9f);
                    if (vfxManager) {
                        vfxManager->SpawnMemoryBurstParticles(pPos, itemColor_, 18, 1.3f);
                    }
                    isDead_ = true;
                    return;
                } else {
                    position_.x += dir.x * moveStep;
                    position_.y += dir.y * moveStep;
                    position_.z += dir.z * moveStep;
                }
            }
        } else {
            // ポップアップ完了後は微弱な上下ボビング
            if (spawnTimer_ >= spawnDuration_) {
                position_.y += std::sinf(animTimer_ * 2.0f) * 0.025f * gameSpeed;
            }

            // ドロップ浮遊中は周囲で下から上へボクセルブロックを発生させる
            ambientVoxelAccum_ += dt;
            if (ambientVoxelAccum_ >= 0.065f && vfxManager) {
                ambientVoxelAccum_ = 0.0f;
                vfxManager->SpawnMemoryAmbientVoxel(position_, itemColor_, 2.25f);
            }

            // プレイヤーが前方に大きく通り過ぎた場合は自然消滅
            if (pPos.z - position_.z > 60.0f || lifeTimer_ <= 0.0f) {
                isDead_ = true;
                return;
            }
        }
    }

    if (object_) {
        // パルス発光カラー（輝度を微細に変動させてキラキラ感）
        Vector4 finalCol = itemColor_;
        float glow = 0.95f + 0.15f * std::sinf(animTimer_ * 4.0f);
        finalCol.x = std::clamp(itemColor_.x * glow, 0.0f, 1.0f);
        finalCol.y = std::clamp(itemColor_.y * glow, 0.0f, 1.0f);
        finalCol.z = std::clamp(itemColor_.z * glow, 0.0f, 1.0f);

        object_->SetColor(finalCol);
        object_->SetShininess(animTimer_);
        object_->SetTranslate(position_);
        object_->SetRotation(rotation_);
        object_->SetScale(Vector3(currentScaleVal, currentScaleVal, currentScaleVal));
        object_->Update();
    }

    if (subObject_) {
        // memory_1 は全メモリ共通のサイバープラチナカラー
        const Vector4 kCommonFrameColor = { 0.78f, 0.88f, 0.98f, 1.0f };
        subObject_->SetColor(kCommonFrameColor);
        subObject_->SetShininess(animTimer_);
        subObject_->SetTranslate(position_);
        subObject_->SetRotation(rotation_);
        subObject_->SetScale(Vector3(currentScaleVal, currentScaleVal, currentScaleVal));
        subObject_->Update();
    }

    // 周囲を公転するオービタル・データビットの更新（拡大したメモリサイズに合わせる）
    float orbitRadius = 2.25f * spawnScaleMul * orbitShrink;
    float orbitSpeed = isAttracting_ ? 2.6f : 1.35f;
    for (int i = 0; i < kOrbitBitCount; ++i) {
        if (!orbitBits_[i]) continue;
        float angle = animTimer_ * orbitSpeed * (i % 2 == 0 ? 1.0f : -1.15f) + (static_cast<float>(i) * 2.094395f);
        // 各ビットで異なる傾斜軌道（XY, XZ, YZをブレンド）
        float tilt = (i == 0) ? 0.45f : ((i == 1) ? -0.55f : 1.05f);
        float c = std::cosf(angle);
        float s = std::sinf(angle);
        Vector3 offset = {
            c * orbitRadius,
            s * std::sinf(tilt) * orbitRadius,
            s * std::cosf(tilt) * orbitRadius
        };
        orbitBits_[i]->SetTranslate({ position_.x + offset.x, position_.y + offset.y, position_.z + offset.z });
        orbitBits_[i]->SetRotation({ angle * 0.7f, angle + 1.57f, tilt });
        float bitScale = spawnScaleMul * orbitShrink;
        orbitBits_[i]->SetScale({ 0.14f * bitScale, 0.14f * bitScale, 0.48f * bitScale });
        orbitBits_[i]->Update();
    }
}

void MemoryItem::Draw() {
    if (isDead_) return;
    if (object_) {
        object_->Draw();
    }
    if (subObject_) {
        subObject_->Draw();
    }
    for (int i = 0; i < kOrbitBitCount; ++i) {
        if (orbitBits_[i]) {
            orbitBits_[i]->Draw();
        }
    }
}
