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
    activeBombs_.clear();

    ModelManager::GetInstance()->LoadModel("collider_sphere_player.obj");
}

void BombWeapon::Reset() {
    ammo_ = maxAmmo_;
    cooldownTimer_ = 0.0f;
    activeBombs_.clear();
}

bool BombWeapon::HasActiveExplosion(Vector3* outPos, float* outRadius) const {
    for (const auto& b : activeBombs_) {
        if (b.isExploding) {
            float t = std::clamp(b.explodeTimer / b.maxExplodeTime, 0.0f, 1.0f);
            float curR = b.explodeRadius * std::sinf(t * 1.5707963f);
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

    for (auto it = activeBombs_.begin(); it != activeBombs_.end();) {
        auto& b = *it;

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
                    if (dx * dx + dy * dy + dz * dz <= 20.0f) { // 半径4.5m以内
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
            ++it;
        } else {
            // 爆発中
            b.explodeTimer += gameSpeed;
            float t = std::clamp(b.explodeTimer / b.maxExplodeTime, 0.0f, 1.0f);

            // 衝撃波の急激な広がり（イーズアウト）
            float currentRadius = b.explodeRadius * std::sinf(t * 1.5707963f);
            float coreRadius = currentRadius * 0.65f;

            // ボム弾頭本体は縮小消滅
            if (b.bombObject) {
                b.bombObject->SetScale({ 0.001f, 0.001f, 0.001f });
            }

            // 外側衝撃波ドーム
            if (b.shockwaveObject) {
                b.shockwaveObject->SetTranslate(b.position);
                b.shockwaveObject->SetScale({ currentRadius, currentRadius, currentRadius });
                float alpha = (1.0f - t) * 0.85f;
                b.shockwaveObject->SetColor({ 1.0f, 0.35f + 0.3f * (1.0f - t), 0.1f, alpha });
                b.shockwaveObject->Update();
            }

            // 内側高輝度ホワイトプラズマコア
            if (b.innerCoreObject) {
                b.innerCoreObject->SetTranslate(b.position);
                b.innerCoreObject->SetScale({ coreRadius, coreRadius, coreRadius });
                float coreAlpha = std::powf(1.0f - t, 2.0f) * 0.95f;
                b.innerCoreObject->SetColor({ 1.0f, 0.95f, 0.75f, coreAlpha });
                b.innerCoreObject->Update();
            }

            // 半径内の敵をすべて殲滅（即死判定）
            float rSq = currentRadius * currentRadius;
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

            // ボスへの範囲ダメージ（各車両へ連続ダメージ）
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

            // 爆発完了で消去
            if (b.explodeTimer >= b.maxExplodeTime) {
                it = activeBombs_.erase(it);
            } else {
                ++it;
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
        // 1. すでに飛行中でセーフティ期間（投下直後）を過ぎた未起爆ボムがあれば、即座に任意起爆！
        for (auto& b : activeBombs_) {
            if (!b.isExploding && b.armingTimer <= 0.0f) {
                b.isExploding = true;
                b.explodeTimer = 0.0f;
                SoundManager::GetInstance()->PlaySE("explosion.mp3", 1.0f);
                return;
            }
        }

        // 2. 残弾があれば新規投下
        if (ammo_ > 0 && cooldownTimer_ <= 0.0f) {
            ammo_--;
            cooldownTimer_ = 20.0f;

            Object3d* playerObj = owner_->GetObject3d();
            if (!playerObj) return;

            const Matrix4x4& mat = playerObj->GetmatWorld();
            Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
            Vector3 forwardVec = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
            float fLen = std::sqrt(forwardVec.x * forwardVec.x + forwardVec.y * forwardVec.y + forwardVec.z * forwardVec.z);
            if (fLen > 0.0001f) { forwardVec.x /= fLen; forwardVec.y /= fLen; forwardVec.z /= fLen; }

            Vector3 targetPos = owner_->GetReticleWorldPosition();
            Vector3 muzzlePos = { playerPos.x + forwardVec.x * 3.0f, playerPos.y + forwardVec.y * 3.0f, playerPos.z + forwardVec.z * 3.0f };

            Vector3 dir = { targetPos.x - muzzlePos.x, targetPos.y - muzzlePos.y, targetPos.z - muzzlePos.z };
            float len = std::sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            if (len > 0.0001f) { dir.x /= len; dir.y /= len; dir.z /= len; }
            else { dir = forwardVec; }

            float bombSpeed = 7.0f; // スピーディかつ力強く前方へ射出
            Vector3 vel = { dir.x * bombSpeed, dir.y * bombSpeed, dir.z * bombSpeed };

            ActiveBomb newBomb;
            newBomb.position = muzzlePos;
            newBomb.velocity = vel;
            newBomb.flightDistance = 0.0f;
            newBomb.armingTimer = 10.0f; // 投下後約0.17秒は誤爆防止
            newBomb.isExploding = false;
            newBomb.explodeTimer = 0.0f;
            newBomb.explodeRadius = 42.0f;

            // ボム弾頭モデル（高輝度レッドエネルギー弾）
            newBomb.bombObject = std::make_unique<Object3d>();
            newBomb.bombObject->Initialize(object3dCommon_);
            newBomb.bombObject->SetModel("collider_sphere_player.obj");
            newBomb.bombObject->SetEnableLighting(false);
            newBomb.bombObject->SetSelectLightings(0);
            newBomb.bombObject->SetColor({ 1.0f, 0.22f, 0.15f, 1.0f });
            newBomb.bombObject->SetScale({ 1.8f, 1.8f, 2.2f });

            // 外側衝撃波ドーム
            newBomb.shockwaveObject = std::make_unique<Object3d>();
            newBomb.shockwaveObject->Initialize(object3dCommon_);
            newBomb.shockwaveObject->SetModel("collider_sphere_player.obj");
            newBomb.shockwaveObject->SetEnableLighting(false);
            newBomb.shockwaveObject->SetSelectLightings(0);
            newBomb.shockwaveObject->SetColor({ 1.0f, 0.4f, 0.1f, 0.85f });
            newBomb.shockwaveObject->SetScale({ 0.001f, 0.001f, 0.001f });

            // 内側高輝度プラズマコア
            newBomb.innerCoreObject = std::make_unique<Object3d>();
            newBomb.innerCoreObject->Initialize(object3dCommon_);
            newBomb.innerCoreObject->SetModel("collider_sphere_player.obj");
            newBomb.innerCoreObject->SetEnableLighting(false);
            newBomb.innerCoreObject->SetSelectLightings(0);
            newBomb.innerCoreObject->SetColor({ 1.0f, 0.95f, 0.75f, 0.95f });
            newBomb.innerCoreObject->SetScale({ 0.001f, 0.001f, 0.001f });

            activeBombs_.push_back(std::move(newBomb));

            SoundManager::GetInstance()->PlaySE("launcher.mp3", 0.9f);
        }
    }
}

void BombWeapon::Draw() {
    for (auto& b : activeBombs_) {
        if (!b.isExploding && b.bombObject) {
            b.bombObject->Draw();
        } else if (b.isExploding) {
            if (b.shockwaveObject) {
                b.shockwaveObject->Draw();
            }
            if (b.innerCoreObject) {
                b.innerCoreObject->Draw();
            }
        }
    }
}

void BombWeapon::DrawUI() {
}

std::string BombWeapon::GetAmmoText() const {
    return "BOMB: " + std::to_string(ammo_) + " / " + std::to_string(maxAmmo_);
}
