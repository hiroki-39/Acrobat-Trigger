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

void EnemyBullet::Update(float gameSpeed) {
    previousPosition_ = object_->GetTranslate();
    Vector3 pos = previousPosition_;

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

void EnemyMissile::Initialize(Object3dCommon* object3dCommon, const Vector3& position, const Vector3& launchVel, Player* targetPlayer, int salvoIndex) {
    SetVelocity(launchVel);
    SetTargetPlayer(targetPlayer);
    hp_ = 1; // プレイヤーの弾1発で撃墜可能
    missilePhase_ = 0; // 0: 上空上昇, 1: プレイヤー追尾突進
    missilePhaseTimer_ = 0.0f;
    SetDeathTimer(360.0f); // 6秒
    salvoIndex_ = salvoIndex;
    hasPassedPlayer_ = false;
    prevDistToPlayer_ = 99999.0f;

    ModelManager::GetInstance()->LoadModel("missile_enemy.obj");
    ModelManager::GetInstance()->LoadModel("collider_sphere_enemy.obj");

    auto obj = std::make_unique<Object3d>();
    obj->Initialize(object3dCommon);
    obj->SetModel("missile_enemy.obj");
    obj->SetTranslate(position);
    obj->SetScale({ 2.8f, 2.8f, 2.8f }); // 迫力のある大型巡航ミサイルサイズ
    obj->SetEnableLighting(false);

    // 進行方向への初期向き（missile.objは-Z軸が先端弾頭）
    Vector3 rot = { 0.0f, 0.0f, 0.0f };
    float speed = std::sqrt(launchVel.x * launchVel.x + launchVel.y * launchVel.y + launchVel.z * launchVel.z);
    if (speed > 0.0001f) {
        float xzLen = std::sqrt(launchVel.x * launchVel.x + launchVel.z * launchVel.z);
        rot.y = std::atan2(-launchVel.x, -launchVel.z);
        rot.x = std::atan2(launchVel.y, xzLen);
    }
    obj->SetRotation(rot);
    SetRotation(rot);

    auto colObj = std::make_unique<Object3d>();
    colObj->Initialize(object3dCommon);
    colObj->SetModel("collider_sphere_enemy.obj");
    colObj->GetModel()->SetColor({ 1.0f, 0.5f, 0.0f, 1.0f });
    colObj->SetTranslate(position);
    colObj->SetScale({ 3.0f, 3.0f, 3.0f });
    SetObjects(std::move(obj), std::move(colObj));
    SetPreviousPosition(position);
}

void EnemyMissile::Update(float gameSpeed) {
    Object3d* obj = GetObject3d();
    if (!obj) return;

    Vector3 pos = obj->GetTranslate();
    SetPreviousPosition(pos);
    Vector3 vel = GetVelocity();
    Vector3 rot = GetRotation();
    Player* targetPlayer = GetTargetPlayer();

    missilePhaseTimer_ += gameSpeed;

    if (missilePhase_ == 0) {
        if (targetPlayer) {
            Vector3 playerPos = targetPlayer->GetWorldPosition();
            Vector3 playerForward = targetPlayer->GetWorldForward();
            Vector3 playerUp = targetPlayer->GetWorldUp();
            Vector3 playerRight = targetPlayer->GetWorldRight();

            float xOffset = ((static_cast<float>(salvoIndex_) - 2.5f) / 2.5f) * 8.0f;
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

            float climbSpeed = 3.2f;
            float steer = 0.22f * gameSpeed;
            vel.x += (toSky.x * climbSpeed - vel.x) * steer;
            vel.y += (toSky.y * climbSpeed - vel.y) * steer;
            vel.z += (toSky.z * climbSpeed - vel.z) * steer;
        } else {
            vel.y += 0.05f * gameSpeed;
        }

        float curSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
        if (curSpeed > 0.0001f) {
            float xzLen = std::sqrt(vel.x * vel.x + vel.z * vel.z);
            rot.y = std::atan2(-vel.x, -vel.z);
            rot.x = std::atan2(vel.y, xzLen);
        }

        if (missilePhaseTimer_ >= 20.0f) {
            missilePhase_ = 1;
            missilePhaseTimer_ = 0.0f;
            prevDistToPlayer_ = 99999.0f;
            hasPassedPlayer_ = false;
        }
    } else {
        if (targetPlayer) {
            Vector3 targetPos = targetPlayer->GetWorldPosition();
            Vector3 playerForward = targetPlayer->GetWorldForward();
            Vector3 toTarget = {
                targetPos.x - pos.x,
                targetPos.y - pos.y,
                targetPos.z - pos.z
            };
            float dist = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);

            Vector3 diff = { pos.x - targetPos.x, pos.y - targetPos.y, pos.z - targetPos.z };
            float forwardDist = diff.x * playerForward.x + diff.y * playerForward.y + diff.z * playerForward.z;

            if (forwardDist < -2.0f) {
                hasPassedPlayer_ = true;
            }
            if (dist < prevDistToPlayer_) {
                prevDistToPlayer_ = dist;
            }

            if (!hasPassedPlayer_) {
                if (dist > 0.001f) {
                    toTarget = { toTarget.x / dist, toTarget.y / dist, toTarget.z / dist };
                }

                float targetSpeed = 1.25f;
                float steerRate = 0.16f * gameSpeed;
                vel.x += (toTarget.x * targetSpeed - vel.x) * steerRate;
                vel.y += (toTarget.y * targetSpeed - vel.y) * steerRate;
                vel.z += (toTarget.z * targetSpeed - vel.z) * steerRate;

                float curSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
                if (curSpeed > 0.001f) {
                    vel = { (vel.x / curSpeed) * targetSpeed, (vel.y / curSpeed) * targetSpeed, (vel.z / curSpeed) * targetSpeed };
                }
            } else {
                DecrementDeathTimer(10.0f * gameSpeed);
            }
        }

        float curSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
        if (curSpeed > 0.0001f) {
            float xzLen = std::sqrt(vel.x * vel.x + vel.z * vel.z);
            rot.y = std::atan2(-vel.x, -vel.z);
            rot.x = std::atan2(vel.y, xzLen);
        }
    }

    pos.x += vel.x * gameSpeed;
    pos.y += vel.y * gameSpeed;
    pos.z += vel.z * gameSpeed;
    SetVelocity(vel);
    SetRotation(rot);
    obj->SetTranslate(pos);
    obj->SetRotation(rot);

    DecrementDeathTimer(gameSpeed);
    if (GetDeathTimer() <= 0.0f) {
        SetDead(true);
    }

    obj->Update();
}

void EnemyMissile::OnDamaged(int damage) {
    hp_ -= damage;
    if (hp_ <= 0) {
        SetDead(true);
    }
}
