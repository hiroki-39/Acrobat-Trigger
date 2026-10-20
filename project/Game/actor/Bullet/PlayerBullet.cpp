#include "PlayerBullet.h"
#include "Game/Actor/Enemy/Enemy.h"
#include <cmath>

void PlayerBullet::Initialize(Object3dCommon* object3dCommon, const Vector3& position, const Vector3& velocity, Object3d* parent, Enemy* targetEnemy) {
    velocity_ = velocity;
    targetEnemy_ = targetEnemy;
    
    // 進行方向への初期向きを計算
    float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);
    if (speed > 0.0001f) {
        rotation_.y = std::atan2(velocity_.x, velocity_.z);
        float xzLen = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
        rotation_.x = std::atan2(-velocity_.y, xzLen);
    }

    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel("beam.obj"); // 専用レーザービームモデル
    object_->SetSelectLightings(6); // ビーム発光モード（中心ホワイトハイライト + 外縁プラズマ）
    object_->SetEnableLighting(false); // 自発光
    object_->SetColor({ 1.0f, 0.12f, 0.65f, 1.0f }); // 背景シアンの補色となる鮮烈なネオンマゼンタ（ピンク）
    object_->SetTranslate(position);
    object_->SetRotation(rotation_);
    object_->SetScale({ 1.35f, 1.35f, 11.0f }); // 視認性と迫力を高めた大型ビーム形状

    // 光の尾（ビームトレイル）の生成
    trailObject_ = std::make_unique<Object3d>();
    trailObject_->Initialize(object3dCommon);
    trailObject_->SetModel("beam.obj");
    trailObject_->SetSelectLightings(6);
    trailObject_->SetEnableLighting(false);
    trailObject_->SetColor({ 1.0f, 0.25f, 0.75f, 0.50f }); // わずかに透過したピンクの残光
    trailObject_->SetTranslate(position);
    trailObject_->SetRotation(rotation_);
    trailObject_->SetScale({ 1.0f, 1.0f, 16.0f }); // 大型ビームに合わせた力強いロングトレイル

    colliderObject_ = std::make_unique<Object3d>();
    colliderObject_->Initialize(object3dCommon);
    colliderObject_->SetModel("collider_sphere_player.obj");
    colliderObject_->GetModel()->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f });
    colliderObject_->SetTranslate(position);
    colliderObject_->SetScale({ 4.2f, 4.2f, 8.0f }); // 高速移動時のすり抜け防止
    previousPosition_ = position;
}

void PlayerBullet::Update(float gameSpeed) {
    previousPosition_ = object_->GetTranslate();
    Vector3 pos = previousPosition_;

    // 微弱な弾道アシスト（急カーブせず直進性を保ちつつ、微小角度・近中距離のみかすり補正）
    if (targetEnemy_ && !targetEnemy_->IsDead()) {
        Vector3 targetPos = targetEnemy_->GetColliderCenter();
        Vector3 toTarget = {
            targetPos.x - pos.x,
            targetPos.y - pos.y,
            targetPos.z - pos.z
        };

        float length = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
        float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);

        // 距離75m以内、かつ進行方向から約5度以内の極微小なズレのみ（弾の勝手な急旋回を防止し、直進性を最優先）
        if (length > 0.0f && length < 75.0f && speed > 0.0001f) {
            Vector3 curDir = { velocity_.x / speed, velocity_.y / speed, velocity_.z / speed };
            Vector3 targetDir = { toTarget.x / length, toTarget.y / length, toTarget.z / length };
            float dot = curDir.x * targetDir.x + curDir.y * targetDir.y + curDir.z * targetDir.z;

            if (dot > 0.996f) { // 約5度以内の極小ズレのみ
                float homingStrength = 0.009f * gameSpeed;
                velocity_.x += (targetDir.x * speed - velocity_.x) * homingStrength;
                velocity_.y += (targetDir.y * speed - velocity_.y) * homingStrength;
                velocity_.z += (targetDir.z * speed - velocity_.z) * homingStrength;

                float newLength = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);
                if (newLength > 0.0f) {
                    velocity_.x = (velocity_.x / newLength) * speed;
                    velocity_.y = (velocity_.y / newLength) * speed;
                    velocity_.z = (velocity_.z / newLength) * speed;
                }
            }
        }
    }

    // 速度ベクトルに従って移動
    pos.x += velocity_.x * gameSpeed;
    pos.y += velocity_.y * gameSpeed;
    pos.z += velocity_.z * gameSpeed;
    object_->SetTranslate(pos);

    // 進行方向に向きを同期
    float curSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);
    if (curSpeed > 0.0001f) {
        rotation_.y = std::atan2(velocity_.x, velocity_.z);
        float xzLen = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
        rotation_.x = std::atan2(-velocity_.y, xzLen);
    }
    object_->SetRotation(rotation_);

    // 寿命
    deathTimer_ -= gameSpeed;
    if (deathTimer_ <= 0.0f) {
        isDead_ = true;
    }

    object_->Update();

    // トレイル（光の尾）の追従更新
    if (trailObject_) {
        // メインビームの後端から後方へ伸びるように配置
        Vector3 trailPos = {
            pos.x - velocity_.x * 0.45f,
            pos.y - velocity_.y * 0.45f,
            pos.z - velocity_.z * 0.45f
        };
        trailObject_->SetTranslate(trailPos);
        trailObject_->SetRotation(rotation_);
        trailObject_->Update();
    }

    if (colliderObject_) {
        colliderObject_->SetTranslate(pos);
        colliderObject_->Update();
    }
}

void PlayerBullet::Draw() {
    // トレイルを先に描画し、メインビームを前面に重ねる
    if (trailObject_) {
        trailObject_->Draw();
    }
    if (object_) {
        object_->Draw();
    }
}

void PlayerBullet::DrawCollider() {
    if (colliderObject_) {
        colliderObject_->Draw();
    }
}
