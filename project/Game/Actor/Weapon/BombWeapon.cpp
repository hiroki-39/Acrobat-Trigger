#include "BombWeapon.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Enemy/Enemy.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "KHEngine/Input/Input.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include <cmath>
#include <algorithm>

void BombWeapon::Initialize(Player* owner, Object3dCommon* object3dCommon) {
    owner_ = owner;
    object3dCommon_ = object3dCommon;
    ammo_ = 3;
    maxAmmo_ = 3;
    cooldownTimer_ = 0.0f;

    ModelManager::GetInstance()->LoadModel("collider_sphere_player.obj");

    // 事前オブジェクトプールの初期化（最大3発分をあらかじめ生成して動的確保/解放を根絶）
    bombPool_.clear();
    bombPool_.resize(kMaxBombPool);

    for (size_t i = 0; i < kMaxBombPool; ++i) {
        auto& slot = bombPool_[i];
        slot.isActive = false;
        slot.isExploding = false;

        // ボム弾頭モデル（高輝度ネオンエネルギー弾）
        slot.bombObject = std::make_unique<Object3d>();
        slot.bombObject->Initialize(object3dCommon_);
        slot.bombObject->SetModel("collider_sphere_player.obj");
        slot.bombObject->SetEnableLighting(false);
        slot.bombObject->SetSelectLightings(0);
        slot.bombObject->SetColor({ 1.0f, 0.28f, 0.12f, 1.0f });
        slot.bombObject->SetScale({ 1.3f, 1.3f, 1.6f });

        // 単一衝撃波ドーム（Z-Fightingを根絶したクリーンなエネルギー衝撃波）
        slot.shockwaveObject = std::make_unique<Object3d>();
        slot.shockwaveObject->Initialize(object3dCommon_);
        slot.shockwaveObject->SetModel("collider_sphere_player.obj");
        slot.shockwaveObject->SetEnableLighting(false);
        slot.shockwaveObject->SetSelectLightings(0);
        slot.shockwaveObject->SetColor({ 1.0f, 0.55f, 0.15f, 0.75f });
        slot.shockwaveObject->SetScale({ 0.001f, 0.001f, 0.001f });
    }
}

void BombWeapon::Reset() {
    ammo_ = maxAmmo_;
    cooldownTimer_ = 0.0f;
    for (auto& slot : bombPool_) {
        slot.isActive = false;
        slot.isExploding = false;
        slot.explodeTimer = 0.0f;
        slot.flightDistance = 0.0f;
    }
}

bool BombWeapon::IsDepleted() const {
    if (ammo_ > 0) return false;
    for (const auto& slot : bombPool_) {
        if (slot.isActive) return false;
    }
    return true;
}

bool BombWeapon::HasActiveExplosion(Vector3* outPos, float* outRadius) const {
    for (const auto& b : bombPool_) {
        if (b.isActive && b.isExploding) {
            float t = std::clamp(b.explodeTimer / b.maxExplodeTime, 0.0f, 1.0f);
            float curR = b.explodeDamageRadius * std::sinf(t * 1.5707963f);
            if (outPos) *outPos = b.position;
            if (outRadius) *outRadius = curR;
            return true;
        }
    }
    return false;
}

void BombWeapon::Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& enemies, ArmoredTrainBoss* boss, Object3d* /*parentCamera*/) {
    if (cooldownTimer_ > 0.0f) {
        cooldownTimer_ -= gameSpeed;
    }

    for (auto& b : bombPool_) {
        if (!b.isActive) continue;

        if (b.armingTimer > 0.0f) {
            b.armingTimer -= gameSpeed;
        }

        if (!b.isExploding) {
            // 前方へのロケット推進飛行
            b.position.x += b.velocity.x * gameSpeed;
            b.position.y += b.velocity.y * gameSpeed;
            b.position.z += b.velocity.z * gameSpeed;

            float moveStep = std::sqrtf(b.velocity.x * b.velocity.x + b.velocity.y * b.velocity.y + b.velocity.z * b.velocity.z) * gameSpeed;
            b.flightDistance += moveStep;

            // 敵との直接接触判定（衝突で即起爆）
            bool hitEnemy = false;
            for (const auto& enemy : enemies) {
                if (enemy && !enemy->IsDead()) {
                    Vector3 ePos = enemy->GetVisualPosition();
                    float dx = ePos.x - b.position.x;
                    float dy = ePos.y - b.position.y;
                    float dz = ePos.z - b.position.z;
                    if (dx * dx + dy * dy + dz * dz <= 25.0f) { // 半径5.0m以内
                        hitEnemy = true;
                        break;
                    }
                }
            }

            if (boss && boss->IsActive()) {
                const auto& carriages = boss->GetCarriages();
                for (const auto& car : carriages) {
                    if (!car.isDestroyed) {
                        float dx = car.position.x - b.position.x;
                        float dy = car.position.y - b.position.y;
                        float dz = car.position.z - b.position.z;
                        if (dx * dx + dy * dy + dz * dz <= 36.0f) {
                            hitEnemy = true;
                            break;
                        }
                    }
                }
            }

            // 最大飛行距離または敵接触で起爆へ移行
            if (hitEnemy || b.flightDistance >= b.maxFlightDistance) {
                b.isExploding = true;
                b.explodeTimer = 0.0f;
                SoundManager::GetInstance()->PlaySE("explosion.mp3", 1.0f);
            }

            if (b.bombObject) {
                b.bombObject->SetTranslate(b.position);
                b.bombObject->Update();
            }
        } else {
            // 爆発中
            b.explodeTimer += gameSpeed;
            float t = std::clamp(b.explodeTimer / b.maxExplodeTime, 0.0f, 1.0f);

            // 視覚エフェクトの制御（カメラを包み込まず、チカチカ・ニアクリップ交差を防ぐ安全な視覚半径）
            float currentVisualR = b.maxVisualRadius * std::sinf(t * 1.5707963f);

            // ゲーム判定（超広範囲の敵・敵弾殲滅判定半径）
            float currentDamageR = b.explodeDamageRadius * std::sinf(t * 1.5707963f);

            // ボム弾頭本体は爆発時に即消滅
            if (b.bombObject) {
                b.bombObject->SetScale({ 0.001f, 0.001f, 0.001f });
            }

            // 外側エネルギー衝撃波ドーム（単一ドームでZ-Fightingを根絶し、滑らかにフェードアウト）
            if (b.shockwaveObject) {
                b.shockwaveObject->SetTranslate(b.position);
                b.shockwaveObject->SetScale({ currentVisualR, currentVisualR, currentVisualR });
                float alpha = (1.0f - t) * 0.70f;
                b.shockwaveObject->SetColor({ 1.0f, 0.45f + 0.35f * (1.0f - t), 0.15f, alpha });
                b.shockwaveObject->Update();
            }

            // 半径内の敵をすべて殲滅（即死判定）
            float rSq = currentDamageR * currentDamageR;
            for (const auto& enemy : enemies) {
                if (enemy && !enemy->IsDead()) {
                    Vector3 ePos = enemy->GetVisualPosition();
                    float dx = ePos.x - b.position.x;
                    float dy = ePos.y - b.position.y;
                    float dz = ePos.z - b.position.z;
                    if (dx * dx + dy * dy + dz * dz <= rSq) {
                        enemy->Kill();
                    }
                }
            }

            // ボスへの範囲ダメージ（各車両へ大ダメージ）
            if (boss && boss->IsActive()) {
                const auto& carriages = boss->GetCarriages();
                for (size_t i = 0; i < carriages.size(); ++i) {
                    if (!carriages[i].isDestroyed) {
                        float dx = carriages[i].position.x - b.position.x;
                        float dy = carriages[i].position.y - b.position.y;
                        float dz = carriages[i].position.z - b.position.z;
                        if (dx * dx + dy * dy + dz * dz <= rSq) {
                            boss->OnDamaged(static_cast<int>(i), 3);
                        }
                    }
                }
            }

            // 爆発完了で非アクティブ化（プールスロットを再利用可能に戻す）
            if (b.explodeTimer >= b.maxExplodeTime) {
                b.isActive = false;
                b.isExploding = false;
            }
        }
    }
}

void BombWeapon::Attack(Input* input,
                        std::list<std::unique_ptr<PlayerBullet>>& /*bullets*/,
                        std::list<std::unique_ptr<PlayerMissile>>& /*missiles*/,
                        const std::list<std::unique_ptr<Enemy>>& /*enemies*/,
                        ArmoredTrainBoss* /*boss*/,
                        Object3d* /*parentCamera*/,
                        float /*gameSpeed*/) {
    if (!input || !owner_) return;

    // スペースキーまたはBキーでボム操作
    bool triggerBomb = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_B);

    if (triggerBomb) {
        // 1. すでに飛行中でセーフティ期間（投下直後約0.4秒）を過ぎた未起爆ボムがあれば、手動で任意起爆！
        for (auto& b : bombPool_) {
            if (b.isActive && !b.isExploding && b.armingTimer <= 0.0f) {
                b.isExploding = true;
                b.explodeTimer = 0.0f;
                SoundManager::GetInstance()->PlaySE("explosion.mp3", 1.0f);
                return;
            }
        }

        // 2. 残弾がありクールダウンが終了していれば新規投下
        if (ammo_ > 0 && cooldownTimer_ <= 0.0f) {
            // プールから空きスロットを検索
            BombSlot* slot = nullptr;
            for (auto& b : bombPool_) {
                if (!b.isActive) {
                    slot = &b;
                    break;
                }
            }

            if (!slot) return; // 空きスロットなし

            ammo_--;
            cooldownTimer_ = 25.0f; // 連打防止インターバル

            Object3d* playerObj = owner_->GetObject3d();
            if (!playerObj) return;

            const Matrix4x4& mat = playerObj->GetmatWorld();
            Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
            Vector3 forwardVec = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
            float fLen = std::sqrt(forwardVec.x * forwardVec.x + forwardVec.y * forwardVec.y + forwardVec.z * forwardVec.z);
            if (fLen > 0.0001f) { forwardVec.x /= fLen; forwardVec.y /= fLen; forwardVec.z /= fLen; }

            Vector3 targetPos = owner_->GetReticleWorldPosition();
            Vector3 muzzlePos = { playerPos.x + forwardVec.x * 3.5f, playerPos.y + forwardVec.y * 3.5f, playerPos.z + forwardVec.z * 3.5f };

            Vector3 dir = { targetPos.x - muzzlePos.x, targetPos.y - muzzlePos.y, targetPos.z - muzzlePos.z };
            float len = std::sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            if (len > 0.0001f) { dir.x /= len; dir.y /= len; dir.z /= len; }
            else { dir = forwardVec; }

            float bombSpeed = 7.0f; // 前方へ高速射出
            Vector3 vel = { dir.x * bombSpeed, dir.y * bombSpeed, dir.z * bombSpeed };

            slot->isActive = true;
            slot->position = muzzlePos;
            slot->velocity = vel;
            slot->flightDistance = 0.0f;
            slot->armingTimer = 25.0f; // 投下後約0.4秒は誤爆防止（通常攻撃連打での自爆を防止）
            slot->isExploding = false;
            slot->explodeTimer = 0.0f;
            slot->explodeDamageRadius = 45.0f;
            slot->maxVisualRadius = 15.0f;

            // 弾頭オブジェクトの初期座標・スケールを即時確定（生成フレームでの原点チラつきを完全根絶）
            if (slot->bombObject) {
                slot->bombObject->SetTranslate(muzzlePos);
                slot->bombObject->SetScale({ 1.3f, 1.3f, 1.6f });
                slot->bombObject->SetColor({ 1.0f, 0.28f, 0.12f, 1.0f });
                slot->bombObject->Update();
            }

            if (slot->shockwaveObject) {
                slot->shockwaveObject->SetTranslate(muzzlePos);
                slot->shockwaveObject->SetScale({ 0.001f, 0.001f, 0.001f });
                slot->shockwaveObject->Update();
            }

            SoundManager::GetInstance()->PlaySE("launcher.mp3", 0.9f);
        }
    }
}

void BombWeapon::Draw() {
    for (auto& b : bombPool_) {
        if (!b.isActive) continue;

        if (!b.isExploding && b.bombObject) {
            b.bombObject->Draw();
        } else if (b.isExploding && b.shockwaveObject) {
            b.shockwaveObject->Draw();
        }
    }
}

void BombWeapon::DrawUI() {
}

std::string BombWeapon::GetAmmoText() const {
    return "BOMB: " + std::to_string(ammo_) + " / " + std::to_string(maxAmmo_);
}
