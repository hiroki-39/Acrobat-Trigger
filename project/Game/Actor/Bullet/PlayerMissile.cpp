#include "PlayerMissile.h"
#include "Game/Actor/Enemy/Enemy.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include <cmath>

void PlayerMissile::Initialize(Object3dCommon* object3dCommon, const Vector3& position, const Vector3& rotation, const Vector3& velocity, Object3d* parent, Enemy* targetEnemy, ArmoredTrainBoss* targetBoss, int targetCarriageIndex) {
    spawnPosition_ = position;
    position_ = position;
    previousPosition_ = position;
    velocity_ = velocity;
    rotation_ = rotation;
    targetEnemy_ = targetEnemy;
    targetBoss_ = targetBoss;
    targetCarriageIndex_ = targetCarriageIndex;
    
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel("missile.obj"); // ユーザー指定の missile.obj を使用
    object_->SetColor({ 0.15f, 0.92f, 1.0f, 1.0f }); // サイバー仮想空間用ネオンシアン
    object_->SetEnvironmentCoefficient(0.0f);
    object_->SetEnableLighting(true);
    object_->SetSelectLightings(11); // ミサイル専用サイバーシェーダー
    object_->SetTranslate(position_);
    object_->SetRotation(rotation_);
    object_->SetScale(Vector3(0.5f, 0.5f, 0.5f)); // 適切なスケールに調整 (mountedMissiles_ のワールドスケール0.5fと一致させる)

    // (環境テクスチャの設定は削除)

    // コライダーの初期化
    ModelManager::GetInstance()->LoadModel("collider_cube.obj");
    colliderObject_ = std::make_unique<Object3d>();
    colliderObject_->Initialize(object3dCommon);
    colliderObject_->SetModel("collider_cube.obj");
    colliderObject_->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f }); // 赤色
    colliderObject_->SetEnvironmentCoefficient(0.0f);
    colliderObject_->SetEnableLighting(false);
    colliderObject_->SetScale(Vector3(1.5f, 1.5f, 1.5f));
}

void PlayerMissile::Update(float gameSpeed) {
    if (isDead_) return;
    
    deathTimer_ -= gameSpeed;
    if (deathTimer_ <= 0.0f) {
        isDead_ = true;
        return;
    }

    // 最大飛行射程チェック（発射地点からの移動距離が射程を超えたら消滅）
    Vector3 flightDelta = {
        position_.x - spawnPosition_.x,
        position_.y - spawnPosition_.y,
        position_.z - spawnPosition_.z
    };
    float flightDistSq = flightDelta.x * flightDelta.x + flightDelta.y * flightDelta.y + flightDelta.z * flightDelta.z;
    if (flightDistSq > maxFlightDistance_ * maxFlightDistance_) {
        isDead_ = true;
        return;
    }

    previousPosition_ = position_;

    if (currentPhase_ == Phase::DROP) {
        phaseTimer_ += gameSpeed;
        
        // 落下フェーズ：自機から下に落ちる挙動
        // （少しだけ前に進みつつ下方向に加速）
        velocity_.y -= 0.05f * gameSpeed;
        
        if (phaseTimer_ >= dropDuration_) {
            currentPhase_ = Phase::FLIGHT;
            // スラスター点火時の初期速度ベクトル（前方向へ）
            velocity_.x = std::sin(rotation_.y) * std::cos(rotation_.x) * 1.5f;
            velocity_.y = -std::sin(rotation_.x) * 1.5f;
            velocity_.z = std::cos(rotation_.y) * std::cos(rotation_.x) * 1.5f;
        }
    } 
    else if (currentPhase_ == Phase::FLIGHT) {
        phaseTimer_ += gameSpeed;
        // ホーミング（追尾）フェーズ
        Vector3 targetPos = { 0, 0, 0 };
        bool hasTarget = false;

        if (targetBoss_ && !targetBoss_->IsDefeated() && !targetBoss_->IsCarriageDestroyed(targetCarriageIndex_)) {
            targetPos = targetBoss_->GetCarriagePosition(targetCarriageIndex_);
            hasTarget = true;
        } else if (targetEnemy_ && !targetEnemy_->IsDead()) {
            targetPos = targetEnemy_->GetColliderCenter();
            hasTarget = true;
        }

        if (hasTarget) {
            Vector3 toTarget = {
                targetPos.x - position_.x,
                targetPos.y - position_.y,
                targetPos.z - position_.z
            };
            
            float distance = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
            if (distance > 0.0001f) {
                toTarget.x /= distance;
                toTarget.y /= distance;
                toTarget.z /= distance;
            }

            // 現在の進行方向をターゲットに向けて少しずつ曲げる (旋回性能)
            float turnSpeed = 0.15f * gameSpeed; 
            velocity_.x += (toTarget.x * 5.0f - velocity_.x) * turnSpeed;
            velocity_.y += (toTarget.y * 5.0f - velocity_.y) * turnSpeed;
            velocity_.z += (toTarget.z * 5.0f - velocity_.z) * turnSpeed;
            
            // 速度ベクトルの正規化とスピード適用
            float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);
            if (speed > 0.0001f) {
                // 点火してから徐々に速度を上げる (スピーディすぎないように)
                float flightTime = phaseTimer_ - dropDuration_;
                float currentMaxSpeed = 1.0f + flightTime * 0.5f; // 毎フレーム 0.5 ずつ加速 (一気に最高速へ)
                if (currentMaxSpeed > 15.0f) currentMaxSpeed = 15.0f; // 最高速度

                velocity_.x = (velocity_.x / speed) * currentMaxSpeed;
                velocity_.y = (velocity_.y / speed) * currentMaxSpeed;
                velocity_.z = (velocity_.z / speed) * currentMaxSpeed;
            }

            // 回転を速度ベクトルに合わせる
            rotation_.y = std::atan2(velocity_.x, velocity_.z);
            float xzLen = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
            rotation_.x = std::atan2(-velocity_.y, xzLen);
        }
    }

    position_.x += velocity_.x * gameSpeed;
    position_.y += velocity_.y * gameSpeed;
    position_.z += velocity_.z * gameSpeed;

    if (object_) {
        object_->SetTranslate(position_);
        object_->SetRotation(rotation_);
        object_->Update();
        
        if (colliderObject_) {
            colliderObject_->SetTranslate(object_->GetTranslate());
            colliderObject_->SetRotation(object_->GetRotation());
            colliderObject_->Update();
        }
    }
}

void PlayerMissile::Draw() {
    if (object_ && !isDead_) {
        object_->Draw();
    }
}

void PlayerMissile::DrawCollider() {
    if (colliderObject_ && !isDead_) {
        colliderObject_->Draw();
    }
}
