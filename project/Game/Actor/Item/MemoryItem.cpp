#include "MemoryItem.h"
#include "Game/Actor/Player/Player.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include <cmath>
#include <algorithm>

float MemoryItem::s_ScaleMultiplier = 1.2f;
Vector4 MemoryItem::s_CustomBitColor = { 1.0f, 0.65f, 0.12f, 1.0f }; // 地形に対して最も際立つエレクトリックオレンジ/ゴールド

void MemoryItem::Initialize(Object3dCommon* object3dCommon, const Vector3& pos, MemoryType type) {
    position_ = pos;
    memoryType_ = type;
    animTimer_ = 0.0f;
    lifeTimer_ = 600.0f;
    isDead_ = false;
    isAttracting_ = false;
    attractSpeed_ = 0.2f;

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

    ModelManager::GetInstance()->LoadModel("memory.obj");

    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel("memory.obj");
    object_->SetColor(itemColor_);
    object_->SetScale(Vector3(baseScale_.x * s_ScaleMultiplier, baseScale_.y * s_ScaleMultiplier, baseScale_.z * s_ScaleMultiplier));
    // 完全自己発光マテリアル（ネオンカード）として描画
    object_->SetEnableLighting(false);
    object_->SetSelectLightings(0);
    object_->SetTranslate(position_);
    object_->Update();
}

void MemoryItem::Update(float gameSpeed, Player* player) {
    if (isDead_) return;

    animTimer_ += 0.05f * gameSpeed;
    lifeTimer_ -= gameSpeed;

    // 自転アニメーション（Y軸メインに少し斜め回転でサイバーチップ感を演出）
    rotation_.y += 0.045f * gameSpeed;
    rotation_.x = 0.35f + std::sinf(animTimer_ * 0.7f) * 0.15f;
    rotation_.z = std::cosf(animTimer_ * 0.5f) * 0.12f;

    // パルス明滅・スケール脈動（遠くからでもアイテムだと一目でわかるように）
    float pulse = 1.0f + 0.12f * std::sinf(animTimer_ * 3.5f);
    float currentScaleVal = baseScale_.x * s_ScaleMultiplier * pulse;

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
                attractSpeed_ += 0.1f * gameSpeed;
                float moveStep = attractSpeed_ * gameSpeed;

                // 吸い込まれるにつれて少し縮小して自機に収束する演出
                float shrinkFactor = std::clamp(dist / magnetRadius_, 0.35f, 1.0f);
                currentScaleVal *= shrinkFactor;

                if (moveStep >= dist || dist < collectRadius_) {
                    // 回収完了！プレイヤーの通常弾を変化（装備）
                    player->EquipMemory(memoryType_);
                    SoundManager::GetInstance()->PlaySE("powerup.mp3", 0.9f);
                    isDead_ = true;
                    return;
                } else {
                    position_.x += dir.x * moveStep;
                    position_.y += dir.y * moveStep;
                    position_.z += dir.z * moveStep;
                }
            }
        } else {
            // 通常浮遊時は微弱な上下ボビング
            position_.y += std::sinf(animTimer_ * 2.0f) * 0.025f * gameSpeed;

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
        float glow = 0.9f + 0.15f * std::sinf(animTimer_ * 4.0f);
        finalCol.x = std::clamp(itemColor_.x * glow, 0.0f, 1.0f);
        finalCol.y = std::clamp(itemColor_.y * glow, 0.0f, 1.0f);
        finalCol.z = std::clamp(itemColor_.z * glow, 0.0f, 1.0f);

        object_->SetColor(finalCol);
        object_->SetTranslate(position_);
        object_->SetRotation(rotation_);
        object_->SetScale(Vector3(currentScaleVal, currentScaleVal, currentScaleVal));
        object_->Update();
    }
}

void MemoryItem::Draw() {
    if (isDead_ || !object_) return;
    object_->Draw();
}
