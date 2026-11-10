#include "CyberBitWeapon.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/PlayerBullet.h"
#include "Game/Actor/Enemy/Enemy.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Input/Input.h"
#include <algorithm>
#include <cmath>

void CyberBitWeapon::Initialize(Player* owner, Object3dCommon* object3dCommon) {
    owner_ = owner;
    object3dCommon_ = object3dCommon;
    ammo_ = 12;
    maxAmmo_ = 12;
    maxSimultaneous_ = 4;
    reloadTimer_ = 0.0f;
    lockOnAnimTimer_ = 0.0f;
    lockOnDelayTimer_ = 0.0f;
    hoverAnimTimer_ = 0.0f;
    normalShotTimer_ = 0.0f;
    spaceKeyWasPressed_ = false;

    // 1. 4つのファンネルモデルパーツをロード
    auto modelMgr = ModelManager::GetInstance();
    modelMgr->LoadModel("resources/3dModels/funnel/funnel_body/funnel_body.obj");
    modelMgr->LoadModel("resources/3dModels/funnel/funnel_core/funnel_core.obj");
    modelMgr->LoadModel("resources/3dModels/funnel/funnel_ring1/funnel_ring1.obj");
    modelMgr->LoadModel("resources/3dModels/funnel/funnel_ring2/funnel_ring2.obj");
    modelMgr->LoadModel("RockOn.obj");

    // 2. 4基のファンネル（サイバービット）端末を生成・セットアップ
    for (int i = 0; i < BIT_COUNT; ++i) {
        auto& b = bits_[i];
        b.state = BitFlightState::IDLE_HOVER;
        b.flightTimer = 0.0f;
        b.hasHitTarget = false;
        b.targetEnemy = nullptr;
        b.targetBoss = nullptr;
        b.targetCarriageIndex = -1;

        // 各端末ごとの独立した自転速度・角度
        b.coreRotY = static_cast<float>(i) * 1.57f;
        b.coreRotSpeed = 0.06f + static_cast<float>(i) * 0.015f;
        b.ring1RotZ = static_cast<float>(i) * 0.8f;
        b.ring1RotSpeed = 0.09f + static_cast<float>(i) * 0.02f;
        b.ring2RotZ = static_cast<float>(i) * 2.1f;
        b.ring2RotSpeed = -0.07f - static_cast<float>(i) * 0.015f;

        // 本体（外枠フレーム）
        b.bodyObject = std::make_unique<Object3d>();
        b.bodyObject->Initialize(object3dCommon_);
        b.bodyObject->SetModel("resources/3dModels/funnel/funnel_body/funnel_body.obj");
        b.bodyObject->SetEnableLighting(false);
        b.bodyObject->SetSelectLightings(0);
        b.bodyObject->SetColor({ 0.90f, 0.94f, 1.0f, 1.0f }); // サイバーシルバー
        b.bodyObject->SetScale({ 0.45f, 0.45f, 0.45f }); // 洗練されたビットサイズ
        b.currentScale = 0.45f;

        // コア（内部ピラミッド結晶）
        b.coreObject = std::make_unique<Object3d>();
        b.coreObject->Initialize(object3dCommon_);
        b.coreObject->SetModel("resources/3dModels/funnel/funnel_core/funnel_core.obj");
        b.coreObject->SetEnableLighting(false);
        b.coreObject->SetSelectLightings(0);
        b.coreObject->SetColor({ 1.0f, 0.15f, 0.85f, 1.0f }); // ネオンマゼンタ
        b.coreObject->SetScale({ 1.0f, 1.0f, 1.0f });
        b.coreObject->SetParent(b.bodyObject.get());

        // リング1（第1回転リング）
        b.ring1Object = std::make_unique<Object3d>();
        b.ring1Object->Initialize(object3dCommon_);
        b.ring1Object->SetModel("resources/3dModels/funnel/funnel_ring1/funnel_ring1.obj");
        b.ring1Object->SetEnableLighting(false);
        b.ring1Object->SetSelectLightings(0);
        b.ring1Object->SetColor({ 0.95f, 0.35f, 1.0f, 1.0f }); // ネオンパープルピンク
        b.ring1Object->SetScale({ 1.0f, 1.0f, 1.0f });
        b.ring1Object->SetParent(b.bodyObject.get());

        // リング2（第2逆回転リング）
        b.ring2Object = std::make_unique<Object3d>();
        b.ring2Object->Initialize(object3dCommon_);
        b.ring2Object->SetModel("resources/3dModels/funnel/funnel_ring2/funnel_ring2.obj");
        b.ring2Object->SetEnableLighting(false);
        b.ring2Object->SetSelectLightings(0);
        b.ring2Object->SetColor({ 0.35f, 0.85f, 1.0f, 1.0f }); // ネオンサイバーシアン
        b.ring2Object->SetScale({ 1.0f, 1.0f, 1.0f });
        b.ring2Object->SetParent(b.bodyObject.get());
    }

    // 3. マルチロックオン用3Dレティクルの初期化
    for (int i = 0; i < MAX_TARGETS; ++i) {
        lockOnReticles_[i] = std::make_unique<Object3d>();
        lockOnReticles_[i]->Initialize(object3dCommon_);
        lockOnReticles_[i]->SetModel("RockOn.obj");
        if (lockOnReticles_[i]->GetModel()) {
            lockOnReticles_[i]->GetModel()->SetColor({ 0.15f, 0.90f, 1.0f, 1.0f }); // 鮮烈なサイバーシアン
        }
        lockOnReticles_[i]->SetScale({ 0.001f, 0.001f, 0.001f });
        lockOnReticles_[i]->SetEnableLighting(false);
    }

    Reset();
}

void CyberBitWeapon::Reset() {
    ammo_ = maxAmmo_;
    reloadTimer_ = 0.0f;
    lockOnAnimTimer_ = 0.0f;
    lockOnDelayTimer_ = 0.0f;
    hoverAnimTimer_ = 0.0f;
    normalShotTimer_ = 0.0f;
    spaceKeyWasPressed_ = false;
    multiLockedEnemies_.clear();

    for (int i = 0; i < MAX_TARGETS; ++i) {
        if (lockOnReticles_[i]) {
            lockOnReticles_[i]->SetScale({ 0.001f, 0.001f, 0.001f });
        }
    }

    if (owner_ && owner_->GetObject3d()) {
        const Matrix4x4& mat = owner_->GetObject3d()->GetmatWorld();
        Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
        for (auto& b : bits_) {
            b.state = BitFlightState::IDLE_HOVER;
            b.position = playerPos;
            b.prevPosition = playerPos;
            b.velocity = { 0.0f, 0.0f, 0.0f };
            b.flightTimer = 0.0f;
            b.hasHitTarget = false;
            b.targetEnemy = nullptr;
            b.targetBoss = nullptr;
            b.targetCarriageIndex = -1;
            b.currentScale = 0.45f;
            if (b.bodyObject) {
                b.bodyObject->SetScale({ 0.45f, 0.45f, 0.45f });
            }
        }
    }
}

void CyberBitWeapon::Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& /*enemies*/, ArmoredTrainBoss* boss, Object3d* /*parentCamera*/) {
    if (reloadTimer_ > 0.0f) reloadTimer_ -= gameSpeed;
    if (lockOnDelayTimer_ > 0.0f) lockOnDelayTimer_ -= gameSpeed;
    if (normalShotTimer_ > 0.0f) normalShotTimer_ -= gameSpeed;

    lockOnAnimTimer_ += 0.1f * gameSpeed;
    hoverAnimTimer_ += 0.05f * gameSpeed;

    if (!owner_ || !owner_->GetObject3d()) return;
    const Matrix4x4& pMat = owner_->GetObject3d()->GetmatWorld();
    Vector3 playerPos = { pMat.m[3][0], pMat.m[3][1], pMat.m[3][2] };
    Vector3 pForward = { pMat.m[2][0], pMat.m[2][1], pMat.m[2][2] };
    Vector3 pUp = { pMat.m[1][0], pMat.m[1][1], pMat.m[1][2] };
    Vector3 pRight = { pMat.m[0][0], pMat.m[0][1], pMat.m[0][2] };

    // 自機周囲での待機オフセット（4基のフォーメーション）
    const Vector3 kIdleOffsets[BIT_COUNT] = {
        { -2.4f,  1.2f, -1.0f }, // 0: 左上翼端
        {  2.4f,  1.2f, -1.0f }, // 1: 右上翼端
        { -2.8f, -0.6f, -1.8f }, // 2: 左下後方
        {  2.8f, -0.6f, -1.8f }  // 3: 右下後方
    };

    // 4基のファンネル端末の飛翔・弧の更新
    for (int i = 0; i < BIT_COUNT; ++i) {
        auto& b = bits_[i];

        // 各パーツの独立自転アニメーション
        b.coreRotY += b.coreRotSpeed * gameSpeed;
        b.ring1RotZ += b.ring1RotSpeed * gameSpeed;
        b.ring2RotZ += b.ring2RotSpeed * gameSpeed;

        // 待機目標位置（自機追従）
        float bob = std::sinf(hoverAnimTimer_ * 2.2f + static_cast<float>(i) * 1.57f) * 0.35f;
        Vector3 homeStation = {
            playerPos.x + pRight.x * kIdleOffsets[i].x + pUp.x * (kIdleOffsets[i].y + bob) + pForward.x * kIdleOffsets[i].z,
            playerPos.y + pRight.y * kIdleOffsets[i].x + pUp.y * (kIdleOffsets[i].y + bob) + pForward.y * kIdleOffsets[i].z,
            playerPos.z + pRight.z * kIdleOffsets[i].x + pUp.z * (kIdleOffsets[i].y + bob) + pForward.z * kIdleOffsets[i].z
        };

        Vector3 lookDir = pForward;

        switch (b.state) {
        case BitFlightState::IDLE_HOVER: {
            // 自機周囲に滑らかに追従
            float lerpRate = 0.22f * gameSpeed;
            b.position.x += (homeStation.x - b.position.x) * lerpRate;
            b.position.y += (homeStation.y - b.position.y) * lerpRate;
            b.position.z += (homeStation.z - b.position.z) * lerpRate;
            b.prevPosition = b.position;

            // ロックオン中の敵がいればそちらへエイム
            if (i < static_cast<int>(multiLockedEnemies_.size())) {
                const auto& t = multiLockedEnemies_[i];
                Vector3 tPos = { 0.0f, 0.0f, 0.0f };
                bool hasTarget = false;
                if (t.boss && !t.boss->IsDefeated() && !t.boss->IsCarriageDestroyed(t.carriageIndex)) {
                    tPos = t.boss->GetCarriagePosition(t.carriageIndex);
                    hasTarget = true;
                } else if (t.enemy && !t.enemy->IsDead()) {
                    tPos = t.enemy->GetColliderCenter();
                    hasTarget = true;
                }
                if (hasTarget) {
                    Vector3 toT = { tPos.x - b.position.x, tPos.y - b.position.y, tPos.z - b.position.z };
                    float len = std::sqrtf(toT.x * toT.x + toT.y * toT.y + toT.z * toT.z);
                    if (len > 0.001f) {
                        lookDir = { toT.x / len, toT.y / len, toT.z / len };
                    }
                }
            }
            break;
        }

        case BitFlightState::ARC_OUTWARD: {
            // 弧の初期展開フェーズ：外側へ綺麗に広がりながら前進
            b.flightTimer += gameSpeed;
            b.prevPosition = b.position;
            b.position.x += b.velocity.x * gameSpeed;
            b.position.y += b.velocity.y * gameSpeed;
            b.position.z += b.velocity.z * gameSpeed;

            // 速度ベクトル方向を向く
            float vLen = std::sqrtf(b.velocity.x * b.velocity.x + b.velocity.y * b.velocity.y + b.velocity.z * b.velocity.z);
            if (vLen > 0.001f) {
                lookDir = { b.velocity.x / vLen, b.velocity.y / vLen, b.velocity.z / vLen };
            }

            // 10フレーム（約0.16秒）外側へ弧を広げたら、ターゲットへのホーミング誘導へ移行
            if (b.flightTimer >= 10.0f) {
                b.state = BitFlightState::HOMING_ATTACK;
            }
            break;
        }

        case BitFlightState::HOMING_ATTACK: {
            b.flightTimer += gameSpeed;
            b.prevPosition = b.position;

            // ターゲット座標の取得
            Vector3 targetPos = playerPos;
            bool targetAlive = false;

            if (b.targetBoss && boss && boss->IsActive()) {
                if (!boss->IsDefeated() && !boss->IsCarriageDestroyed(b.targetCarriageIndex)) {
                    targetPos = boss->GetCarriagePosition(b.targetCarriageIndex);
                    targetAlive = true;
                }
            } else if (b.targetEnemy) {
                if (!b.targetEnemy->IsDead()) {
                    targetPos = b.targetEnemy->GetColliderCenter();
                    targetAlive = true;
                }
            }

            // ターゲットが生存していればターゲットへ向けて操舵
            if (targetAlive) {
                Vector3 toTarget = { targetPos.x - b.position.x, targetPos.y - b.position.y, targetPos.z - b.position.z };
                float dist = std::sqrtf(toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z);

                // 孤を描く誘導ステアリング（徐々に加速しながら敵へ向かってカーブ旋回）
                if (dist > 0.001f) {
                    float targetSpeed = (std::min)(3.6f, 2.2f + b.flightTimer * 0.04f);
                    Vector3 desiredVel = {
                        (toTarget.x / dist) * targetSpeed,
                        (toTarget.y / dist) * targetSpeed,
                        (toTarget.z / dist) * targetSpeed
                    };

                    float steerRate = 0.22f * gameSpeed;
                    b.velocity.x += (desiredVel.x - b.velocity.x) * steerRate;
                    b.velocity.y += (desiredVel.y - b.velocity.y) * steerRate;
                    b.velocity.z += (desiredVel.z - b.velocity.z) * steerRate;

                    // 速度を targetSpeed に正規化
                    float curSpeed = std::sqrtf(b.velocity.x * b.velocity.x + b.velocity.y * b.velocity.y + b.velocity.z * b.velocity.z);
                    if (curSpeed > 0.001f) {
                        b.velocity.x = (b.velocity.x / curSpeed) * targetSpeed;
                        b.velocity.y = (b.velocity.y / curSpeed) * targetSpeed;
                        b.velocity.z = (b.velocity.z / curSpeed) * targetSpeed;
                    }
                }

                // 移動更新
                b.position.x += b.velocity.x * gameSpeed;
                b.position.y += b.velocity.y * gameSpeed;
                b.position.z += b.velocity.z * gameSpeed;

                // トンネリング防止：移動線分と敵コライダー球（半径3.5m）の衝突判定
                Vector3 moveDelta = { b.position.x - b.prevPosition.x, b.position.y - b.prevPosition.y, b.position.z - b.prevPosition.z };
                Vector3 toPrev = { targetPos.x - b.prevPosition.x, targetPos.y - b.prevPosition.y, targetPos.z - b.prevPosition.z };
                float moveLenSq = moveDelta.x * moveDelta.x + moveDelta.y * moveDelta.y + moveDelta.z * moveDelta.z;
                float t = (moveLenSq > 0.0001f) ? (toPrev.x * moveDelta.x + toPrev.y * moveDelta.y + toPrev.z * moveDelta.z) / moveLenSq : 0.0f;
                t = std::clamp(t, 0.0f, 1.0f);
                Vector3 closestPt = { b.prevPosition.x + moveDelta.x * t, b.prevPosition.y + moveDelta.y * t, b.prevPosition.z + moveDelta.z * t };
                Vector3 diff = { targetPos.x - closestPt.x, targetPos.y - closestPt.y, targetPos.z - closestPt.z };
                float closestDistSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

                float hitRadius = 3.5f;
                if ((dist < hitRadius || closestDistSq <= hitRadius * hitRadius) && !b.hasHitTarget) {
                    b.hasHitTarget = true;
                    // ダメージ・撃破付与
                    if (b.targetBoss && boss) {
                        boss->OnDamaged(b.targetCarriageIndex, 3);
                    } else if (b.targetEnemy) {
                        b.targetEnemy->Kill(); // 一撃で確実に撃破！
                    }
                    // 攻撃完了 -> 弧を描いて自機へ帰還
                    b.state = BitFlightState::RETURNING;
                    b.flightTimer = 0.0f;
                    break;
                }
            } else {
                // ターゲット喪失時は直進して帰還へ移行
                b.position.x += b.velocity.x * gameSpeed;
                b.position.y += b.velocity.y * gameSpeed;
                b.position.z += b.velocity.z * gameSpeed;
                b.state = BitFlightState::RETURNING;
                b.flightTimer = 0.0f;
            }

            float vLen = std::sqrtf(b.velocity.x * b.velocity.x + b.velocity.y * b.velocity.y + b.velocity.z * b.velocity.z);
            if (vLen > 0.001f) {
                lookDir = { b.velocity.x / vLen, b.velocity.y / vLen, b.velocity.z / vLen };
            }

            // 最大飛行時間（約3秒）を超えたら強制帰還
            if (b.flightTimer > 180.0f) {
                b.state = BitFlightState::RETURNING;
                b.flightTimer = 0.0f;
            }
            break;
        }

        case BitFlightState::RETURNING: {
            b.flightTimer += gameSpeed;
            b.prevPosition = b.position;

            // 自機の待機ステーションへ向かって弧を描いて旋回帰還
            Vector3 toHome = { homeStation.x - b.position.x, homeStation.y - b.position.y, homeStation.z - b.position.z };
            float hDist = std::sqrtf(toHome.x * toHome.x + toHome.y * toHome.y + toHome.z * toHome.z);

            // 1. 至近距離（1.0m以内）または帰還時間90フレーム（約1.5秒）経過で確実にドッキング完了
            if (hDist < 1.0f || b.flightTimer >= 90.0f) {
                b.state = BitFlightState::IDLE_HOVER;
                b.position = homeStation;
                b.prevPosition = homeStation;
                b.velocity = { 0.0f, 0.0f, 0.0f };
                b.targetEnemy = nullptr;
                b.targetBoss = nullptr;
                b.targetCarriageIndex = -1;
                b.hasHitTarget = false;
                break;
            }

            // 2. 近距離アプローチ（5.0m以内）：ステアリング旋回から滑らかな直接位置補間（Lerp吸着）へ切り替え
            // これにより、高速接近時のオーバーシュートや自機周囲のぐるぐる周回バグが100%根絶される！
            if (hDist < 5.0f) {
                float dockLerp = (std::min)(1.0f, 0.28f * gameSpeed);
                b.position.x += (homeStation.x - b.position.x) * dockLerp;
                b.position.y += (homeStation.y - b.position.y) * dockLerp;
                b.position.z += (homeStation.z - b.position.z) * dockLerp;
                b.velocity = { 0.0f, 0.0f, 0.0f };

                // 姿勢は正面へスムーズに移行
                lookDir.x += (pForward.x - lookDir.x) * 0.25f;
                lookDir.y += (pForward.y - lookDir.y) * 0.25f;
                lookDir.z += (pForward.z - lookDir.z) * 0.25f;
                break;
            }

            // 3. 遠距離アプローチ：距離に応じたスムーズなアライバル減速
            float returnSpeed = (std::min)(3.2f, (std::max)(1.2f, hDist * 0.35f));
            Vector3 desiredVel = {
                (toHome.x / hDist) * returnSpeed,
                (toHome.y / hDist) * returnSpeed,
                (toHome.z / hDist) * returnSpeed
            };

            float steerRate = 0.28f * gameSpeed;
            b.velocity.x += (desiredVel.x - b.velocity.x) * steerRate;
            b.velocity.y += (desiredVel.y - b.velocity.y) * steerRate;
            b.velocity.z += (desiredVel.z - b.velocity.z) * steerRate;

            float curSpeed = std::sqrtf(b.velocity.x * b.velocity.x + b.velocity.y * b.velocity.y + b.velocity.z * b.velocity.z);
            if (curSpeed > 0.001f) {
                b.velocity.x = (b.velocity.x / curSpeed) * returnSpeed;
                b.velocity.y = (b.velocity.y / curSpeed) * returnSpeed;
                b.velocity.z = (b.velocity.z / curSpeed) * returnSpeed;
            }

            b.position.x += b.velocity.x * gameSpeed;
            b.position.y += b.velocity.y * gameSpeed;
            b.position.z += b.velocity.z * gameSpeed;

            float vLen = std::sqrtf(b.velocity.x * b.velocity.x + b.velocity.y * b.velocity.y + b.velocity.z * b.velocity.z);
            if (vLen > 0.001f) {
                lookDir = { b.velocity.x / vLen, b.velocity.y / vLen, b.velocity.z / vLen };
            }
            break;
        }
        }

        b.lastLookDir = lookDir;

        // 姿勢の計算（OBJ先端が-Zなので180度反転して先端を進行方向に向ける）
        float dirLen = std::sqrtf(lookDir.x * lookDir.x + lookDir.y * lookDir.y + lookDir.z * lookDir.z);
        if (dirLen > 0.0001f) {
            lookDir.x /= dirLen; lookDir.y /= dirLen; lookDir.z /= dirLen;
        }

        float yaw = std::atan2f(lookDir.x, lookDir.z) + 3.14159265f;
        float xzLen = std::sqrtf(lookDir.x * lookDir.x + lookDir.z * lookDir.z);
        float pitch = std::atan2f(lookDir.y, xzLen);

        // 飛翔時・待機時の動的スケーリング（飛翔時は迫力ある大型化＋遠方視認性向上、帰還時は自機に近づくにつれて滑らかに通常サイズへ縮小）
        float targetScaleVal = 0.45f;
        if (b.state == BitFlightState::RETURNING) {
            // 自機の待機ステーションまでの距離に応じて 1.10f から 0.45f へ滑らかに縮小
            Vector3 toHome = { homeStation.x - b.position.x, homeStation.y - b.position.y, homeStation.z - b.position.z };
            float hDist = std::sqrtf(toHome.x * toHome.x + toHome.y * toHome.y + toHome.z * toHome.z);
            float t = std::clamp(hDist / 25.0f, 0.0f, 1.0f);
            targetScaleVal = 0.45f + (1.10f - 0.45f) * t;
        } else if (b.state != BitFlightState::IDLE_HOVER) {
            // 攻撃飛翔時は基本 1.1f、さらに奥へ進むほど遠近感の縮小を補正してしっかり見えるサイズに
            float dz = (std::max)(0.0f, b.position.z - playerPos.z);
            targetScaleVal = 1.10f + std::clamp(dz * 0.012f, 0.0f, 0.90f);
        }
        b.currentScale += (targetScaleVal - b.currentScale) * 0.22f * gameSpeed;

        // 1. 本体のトランスフォーム更新
        if (b.bodyObject) {
            b.bodyObject->SetScale({ b.currentScale, b.currentScale, b.currentScale });
            b.bodyObject->SetTranslate(b.position);
            b.bodyObject->SetRotation({ pitch, yaw, 0.0f });
            b.bodyObject->Update();
        }

        // 2. コアのトランスフォーム更新（ローカルY軸自転）
        if (b.coreObject) {
            b.coreObject->SetRotation({ 0.0f, b.coreRotY, 0.0f });
            b.coreObject->Update();
        }

        // 3. リング1のトランスフォーム更新（ローカルZ軸自転）
        if (b.ring1Object) {
            b.ring1Object->SetRotation({ 0.0f, 0.0f, b.ring1RotZ });
            b.ring1Object->Update();
        }

        // 4. リング2のトランスフォーム更新（ローカルZ軸逆回転）
        if (b.ring2Object) {
            b.ring2Object->SetRotation({ 0.0f, 0.0f, b.ring2RotZ });
            b.ring2Object->Update();
        }
    }

    for (int i = 0; i < MAX_TARGETS; ++i) {
        if (lockOnReticles_[i]) lockOnReticles_[i]->Update();
    }
}

void CyberBitWeapon::Attack(Input* input,
                            std::list<std::unique_ptr<PlayerBullet>>& bullets,
                            std::list<std::unique_ptr<PlayerMissile>>& /*missiles*/,
                            const std::list<std::unique_ptr<Enemy>>& enemies,
                            ArmoredTrainBoss* boss,
                            Object3d* parentCamera,
                            float gameSpeed) {
    if (!input || !owner_) return;

    Object3d* playerObj = owner_->GetObject3d();
    if (!playerObj) return;

    const Matrix4x4& mat = playerObj->GetmatWorld();
    Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
    Vector3 forwardVec = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
    float fLen = std::sqrt(forwardVec.x * forwardVec.x + forwardVec.y * forwardVec.y + forwardVec.z * forwardVec.z);
    if (fLen > 0.0001f) {
        forwardVec.x /= fLen; forwardVec.y /= fLen; forwardVec.z /= fLen;
    }
    Vector3 rightVec = { mat.m[0][0], mat.m[0][1], mat.m[0][2] };
    Vector3 upVec = { mat.m[1][0], mat.m[1][1], mat.m[1][2] };

    bool isSpaceHolding = input->PushKey(DIK_SPACE);
    int allowedLocks = (std::min)(maxSimultaneous_, ammo_);

    // -------------------------------------------------------------
    // 1. スペース押し込み中：マルチロックオン探索・追跡
    // -------------------------------------------------------------
    if (isSpaceHolding && reloadTimer_ <= 0.0f && ammo_ > 0) {
        // 通常敵スキャン
        for (const auto& enemy : enemies) {
            if (!enemy || enemy->IsDead() || !enemy->IsActive()) continue;
            if (static_cast<int>(multiLockedEnemies_.size()) >= allowedLocks) break;

            Vector3 ePos = enemy->GetColliderCenter();
            float dz = ePos.z - playerPos.z;
            if (dz > 0.0f && dz < lockOnMaxDistance_) {
                float dx = ePos.x - playerPos.x;
                float dy = ePos.y - playerPos.y;
                float distSq = dx * dx + dy * dy + dz * dz;

                // 視野角チェック（正面60度以内）
                float dot = (dx * forwardVec.x + dy * forwardVec.y + dz * forwardVec.z) / std::sqrt(distSq);
                if (dot > 0.65f) {
                    bool alreadyLocked = false;
                    for (const auto& t : multiLockedEnemies_) {
                        if (t.enemy == enemy.get()) {
                            alreadyLocked = true;
                            break;
                        }
                    }

                    if (!alreadyLocked && lockOnDelayTimer_ <= 0.0f) {
                        multiLockedEnemies_.push_back({ enemy.get(), nullptr, -1, 0.0f });
                        lockOnDelayTimer_ = lockOnInterval_;
                        // SEは鳴らさない
                    }
                }
            }
        }

        // 装甲列車ボス部位スキャン
        if (boss && boss->IsActive() && static_cast<int>(multiLockedEnemies_.size()) < allowedLocks) {
            const auto& carriages = boss->GetCarriages();
            for (size_t c = 0; c < carriages.size(); ++c) {
                if (carriages[c].isDestroyed) continue;
                if (static_cast<int>(multiLockedEnemies_.size()) >= allowedLocks) break;

                Vector3 cPos = carriages[c].position;
                float dz = cPos.z - playerPos.z;
                if (dz > -10.0f && dz < lockOnMaxDistance_) {
                    float dx = cPos.x - playerPos.x;
                    float dy = cPos.y - playerPos.y;
                    float distSq = dx * dx + dy * dy + dz * dz;

                    float dot = (dx * forwardVec.x + dy * forwardVec.y + dz * forwardVec.z) / std::sqrt(distSq);
                    if (dot > 0.50f) {
                        bool alreadyLocked = false;
                        for (const auto& t : multiLockedEnemies_) {
                            if (t.boss == boss && t.carriageIndex == static_cast<int>(c)) {
                                alreadyLocked = true;
                                break;
                            }
                        }

                        if (!alreadyLocked && lockOnDelayTimer_ <= 0.0f) {
                            multiLockedEnemies_.push_back({ nullptr, boss, static_cast<int>(c), 0.0f });
                            lockOnDelayTimer_ = lockOnInterval_;
                            // SEは鳴らさない
                        }
                    }
                }
            }
        }
    }

    // -------------------------------------------------------------
    // 2. ロックオンレティクル描画トランスフォーム更新
    // -------------------------------------------------------------
    for (int i = 0; i < MAX_TARGETS; ++i) {
        if (!lockOnReticles_[i]) continue;

        bool isValidTarget = false;
        Vector3 targetPos = { 0.0f, 0.0f, 0.0f };

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

            float dz = targetPos.z - playerPos.z;
            float baseScale = 2.0f + (std::max)(0.0f, dz) * 0.02f;
            float scale = baseScale + (baseScale * 0.15f) * std::sinf(lockOnAnimTimer_ * 2.0f);
            lockOnReticles_[i]->SetScale({ scale, scale, scale });
            lockOnReticles_[i]->SetTranslate(targetPos);

            if (!isLockCompleted) {
                float duration = (std::max)(1.0f, lockOnCompleteTime_);
                float rotationAngle = (lockTime / duration) * 6.2831853f;
                lockOnReticles_[i]->SetRotation({ 0.0f, 0.0f, rotationAngle });
            } else {
                lockOnReticles_[i]->SetRotation({ 0.0f, 0.0f, 0.0f });
            }
        } else {
            lockOnReticles_[i]->SetScale({ 0.001f, 0.001f, 0.001f });
        }
    }

    // -------------------------------------------------------------
    // 3. スペースキー解放時：ファンネル端末自体が弧を描いて一斉突翔！
    // -------------------------------------------------------------
    if (!isSpaceHolding && spaceKeyWasPressed_ && !multiLockedEnemies_.empty() && reloadTimer_ <= 0.0f) {
        int launchedCount = 0;

        // 各端末の初期外向き初速パラメータ（自機から外側へ美しく弧を描いて広がる）
        const struct ArcDir { float r; float u; float f; } kArcDirs[BIT_COUNT] = {
            { -1.5f,  1.0f, 1.4f }, // 0: 左上へ美しく広がる
            {  1.5f,  1.0f, 1.4f }, // 1: 右上へ美しく広がる
            { -1.8f, -0.6f, 1.2f }, // 2: 左下へ美しく広がる
            {  1.8f, -0.6f, 1.2f }  // 3: 右下へ美しく広がる
        };

        for (size_t i = 0; i < multiLockedEnemies_.size() && i < BIT_COUNT; ++i) {
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

            auto& b = bits_[i];
            b.state = BitFlightState::ARC_OUTWARD;
            b.flightTimer = 0.0f;
            b.hasHitTarget = false;
            b.targetEnemy = t.enemy;
            b.targetBoss = t.boss;
            b.targetCarriageIndex = t.carriageIndex;

            // 外側へ大きく弧を描く初期速度を付与
            b.velocity = {
                forwardVec.x * kArcDirs[i].f + rightVec.x * kArcDirs[i].r + upVec.x * kArcDirs[i].u,
                forwardVec.y * kArcDirs[i].f + rightVec.y * kArcDirs[i].r + upVec.y * kArcDirs[i].u,
                forwardVec.z * kArcDirs[i].f + rightVec.z * kArcDirs[i].r + upVec.z * kArcDirs[i].u
            };

            launchedCount++;
            if (i < lockOnReticles_.size() && lockOnReticles_[i]) {
                lockOnReticles_[i]->SetScale({ 0.001f, 0.001f, 0.001f });
            }
        }

        if (launchedCount > 0) {
            ammo_ -= launchedCount;
            if (ammo_ < 0) ammo_ = 0;
            // SEは鳴らさない
        }

        multiLockedEnemies_.clear();
        reloadTimer_ = reloadTime_;
    }
    // ロックオンなしでタップ（短押し）された場合は自機主砲（通常弾）を発射（SEなし）
    else if (!isSpaceHolding && spaceKeyWasPressed_ && multiLockedEnemies_.empty()) {
        if (normalShotTimer_ <= 0.0f) {
            normalShotTimer_ = 14.0f;
            Vector3 targetPos = owner_->GetReticleWorldPosition();
            float bSpeed = owner_->GetBulletSpeed();
            Enemy* hoverEnemy = owner_->GetReticleHoverEnemy();

            Vector3 muzzlePos = { playerPos.x + forwardVec.x * 2.2f, playerPos.y + forwardVec.y * 2.2f, playerPos.z + forwardVec.z * 2.2f };
            Vector3 dir = { targetPos.x - muzzlePos.x, targetPos.y - muzzlePos.y, targetPos.z - muzzlePos.z };
            float len = std::sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            if (len > 0.0001f) {
                dir.x /= len; dir.y /= len; dir.z /= len;
            }
            Vector3 velocity = { dir.x * bSpeed, dir.y * bSpeed, dir.z * bSpeed };

            auto newBullet = std::make_unique<PlayerBullet>();
            newBullet->Initialize(object3dCommon_, muzzlePos, velocity, parentCamera, hoverEnemy);
            bullets.push_back(std::move(newBullet));
            // SEは鳴らさない
        }
    }

    spaceKeyWasPressed_ = isSpaceHolding;

    // 死亡ターゲットの除去
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

void CyberBitWeapon::Draw() {
    // 4基のファンネル端末を描画（待機中・弧を描いて飛翔中・帰還中すべて描画）
    for (const auto& b : bits_) {
        if (b.bodyObject) b.bodyObject->Draw();
        if (b.coreObject) b.coreObject->Draw();
        if (b.ring1Object) b.ring1Object->Draw();
        if (b.ring2Object) b.ring2Object->Draw();
    }

    // マルチロックオン照準枠を描画
    for (int i = 0; i < MAX_TARGETS; ++i) {
        if (lockOnReticles_[i]) {
            lockOnReticles_[i]->Draw();
        }
    }
}

void CyberBitWeapon::DrawUI() {
}

bool CyberBitWeapon::IsDepleted() const {
    if (ammo_ > 0 || !multiLockedEnemies_.empty()) return false;
    for (const auto& b : bits_) {
        if (b.state != BitFlightState::IDLE_HOVER) return false;
    }
    return true;
}

std::string CyberBitWeapon::GetAmmoText() const {
    return "CYBER BIT " + std::to_string(ammo_) + " / " + std::to_string(maxAmmo_);
}
