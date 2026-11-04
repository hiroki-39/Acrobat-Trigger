#include "CyberBitWeapon.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/PlayerMissile.h"
#include "Game/Actor/Enemy/Enemy.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Input/Input.h"
#include <algorithm>
#include <cmath>

void CyberBitWeapon::Initialize(Player* owner, Object3dCommon* object3dCommon) {
    owner_ = owner;
    object3dCommon_ = object3dCommon;
    ammo_ = 24;
    maxAmmo_ = 24;
    maxSimultaneous_ = 4;
    reloadTimer_ = 0.0f;
    lockOnAnimTimer_ = 0.0f;
    lockOnDelayTimer_ = 0.0f;

    ModelManager::GetInstance()->LoadModel("missile.obj");
    ModelManager::GetInstance()->LoadModel("RockOn.obj");

    Object3d* playerObj = owner_->GetObject3d();

    for (int i = 0; i < MAX_TARGETS; ++i) {
        mountedBits_[i] = std::make_unique<Object3d>();
        mountedBits_[i]->Initialize(object3dCommon);
        mountedBits_[i]->SetModel("missile.obj");
        if (playerObj) {
            mountedBits_[i]->SetParent(playerObj);
        }

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

        mountedBits_[i]->SetTranslate({ xOffset, yOffset, zOffset });
        mountedBits_[i]->SetScale({ 1.0f, 1.0f, 1.0f });

        lockOnReticles_[i] = std::make_unique<Object3d>();
        lockOnReticles_[i]->Initialize(object3dCommon);
        lockOnReticles_[i]->SetModel("RockOn.obj");
        if (lockOnReticles_[i]->GetModel()) {
            lockOnReticles_[i]->GetModel()->SetColor({ 0.2f, 0.8f, 1.0f, 1.0f }); // サイバーシアン
        }
        lockOnReticles_[i]->SetScale({ 0.001f, 0.001f, 0.001f });
        lockOnReticles_[i]->SetEnableLighting(false);

        mountedBits_[i]->Update();
        lockOnReticles_[i]->Update();
    }
}

void CyberBitWeapon::Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& /*enemies*/, ArmoredTrainBoss* /*boss*/, Object3d* /*parentCamera*/) {
    if (reloadTimer_ > 0.0f) {
        reloadTimer_ -= gameSpeed;
    }
    if (lockOnDelayTimer_ > 0.0f) {
        lockOnDelayTimer_ -= gameSpeed;
    }
    lockOnAnimTimer_ += 0.1f * gameSpeed;

    for (int i = 0; i < MAX_TARGETS; ++i) {
        if (mountedBits_[i]) mountedBits_[i]->Update();
        if (lockOnReticles_[i]) lockOnReticles_[i]->Update();
    }
}

void CyberBitWeapon::Attack(Input* input,
                            std::list<std::unique_ptr<PlayerBullet>>& /*bullets*/,
                            std::list<std::unique_ptr<PlayerMissile>>& missiles,
                            const std::list<std::unique_ptr<Enemy>>& enemies,
                            ArmoredTrainBoss* boss,
                            Object3d* parentCamera,
                            float gameSpeed) {
    if (!input || !owner_) return;

    Object3d* playerObj = owner_->GetObject3d();
    if (!playerObj) return;

    const Matrix4x4& mat = playerObj->GetmatWorld();
    Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };

    // 最大同時ロック数の判定（残弾数を超えないように制限）
    int allowedLocks = (std::min)(maxSimultaneous_, ammo_);

    // 1. スペース押し込み中：ロックオン探索
    if (input->PushKey(DIK_SPACE) && reloadTimer_ <= 0.0f && ammo_ > 0) {
        // 通常敵スキャン
        for (const auto& enemy : enemies) {
            if (!enemy || enemy->IsDead()) continue;

            auto it = std::find_if(multiLockedEnemies_.begin(), multiLockedEnemies_.end(),
                [&](const LockOnTarget& target) { return target.enemy == enemy.get(); });
            if (it != multiLockedEnemies_.end()) continue;

            if (static_cast<int>(multiLockedEnemies_.size()) >= allowedLocks) break;

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

        // 装甲列車ボスの部位スキャン
        if (boss && boss->IsActive() && !boss->IsDefeated()) {
            const auto& carriages = boss->GetCarriages();
            for (int i = 0; i < static_cast<int>(carriages.size()); ++i) {
                if (boss->IsCarriageDestroyed(i)) continue;

                auto it = std::find_if(multiLockedEnemies_.begin(), multiLockedEnemies_.end(),
                    [&](const LockOnTarget& target) { return target.boss == boss && target.carriageIndex == i; });
                if (it != multiLockedEnemies_.end()) continue;

                if (static_cast<int>(multiLockedEnemies_.size()) >= allowedLocks) break;

                Vector3 carPos = boss->GetCarriagePosition(i);
                float distSq = (carPos.x - playerPos.x) * (carPos.x - playerPos.x) +
                               (carPos.y - playerPos.y) * (carPos.y - playerPos.y) +
                               (carPos.z - playerPos.z) * (carPos.z - playerPos.z);

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

    // 2. ロックオン枠の更新
    for (int i = 0; i < MAX_TARGETS; ++i) {
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

    // 3. スペースキー解放：一斉発射
    if (!input->PushKey(DIK_SPACE) && !multiLockedEnemies_.empty() && reloadTimer_ <= 0.0f) {
        bool hasLaunched = false;
        int firedCount = 0;

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

            const Matrix4x4& mountedMat = mountedBits_[i]->GetmatWorld();
            Vector3 spawnPos = { mountedMat.m[3][0], mountedMat.m[3][1], mountedMat.m[3][2] };
            Vector3 spawnRot = playerObj->GetRotation();

            auto newMissile = std::make_unique<PlayerMissile>();
            newMissile->Initialize(object3dCommon_, spawnPos, spawnRot, { 0, 0, 0 }, parentCamera, t.enemy, t.boss, t.carriageIndex);
            missiles.push_back(std::move(newMissile));

            hasLaunched = true;
            firedCount++;
            lockOnReticles_[i]->SetScale({ 0.001f, 0.001f, 0.001f });
        }

        if (hasLaunched) {
            SoundManager::GetInstance()->PlaySE("launcher.mp3", 0.9f);
            ammo_ -= firedCount;
            if (ammo_ < 0) ammo_ = 0;
        }

        multiLockedEnemies_.clear();
        reloadTimer_ = reloadTime_;
    }

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
    // マウントされているビットの描画（残弾がある時のみ表示）
    if (ammo_ > 0) {
        for (int i = 0; i < maxSimultaneous_; ++i) {
            if (mountedBits_[i]) {
                mountedBits_[i]->Draw();
            }
        }
    }

    // ロックオン照準枠の描画
    for (int i = 0; i < MAX_TARGETS; ++i) {
        if (lockOnReticles_[i]) {
            lockOnReticles_[i]->Draw();
        }
    }
}

void CyberBitWeapon::DrawUI() {
    // UIテキストやゲージ用（必要に応じて）
}

void CyberBitWeapon::Reset() {
    ammo_ = maxAmmo_;
    reloadTimer_ = 0.0f;
    multiLockedEnemies_.clear();
    for (int i = 0; i < MAX_TARGETS; ++i) {
        if (lockOnReticles_[i]) {
            lockOnReticles_[i]->SetScale({ 0.001f, 0.001f, 0.001f });
        }
    }
}

bool CyberBitWeapon::IsDepleted() const {
    return (ammo_ <= 0 && multiLockedEnemies_.empty());
}

float CyberBitWeapon::GetRemainingRatio() const {
    return maxAmmo_ > 0 ? (static_cast<float>(ammo_) / static_cast<float>(maxAmmo_)) : 0.0f;
}

std::string CyberBitWeapon::GetAmmoText() const {
    return std::to_string(ammo_) + " / " + std::to_string(maxAmmo_);
}
