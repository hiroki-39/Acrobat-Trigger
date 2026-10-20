#include "Player.h"
#include "Game/Actor/Bullet/PlayerMissile.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Debug/Imgui/ImGuiManager.h"
#include "externals/nlohmann/json.hpp"
#include <fstream>
#include <algorithm>
#include "Game/Actor/Enemy/Enemy.h"
#include "KHEngine/Graphics/3d/Particle/ParticleManager.h"
#include "KHEngine/Sound/Core/SoundManager.h"

void Player::Initialize(Object3dCommon* object3dCommon, uint32_t skyboxTexIndex) {
    // プレイヤーと関連オブジェクトの初期化
    object3dCommon_ = object3dCommon;
    input_ = EngineServices::GetInstance()->GetInput();

    skyboxTexIndex_ = skyboxTexIndex;

    // プレイヤー本体のモデル設定（タイトルシーンのサイバー機体と完全に統一）
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel(modelName_);
    color_ = { 1.0f, 1.0f, 1.0f, 1.0f }; // タイトルシーンと同一のクリアホワイト（シェーダー発色を完全再現）
    object_->SetColor(color_);
    object_->SetSelectLightings(8); // タイトルシーンと同一のサイバー仮想空間自機シェーダー（case 8: ホログラム装甲・ネオンパネル・ネオンリム発光）
    object_->SetEnvironmentTextureIndex(skyboxTexIndex);
    object_->SetEnvironmentCoefficient(0.0f); // ホログラム発光を純粋に保つため環境反射オフ
    object_->SetEnableLighting(true);
    object_->SetScale(playerScale_);
     
    // 奥側のレティクル（敵追従照準枠）設定
    reticle_ = std::make_unique<Object3d>();
    reticle_->Initialize(object3dCommon);
    reticle_->SetModel("crossHair.obj");
    reticleColor_ = { 1.0f, 0.58f, 0.08f, 0.95f }; 
    reticle_->SetColor(reticleColor_);
    reticle_->SetEnvironmentTextureIndex(skyboxTexIndex);
    reticle_->SetEnvironmentCoefficient(0.0f);
    reticle_->SetEnableLighting(false);
    reticle_->SetSelectLightings(0);
    baseReticleScale_ = Vector3(1.6f, 1.6f, 1.6f);
    dynamicReticleDistance_ = 45.0f; // 俊敏な照準追従を実現する標準距離45m
    reticle_->SetScale(baseReticleScale_);
    
    // 手前側のレティクル（自機前方の赤丸ビルボード・銃口ポインタ）設定
    frontReticle_ = std::make_unique<Object3d>();
    frontReticle_->Initialize(object3dCommon);
    frontReticle_->SetModel("frontReticle.obj");
    frontReticle_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    frontReticle_->SetEnvironmentTextureIndex(skyboxTexIndex);
    frontReticle_->SetEnvironmentCoefficient(0.0f);
    frontReticle_->SetEnableLighting(false);
    frontReticle_->SetSelectLightings(0);
    frontReticle_->SetScale(Vector3(1.2f, 1.2f, 1.2f)); 
    
    reticlePosition_ = { 0.0f, 0.0f, 45.0f }; 

    
    if (auto pp = EngineServices::GetInstance()->GetPostProcess()) {
        pp->SetEffectActive("Grayscale", false);
    }
    
    ModelManager::GetInstance()->LoadModel("collider_cube_player.obj");
    colliderObject_ = std::make_unique<Object3d>();
    colliderObject_->Initialize(object3dCommon);
    colliderObject_->SetModel("collider_cube_player.obj"); 
    colliderObject_->GetModel()->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f }); 
    colliderObject_->SetEnvironmentCoefficient(0.0f);
    colliderObject_->SetEnableLighting(false);
    colliderObject_->SetScale(colliderSize_); 

    
    ModelManager::GetInstance()->LoadModel("missile.obj");
    ModelManager::GetInstance()->LoadModel("RockOn.obj");
    for (int i = 0; i < MAX_MISSILES; ++i) {
        mountedMissiles_[i] = std::make_unique<Object3d>();
        mountedMissiles_[i]->Initialize(object3dCommon);
        mountedMissiles_[i]->SetModel("missile.obj");
        mountedMissiles_[i]->SetParent(object_.get());
        float xOffset = 0.0f;
        float yOffset = -0.2f;
        float zOffset = 0.0f;
        if (i == 0) { xOffset = -1.5f; yOffset = -0.2f; zOffset = 0.0f; } 
        else if (i == 1) { xOffset = 1.5f; yOffset = -0.2f; zOffset = 0.0f; } 
        else if (i == 2) { xOffset = -2.8f; yOffset = -0.2f; zOffset = 0.0f; } 
        else if (i == 3) { xOffset = 2.8f; yOffset = -0.2f; zOffset = 0.0f; } 
        else if (i == 4) { xOffset = -2.0f; yOffset = -0.35f; zOffset = -0.8f; } 
        else if (i == 5) { xOffset = 2.0f; yOffset = -0.35f; zOffset = -0.8f; } 
        else if (i == 6) { xOffset = -3.4f; yOffset = -0.35f; zOffset = -0.8f; } 
        else if (i == 7) { xOffset = 3.4f; yOffset = -0.35f; zOffset = -0.8f; } 
        
        mountedMissiles_[i]->SetTranslate({xOffset, yOffset, zOffset});
        mountedMissiles_[i]->SetScale({1.0f, 1.0f, 1.0f}); 
        
        lockOnReticles_[i] = std::make_unique<Object3d>();
        lockOnReticles_[i]->Initialize(object3dCommon);
        lockOnReticles_[i]->SetModel("RockOn.obj");
        lockOnReticles_[i]->GetModel()->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        lockOnReticles_[i]->SetScale({0.001f, 0.001f, 0.001f}); 
        lockOnReticles_[i]->SetEnableLighting(false);
        
        mountedMissiles_[i]->Update();
        lockOnReticles_[i]->Update();
    }

    // 接地マーカー（プロジェクションシャドウ）の初期化
    ModelManager::GetInstance()->LoadModel("ground_marker.obj");
    groundMarkerObject_ = std::make_unique<Object3d>();
    groundMarkerObject_->Initialize(object3dCommon);
    groundMarkerObject_->SetModel("ground_marker.obj");
    groundMarkerObject_->SetEnableLighting(false);
    groundMarkerObject_->SetSelectLightings(0);
    groundMarkerObject_->SetColor({ 0.0f, 0.90f, 1.0f, 0.85f });
    groundMarkerObject_->SetScale({ 2.2f, 1.0f, 2.2f });

    // ツインバーニア（推進噴射炎コーン）の初期化
    ModelManager::GetInstance()->LoadModel("vernier_cone.obj");
    vernierLeft_ = std::make_unique<Object3d>();
    vernierLeft_->Initialize(object3dCommon);
    vernierLeft_->SetModel("vernier_cone.obj");
    vernierLeft_->SetParent(object_.get());
    vernierLeft_->SetTranslate({ -0.55f, -0.05f, -1.85f });
    vernierLeft_->SetScale({ 0.45f, 0.45f, 1.2f });
    vernierLeft_->SetEnableLighting(false);
    vernierLeft_->SetSelectLightings(0);
    vernierLeft_->SetColor({ 0.35f, 0.90f, 1.0f, 0.95f });

    vernierRight_ = std::make_unique<Object3d>();
    vernierRight_->Initialize(object3dCommon);
    vernierRight_->SetModel("vernier_cone.obj");
    vernierRight_->SetParent(object_.get());
    vernierRight_->SetTranslate({ 0.55f, -0.05f, -1.85f });
    vernierRight_->SetScale({ 0.45f, 0.45f, 1.2f });
    vernierRight_->SetEnableLighting(false);
    vernierRight_->SetSelectLightings(0);
    vernierRight_->SetColor({ 0.35f, 0.90f, 1.0f, 0.95f });

    vernierLeft_->Update();
    vernierRight_->Update();

    // ロール回避残像（ワイヤーフレーム アフターイメージ）オブジェクトプール初期化
    for (int i = 0; i < MAX_AFTER_IMAGES; ++i) {
        rollAfterImages_[i].object = std::make_unique<Object3d>();
        rollAfterImages_[i].object->Initialize(object3dCommon);
        rollAfterImages_[i].object->SetModel(modelName_);
        rollAfterImages_[i].object->SetColor({ 0.15f, 0.85f, 1.0f, 0.0f });
        rollAfterImages_[i].active = false;
        rollAfterImages_[i].lifeTimer = 0.0f;
    }

    // 共有モデル player.obj 本体のマテリアルカラーを不透明ホワイトに完全リセット
    if (auto pModel = ModelManager::GetInstance()->FindModel(modelName_)) {
        pModel->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }
    object_->SetColor(color_);

    SetPowerUpLevel(0);
    ResetBoost();
}

// プレイヤー被弾時の処理
void Player::OnCollision() {
    if (isGodMode_ || invincibilityTimer_ > 0.0f || isDead_ || isRolling_) return; 
    
    hp_ -= 1000;
    // 被弾しても強化状態（ダブルショット）は維持する
    if (hp_ <= 0) {
        isDead_ = true;
    } else {
        invincibilityTimer_ = 60.0f; 
    }
}

// 地形・壁との衝突およびノックバック（はじかれ）処理
bool Player::OnTerrainCollision(const Vector3& worldNormal, float penetrationDepth, Object3d* parentCamera, const Vector3* hitPoint) {
    if (isDead_) return false;

    // ワールド法線をカメラのローカル空間（X:左右, Y:上下, Z:前後）に変換
    Vector3 localNormal = worldNormal;
    if (parentCamera) {
        const Matrix4x4& cMat = parentCamera->GetmatWorld();
        localNormal.x = worldNormal.x * cMat.m[0][0] + worldNormal.y * cMat.m[0][1] + worldNormal.z * cMat.m[0][2];
        localNormal.y = worldNormal.x * cMat.m[1][0] + worldNormal.y * cMat.m[1][1] + worldNormal.z * cMat.m[1][2];
        localNormal.z = worldNormal.x * cMat.m[2][0] + worldNormal.y * cMat.m[2][1] + worldNormal.z * cMat.m[2][2];
        float len = std::sqrt(localNormal.x * localNormal.x + localNormal.y * localNormal.y + localNormal.z * localNormal.z);
        if (len > 0.0001f) {
            localNormal.x /= len;
            localNormal.y /= len;
            localNormal.z /= len;
        }
    }

    // 衝突点から自機中心へのベクトル（もし渡されていればカメラ空間へ変換）
    Vector3 hitOffsetLocal = { 0.0f, 0.0f, 0.0f };
    bool hasHitOffset = false;
    if (hitPoint && parentCamera) {
        OBB myOBB = GetWorldOBB();
        Vector3 diffWorld = { myOBB.center.x - hitPoint->x, myOBB.center.y - hitPoint->y, myOBB.center.z - hitPoint->z };
        const Matrix4x4& cMat = parentCamera->GetmatWorld();
        hitOffsetLocal.x = diffWorld.x * cMat.m[0][0] + diffWorld.y * cMat.m[0][1] + diffWorld.z * cMat.m[0][2];
        hitOffsetLocal.y = diffWorld.x * cMat.m[1][0] + diffWorld.y * cMat.m[1][1] + diffWorld.z * cMat.m[1][2];
        hitOffsetLocal.z = diffWorld.x * cMat.m[2][0] + diffWorld.y * cMat.m[2][1] + diffWorld.z * cMat.m[2][2];
        hasHitOffset = true;
    }

    // 移動制限枠（画面端）の空きスペース状況を計算
    constexpr float kBorderMargin = 2.5f; // 枠端と判定する安全マージン
    float spaceLeft = logicalPosition_.x - (-playerLimitX_);
    float spaceRight = playerLimitX_ - logicalPosition_.x;
    float spaceDown = logicalPosition_.y - playerLimitYMin_;
    float spaceUp = playerLimitYMax_ - logicalPosition_.y;

    // 弾き出し（押し出し・スライド）方向の決定
    float pushDirX = localNormal.x;
    float pushDirY = localNormal.y;
    float pushScale = 1.0f;

    // 正面衝突判定（壁の法線が手前・カメラ向きを向いており、Z方向への突入である場合）
    bool isHeadOn = (localNormal.z < -0.2f);
    if (isHeadOn) {
        pushScale = 1.6f; // 正面激突時は建物から素早く脱出できるように押し出し力を強化

        // 水平方向（左右）の弾き先決定
        float desiredX = 0.0f;
        if (hasHitOffset && std::abs(hitOffsetLocal.x) > 0.15f) {
            desiredX = (hitOffsetLocal.x > 0.0f) ? 1.0f : -1.0f;
        } else if (std::abs(localNormal.x) > 0.15f) {
            desiredX = (localNormal.x > 0.0f) ? 1.0f : -1.0f;
        } else {
            // 中心に近い場合は、より移動スペースが広く空いている方へ逃げる
            desiredX = (spaceRight >= spaceLeft) ? 1.0f : -1.0f;
        }

        // 【移動範囲外対策】弾き先が枠外になりそうな場合は逆方向へ反転
        if (desiredX > 0.0f && spaceRight < kBorderMargin) {
            desiredX = -1.0f; // 右が壁なら左へ
        } else if (desiredX < 0.0f && spaceLeft < kBorderMargin) {
            desiredX = 1.0f;  // 左が壁なら右へ
        }

        // 垂直方向（上下）の弾き先決定
        float desiredY = 0.0f;
        if (hasHitOffset && std::abs(hitOffsetLocal.y) > 0.15f) {
            desiredY = (hitOffsetLocal.y > 0.0f) ? 1.0f : -1.0f;
        } else if (std::abs(localNormal.y) > 0.15f) {
            desiredY = (localNormal.y > 0.0f) ? 1.0f : -1.0f;
        } else {
            // 基本は地面激突を避けて上空へ弾く
            desiredY = 1.0f;
        }

        // 【移動範囲外対策】上下の枠外チェック
        if (desiredY < 0.0f && spaceDown < kBorderMargin) {
            desiredY = 1.0f; // 下が地面なら上へ反転
        } else if (desiredY > 0.0f && spaceUp < kBorderMargin) {
            desiredY = -1.0f; // 天井なら下へ
        }

        pushDirX = desiredX;
        pushDirY = desiredY;
    } else {
        // 斜め・側面壁の場合の移動範囲外対策
        if (pushDirX > 0.0f && spaceRight < kBorderMargin) {
            pushDirX = 0.0f;
        } else if (pushDirX < 0.0f && spaceLeft < kBorderMargin) {
            pushDirX = 0.0f;
        }
        if (pushDirY < 0.0f && spaceDown < kBorderMargin) {
            pushDirY = 0.0f;
        } else if (pushDirY > 0.0f && spaceUp < kBorderMargin) {
            pushDirY = 0.0f;
        }
    }

    // 押し出し方向ベクトルの正規化
    float dirLen = std::sqrt(pushDirX * pushDirX + pushDirY * pushDirY);
    if (dirLen > 0.0001f) {
        pushDirX /= dirLen;
        pushDirY /= dirLen;
    } else {
        // 逃げ場がない場合は安全な上空（+Y）方向をデフォルトとする
        pushDirX = 0.0f;
        pushDirY = 1.0f;
    }

    // めり込み押し戻しの適用（板や建物の厚みを確実に一発で脱出できるよう最低脱出量を確保）
    constexpr float kMinEscapePush = 1.6f;
    float basePenetration = (std::max)(penetrationDepth + terrainPushMargin_, kMinEscapePush);
    float clampedPenetration = (std::min)(basePenetration, 3.0f);
    float pushAmount = clampedPenetration * pushScale;

    // 自機位置の押し戻し
    logicalPosition_.x += pushDirX * pushAmount;
    logicalPosition_.y += pushDirY * pushAmount;

    // 【最重要】レティクルも自機と全く同じ量だけ外側へ押し出す！
    // （レティクルが建物内に残っていると自機がゴム紐のように引き戻されてしまう現象を完全に遮断）
    reticlePosition_.x += pushDirX * pushAmount;
    reticlePosition_.y += pushDirY * pushAmount;

    // 【最重要】移動制限枠（playerLimitX_, playerLimitYMin_, playerLimitYMax_）内に厳格にクランプ
    logicalPosition_.x = std::clamp(logicalPosition_.x, -playerLimitX_, playerLimitX_);
    logicalPosition_.y = std::clamp(logicalPosition_.y, playerLimitYMin_, playerLimitYMax_);
    reticlePosition_.x = std::clamp(reticlePosition_.x, -playerLimitX_ * 1.05f, playerLimitX_ * 1.05f);
    reticlePosition_.y = std::clamp(reticlePosition_.y, playerLimitYMin_ * 1.05f, playerLimitYMax_ * 1.05f);

    // 障害物方向へのスティック入力を一時遮断（ノックバック中の再突入をブロック）
    terrainBlockTimer_ = 14.0f;
    terrainBlockDir_ = { -pushDirX, -pushDirY };

    // 速度（慣性）の制御: 壁へ向かう速度成分を完全に打ち消す
    float velDot = velocity_.x * pushDirX + velocity_.y * pushDirY;
    if (velDot < 0.0f) {
        velocity_.x -= velDot * pushDirX;
        velocity_.y -= velDot * pushDirY;
    }

    // 衝突中は外側への反発速度（ノックバック）を付与
    velocity_.x += pushDirX * (terrainKnockbackPower_ * 1.5f);
    velocity_.y += pushDirY * (terrainKnockbackPower_ * 1.5f);

    // 枠端に到達している方向の速度はゼロにリセット
    if (logicalPosition_.x <= -playerLimitX_ + 0.1f && velocity_.x < 0.0f) velocity_.x = 0.0f;
    if (logicalPosition_.x >= playerLimitX_ - 0.1f && velocity_.x > 0.0f) velocity_.x = 0.0f;
    if (logicalPosition_.y <= playerLimitYMin_ + 0.1f && velocity_.y < 0.0f) velocity_.y = 0.0f;
    if (logicalPosition_.y >= playerLimitYMax_ - 0.1f && velocity_.y > 0.0f) velocity_.y = 0.0f;

    // ダメージ＆無敵時間
    bool causedDamage = false;
    if (invincibilityTimer_ <= 0.0f) {
        hp_ -= 1000;
        if (hp_ <= 0) {
            isDead_ = true;
        } else {
            invincibilityTimer_ = 45.0f; // 45フレーム無敵
        }
        causedDamage = true;
    }

    // 位置更新を即時3Dオブジェクトに反映
    Update3DObjectOnly();

    return causedDamage;
}

OBB Player::GetWorldOBB() const {
    Object3d* targetObj = colliderObject_ ? colliderObject_.get() : object_.get();
    if (!targetObj) {
        return CollisionMath::CreateOBB(logicalPosition_, colliderSize_, Matrix4x4::Identity());
    }

    const Matrix4x4& wMat = targetObj->GetmatWorld();
    Vector3 pWorldPos = { wMat.m[3][0], wMat.m[3][1], wMat.m[3][2] };

    Matrix4x4 rotMat = wMat;
    for (int i = 0; i < 3; ++i) {
        float len = std::sqrt(rotMat.m[i][0] * rotMat.m[i][0] + rotMat.m[i][1] * rotMat.m[i][1] + rotMat.m[i][2] * rotMat.m[i][2]);
        if (len > 0.0001f) {
            rotMat.m[i][0] /= len;
            rotMat.m[i][1] /= len;
            rotMat.m[i][2] /= len;
        }
    }

    return CollisionMath::CreateOBB(pWorldPos, colliderSize_, rotMat);
}

// プレイヤーの毎フレームの更新処理
void Player::Update(std::list<std::unique_ptr<PlayerBullet>>& bullets, std::list<std::unique_ptr<PlayerMissile>>& missiles, const std::list<std::unique_ptr<Enemy>>& enemies, Object3d* parentCamera, float gameSpeed, ArmoredTrainBoss* boss) {
    if (isCutsceneActive_) {
        prevLogicalPosition_ = logicalPosition_;
        logicalPosition_ = cutscenePos_;
        baseRotation_ = cutsceneRot_;
        velocity_ = { 0.0f, 0.0f };
        Update3DObjectOnly();
        return;
    }

    prevLogicalPosition_ = logicalPosition_;
    Move(gameSpeed);
    UpdateReticleAndAimAssist(enemies, boss, gameSpeed);
    Attack(bullets, missiles, enemies, parentCamera, gameSpeed, boss);

    if (invincibilityTimer_ > 0.0f) {
        invincibilityTimer_ -= gameSpeed;
    }

    object_->Update();
    
    if (accessory_) {
        Vector3 rot = accessory_->GetRotation();
        rot.y += 0.05f * gameSpeed;
        accessory_->SetRotation(rot);
        accessory_->Update();
    }
    
    if (colliderObject_) {
        colliderObject_->SetTranslate(object_->GetTranslate());
        colliderObject_->SetRotation(object_->GetRotation());
        colliderObject_->Update();
    }

    for (int i = 0; i < MAX_MISSILES; ++i) {
        if (mountedMissiles_[i]) mountedMissiles_[i]->Update();
        if (lockOnReticles_[i]) lockOnReticles_[i]->Update();
    }

    UpdateGroundMarker();
    UpdateVerniers(gameSpeed);
    UpdateRollAfterImages(gameSpeed, parentCamera);
}

void Player::Draw() {
    if (!isVisible_) return;

    // カットシーン演出中は照準UIを非表示にして映画的没入感を高める
    if (!isCutsceneActive_) {
        // 奥照準・手前照準ともに、GamePlayScene::DrawUI（最前面2D UIオーバーレイ）で
        // スターフォックス風のデュアル・スクエア・レティクルとして描画（地形潜り込み・Z遮蔽・解像度低下を完全解消）
        // reticle_->Draw();
        // if (frontReticle_) {
        //     frontReticle_->Draw();
        // }
    }

    // 接地マーカー（プロジェクションシャドウ）描画（自機生存時かつ自機が表示されている時）
    if (!isDead_ && groundMarkerObject_) {
        float playerAlpha = object_ ? object_->GetColor().w : 1.0f;
        if (playerAlpha > 0.05f) {
            groundMarkerObject_->Draw();
        }
    }
    
    bool shouldDrawPlayer = true;
    if (invincibilityTimer_ > 0.0f) {
        if (std::fmod(invincibilityTimer_ / 4.0f, 2.0f) < 1.0f) {
            shouldDrawPlayer = false;
        }
    }
    
    if (shouldDrawPlayer) {
        if (object_) {
            object_->Draw();
        }
        if (accessory_) {
            accessory_->Draw();
        }
        for (int i = 0; i < maxMissiles_; ++i) {
            if (mountedMissiles_[i]) {
                if (currentWeapon_ == WeaponType::MISSILE && missileReloadTimer_ <= 0.0f) {
                    mountedMissiles_[i]->Draw();
                }
            }
        }
    }
    
    // ロール回避残像（ワイヤーフレーム アフターイメージ）描画
    if (!isDead_ && object3dCommon_) {
        bool hasActiveAfterImage = false;
        for (int i = 0; i < MAX_AFTER_IMAGES; ++i) {
            if (rollAfterImages_[i].active && rollAfterImages_[i].object) {
                hasActiveAfterImage = true;
                break;
            }
        }
        if (hasActiveAfterImage) {
            object3dCommon_->SetWireframeDrawSetting();
            for (int i = 0; i < MAX_AFTER_IMAGES; ++i) {
                if (rollAfterImages_[i].active && rollAfterImages_[i].object) {
                    rollAfterImages_[i].object->Draw();
                }
            }
            object3dCommon_->SetCommonDrawSetting();
        }
    }

    for (int i = 0; i < MAX_MISSILES; ++i) {
        if (lockOnReticles_[i]) lockOnReticles_[i]->Draw();
    }
}

// -------------------------------------------------------------
// 接地マーカー（プロジェクションシャドウ）更新処理
// -------------------------------------------------------------
void Player::UpdateGroundMarker() {
    if (!groundMarkerObject_) return;

    // 自機の地上高度を算出（playerLimitYMin_ を地面基準面とする）
    float groundLevelY = playerLimitYMin_ - 0.45f;
    float currentY = logicalPosition_.y;
    float altitude = (std::max)(0.0f, currentY - groundLevelY);
    float maxAltitude = (std::max)(1.0f, playerLimitYMax_ - groundLevelY);
    float altitudeRatio = (std::clamp)(altitude / maxAltitude, 0.0f, 1.0f);

    // 高度に応じた動的スケール：
    // 地面近くではコンパクトに締まり (2.2m)、上昇するほど淡く広がる (3.8m)
    float markerScale = 2.2f + 1.6f * altitudeRatio;
    groundMarkerObject_->SetScale({ markerScale, 1.0f, markerScale });

    // 投影位置：Xは自機の直下、Yは地面直上、Zは自機と同等（わずかに前方に配置して視認性向上）
    float markerZ = logicalPosition_.z + 0.5f;
    groundMarkerObject_->SetTranslate({ logicalPosition_.x, groundLevelY, markerZ });

    // 自機のバンク角（左右ロール傾き）に連動してマーカーも回転し姿勢を投影
    groundMarkerObject_->SetRotation({ 0.0f, 0.0f, currentBank_ * 0.45f });

    // 高度に応じた透明度（アルファ減衰）：
    // 地面近くでは鮮烈に発光（0.88f）、上空では淡く溶け込む（0.22f）
    float alpha = 0.88f - 0.66f * altitudeRatio;
    groundMarkerObject_->SetColor({ 0.0f, 0.92f, 1.0f, alpha });

    groundMarkerObject_->Update();
}

// -------------------------------------------------------------
// ツインバーニア（推進噴射炎コーン）更新処理
// -------------------------------------------------------------
void Player::UpdateVerniers(float gameSpeed) {
    if (!vernierLeft_ || !vernierRight_) return;

    static float vernierPulsePhase = 0.0f;
    vernierPulsePhase += 0.35f * gameSpeed;
    float pulse = 1.0f + 0.08f * std::sinf(vernierPulsePhase);

    // ブースト中判定
    bool boostActive = isBoosting_ || isCutsceneActive_;

    // コーンの基本スケールとカラー
    float baseScaleZ = boostActive ? 2.4f : 1.25f;
    float scaleX = boostActive ? 0.55f : 0.42f;
    Vector3 vernierScale = { scaleX * pulse, scaleX * pulse, baseScaleZ * pulse };

    Vector4 vernierColor;
    if (boostActive) {
        // ブースト時：超高温ネオンオレンジ〜マゼンタ
        vernierColor = { 1.0f, 0.38f, 0.08f, 1.0f };
    } else {
        // 通常時：鮮烈なネオンシアン＋白ハイライト
        vernierColor = { 0.30f, 0.90f, 1.0f, 0.92f };
    }

    vernierLeft_->SetScale(vernierScale);
    vernierRight_->SetScale(vernierScale);

    vernierLeft_->SetColor(vernierColor);
    vernierRight_->SetColor(vernierColor);

    vernierLeft_->Update();
    vernierRight_->Update();
}

// -------------------------------------------------------------
// ロール回避残像（アフターイメージ）更新処理
// -------------------------------------------------------------
void Player::UpdateRollAfterImages(float gameSpeed, Object3d* parentCamera) {
    // ロール中の残像スポーン処理
    if (isRolling_ && rollTimer_ > 0.0f) {
        rollAfterImageSpawnTimer_ -= gameSpeed;
        if (rollAfterImageSpawnTimer_ <= 0.0f) {
            rollAfterImageSpawnTimer_ = 2.5f; // 約2〜3フレームごとに1体スポーン

            // 空きスロットまたは最も古いスロットを探す
            int targetIdx = -1;
            float maxAge = -1.0f;
            for (int i = 0; i < MAX_AFTER_IMAGES; ++i) {
                if (!rollAfterImages_[i].active) {
                    targetIdx = i;
                    break;
                }
                if (rollAfterImages_[i].lifeTimer > maxAge) {
                    maxAge = rollAfterImages_[i].lifeTimer;
                    targetIdx = i;
                }
            }

            if (targetIdx >= 0) {
                auto& img = rollAfterImages_[targetIdx];
                img.active = true;
                img.lifeTimer = 0.0f;
                img.maxLife = 12.0f; // 約0.2秒（12フレーム）

                if (img.object) {
                    if (parentCamera) {
                        img.object->SetParent(parentCamera);
                    }
                    // 現在の自機の姿勢（Translate, Rotation, Scale）をスナップショット記録
                    img.object->SetTranslate(object_->GetTranslate());
                    img.object->SetRotation(object_->GetRotation());
                    img.object->SetScale(object_->GetScale());
                }
            }
        }
    } else {
        rollAfterImageSpawnTimer_ = 0.0f;
    }

    // 既存残像の寿命進行とフェードアウト
    for (int i = 0; i < MAX_AFTER_IMAGES; ++i) {
        if (!rollAfterImages_[i].active) continue;

        rollAfterImages_[i].lifeTimer += gameSpeed;
        if (rollAfterImages_[i].lifeTimer >= rollAfterImages_[i].maxLife) {
            rollAfterImages_[i].active = false;
            continue;
        }

        // 残り寿命に応じたアルファフェードアウト (0.65f -> 0.0f)
        float lifeRatio = rollAfterImages_[i].lifeTimer / rollAfterImages_[i].maxLife;
        float alpha = 0.65f * (1.0f - lifeRatio);

        if (rollAfterImages_[i].object) {
            // クールな半透明ネオンシアン（加算合成的に残像が浮かぶ）
            rollAfterImages_[i].object->SetColor({ 0.15f, 0.85f, 1.0f, alpha });
            rollAfterImages_[i].object->Update();
        }
    }
}

void Player::DrawCollider() {
    if (colliderObject_) {
        colliderObject_->Draw();
    }
}

void Player::SetReticleColor(const Vector4& color) {
    reticleColor_ = color;
    if (reticle_) {
        reticle_->SetColor(reticleColor_);
    }
    if (frontReticle_) {
        frontReticle_->SetColor({ 1.0f, 0.25f, 0.25f, 0.95f });
    }
}

Vector3 Player::GetReticleWorldPosition() const {
    if (!reticle_) return {0,0,0};
    const Matrix4x4& mat = reticle_->GetmatWorld();
    return { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
}

Vector3 Player::GetFrontReticleWorldPosition() const {
    if (!object_) return {0,0,0};
    const Matrix4x4& mat = object_->GetmatWorld();
    // 自機の前方方向ベクトル（Z軸）
    Vector3 forward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
    float len = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (len > 0.0001f) {
        forward.x /= len;
        forward.y /= len;
        forward.z /= len;
    }
    // 仕様書: 手前照準の3D座標 = 自機の座標 + (自機の正面方向ベクトル × 手前距離 D1)
    // 自機機首前方14mに配置（自機移動・ピッチ・ロール・首振りに完全同期）
    float frontDist = 14.0f;
    return Vector3{
        mat.m[3][0] + forward.x * frontDist,
        mat.m[3][1] + forward.y * frontDist,
        mat.m[3][2] + forward.z * frontDist
    };
}

// -------------------------------------------------------------
// 照準のZ深度適応・エイムアシスト・ヒット確信フィードバック処理
// -------------------------------------------------------------
void Player::UpdateReticleAndAimAssist(const std::list<std::unique_ptr<Enemy>>& enemies, ArmoredTrainBoss* boss, float gameSpeed) {
    if (!reticle_) return;

    Vector3 playerWorldPos = GetWorldPosition();
    Vector3 reticleWorldPos = GetReticleWorldPosition();

    Vector3 aimDir = {
        reticleWorldPos.x - playerWorldPos.x,
        reticleWorldPos.y - playerWorldPos.y,
        reticleWorldPos.z - playerWorldPos.z
    };
    float aimLen = std::sqrtf(aimDir.x * aimDir.x + aimDir.y * aimDir.y + aimDir.z * aimDir.z);
    if (aimLen > 0.0001f) {
        aimDir.x /= aimLen;
        aimDir.y /= aimLen;
        aimDir.z /= aimLen;
    } else {
        aimDir = { 0.0f, 0.0f, 1.0f };
    }

    Enemy* bestEnemy = nullptr;
    ArmoredTrainBoss* bestBoss = nullptr;
    int bestCarriage = -1;
    Vector3 bestTargetPos = { 0.0f, 0.0f, 0.0f };
    float bestScore = 999999.0f;
    float bestProjDist = dynamicReticleDistance_;

    // 1. 通常敵のスキャン
    for (const auto& enemy : enemies) {
        if (!enemy || enemy->IsDead()) continue;

        Vector3 ePos = enemy->GetColliderCenter();
        Vector3 toE = { ePos.x - playerWorldPos.x, ePos.y - playerWorldPos.y, ePos.z - playerWorldPos.z };
        float projDist = toE.x * aimDir.x + toE.y * aimDir.y + toE.z * aimDir.z;

        // 自機の前方の実効交戦距離（10m 〜 130m）内の敵のみスキャン
        if (projDist < 10.0f || projDist > 130.0f) continue;

        Vector3 closestPt = {
            playerWorldPos.x + aimDir.x * projDist,
            playerWorldPos.y + aimDir.y * projDist,
            playerWorldPos.z + aimDir.z * projDist
        };

        Vector3 diff = { ePos.x - closestPt.x, ePos.y - closestPt.y, ePos.z - closestPt.z };
        float offsetDist = std::sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        // 照準枠の許容半径（プレイヤーが自力で狙って撃つ感覚を重視したタイトな判定: 50mで約2.9m）
        float thresholdRadius = 1.6f + projDist * 0.026f;

        if (offsetDist < thresholdRadius) {
            float score = offsetDist / thresholdRadius;
            if (score < bestScore) {
                bestScore = score;
                bestEnemy = enemy.get();
                bestBoss = nullptr;
                bestCarriage = -1;
                bestTargetPos = ePos;
                bestProjDist = projDist;
            }
        }
    }

    // 2. ボス（ArmoredTrainBoss）のスキャン
    if (boss && !boss->IsDefeated()) {
        int carriageCount = static_cast<int>(boss->GetCarriages().size());
        for (int cIdx = 0; cIdx < carriageCount; ++cIdx) {
            if (boss->IsCarriageDestroyed(cIdx)) continue;

            Vector3 bPos = boss->GetCarriagePosition(cIdx);
            Vector3 toB = { bPos.x - playerWorldPos.x, bPos.y - playerWorldPos.y, bPos.z - playerWorldPos.z };
            float projDist = toB.x * aimDir.x + toB.y * aimDir.y + toB.z * aimDir.z;

            // ボス実効射程（10m 〜 160m）
            if (projDist < 10.0f || projDist > 160.0f) continue;

            Vector3 closestPt = {
                playerWorldPos.x + aimDir.x * projDist,
                playerWorldPos.y + aimDir.y * projDist,
                playerWorldPos.z + aimDir.z * projDist
            };

            Vector3 diff = { bPos.x - closestPt.x, bPos.y - closestPt.y, bPos.z - closestPt.z };
            float offsetDist = std::sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
            // ボス吸い付き許容半径も適切に引き締め
            float thresholdRadius = 3.2f + projDist * 0.035f;

            if (offsetDist < thresholdRadius) {
                float score = offsetDist / thresholdRadius;
                if (score < bestScore) {
                    bestScore = score;
                    bestEnemy = nullptr;
                    bestBoss = boss;
                    bestCarriage = cIdx;
                    bestTargetPos = bPos;
                    bestProjDist = projDist;
                }
            }
        }
    }

    // 判定結果の更新
    if (bestEnemy || bestBoss) {
        isTargetInReticle_ = true;
        reticleHoverEnemy_ = bestEnemy;
        reticleHoverBoss_ = bestBoss;
        reticleHoverCarriage_ = bestCarriage;
        reticleHoverTargetPos_ = bestTargetPos;

        // 敵のメッシュ厚み・半径（通常敵: 約5〜6m、ボス車体: 約14m）を差し引き、
        // 必ず敵のモデル前面（手前側）に照準枠を配置。敵の立体モデルに埋もれてオクルード（消滅）するのを完全に防止！
        float frontMargin = (bestBoss != nullptr) ? 14.0f : 6.0f;
        float targetDist = std::clamp(bestProjDist - frontMargin, 20.0f, 220.0f);
        dynamicReticleDistance_ += (targetDist - dynamicReticleDistance_) * 0.35f * gameSpeed;

        // 初回捕捉時：スナップ収束アニメーション開始
        if (!prevTargetInReticle_) {
            reticleSnapAnimTimer_ = 1.0f;
        }
    } else {
        isTargetInReticle_ = false;
        reticleHoverEnemy_ = nullptr;
        reticleHoverBoss_ = nullptr;
        reticleHoverCarriage_ = -1;

        // 敵がいない時は標準距離 45.0f へスムーズに復帰
        dynamicReticleDistance_ += (45.0f - dynamicReticleDistance_) * 0.12f * gameSpeed;
    }

    // スナップ収束アニメーションタイマー更新
    if (reticleSnapAnimTimer_ > 0.0f) {
        reticleSnapAnimTimer_ -= 0.12f * gameSpeed;
        if (reticleSnapAnimTimer_ < 0.0f) reticleSnapAnimTimer_ = 0.0f;
    }

    // 距離に応じた視覚スケール補正（遠くの敵でもレティクルが極小にならず敵を囲めるよう調整）
    float distScaleFactor = std::clamp(dynamicReticleDistance_ / 32.0f, 1.0f, 2.8f);

    if (isTargetInReticle_) {
        // ヒット確信フィードバック：どんな背景・敵色の上でも際立つ超高輝度ネオンエレクトリックゴールド（黄色）
        SetReticleColor({ 1.0f, 0.95f, 0.15f, 1.0f });

        // スナップ収束演出：捉えた瞬間にキュッと収束（0.88倍）してから、敵を囲む迫力あるサイズに弾む
        float snapScale = 1.25f;
        if (reticleSnapAnimTimer_ > 0.0f) {
            snapScale = 0.88f + 0.37f * (1.0f - reticleSnapAnimTimer_);
        } else {
            // ロックオン中の微細なパルス（脈動）
            static float pulsePhase = 0.0f;
            pulsePhase += 0.15f * gameSpeed;
            snapScale = 1.25f + 0.06f * std::sinf(pulsePhase);
        }

        Vector3 finalScale = {
            baseReticleScale_.x * distScaleFactor * snapScale,
            baseReticleScale_.y * distScaleFactor * snapScale,
            baseReticleScale_.z * distScaleFactor * snapScale
        };
        reticle_->SetScale(finalScale);
    } else {
        // 通常状態：地形（シアン・白系）に対して最大の補色コントラストを発揮するエレクトリックオレンジ
        SetReticleColor({ 1.0f, 0.58f, 0.08f, 0.95f });
        Vector3 finalScale = {
            baseReticleScale_.x * distScaleFactor,
            baseReticleScale_.y * distScaleFactor,
            baseReticleScale_.z * distScaleFactor
        };
        reticle_->SetScale(finalScale);
    }

    prevTargetInReticle_ = isTargetInReticle_;
}

// プレイヤーの移動とカメラ追従処理
void Player::Move(float gameSpeed) {
    if (!input_) return;

    
    // 回避（バレルロール）判定：Qキーで左回避、Eキーで右回避（単押しで即座に発動）
    if (!isRolling_) {
        if (input_->TriggerKey(DIK_Q)) {
            isRolling_ = true;
            rollTimer_ = 0.0f;
            rollDirection_ = -1.0f; 
            isDodgeTriggered_ = true;
        } else if (input_->TriggerKey(DIK_E)) {
            isRolling_ = true;
            rollTimer_ = 0.0f;
            rollDirection_ = 1.0f; 
            isDodgeTriggered_ = true;
        }
    }

    if (isRolling_) {
        rollTimer_ += gameSpeed;
        if (rollTimer_ >= rollMaxTime_) {
            isRolling_ = false;
        }
    }

    // ブーストキー判定
    bool boostKeyPressed = input_->PushKey(DIK_LSHIFT);
    
    // オーバーヒートからの復帰判定（25%まで回復したら再使用可能）
    if (isBoostOverheated_) {
        if (boostEnergy_ >= maxBoostEnergy_ * 0.25f) {
            isBoostOverheated_ = false;
        }
    }

    if (boostKeyPressed && !isBoostOverheated_ && boostEnergy_ > 0.0f) {
        isBoosting_ = true;
        boostEnergy_ -= boostConsumeRate_ * gameSpeed;
        boostRecoverDelayTimer_ = kBoostRecoverDelay_;
        if (boostEnergy_ <= 0.0f) {
            boostEnergy_ = 0.0f;
            isBoostOverheated_ = true;
            isBoosting_ = false; // 枯渇した瞬間にブースト強制終了
        }
    } else {
        isBoosting_ = false;
        // ブースト非使用時のエネルギー回復処理（少しディレイを置いてから自然回復）
        if (boostRecoverDelayTimer_ > 0.0f) {
            boostRecoverDelayTimer_ -= gameSpeed;
        } else {
            boostEnergy_ = std::min(maxBoostEnergy_, boostEnergy_ + boostRecoverRate_ * gameSpeed);
        }
    }

    bool isBraking = input_->PushKey(DIK_LCONTROL);
    
    float targetZ = 0.0f; 
    if (isBoosting_) {
        targetZ = 3.0f;  // ブーストで少し前へ
    } else if (isBraking) {
        targetZ = -2.0f; // ブレーキで少し後ろへ
    }
    logicalPosition_.z += (targetZ - logicalPosition_.z) * 0.1f * gameSpeed;

    
    float currentSpeed = speed_;
    if (isBoosting_) currentSpeed *= 1.4f;

    // スティック / キー入力の検出
    float inputX = 0.0f;
    float inputY = 0.0f;
    if (input_->PushKey(DIK_A) || input_->PushKey(DIK_LEFT))  inputX -= 1.0f;
    if (input_->PushKey(DIK_D) || input_->PushKey(DIK_RIGHT)) inputX += 1.0f;
    if (input_->PushKey(DIK_W) || input_->PushKey(DIK_UP))    inputY += 1.0f;
    if (input_->PushKey(DIK_S) || input_->PushKey(DIK_DOWN))  inputY -= 1.0f;

    // 回避・ロールキー押下時の横ブースト
    float currentSpeedX = currentSpeed;
    if (input_->PushKey(DIK_Q) || input_->PushKey(DIK_E)) {
        currentSpeedX *= 1.4f;
    }


    // 移動制限（バウンディング枠）の目標値へ滑らかに補間
    float limitLerpSpeed = 0.05f * gameSpeed;
    playerLimitX_ += (targetLimitX_ - playerLimitX_) * limitLerpSpeed;
    playerLimitYMin_ += (targetLimitYMin_ - playerLimitYMin_) * limitLerpSpeed;
    playerLimitYMax_ += (targetLimitYMax_ - playerLimitYMax_) * limitLerpSpeed;

    // -------------------------------------------------------------
    // 1. レティクルの先行移動（プレイヤー入力に応じたキビキビとした照準移動）
    // -------------------------------------------------------------
    float reticleSpeedX = currentSpeedX * 1.55f;
    float reticleSpeedY = currentSpeed * 1.55f;
    float targetVelX = inputX * reticleSpeedX;
    float targetVelY = inputY * reticleSpeedY;

    // 障害物・壁方向への入力遮断（ノックバック中の再突入防止）
    if (terrainBlockTimer_ > 0.0f) {
        terrainBlockTimer_ -= gameSpeed;
        float inputDot = targetVelX * terrainBlockDir_.x + targetVelY * terrainBlockDir_.y;
        if (inputDot > 0.0f) {
            targetVelX -= terrainBlockDir_.x * inputDot;
            targetVelY -= terrainBlockDir_.y * inputDot;
        }
    }

    // レティクル速度の加減速（滑らかな慣性）
    float reticleAccel = 0.28f * gameSpeed;
    velocity_.x += (targetVelX - velocity_.x) * reticleAccel;
    velocity_.y += (targetVelY - velocity_.y) * reticleAccel;

    reticlePosition_.x += velocity_.x * gameSpeed;
    reticlePosition_.y += velocity_.y * gameSpeed;
    reticlePosition_.x = std::clamp(reticlePosition_.x, -playerLimitX_ * 1.05f, playerLimitX_ * 1.05f);
    reticlePosition_.y = std::clamp(reticlePosition_.y, playerLimitYMin_ * 1.05f, playerLimitYMax_ * 1.05f);
    reticlePosition_.z = logicalPosition_.z + dynamicReticleDistance_;

    // -------------------------------------------------------------
    // 2. 自機の遅延追従（レティクルを追いかけるスターフォックス特有の浮遊感・操作感）
    // -------------------------------------------------------------
    float shipFollowLerp = 0.13f * gameSpeed;
    logicalPosition_.x += (reticlePosition_.x - logicalPosition_.x) * shipFollowLerp;
    logicalPosition_.y += (reticlePosition_.y - logicalPosition_.y) * shipFollowLerp;
    logicalPosition_.x = std::clamp(logicalPosition_.x, -playerLimitX_, playerLimitX_);
    logicalPosition_.y = std::clamp(logicalPosition_.y, playerLimitYMin_, playerLimitYMax_);

    // -------------------------------------------------------------
    // 3. 機体姿勢制御（ロールとヨーの分離、スターフォックス仕様）
    // 1. 横移動時のバンク（Roll/Z軸回転）：スティック入力に応じて最大50度(約0.87rad)傾ける
    // 2. 首振り（Yaw/Y軸回転）：自機からレティクルへの方向を向く
    // 3. ピッチ（Pitch/X軸回転）：上下移動時に機首を向ける
    // -------------------------------------------------------------
    const float maxBankAngle = 0.87f;  // 約50度
    const float maxYawAngle = 0.22f;   // 約12.6度
    const float maxPitchAngle = 0.35f; // 約20度

    float targetBank = 0.0f;
    if (inputX < -0.01f) {
        targetBank = maxBankAngle;  // 左移動で左傾斜
    } else if (inputX > 0.01f) {
        targetBank = -maxBankAngle; // 右移動で右傾斜
    }

    // レティクル方向を向く機首旋回（レティクルと自機のオフセットに応じたYaw/Pitch）
    float targetYaw = std::clamp((reticlePosition_.x - logicalPosition_.x) * 0.08f, -maxYawAngle, maxYawAngle);
    float targetPitch = 0.0f;
    if (inputY > 0.01f) {
        if (logicalPosition_.y < playerLimitYMax_) targetPitch = -maxPitchAngle; // 機首上げ
    } else if (inputY < -0.01f) {
        if (logicalPosition_.y > playerLimitYMin_) targetPitch = maxPitchAngle;  // 機首下げ
    }

    // 滑らかな姿勢追従（揺れの少ない滑らかな復帰）
    float bankSpeed = 0.16f * gameSpeed;
    currentBank_ += (targetBank - currentBank_) * bankSpeed;

    float yawSpeed = 0.14f * gameSpeed;
    currentYaw_ += (targetYaw - currentYaw_) * yawSpeed;

    float pitchSpeed = 0.15f * gameSpeed;
    currentPitch_ += (targetPitch - currentPitch_) * pitchSpeed;

    // バレルロール（回避）アクション
    float finalBank = currentBank_;
    if (isRolling_) {
        float rollAngle = (static_cast<float>(rollTimer_) / rollMaxTime_) * 3.14159265f * 2.0f;
        finalBank += rollAngle * -rollDirection_;
    }
    finalBank_ = finalBank;

    // -------------------------------------------------------------
    // レティクル自体のロール回転を弱める（ほぼ水平を保つ）
    // -------------------------------------------------------------
    float reticleRoll = finalBank * 0.08f; // 最大傾斜50度に対して、レティクルは4度程度に抑制
    reticle_->SetTranslate(reticlePosition_);
    reticle_->SetRotation(Vector3(0.0f, 0.0f, reticleRoll));
    reticle_->Update();

    // スクリーン基準の回転を機体のローカル座標へマッピング
    float visualPitch = currentPitch_ * std::cos(finalBank) + currentYaw_ * std::sin(finalBank);
    float visualYaw   = -currentPitch_ * std::sin(finalBank) + currentYaw_ * std::cos(finalBank);

    object_->SetScale(playerScale_);
    object_->SetRotation(Vector3(
        baseRotation_.x + visualPitch + modelRotOffset_.x,
        baseRotation_.y + visualYaw   + modelRotOffset_.y,
        baseRotation_.z + finalBank   + modelRotOffset_.z
    ));
    object_->SetTranslate(Vector3(
        logicalPosition_.x + modelPosOffset_.x, 
        logicalPosition_.y + modelPosOffset_.y, 
        logicalPosition_.z + modelPosOffset_.z
    ));
    object_->Update();

    // -------------------------------------------------------------
    // 手前照準（赤丸 / frontReticle_）: 3D自機追従ビルボード
    // 仕様書: PosNear = PosShip + (DirForward * DistNear)
    //         LookAt(PosCamera); // ビルボード
    // 親は自機 object_ のため自機の移動・首振りに完全同期（遅延ゼロ）。
    // 自機のロール角を打ち消すことでプレイヤー画面に対して常に正対真円を維持！
    // -------------------------------------------------------------
    if (frontReticle_) {
        frontReticle_->SetTranslate(Vector3(0.0f, 0.0f, 8.5f));
        frontReticle_->SetRotation(Vector3(0.0f, 0.0f, -finalBank));
        frontReticle_->Update();
    }
}

// プレイヤーの攻撃（弾・ミサイル発射）処理
void Player::Attack(std::list<std::unique_ptr<PlayerBullet>>& bullets, std::list<std::unique_ptr<PlayerMissile>>& missiles, const std::list<std::unique_ptr<Enemy>>& enemies, Object3d* parentCamera, float gameSpeed, ArmoredTrainBoss* boss) {
    if (!input_) return;

    
    if (input_->TriggerKey(DIK_R)) {
        currentWeapon_ = (currentWeapon_ == WeaponType::NORMAL) ? WeaponType::MISSILE : WeaponType::NORMAL;
    }

    
    if (missileReloadTimer_ > 0.0f) {
        missileReloadTimer_ -= gameSpeed;
    }

    if (attackTimer_ > 0.0f) {
        attackTimer_ -= gameSpeed;
    }

    
    if (currentWeapon_ == WeaponType::MISSILE) {
        lockOnAnimTimer_ += 0.1f * gameSpeed; 
        if (lockOnDelayTimer_ > 0.0f) {
            lockOnDelayTimer_ -= gameSpeed;
        }

        if (input_->PushKey(DIK_SPACE) && missileReloadTimer_ <= 0.0f) {
            const Matrix4x4& mat = object_->GetmatWorld();
            Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
            
            // 1. 通常敵のロックオン探索
            for (const auto& enemy : enemies) {
                if (enemy->IsDead()) continue;
                
                auto it = std::find_if(multiLockedEnemies_.begin(), multiLockedEnemies_.end(),
                    [&](const LockOnTarget& target) { return target.enemy == enemy.get(); });
                if (it != multiLockedEnemies_.end()) {
                    continue;
                }

                if (multiLockedEnemies_.size() >= static_cast<size_t>(maxMissiles_)) {
                    break;
                }

                Vector3 ePos = enemy->GetColliderCenter();
                float distSq = (ePos.x - playerPos.x) * (ePos.x - playerPos.x) + 
                               (ePos.y - playerPos.y) * (ePos.y - playerPos.y) + 
                               (ePos.z - playerPos.z) * (ePos.z - playerPos.z);
                
                if (distSq < (lockOnMaxDistance_ * lockOnMaxDistance_) && ePos.z > playerPos.z) { 
                    if (lockOnDelayTimer_ <= 0.0f) {
                        multiLockedEnemies_.push_back({ enemy.get(), nullptr, -1, 0.0f });
                        lockOnDelayTimer_ = lockOnInterval_; 
                        break; 
                    }
                }
            }

            // 2. 装甲列車ボスの部位（各車両）のロックオン探索
            if (boss && boss->IsActive() && !boss->IsDefeated()) {
                const auto& carriages = boss->GetCarriages();
                for (int i = 0; i < static_cast<int>(carriages.size()); ++i) {
                    if (boss->IsCarriageDestroyed(i)) continue;

                    // 既にこの車両部位をロックオン済みかチェック
                    auto it = std::find_if(multiLockedEnemies_.begin(), multiLockedEnemies_.end(),
                        [&](const LockOnTarget& target) { return target.boss == boss && target.carriageIndex == i; });
                    if (it != multiLockedEnemies_.end()) {
                        continue;
                    }

                    if (multiLockedEnemies_.size() >= static_cast<size_t>(maxMissiles_)) {
                        break;
                    }

                    Vector3 carPos = boss->GetCarriagePosition(i);
                    float distSq = (carPos.x - playerPos.x) * (carPos.x - playerPos.x) + 
                                   (carPos.y - playerPos.y) * (carPos.y - playerPos.y) + 
                                   (carPos.z - playerPos.z) * (carPos.z - playerPos.z);

                    // 射程内、かつ自機の前方にある車両をロック
                    if (distSq < (lockOnMaxDistance_ * lockOnMaxDistance_) && carPos.z > (playerPos.z - 20.0f)) {
                        if (lockOnDelayTimer_ <= 0.0f) {
                            multiLockedEnemies_.push_back({ nullptr, boss, i, 0.0f });
                            lockOnDelayTimer_ = lockOnInterval_;
                            break;
                        }
                    }
                }
            }
        }

        
        for (int i = 0; i < MAX_MISSILES; ++i) {
            bool isValidTarget = false;
            Vector3 targetPos = { 0, 0, 0 };
            if (i < static_cast<int>(multiLockedEnemies_.size())) {
                const auto& t = multiLockedEnemies_[i];
                if (t.boss) {
                    if (!t.boss->IsDefeated() && !t.boss->IsCarriageDestroyed(t.carriageIndex)) {
                        isValidTarget = true;
                        targetPos = t.boss->GetCarriagePosition(t.carriageIndex);
                    }
                } else if (t.enemy) {
                    if (!t.enemy->IsDead()) {
                        isValidTarget = true;
                        targetPos = t.enemy->GetColliderCenter();
                    }
                }
            }

            if (isValidTarget) {
                multiLockedEnemies_[i].lockedTime += gameSpeed;
                float lockTime = multiLockedEnemies_[i].lockedTime;
                bool isLockCompleted = lockTime >= lockOnCompleteTime_; 

                const Matrix4x4& mat = object_->GetmatWorld();
                Vector3 pPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
                float dz = targetPos.z - pPos.z;
                float baseScale = 2.0f + (std::max)(0.0f, dz) * 0.02f;
                float scale = baseScale + (baseScale * 0.15f) * std::sinf(lockOnAnimTimer_ * 2.0f);
                lockOnReticles_[i]->SetScale({scale, scale, scale});
                lockOnReticles_[i]->SetTranslate(targetPos);
                
                if (!isLockCompleted) {
                    float duration = (std::max)(1.0f, lockOnCompleteTime_);
                    float rotationAngle = (lockTime / duration) * 6.2831853f; 
                    lockOnReticles_[i]->SetRotation({0.0f, 0.0f, rotationAngle});
                } else {
                    lockOnReticles_[i]->SetRotation({0.0f, 0.0f, 0.0f}); 
                }
            } else {
                lockOnReticles_[i]->SetScale({0.001f, 0.001f, 0.001f});
            }
        }

        
        if (!input_->PushKey(DIK_SPACE) && !multiLockedEnemies_.empty() && missileReloadTimer_ <= 0.0f) {
            bool hasLaunched = false;
            for (size_t i = 0; i < multiLockedEnemies_.size(); ++i) {
                const auto& t = multiLockedEnemies_[i];
                bool isDead = false;
                if (t.boss) {
                    isDead = t.boss->IsDefeated() || t.boss->IsCarriageDestroyed(t.carriageIndex);
                } else if (t.enemy) {
                    isDead = t.enemy->IsDead();
                } else {
                    isDead = true;
                }
                if (isDead) continue;
                
                const Matrix4x4& mountedMat = mountedMissiles_[i]->GetmatWorld();
                Vector3 spawnPos = { mountedMat.m[3][0], mountedMat.m[3][1], mountedMat.m[3][2] };
                Vector3 spawnRot = object_->GetRotation(); 
                
                auto newMissile = std::make_unique<PlayerMissile>();
                newMissile->Initialize(object3dCommon_, spawnPos, spawnRot, {0,0,0}, parentCamera, t.enemy, t.boss, t.carriageIndex);
                missiles.push_back(std::move(newMissile));
                hasLaunched = true;

                lockOnReticles_[i]->SetScale({0.001f, 0.001f, 0.001f});
            }

            if (hasLaunched) {
                SoundManager::GetInstance()->PlaySE("launcher.mp3", 0.9f);
            }
            
            multiLockedEnemies_.clear();
            missileReloadTimer_ = missileReloadTime_;
            attackTimer_ = attackInterval_;
        }
        
        multiLockedEnemies_.erase(
            std::remove_if(multiLockedEnemies_.begin(), multiLockedEnemies_.end(),
                [](const LockOnTarget& t) {
                    if (t.boss) {
                        return t.boss->IsDefeated() || t.boss->IsCarriageDestroyed(t.carriageIndex);
                    }
                    return t.enemy ? t.enemy->IsDead() : true;
                }),
            multiLockedEnemies_.end()
        );
    } 
    
    else if (attackTimer_ <= 0.0f && (input_->PushKey(DIK_SPACE))) {
        attackTimer_ = attackInterval_; 
        
        const Matrix4x4& mat = object_->GetmatWorld();
        Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
        
        // 自機正面ベクトル
        Vector3 forwardVec = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
        float fLen = std::sqrt(forwardVec.x * forwardVec.x + forwardVec.y * forwardVec.y + forwardVec.z * forwardVec.z);
        if (fLen > 0.0001f) { forwardVec.x /= fLen; forwardVec.y /= fLen; forwardVec.z /= fLen; }

        Vector3 targetPos = GetReticleWorldPosition();
        
        // エイムアシスト（プレイヤー自力でのエイミング感を最優先するため、ごく微弱な25%の引き寄せのみ）
        if (isTargetInReticle_) {
            targetPos.x += (reticleHoverTargetPos_.x - targetPos.x) * 0.25f;
            targetPos.y += (reticleHoverTargetPos_.y - targetPos.y) * 0.25f;
            targetPos.z += (reticleHoverTargetPos_.z - targetPos.z) * 0.25f;
        }

        if (isDoubleShot_) {
            // 2連装: 左右の銃口から目標座標（奥照準）へ向けて正確に集束射出（コンバージェンス射撃）
            Vector3 rightDir = { mat.m[0][0], mat.m[0][1], mat.m[0][2] };
            float offsetAmount = 2.0f;
            Vector3 rightOffset = { playerPos.x + rightDir.x * offsetAmount + forwardVec.x * 1.5f, playerPos.y + rightDir.y * offsetAmount + forwardVec.y * 1.5f, playerPos.z + rightDir.z * offsetAmount + forwardVec.z * 1.5f };
            Vector3 leftOffset = { playerPos.x - rightDir.x * offsetAmount + forwardVec.x * 1.5f, playerPos.y - rightDir.y * offsetAmount + forwardVec.y * 1.5f, playerPos.z - rightDir.z * offsetAmount + forwardVec.z * 1.5f };

            Vector3 rightDirToTarget = { targetPos.x - rightOffset.x, targetPos.y - rightOffset.y, targetPos.z - rightOffset.z };
            float rLen = std::sqrtf(rightDirToTarget.x * rightDirToTarget.x + rightDirToTarget.y * rightDirToTarget.y + rightDirToTarget.z * rightDirToTarget.z);
            if (rLen > 0.0001f) { rightDirToTarget.x /= rLen; rightDirToTarget.y /= rLen; rightDirToTarget.z /= rLen; }
            Vector3 rightVelocity = { rightDirToTarget.x * bulletSpeed_, rightDirToTarget.y * bulletSpeed_, rightDirToTarget.z * bulletSpeed_ };

            Vector3 leftDirToTarget = { targetPos.x - leftOffset.x, targetPos.y - leftOffset.y, targetPos.z - leftOffset.z };
            float lLen = std::sqrtf(leftDirToTarget.x * leftDirToTarget.x + leftDirToTarget.y * leftDirToTarget.y + leftDirToTarget.z * leftDirToTarget.z);
            if (lLen > 0.0001f) { leftDirToTarget.x /= lLen; leftDirToTarget.y /= lLen; leftDirToTarget.z /= lLen; }
            Vector3 leftVelocity = { leftDirToTarget.x * bulletSpeed_, leftDirToTarget.y * bulletSpeed_, leftDirToTarget.z * bulletSpeed_ };

            std::unique_ptr<PlayerBullet> rightBullet = std::make_unique<PlayerBullet>();
            rightBullet->Initialize(object3dCommon_, rightOffset, rightVelocity, parentCamera, reticleHoverEnemy_);
            bullets.push_back(std::move(rightBullet));

            std::unique_ptr<PlayerBullet> leftBullet = std::make_unique<PlayerBullet>();
            leftBullet->Initialize(object3dCommon_, leftOffset, leftVelocity, parentCamera, reticleHoverEnemy_);
            bullets.push_back(std::move(leftBullet));
        } else {
            // 通常の1発発射: 機首銃口から奥照準の中心へ向けて射出
            Vector3 muzzlePos = { playerPos.x + forwardVec.x * 2.2f, playerPos.y + forwardVec.y * 2.2f, playerPos.z + forwardVec.z * 2.2f };
            Vector3 direction = {
                targetPos.x - muzzlePos.x,
                targetPos.y - muzzlePos.y,
                targetPos.z - muzzlePos.z
            };
            float length = std::sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
            if (length > 0.0f) {
                direction.x /= length;
                direction.y /= length;
                direction.z /= length;
            }
            Vector3 velocity = {
                direction.x * bulletSpeed_,
                direction.y * bulletSpeed_,
                direction.z * bulletSpeed_
            };

            std::unique_ptr<PlayerBullet> newBullet = std::make_unique<PlayerBullet>();
            newBullet->Initialize(object3dCommon_, muzzlePos, velocity, parentCamera, reticleHoverEnemy_);
            bullets.push_back(std::move(newBullet));
        }

        // 通常ショットSE再生
        SoundManager::GetInstance()->PlaySE("player_shot.mp3", 0.7f);
    }
}

void Player::LoadSettings(const std::string& filepath) {
    std::ifstream file(filepath);
    if (file.is_open()) {
        nlohmann::json j;
        file >> j;
        if (j.contains("speed")) speed_ = j["speed"];
        if (j.contains("reticleSpeed")) reticleSpeed_ = j["reticleSpeed"];
        if (j.contains("moveLimitX")) moveLimitX_ = j["moveLimitX"];
        if (j.contains("moveLimitY")) moveLimitY_ = j["moveLimitY"];
        if (j.contains("attackInterval")) attackInterval_ = j["attackInterval"];
        if (j.contains("rollMaxTime")) rollMaxTime_ = j["rollMaxTime"];
        if (j.contains("playerLimitX")) { playerLimitX_ = j["playerLimitX"]; targetLimitX_ = playerLimitX_; }
        if (j.contains("playerLimitYMin")) { playerLimitYMin_ = j["playerLimitYMin"]; targetLimitYMin_ = playerLimitYMin_; }
        if (j.contains("playerLimitYMax")) { playerLimitYMax_ = j["playerLimitYMax"]; targetLimitYMax_ = playerLimitYMax_; }
        if (j.contains("followSpeed")) followSpeed_ = j["followSpeed"];
        if (j.contains("bulletSpeed")) bulletSpeed_ = j["bulletSpeed"];
        if (j.contains("modelName")) modelName_ = j["modelName"];
        if (j.contains("reflection")) reflection_ = j["reflection"];
        if (j.contains("playerScale")) {
            playerScale_ = { j["playerScale"][0], j["playerScale"][1], j["playerScale"][2] };
        }
        if (j.contains("colliderSize")) {
            colliderSize_ = { j["colliderSize"][0], j["colliderSize"][1], j["colliderSize"][2] };
            if (colliderObject_) {
                colliderObject_->SetScale(colliderSize_);
            }
        }
        if (j.contains("hp")) {
            hp_ = j["hp"];
        }
        if (j.contains("maxHp")) {
            maxHp_ = j["maxHp"];
        }
        if (j.contains("color")) {
            auto c = j["color"];
            if (c.is_array() && c.size() == 4) {
                color_ = { c[0], c[1], c[2], c[3] };
            }
        }
        if (j.contains("modelPosOffset")) {
            auto arr = j["modelPosOffset"];
            if (arr.is_array() && arr.size() == 3) modelPosOffset_ = { arr[0], arr[1], arr[2] };
        }
        if (j.contains("modelRotOffset")) {
            auto arr = j["modelRotOffset"];
            if (arr.is_array() && arr.size() == 3) modelRotOffset_ = { arr[0], arr[1], arr[2] };
        }
        if (j.contains("terrainKnockbackPower")) terrainKnockbackPower_ = j["terrainKnockbackPower"];
        if (j.contains("terrainPushMargin")) terrainPushMargin_ = j["terrainPushMargin"];
        if (j.contains("terrainCollisionRadius")) terrainCollisionRadius_ = j["terrainCollisionRadius"];
        if (j.contains("lockOnCompleteTime")) lockOnCompleteTime_ = j["lockOnCompleteTime"];
        if (j.contains("lockOnInterval")) lockOnInterval_ = j["lockOnInterval"];
        if (j.contains("missileReloadTime")) missileReloadTime_ = j["missileReloadTime"];
        if (j.contains("lockOnMaxDistance")) lockOnMaxDistance_ = j["lockOnMaxDistance"];
        file.close();
        
        if (object_) {
            object_->SetModel(modelName_);
            object_->SetColor(color_);
            object_->SetEnvironmentCoefficient(reflection_ ? 1.0f : 0.0f);
            if (accessory_) {
                accessory_->SetParent(object_.get());
            }
        }
    }
}

void Player::SaveSettings(const std::string& filepath) {
    nlohmann::json j;
    j["speed"] = speed_;
    j["reticleSpeed"] = reticleSpeed_;
    j["moveLimitX"] = moveLimitX_;
    j["moveLimitY"] = moveLimitY_;
    j["attackInterval"] = attackInterval_;
    j["rollMaxTime"] = rollMaxTime_;
    j["playerLimitX"] = playerLimitX_;
    j["playerLimitYMin"] = playerLimitYMin_;
    j["playerLimitYMax"] = playerLimitYMax_;
    j["followSpeed"] = followSpeed_;
    j["bulletSpeed"] = bulletSpeed_;
    j["modelName"] = modelName_;
    j["reflection"] = reflection_;
    j["color"] = { color_.x, color_.y, color_.z, color_.w };
    j["modelPosOffset"] = { modelPosOffset_.x, modelPosOffset_.y, modelPosOffset_.z };
    j["modelRotOffset"] = { modelRotOffset_.x, modelRotOffset_.y, modelRotOffset_.z };
    j["playerScale"] = { playerScale_.x, playerScale_.y, playerScale_.z };
    j["colliderSize"] = { colliderSize_.x, colliderSize_.y, colliderSize_.z };
    j["hp"] = hp_;
    j["maxHp"] = maxHp_;
    j["terrainKnockbackPower"] = terrainKnockbackPower_;
    j["terrainPushMargin"] = terrainPushMargin_;
    j["terrainCollisionRadius"] = terrainCollisionRadius_;
    j["lockOnCompleteTime"] = lockOnCompleteTime_;
    j["lockOnInterval"] = lockOnInterval_;
    j["missileReloadTime"] = missileReloadTime_;
    j["lockOnMaxDistance"] = lockOnMaxDistance_;

    std::filesystem::path p(filepath);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    std::ofstream file(filepath);
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
    }
}

// プレイヤー関連のUI描画処理
void Player::DrawUI() {
#ifdef USE_IMGUI
    ImGui::Begin("Player Editor");
    DrawImGuiContent();
    ImGui::End();
#endif
}

void Player::DrawImGuiContent() {
#ifdef USE_IMGUI
    // デバッグ・チート機能
    if (ImGui::CollapsingHeader("Debug & Cheats", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("ゴッドモード (無敵 / God Mode)", &isGodMode_);
        ImGui::SameLine();
        if (ImGui::Button("HP 全快 (Heal Full)")) {
            hp_ = maxHp_;
            isDead_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("即死 (Instant Kill)")) {
            hp_ = 0;
            isDead_ = true;
        }

        ImGui::DragInt("HP", &hp_, 10, 0, 10000);
        ImGui::DragInt("Max HP", &maxHp_, 10, 1, 10000);
        ImGui::Checkbox("ダブルショット (Double Shot)", &isDoubleShot_);
    }

    // 移動・可動域設定
    if (ImGui::CollapsingHeader("Movement & Range Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("左右制限 X (Player Limit X)", &playerLimitX_, 5.0f, 50.0f, "%.1f");
        moveLimitX_ = playerLimitX_;
        targetLimitX_ = playerLimitX_;

        ImGui::SliderFloat("下限 Y (Player Limit Y Min)", &playerLimitYMin_, -20.0f, 0.0f, "%.1f");
        ImGui::SliderFloat("上限 Y (Player Limit Y Max)", &playerLimitYMax_, 0.0f, 30.0f, "%.1f");
        targetLimitYMin_ = playerLimitYMin_;
        targetLimitYMax_ = playerLimitYMax_;

        ImGui::Separator();
        ImGui::SliderFloat("移動速度 (Speed)", &speed_, 0.05f, 2.0f, "%.2f");
        ImGui::SliderFloat("照準速度 (Reticle Speed)", &reticleSpeed_, 0.1f, 3.0f, "%.2f");
        ImGui::SliderFloat("追従速度 (Follow Speed)", &followSpeed_, 0.01f, 0.5f, "%.3f");
        ImGui::SliderFloat("ロール時間 (Roll Max Time)", &rollMaxTime_, 5.0f, 60.0f, "%.0f frames");

        ImGui::Separator();
        ImGui::Text("ブーストゲージ設定 (Boost Settings)");
        char boostOverlay[64];
        if (isBoostOverheated_) snprintf(boostOverlay, sizeof(boostOverlay), "OVERHEAT! (%.0f%%)", GetBoostRatio() * 100.0f);
        else if (isBoosting_) snprintf(boostOverlay, sizeof(boostOverlay), "BOOSTING (%.0f%%)", GetBoostRatio() * 100.0f);
        else snprintf(boostOverlay, sizeof(boostOverlay), "%.0f / %.0f (%.0f%%)", boostEnergy_, maxBoostEnergy_, GetBoostRatio() * 100.0f);
        ImGui::ProgressBar(GetBoostRatio(), ImVec2(-1, 18), boostOverlay);
        ImGui::SliderFloat("最大ブースト量 (Max Boost)", &maxBoostEnergy_, 50.0f, 300.0f, "%.0f");
        ImGui::SliderFloat("消費速度 (Consume Rate)", &boostConsumeRate_, 0.1f, 2.0f, "%.2f /frame");
        ImGui::SliderFloat("回復速度 (Recover Rate)", &boostRecoverRate_, 0.05f, 1.5f, "%.2f /frame");
        if (ImGui::Button("ブースト全快 (Refill Boost)")) {
            ResetBoost();
        }

        ImGui::Separator();
        ImGui::Text("地形衝突パラメータ");
        ImGui::SliderFloat("ノックバック力", &terrainKnockbackPower_, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("めり込み押し戻しマージン", &terrainPushMargin_, 0.01f, 0.5f, "%.2f");
        ImGui::SliderFloat("衝突判定半径", &terrainCollisionRadius_, 0.1f, 3.0f, "%.2f");
    }

    // 攻撃・戦闘設定
    if (ImGui::CollapsingHeader("Combat Settings")) {
        ImGui::SliderFloat("攻撃間隔 (Attack Interval)", &attackInterval_, 1.0f, 60.0f, "%.0f frames");
        ImGui::SliderFloat("弾速 (Bullet Speed)", &bulletSpeed_, 0.5f, 10.0f, "%.1f");
    }

    // ロックオン弾（ミサイル）設定
    if (ImGui::CollapsingHeader("ロックオン弾設定 (Lock-On / Missile Settings)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("ロックオン完了時間 (Lock Time)", &lockOnCompleteTime_, 1.0f, 120.0f, "%.0f frames");
        ImGui::TextDisabled("  ※約 %.2f 秒 (捕捉から照準完了までの時間)", lockOnCompleteTime_ / 60.0f);

        ImGui::SliderFloat("連続ロックオン間隔 (Lock Interval)", &lockOnInterval_, 5.0f, 180.0f, "%.0f frames");
        ImGui::TextDisabled("  ※約 %.2f 秒 (次の敵をロックするまでの間隔)", lockOnInterval_ / 60.0f);

        ImGui::SliderFloat("ミサイルリロード時間 (Reload Time)", &missileReloadTime_, 10.0f, 300.0f, "%.0f frames");
        ImGui::TextDisabled("  ※約 %.2f 秒 (全弾発射後の再装填時間)", missileReloadTime_ / 60.0f);

        ImGui::SliderFloat("ロックオン最大射程 (Max Distance)", &lockOnMaxDistance_, 50.0f, 800.0f, "%.0f m");

        ImGui::Spacing();
        if (ImGui::Button("ロックオン設定をJSONに保存 (Save Settings)", ImVec2(-1, 28))) {
            SaveSettings("resources/json/player/player_settings.json");
        }
    }

    // モデル・外観設定
    if (ImGui::CollapsingHeader("Model & Visual Settings")) {
        const char* models[] = { "cube.obj", "player.obj", "monsterBall.obj", "suzanne.obj" };
        int currentModel = 0;
        for (int i = 0; i < 4; ++i) {
            if (modelName_ == models[i]) {
                currentModel = i;
                break;
            }
        }
        if (ImGui::Combo("Model", &currentModel, models, 4)) {
            modelName_ = models[currentModel];
            if (object_) {
                object_->SetModel(modelName_);
                object_->SetColor(color_);
                object_->SetEnvironmentTextureIndex(skyboxTexIndex_);
                object_->SetEnvironmentCoefficient(reflection_ ? 1.0f : 0.0f);
                if (accessory_) {
                    accessory_->SetParent(object_.get());
                }
            }
        }
        
        float col[4] = { color_.x, color_.y, color_.z, color_.w };
        if (ImGui::ColorEdit4("Color", col)) {
            color_ = { col[0], col[1], col[2], col[3] };
            if (object_) {
                object_->SetColor(color_);
            }
        }
        
        if (ImGui::Checkbox("Reflection", &reflection_)) {
            if (object_) {
                object_->SetEnvironmentCoefficient(reflection_ ? 1.0f : 0.0f);
            }
        }
        
        ImGui::Separator();
        float posOffset[3] = { modelPosOffset_.x, modelPosOffset_.y, modelPosOffset_.z };
        if (ImGui::DragFloat3("Position Offset", posOffset, 0.1f)) {
            modelPosOffset_ = { posOffset[0], posOffset[1], posOffset[2] };
        }
        float rotOffset[3] = { modelRotOffset_.x, modelRotOffset_.y, modelRotOffset_.z };
        if (ImGui::DragFloat3("Rotation Offset", rotOffset, 0.05f)) {
            modelRotOffset_ = { rotOffset[0], rotOffset[1], rotOffset[2] };
        }
        if (ImGui::DragFloat3("Player Scale", &playerScale_.x, 0.01f)) {
            if (object_) object_->SetScale(playerScale_);
        }
        if (ImGui::DragFloat3("Collider Size", &colliderSize_.x, 0.1f)) {
            if (colliderObject_) colliderObject_->SetScale(colliderSize_);
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("設定をJSONに保存 (Save Settings)", ImVec2(200, 32))) {
        SaveSettings("resources/json/player/player_settings.json");
    }
    ImGui::SameLine();
    if (ImGui::Button("JSONから再読込 (Reload Settings)", ImVec2(200, 32))) {
        LoadSettings("resources/json/player/player_settings.json");
    }
#endif
}

bool Player::IsBanking() const {
    return std::abs(currentBank_) > 0.65f || isRolling_ || std::abs(currentPitch_) > 0.35f;
}

Vector3 Player::GetLeftWingPosition() const {
    const Matrix4x4& mat = object_->GetmatWorld();
    float wingSpan = 3.8f;
    Vector3 leftWingLocal = { -wingSpan, 0.0f, -0.5f };
    return Vector3{
        leftWingLocal.x * mat.m[0][0] + leftWingLocal.y * mat.m[1][0] + leftWingLocal.z * mat.m[2][0] + mat.m[3][0],
        leftWingLocal.x * mat.m[0][1] + leftWingLocal.y * mat.m[1][1] + leftWingLocal.z * mat.m[2][1] + mat.m[3][1],
        leftWingLocal.x * mat.m[0][2] + leftWingLocal.y * mat.m[1][2] + leftWingLocal.z * mat.m[2][2] + mat.m[3][2]
    };
}

Vector3 Player::GetRightWingPosition() const {
    const Matrix4x4& mat = object_->GetmatWorld();
    float wingSpan = 3.8f;
    Vector3 rightWingLocal = { wingSpan, 0.0f, -0.5f };
    return Vector3{
        rightWingLocal.x * mat.m[0][0] + rightWingLocal.y * mat.m[1][0] + rightWingLocal.z * mat.m[2][0] + mat.m[3][0],
        rightWingLocal.x * mat.m[0][1] + rightWingLocal.y * mat.m[1][1] + rightWingLocal.z * mat.m[2][1] + mat.m[3][1],
        rightWingLocal.x * mat.m[0][2] + rightWingLocal.y * mat.m[1][2] + rightWingLocal.z * mat.m[2][2] + mat.m[3][2]
    };
}

void Player::PowerUp() {
    SetPowerUpLevel(powerUpLevel_ + 1);
}

void Player::SetPowerUpLevel(int level) {
    powerUpLevel_ = level;
    if (level <= 0) {
        // 通常状態
        isDoubleShot_ = false;
        maxMissiles_ = 4;
        lockOnCompleteTime_ = defaultLockOnCompleteTime_;
        lockOnInterval_ = defaultLockOnInterval_;
        missileReloadTime_ = defaultMissileReloadTime_;
    } else if (level == 1) {
        // 強化リング1個目: ダブルショット解放
        isDoubleShot_ = true;
        maxMissiles_ = 4;
        lockOnCompleteTime_ = defaultLockOnCompleteTime_;
        lockOnInterval_ = defaultLockOnInterval_;
        missileReloadTime_ = defaultMissileReloadTime_;
    } else if (level >= 2) {
        // 強化リング2個目: 最大ロックオン数が4から8へ拡張 & ロックオン速度大幅向上！
        isDoubleShot_ = true;
        maxMissiles_ = 8;
        lockOnCompleteTime_ = 10.0f; // 20f -> 10f (ロックオン所要時間半分・高速化)
        lockOnInterval_ = 24.0f;     // 60f -> 24f (連続捕捉間隔が大幅短縮)
        missileReloadTime_ = 90.0f;  // 120f -> 90f (リロード短縮)
    }
}