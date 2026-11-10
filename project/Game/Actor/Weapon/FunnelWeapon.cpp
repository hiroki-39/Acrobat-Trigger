#include "FunnelWeapon.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/PlayerBullet.h"
#include "Game/Actor/Enemy/Enemy.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "KHEngine/Input/Input.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include <cmath>
#include <algorithm>

void FunnelWeapon::Initialize(Player* owner, Object3dCommon* object3dCommon) {
    owner_ = owner;
    object3dCommon_ = object3dCommon;
    remainingTime_ = maxTime_ = 900.0f; // 15秒
    playerAttackTimer_ = 0.0f;
    hoverAnimTimer_ = 0.0f;
    lockedEnemy_ = nullptr;
    lockedBossCarriage_ = -1;
    hasTarget_ = false;
    pendingBulletMuzzles_.clear();
    pendingBulletDirs_.clear();

    // 4つのファンネル用モデルパーツをロード
    auto modelMgr = ModelManager::GetInstance();
    modelMgr->LoadModel("resources/3dModels/funnel/funnel_body/funnel_body.obj");
    modelMgr->LoadModel("resources/3dModels/funnel/funnel_core/funnel_core.obj");
    modelMgr->LoadModel("resources/3dModels/funnel/funnel_ring1/funnel_ring1.obj");
    modelMgr->LoadModel("resources/3dModels/funnel/funnel_ring2/funnel_ring2.obj");

    funnels_.clear();
    funnels_.resize(kFunnelCount);

    for (size_t i = 0; i < funnels_.size(); ++i) {
        auto& f = funnels_[i];
        f.state = BitState::IDLE_HOVER;
        f.shotsFired = 0;
        f.maxShots = 3;
        f.shotTimer = 0.0f;
        f.shotInterval = 16.0f;

        // 各端末ごとの回転初期値と速度（バラバラに動く）
        f.coreRotY = static_cast<float>(i) * 1.57f;
        f.coreRotSpeed = 0.06f + static_cast<float>(i) * 0.015f;
        f.ring1RotZ = static_cast<float>(i) * 0.8f;
        f.ring1RotSpeed = 0.08f + static_cast<float>(i) * 0.02f;
        f.ring2RotZ = static_cast<float>(i) * 2.1f;
        f.ring2RotSpeed = -0.07f - static_cast<float>(i) * 0.015f;

        // 1. 本体（外枠フレーム）
        f.bodyObject = std::make_unique<Object3d>();
        f.bodyObject->Initialize(object3dCommon_);
        f.bodyObject->SetModel("resources/3dModels/funnel/funnel_body/funnel_body.obj");
        f.bodyObject->SetEnableLighting(false);
        f.bodyObject->SetSelectLightings(0);
        f.bodyObject->SetColor({ 0.90f, 0.94f, 1.0f, 1.0f }); // クールなサイバーシルバー
        f.bodyObject->SetScale({ 0.32f, 0.32f, 0.32f });

        // 2. コア（内部ピラミッド結晶）
        f.coreObject = std::make_unique<Object3d>();
        f.coreObject->Initialize(object3dCommon_);
        f.coreObject->SetModel("resources/3dModels/funnel/funnel_core/funnel_core.obj");
        f.coreObject->SetEnableLighting(false);
        f.coreObject->SetSelectLightings(0);
        f.coreObject->SetColor({ 1.0f, 0.15f, 0.85f, 1.0f }); // 鮮烈なネオンマゼンタ
        f.coreObject->SetScale({ 1.0f, 1.0f, 1.0f });
        f.coreObject->SetParent(f.bodyObject.get());

        // 3. リング1（第1回転リング）
        f.ring1Object = std::make_unique<Object3d>();
        f.ring1Object->Initialize(object3dCommon_);
        f.ring1Object->SetModel("resources/3dModels/funnel/funnel_ring1/funnel_ring1.obj");
        f.ring1Object->SetEnableLighting(false);
        f.ring1Object->SetSelectLightings(0);
        f.ring1Object->SetColor({ 0.95f, 0.35f, 1.0f, 1.0f }); // ネオンパープルピンク
        f.ring1Object->SetScale({ 1.0f, 1.0f, 1.0f });
        f.ring1Object->SetParent(f.bodyObject.get());

        // 4. リング2（第2逆回転リング）
        f.ring2Object = std::make_unique<Object3d>();
        f.ring2Object->Initialize(object3dCommon_);
        f.ring2Object->SetModel("resources/3dModels/funnel/funnel_ring2/funnel_ring2.obj");
        f.ring2Object->SetEnableLighting(false);
        f.ring2Object->SetSelectLightings(0);
        f.ring2Object->SetColor({ 0.35f, 0.85f, 1.0f, 1.0f }); // ネオンサイバーシアン
        f.ring2Object->SetScale({ 1.0f, 1.0f, 1.0f });
        f.ring2Object->SetParent(f.bodyObject.get());
    }

    Reset();
}

void FunnelWeapon::Reset() {
    remainingTime_ = maxTime_;
    playerAttackTimer_ = 0.0f;
    hoverAnimTimer_ = 0.0f;
    lockedEnemy_ = nullptr;
    lockedBossCarriage_ = -1;
    hasTarget_ = false;
    pendingBulletMuzzles_.clear();
    pendingBulletDirs_.clear();

    if (owner_ && owner_->GetObject3d()) {
        const Matrix4x4& mat = owner_->GetObject3d()->GetmatWorld();
        Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
        for (auto& f : funnels_) {
            f.state = BitState::IDLE_HOVER;
            f.position = playerPos;
            f.shotsFired = 0;
            f.shotTimer = 0.0f;
        }
    }
}

bool FunnelWeapon::FindBestTarget(const std::list<std::unique_ptr<Enemy>>& enemies, ArmoredTrainBoss* boss, const Vector3& playerPos, Vector3& outTargetPos, const Enemy*& outEnemy, int& outCarriage) {
    float closestDistSq = 180.0f * 180.0f; // 索敵範囲 180m
    bool found = false;

    // 1. 通常敵の索敵（自機前方、最も近い生きている敵）
    for (const auto& enemy : enemies) {
        if (enemy && !enemy->IsDead()) {
            Vector3 ePos = enemy->GetVisualPosition();
            float dz = ePos.z - playerPos.z;
            if (dz > -5.0f) { // 前方
                float dx = ePos.x - playerPos.x;
                float dy = ePos.y - playerPos.y;
                float distSq = dx * dx + dy * dy + dz * dz;
                if (distSq < closestDistSq) {
                    closestDistSq = distSq;
                    outTargetPos = ePos;
                    outEnemy = enemy.get();
                    outCarriage = -1;
                    found = true;
                }
            }
        }
    }

    // 2. ボス部位の索敵
    if (!found && boss && boss->IsActive()) {
        const auto& carriages = boss->GetCarriages();
        for (size_t i = 0; i < carriages.size(); ++i) {
            if (!carriages[i].isDestroyed) {
                Vector3 cPos = carriages[i].position;
                float dx = cPos.x - playerPos.x;
                float dy = cPos.y - playerPos.y;
                float dz = cPos.z - playerPos.z;
                if (dz > -10.0f) {
                    float distSq = dx * dx + dy * dy + dz * dz;
                    if (distSq < closestDistSq) {
                        closestDistSq = distSq;
                        outTargetPos = cPos;
                        outEnemy = nullptr;
                        outCarriage = static_cast<int>(i);
                        found = true;
                    }
                }
            }
        }
    }

    return found;
}

void FunnelWeapon::Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& enemies, ArmoredTrainBoss* boss, Object3d* /*parentCamera*/) {
    remainingTime_ -= gameSpeed;
    if (remainingTime_ < 0.0f) remainingTime_ = 0.0f;

    if (playerAttackTimer_ > 0.0f) {
        playerAttackTimer_ -= gameSpeed;
    }

    hoverAnimTimer_ += 0.05f * gameSpeed;

    if (!owner_ || !owner_->GetObject3d()) return;
    const Matrix4x4& pMat = owner_->GetObject3d()->GetmatWorld();
    Vector3 playerPos = { pMat.m[3][0], pMat.m[3][1], pMat.m[3][2] };
    Vector3 pForward = { pMat.m[2][0], pMat.m[2][1], pMat.m[2][2] };
    Vector3 pUp = { pMat.m[1][0], pMat.m[1][1], pMat.m[1][2] };
    Vector3 pRight = { pMat.m[0][0], pMat.m[0][1], pMat.m[0][2] };

    // 自機周囲での待機オフセット（4基のフォーメーション）
    const Vector3 kIdleOffsets[kFunnelCount] = {
        { -2.4f,  1.2f, -1.0f }, // 0: 左上翼端
        {  2.4f,  1.2f, -1.0f }, // 1: 右上翼端
        { -2.8f, -0.6f, -1.8f }, // 2: 左下後方
        {  2.8f, -0.6f, -1.8f }  // 3: 右下後方
    };

    // 包囲配置オフセット（敵の周囲4箇所）
    const Vector3 kSurroundOffsets[kFunnelCount] = {
        {  0.0f,  3.6f,  0.5f }, // 0: 敵の上空
        {  3.6f,  0.0f, -1.0f }, // 1: 敵の右
        {  0.0f, -3.0f, -1.0f }, // 2: 敵の下
        { -3.6f,  0.0f,  0.5f }  // 3: 敵の左
    };

    // ターゲットの生存確認
    if (hasTarget_) {
        bool targetDead = false;
        if (lockedEnemy_) {
            if (lockedEnemy_->IsDead()) targetDead = true;
            else lockedTargetPos_ = lockedEnemy_->GetVisualPosition();
        } else if (lockedBossCarriage_ >= 0 && boss && boss->IsActive()) {
            const auto& carriages = boss->GetCarriages();
            if (lockedBossCarriage_ < static_cast<int>(carriages.size())) {
                if (carriages[lockedBossCarriage_].isDestroyed) targetDead = true;
                else lockedTargetPos_ = carriages[lockedBossCarriage_].position;
            } else {
                targetDead = true;
            }
        } else {
            targetDead = true;
        }

        if (targetDead) {
            hasTarget_ = false;
            lockedEnemy_ = nullptr;
            lockedBossCarriage_ = -1;
            // ターゲット死亡時、攻撃中・接近中だった端末は帰還へ
            for (auto& f : funnels_) {
                if (f.state == BitState::APPROACH || f.state == BitState::ATTACKING) {
                    f.state = BitState::RETURNING;
                }
            }
        }
    }

    // 全端末が自機周囲に待機しているかチェックし、新しいターゲットを索敵
    bool allAtHome = true;
    for (const auto& f : funnels_) {
        if (f.state != BitState::IDLE_HOVER) {
            allAtHome = false;
            break;
        }
    }

    if (allAtHome && !hasTarget_) {
        Vector3 newTargetPos;
        const Enemy* newEnemy = nullptr;
        int newCarriage = -1;
        if (FindBestTarget(enemies, boss, playerPos, newTargetPos, newEnemy, newCarriage)) {
            hasTarget_ = true;
            lockedTargetPos_ = newTargetPos;
            lockedEnemy_ = newEnemy;
            lockedBossCarriage_ = newCarriage;

            // 4基一斉に出撃！
            for (size_t i = 0; i < funnels_.size(); ++i) {
                funnels_[i].state = BitState::APPROACH;
                funnels_[i].shotsFired = 0;
                funnels_[i].shotTimer = 8.0f + static_cast<float>(i) * 6.0f; // 到着後に順繰りに射撃開始
            }
            SoundManager::GetInstance()->PlaySE("shot.mp3", 0.7f);
        }
    }

    // 各ファンネルのステートマシン更新
    for (size_t i = 0; i < funnels_.size(); ++i) {
        auto& f = funnels_[i];

        // 各パーツの自転更新（それぞれバラバラに回転）
        f.coreRotY += f.coreRotSpeed * gameSpeed;
        f.ring1RotZ += f.ring1RotSpeed * gameSpeed;
        f.ring2RotZ += f.ring2RotSpeed * gameSpeed;

        Vector3 destPos = playerPos;
        Vector3 lookDir = pForward;
        float moveRate = 0.16f;

        // 自機の待機目標座標を計算
        float bob = std::sinf(hoverAnimTimer_ * 2.2f + static_cast<float>(i) * 1.57f) * 0.35f;
        Vector3 homeStationPos = {
            playerPos.x + pRight.x * kIdleOffsets[i].x + pUp.x * (kIdleOffsets[i].y + bob) + pForward.x * kIdleOffsets[i].z,
            playerPos.y + pRight.y * kIdleOffsets[i].x + pUp.y * (kIdleOffsets[i].y + bob) + pForward.y * kIdleOffsets[i].z,
            playerPos.z + pRight.z * kIdleOffsets[i].x + pUp.z * (kIdleOffsets[i].y + bob) + pForward.z * kIdleOffsets[i].z
        };

        switch (f.state) {
        case BitState::IDLE_HOVER: {
            destPos = homeStationPos;
            lookDir = pForward;
            moveRate = 0.20f;
            break;
        }

        case BitState::APPROACH: {
            // 敵の包囲位置へ高速飛翔
            destPos = {
                lockedTargetPos_.x + kSurroundOffsets[i].x,
                lockedTargetPos_.y + kSurroundOffsets[i].y,
                lockedTargetPos_.z + kSurroundOffsets[i].z
            };

            // 進行方向を向く
            Vector3 toDest = { destPos.x - f.position.x, destPos.y - f.position.y, destPos.z - f.position.z };
            float dist = std::sqrtf(toDest.x * toDest.x + toDest.y * toDest.y + toDest.z * toDest.z);
            if (dist > 0.001f) {
                lookDir = { toDest.x / dist, toDest.y / dist, toDest.z / dist };
            }

            moveRate = 0.26f; // 超音速アプローチ

            // 包囲位置に接近したら攻撃ステートへ
            if (dist < 2.5f) {
                f.state = BitState::ATTACKING;
            }
            break;
        }

        case BitState::ATTACKING: {
            // 敵の周囲で少し旋回しながら位置キープ
            float orbit = hoverAnimTimer_ * 1.5f + static_cast<float>(i) * 1.57f;
            destPos = {
                lockedTargetPos_.x + kSurroundOffsets[i].x + std::cosf(orbit) * 1.2f,
                lockedTargetPos_.y + kSurroundOffsets[i].y + std::sinf(orbit) * 0.8f,
                lockedTargetPos_.z + kSurroundOffsets[i].z
            };

            // 敵の中心を向く
            Vector3 toEnemy = { lockedTargetPos_.x - f.position.x, lockedTargetPos_.y - f.position.y, lockedTargetPos_.z - f.position.z };
            float eDist = std::sqrtf(toEnemy.x * toEnemy.x + toEnemy.y * toEnemy.y + toEnemy.z * toEnemy.z);
            if (eDist > 0.001f) {
                lookDir = { toEnemy.x / eDist, toEnemy.y / eDist, toEnemy.z / eDist };
            }

            moveRate = 0.18f;

            // 射撃シーケンス
            f.shotTimer -= gameSpeed;
            if (f.shotTimer <= 0.0f) {
                f.shotTimer = f.shotInterval;
                f.shotsFired++;

                // ファンネル先端（小型サイズ対応）から敵へ弾を発射
                Vector3 muzzlePos = {
                    f.position.x + lookDir.x * 0.45f,
                    f.position.y + lookDir.y * 0.45f,
                    f.position.z + lookDir.z * 0.45f
                };
                pendingBulletMuzzles_.push_back(muzzlePos);
                pendingBulletDirs_.push_back(lookDir);

                // 所定回数（3発）撃ち終わったら帰還へ
                if (f.shotsFired >= f.maxShots) {
                    f.state = BitState::RETURNING;
                }
            }
            break;
        }

        case BitState::RETURNING: {
            // 自機待機位置へ高速帰還
            destPos = homeStationPos;
            Vector3 toHome = { destPos.x - f.position.x, destPos.y - f.position.y, destPos.z - f.position.z };
            float hDist = std::sqrtf(toHome.x * toHome.x + toHome.y * toHome.y + toHome.z * toHome.z);
            if (hDist > 0.001f) {
                lookDir = { toHome.x / hDist, toHome.y / hDist, toHome.z / hDist };
            }

            moveRate = 0.28f; // スピーディな帰還

            // 待機位置に接近したら待機モードへ合流
            if (hDist < 1.4f) {
                f.state = BitState::IDLE_HOVER;
                f.shotsFired = 0;
            }
            break;
        }
        }

        // 位置のイージング移動
        f.position.x += (destPos.x - f.position.x) * (moveRate * gameSpeed);
        f.position.y += (destPos.y - f.position.y) * (moveRate * gameSpeed);
        f.position.z += (destPos.z - f.position.z) * (moveRate * gameSpeed);

        // 姿勢の計算（lookDirを向く、OBJ先端が-Zなので180度反転して先端を進行方向に向ける）
        float dirLen = std::sqrtf(lookDir.x * lookDir.x + lookDir.y * lookDir.y + lookDir.z * lookDir.z);
        if (dirLen > 0.0001f) {
            lookDir.x /= dirLen; lookDir.y /= dirLen; lookDir.z /= dirLen;
        }

        float yaw = std::atan2f(lookDir.x, lookDir.z) + 3.14159265f;
        float xzLen = std::sqrtf(lookDir.x * lookDir.x + lookDir.z * lookDir.z);
        float pitch = std::atan2f(lookDir.y, xzLen);

        // 1. 本体のトランスフォーム更新
        if (f.bodyObject) {
            f.bodyObject->SetTranslate(f.position);
            f.bodyObject->SetRotation({ pitch, yaw, 0.0f });
            f.bodyObject->Update();
        }

        // 2. コアのトランスフォーム更新（ローカルY軸回転＋わずかなX揺らぎ）
        if (f.coreObject) {
            f.coreObject->SetRotation({ 0.0f, f.coreRotY, 0.0f });
            f.coreObject->Update();
        }

        // 3. リング1のトランスフォーム更新（ローカルZ軸回転＝Blenderの前後軸Y回転）
        if (f.ring1Object) {
            f.ring1Object->SetRotation({ 0.0f, 0.0f, f.ring1RotZ });
            f.ring1Object->Update();
        }

        // 4. リング2のトランスフォーム更新（ローカルZ軸逆回転）
        if (f.ring2Object) {
            f.ring2Object->SetRotation({ 0.0f, 0.0f, f.ring2RotZ });
            f.ring2Object->Update();
        }
    }
}

void FunnelWeapon::Attack(Input* input,
                          std::list<std::unique_ptr<PlayerBullet>>& bullets,
                          std::list<std::unique_ptr<PlayerMissile>>& /*missiles*/,
                          const std::list<std::unique_ptr<Enemy>>& /*enemies*/,
                          ArmoredTrainBoss* /*boss*/,
                          Object3d* parentCamera,
                          float /*gameSpeed*/) {
    // 1. ファンネルが放ったビームの実体化（敵へ向かって飛ぶ高精度レーザー）
    if (!pendingBulletMuzzles_.empty()) {
        float fSpeed = 13.0f; // 高速で敵に着弾
        for (size_t i = 0; i < pendingBulletMuzzles_.size(); ++i) {
            Vector3 mPos = pendingBulletMuzzles_[i];
            Vector3 dir = pendingBulletDirs_[i];
            Vector3 vel = { dir.x * fSpeed, dir.y * fSpeed, dir.z * fSpeed };

            auto bullet = std::make_unique<PlayerBullet>();
            bullet->Initialize(object3dCommon_, mPos, vel, parentCamera, nullptr);
            bullet->SetDamage(0.35f); // 攻撃力を約1/3に下げ、敵の一撃死を防止（複数発ヒットで撃破）
            bullet->SetBulletColor({ 1.0f, 0.18f, 0.88f, 1.0f }); // 鮮烈なサイバーマゼンタ
            bullet->SetBulletScale({ 0.45f, 0.45f, 6.0f }); // 小型端末にマッチしたシャープなニードルレーザー
            bullets.push_back(std::move(bullet));
        }
        SoundManager::GetInstance()->PlaySE("shot.mp3", 0.65f);
        pendingBulletMuzzles_.clear();
        pendingBulletDirs_.clear();
    }

    // 2. プレイヤー自身の主砲射撃（スペースキー長押しで手動連射可能）
    if (input && owner_ && input->PushKey(DIK_SPACE)) {
        if (playerAttackTimer_ <= 0.0f) {
            playerAttackTimer_ = 12.0f;

            Object3d* playerObj = owner_->GetObject3d();
            if (!playerObj) return;

            const Matrix4x4& mat = playerObj->GetmatWorld();
            Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
            Vector3 forwardVec = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
            float fLen = std::sqrt(forwardVec.x * forwardVec.x + forwardVec.y * forwardVec.y + forwardVec.z * forwardVec.z);
            if (fLen > 0.0001f) { forwardVec.x /= fLen; forwardVec.y /= fLen; forwardVec.z /= fLen; }

            Vector3 targetPos = owner_->GetReticleWorldPosition();
            Vector3 muzzlePos = { playerPos.x + forwardVec.x * 2.5f, playerPos.y + forwardVec.y * 2.5f, playerPos.z + forwardVec.z * 2.5f };

            Vector3 dir = { targetPos.x - muzzlePos.x, targetPos.y - muzzlePos.y, targetPos.z - muzzlePos.z };
            float len = std::sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            if (len > 0.0001f) { dir.x /= len; dir.y /= len; dir.z /= len; }
            else { dir = forwardVec; }

            float bSpeed = 9.5f;
            Vector3 vel = { dir.x * bSpeed, dir.y * bSpeed, dir.z * bSpeed };

            auto bullet = std::make_unique<PlayerBullet>();
            bullet->Initialize(object3dCommon_, muzzlePos, vel, parentCamera, nullptr);
            bullet->SetDamage(1);
            bullet->SetBulletColor({ 1.0f, 0.25f, 0.85f, 1.0f });
            bullet->SetBulletScale({ 1.35f, 1.35f, 11.0f });
            bullets.push_back(std::move(bullet));

            SoundManager::GetInstance()->PlaySE("shot.mp3", 0.75f);
        }
    }
}

void FunnelWeapon::Draw() {
    for (auto& f : funnels_) {
        if (f.bodyObject)  f.bodyObject->Draw();
        if (f.coreObject)  f.coreObject->Draw();
        if (f.ring1Object) f.ring1Object->Draw();
        if (f.ring2Object) f.ring2Object->Draw();
    }
}

void FunnelWeapon::DrawUI() {
}

std::string FunnelWeapon::GetAmmoText() const {
    int sec = static_cast<int>(std::ceil(remainingTime_ / 60.0f));
    return "FUNNEL: " + std::to_string(sec) + "s";
}
