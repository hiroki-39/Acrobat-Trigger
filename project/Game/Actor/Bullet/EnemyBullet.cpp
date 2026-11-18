#include "EnemyBullet.h"
#include "Game/Actor/Player/Player.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include <cmath>

void EnemyBullet::Initialize(Object3dCommon* object3dCommon, const Vector3& position, const Vector3& velocity, bool isHoming, Player* targetPlayer) {
    velocity_ = velocity;
    isHoming_ = isHoming;
    targetPlayer_ = targetPlayer;
    
    // 進行方向への初期向きを計算
    float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);
    if (speed > 0.0001f) {
        rotation_.y = std::atan2(velocity_.x, velocity_.z);
        float xzLen = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
        rotation_.x = std::atan2(-velocity_.y, xzLen);
    }

    // 敵弾専用モデルをロード
    ModelManager::GetInstance()->LoadModel("beam_enemy.obj");

    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel("beam_enemy.obj"); // 敵弾専用ビームモデルを使用
    object_->SetSelectLightings(6); // ビーム発光モード（中心ハイライト + 外縁プラズマ）
    object_->SetEnableLighting(false); // 自発光
    object_->SetColor({ 1.0f, 0.92f, 0.08f, 1.0f }); // 鮮烈な発光イエロービーム（ボスのバイオレット装甲や背景に対して視認性を大幅向上）
    object_->SetTranslate(position);
    object_->SetRotation(rotation_);
    object_->SetScale({ 0.6f, 0.6f, 4.2f }); // プレイヤー弾より少し太めで視認性の高いシャープなレーザー形状

    colliderObject_ = std::make_unique<Object3d>();
    colliderObject_->Initialize(object3dCommon);
    colliderObject_->SetModel("collider_sphere_enemy.obj"); 
    colliderObject_->GetModel()->SetColor({ 1.0f, 0.85f, 0.0f, 1.0f }); // 弾コライダーもイエロー 
    colliderObject_->SetTranslate(position);
    colliderObject_->SetScale({ 3.0f, 3.0f, 3.0f }); 
    previousPosition_ = position;
}

void EnemyBullet::InitializeMissile(Object3dCommon* object3dCommon, const Vector3& position, const Vector3& launchVel, Player* targetPlayer, int salvoIndex) {
    velocity_ = launchVel;
    isHoming_ = true;
    targetPlayer_ = targetPlayer;
    isMissile_ = true;
    isDestructible_ = true;
    hp_ = 1; // プレイヤーの弾1発で撃墜可能
    missilePhase_ = 0; // 0: 上空上昇, 1: プレイヤー追尾突進
    missilePhaseTimer_ = 0.0f;
    deathTimer_ = 360.0f; // 6秒
    salvoIndex_ = salvoIndex;
    hasPassedPlayer_ = false;
    prevDistToPlayer_ = 99999.0f;

    ModelManager::GetInstance()->LoadModel("missile_enemy.obj");
    ModelManager::GetInstance()->LoadModel("collider_sphere_enemy.obj");

    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel("missile_enemy.obj");
    object_->SetTranslate(position);
    object_->SetScale({ 2.8f, 2.8f, 2.8f }); // 迫力のある大型巡航ミサイルサイズ（幅約0.84m、長さ約7.6m）
    object_->SetEnableLighting(false); // 背景と同化しないよう陰影による黒つぶれを防ぎ、鮮烈な赤と黒の警戒色を強調

    // 進行方向への初期向き（missile.objは-Z軸が先端弾頭）
    float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);
    if (speed > 0.0001f) {
        float xzLen = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
        rotation_.y = std::atan2(-velocity_.x, -velocity_.z);
        rotation_.x = std::atan2(velocity_.y, xzLen);
    }
    object_->SetRotation(rotation_);

    colliderObject_ = std::make_unique<Object3d>();
    colliderObject_->Initialize(object3dCommon);
    colliderObject_->SetModel("collider_sphere_enemy.obj");
    colliderObject_->GetModel()->SetColor({ 1.0f, 0.5f, 0.0f, 1.0f });
    colliderObject_->SetTranslate(position);
    colliderObject_->SetScale({ 3.0f, 3.0f, 3.0f }); // モデルサイズに適合したヒット判定サイズ
    previousPosition_ = position;
}

void EnemyBullet::Update(float gameSpeed) {
    previousPosition_ = object_->GetTranslate();
    Vector3 pos = previousPosition_;

    // --- ミサイル特殊飛翔ロジック（正面視界上空へ素早く集結 ➔ プレイヤーへ正面急降下突進） ---
    if (isMissile_) {
        missilePhaseTimer_ += gameSpeed;

        if (missilePhase_ == 0) {
            // フェーズ0: プレイヤーの正面視界前方上空へ向かってクイックに舞い上がり（約20フレーム/0.33秒に短縮）
            if (targetPlayer_) {
                Vector3 playerPos = targetPlayer_->GetWorldPosition();
                Vector3 playerForward = targetPlayer_->GetWorldForward();
                Vector3 playerUp = targetPlayer_->GetWorldUp();
                Vector3 playerRight = targetPlayer_->GetWorldRight();

                // 扇状の左右散開（6発が画面上部に横並びに綺麗に整列）
                float xOffset = ((static_cast<float>(salvoIndex_) - 2.5f) / 2.5f) * 8.0f;

                // 自機の前方40m、上空+15m の空中に素早く配置
                Vector3 skyTarget = {
                    playerPos.x + playerForward.x * 40.0f + playerUp.x * 15.0f + playerRight.x * xOffset,
                    playerPos.y + playerForward.y * 40.0f + playerUp.y * 15.0f + playerRight.y * xOffset,
                    playerPos.z + playerForward.z * 40.0f + playerUp.z * 15.0f + playerRight.z * xOffset
                };

                Vector3 toSky = { skyTarget.x - pos.x, skyTarget.y - pos.y, skyTarget.z - pos.z };
                float skyDist = std::sqrt(toSky.x * toSky.x + toSky.y * toSky.y + toSky.z * toSky.z);
                if (skyDist > 0.001f) {
                    toSky = { toSky.x / skyDist, toSky.y / skyDist, toSky.z / skyDist };
                }

                float climbSpeed = 3.2f; // クイックな上昇
                float steer = 0.22f * gameSpeed;
                velocity_.x += (toSky.x * climbSpeed - velocity_.x) * steer;
                velocity_.y += (toSky.y * climbSpeed - velocity_.y) * steer;
                velocity_.z += (toSky.z * climbSpeed - velocity_.z) * steer;
            } else {
                velocity_.y += 0.05f * gameSpeed;
            }

            // 進行方向に姿勢を同期（上空へ向かって飛翔）
            float curSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);
            if (curSpeed > 0.0001f) {
                float xzLen = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
                rotation_.y = std::atan2(-velocity_.x, -velocity_.z);
                rotation_.x = std::atan2(velocity_.y, xzLen);
            }

            // 上昇時間を短縮（38f ➔ 20f: 約0.33秒）
            if (missilePhaseTimer_ >= 20.0f) {
                missilePhase_ = 1;
                missilePhaseTimer_ = 0.0f;
                prevDistToPlayer_ = 99999.0f;
                hasPassedPlayer_ = false;
            }
        } else {
            // フェーズ1: プレイヤー正面へ向けて一直線に急降下突進（速度を抑えて確実にプレイヤーを捉える）
            if (targetPlayer_) {
                Vector3 targetPos = targetPlayer_->GetWorldPosition();
                Vector3 playerForward = targetPlayer_->GetWorldForward();
                Vector3 toTarget = {
                    targetPos.x - pos.x,
                    targetPos.y - pos.y,
                    targetPos.z - pos.z
                };
                float dist = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);

                // 自機から見たミサイルの前後位置（前方内積）
                Vector3 diff = { pos.x - targetPos.x, pos.y - targetPos.y, pos.z - targetPos.z };
                float forwardDist = diff.x * playerForward.x + diff.y * playerForward.y + diff.z * playerForward.z;

                // 自機の背後へ抜けた場合（forwardDist < -2.0m）、Uターンせず直進して消滅へ
                if (forwardDist < -2.0f) {
                    hasPassedPlayer_ = true;
                }
                if (dist < prevDistToPlayer_) {
                    prevDistToPlayer_ = dist;
                }

                if (!hasPassedPlayer_) {
                    // 自機中心へ向かう高精度誘導（速度を1.25fに抑え、しっかり追従）
                    if (dist > 0.001f) {
                        toTarget = { toTarget.x / dist, toTarget.y / dist, toTarget.z / dist };
                    }

                    float targetSpeed = 1.25f; // 速度を抑えてプレイヤーが迎撃・認識しやすくする
                    float steerRate = 0.16f * gameSpeed; // しっかりプレイヤーを狙う高い追従性
                    velocity_.x += (toTarget.x * targetSpeed - velocity_.x) * steerRate;
                    velocity_.y += (toTarget.y * targetSpeed - velocity_.y) * steerRate;
                    velocity_.z += (toTarget.z * targetSpeed - velocity_.z) * steerRate;

                    float curSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);
                    if (curSpeed > 0.001f) {
                        velocity_ = { (velocity_.x / curSpeed) * targetSpeed, (velocity_.y / curSpeed) * targetSpeed, (velocity_.z / curSpeed) * targetSpeed };
                    }
                } else {
                    // 自機を通過した後は旋回せず、直進して速やかに自爆消滅（Uターン禁止）
                    deathTimer_ -= 10.0f * gameSpeed;
                }
            }

            // 進行方向に姿勢を滑らかに同期（missile.objは-Z先端）
            float curSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);
            if (curSpeed > 0.0001f) {
                float xzLen = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
                rotation_.y = std::atan2(-velocity_.x, -velocity_.z);
                rotation_.x = std::atan2(velocity_.y, xzLen);
            }
        }

        pos.x += velocity_.x * gameSpeed;
        pos.y += velocity_.y * gameSpeed;
        pos.z += velocity_.z * gameSpeed;
        object_->SetTranslate(pos);
        object_->SetRotation(rotation_);

        deathTimer_ -= gameSpeed;
        if (deathTimer_ <= 0.0f) {
            isDead_ = true;
        }

        object_->Update();
        // colliderObject_ の更新は DrawCollider 呼び出し時のみ行う（毎フレームの定数バッファ転送負荷を削減）
        return;
    }

    // --- 通常弾ホーミング処理 ---
    if (isHoming_ && targetPlayer_) {
        Vector3 targetPos = targetPlayer_->GetWorldPosition();
        Vector3 toTarget = {
            targetPos.x - pos.x,
            targetPos.y - pos.y,
            targetPos.z - pos.z
        };

        float length = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);
        if (length > 0.0f) {
            toTarget.x /= length;
            toTarget.y /= length;
            toTarget.z /= length;
        }

        float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);

        float homingStrength = 0.08f * gameSpeed; // プレイヤーのホーミングより少し弱めにする
        velocity_.x += (toTarget.x * speed - velocity_.x) * homingStrength;
        velocity_.y += (toTarget.y * speed - velocity_.y) * homingStrength;
        velocity_.z += (toTarget.z * speed - velocity_.z) * homingStrength;

        float newLength = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y + velocity_.z * velocity_.z);
        if (newLength > 0.0f) {
            velocity_.x = (velocity_.x / newLength) * speed;
            velocity_.y = (velocity_.y / newLength) * speed;
            velocity_.z = (velocity_.z / newLength) * speed;
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
    // colliderObject_ の更新は DrawCollider 呼び出し時のみ行う
}

void EnemyBullet::Draw() {
    if (object_) {
        object_->Draw();
    }
}

void EnemyBullet::DrawCollider() {
    if (colliderObject_ && object_) {
        colliderObject_->SetTranslate(object_->GetTranslate());
        colliderObject_->SetRotation(rotation_);
        colliderObject_->Update();
        colliderObject_->Draw();
    }
}

void EnemyBullet::OnDamaged(int damage) {
    if (!isDestructible_) return;
    hp_ -= damage;
    if (hp_ <= 0) {
        isDead_ = true;
    }
}
