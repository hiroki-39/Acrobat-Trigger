#include "ChargeShotWeapon.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/PlayerBullet.h"
#include "KHEngine/Input/Input.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include <cmath>
#include <algorithm>

void ChargeShotWeapon::Initialize(Player* owner, Object3dCommon* object3dCommon) {
    owner_ = owner;
    object3dCommon_ = object3dCommon;
    ammo_ = 6;
    maxAmmo_ = 6;
    chargeTimer_ = 0.0f;
    maxChargeTime_ = 48.0f; // 約0.8秒で心地よくフルチャージ
    isCharging_ = false;
    hasPlayedMaxChargeSE_ = false;
    coreAnimTimer_ = 0.0f;

    ModelManager::GetInstance()->LoadModel("collider_sphere_player.obj");

    // 内部コア球体（高密度プラズマ核）
    chargeCoreObject_ = std::make_unique<Object3d>();
    chargeCoreObject_->Initialize(object3dCommon);
    chargeCoreObject_->SetModel("collider_sphere_player.obj");
    chargeCoreObject_->SetEnableLighting(false);
    chargeCoreObject_->SetSelectLightings(0);
    chargeCoreObject_->SetColor({ 1.0f, 0.95f, 0.25f, 1.0f });
    chargeCoreObject_->SetScale({ 0.001f, 0.001f, 0.001f });

    // 外部オーラ球体（周囲に膨張・回転するプラズマ殻）
    chargeAuraObject_ = std::make_unique<Object3d>();
    chargeAuraObject_->Initialize(object3dCommon);
    chargeAuraObject_->SetModel("collider_sphere_player.obj");
    chargeAuraObject_->SetEnableLighting(false);
    chargeAuraObject_->SetSelectLightings(0);
    chargeAuraObject_->SetColor({ 1.0f, 0.65f, 0.15f, 0.5f });
    chargeAuraObject_->SetScale({ 0.001f, 0.001f, 0.001f });
}

void ChargeShotWeapon::Reset() {
    ammo_ = maxAmmo_;
    chargeTimer_ = 0.0f;
    isCharging_ = false;
    hasPlayedMaxChargeSE_ = false;
}

void ChargeShotWeapon::Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& /*enemies*/, ArmoredTrainBoss* /*boss*/, Object3d* /*parentCamera*/) {
    coreAnimTimer_ += 0.25f * gameSpeed;

    if (isCharging_ && owner_ && owner_->GetObject3d()) {
        const Matrix4x4& mat = owner_->GetObject3d()->GetmatWorld();
        Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
        Vector3 forwardVec = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
        float fLen = std::sqrt(forwardVec.x * forwardVec.x + forwardVec.y * forwardVec.y + forwardVec.z * forwardVec.z);
        if (fLen > 0.0001f) { forwardVec.x /= fLen; forwardVec.y /= fLen; forwardVec.z /= fLen; }

        // 機首直前に配置
        Vector3 muzzlePos = { playerPos.x + forwardVec.x * 2.8f, playerPos.y + forwardVec.y * 2.8f, playerPos.z + forwardVec.z * 2.8f };
        float chargeRatio = std::clamp(chargeTimer_ / maxChargeTime_, 0.0f, 1.0f);

        // フルチャージ達成時のワンショット通知SE
        if (chargeRatio >= 0.95f && !hasPlayedMaxChargeSE_) {
            hasPlayedMaxChargeSE_ = true;
            SoundManager::GetInstance()->PlaySE("shot.mp3", 0.9f);
        }

        // コアとオーラの脈動アニメーション
        float corePulse = 1.0f;
        float auraPulse = 1.0f;
        if (chargeRatio >= 0.95f) {
            // フルチャージ時は激しく白熱スパーク！
            corePulse = 1.0f + 0.20f * std::sinf(coreAnimTimer_ * 14.0f);
            auraPulse = 1.0f + 0.35f * std::cosf(coreAnimTimer_ * 10.0f);
            chargeCoreObject_->SetColor({ 1.0f, 1.0f, 0.85f, 1.0f }); // 限界突破の白熱ゴールド
            chargeAuraObject_->SetColor({ 1.0f, 0.85f, 0.20f, 0.7f });
        } else {
            // チャージ途中はエネルギー収束感を表現
            corePulse = 1.0f + 0.12f * std::sinf(coreAnimTimer_ * 6.0f);
            auraPulse = 1.0f + 0.18f * std::cosf(coreAnimTimer_ * 5.0f);
            chargeCoreObject_->SetColor({ 1.0f, 0.75f + 0.2f * chargeRatio, 0.15f, 1.0f });
            chargeAuraObject_->SetColor({ 1.0f, 0.45f, 0.1f, 0.35f + 0.25f * chargeRatio });
        }

        float coreScale = (0.6f + 1.6f * chargeRatio) * corePulse;
        float auraScale = (0.9f + 2.3f * chargeRatio) * auraPulse;

        chargeCoreObject_->SetTranslate(muzzlePos);
        chargeCoreObject_->SetScale({ coreScale, coreScale, coreScale });
        chargeCoreObject_->Update();

        chargeAuraObject_->SetTranslate(muzzlePos);
        chargeAuraObject_->SetScale({ auraScale, auraScale, auraScale });
        chargeAuraObject_->Update();
    } else {
        if (chargeCoreObject_) chargeCoreObject_->SetScale({ 0.001f, 0.001f, 0.001f });
        if (chargeAuraObject_) chargeAuraObject_->SetScale({ 0.001f, 0.001f, 0.001f });
    }
}

void ChargeShotWeapon::Attack(Input* input,
                              std::list<std::unique_ptr<PlayerBullet>>& bullets,
                              std::list<std::unique_ptr<PlayerMissile>>& /*missiles*/,
                              const std::list<std::unique_ptr<Enemy>>& /*enemies*/,
                              ArmoredTrainBoss* /*boss*/,
                              Object3d* parentCamera,
                              float gameSpeed) {
    if (!input || !owner_ || ammo_ <= 0) {
        isCharging_ = false;
        chargeTimer_ = 0.0f;
        hasPlayedMaxChargeSE_ = false;
        return;
    }

    // スペースキー長押しでエネルギーをチャージ
    if (input->PushKey(DIK_SPACE)) {
        if (!isCharging_) {
            isCharging_ = true;
            chargeTimer_ = 0.0f;
            hasPlayedMaxChargeSE_ = false;
        }
        chargeTimer_ += gameSpeed;
    } else {
        // キーを離した瞬間に発射判定
        if (isCharging_) {
            float chargeRatio = std::clamp(chargeTimer_ / maxChargeTime_, 0.0f, 1.0f);

            // 0.3以上のチャージがあれば発射
            if (chargeRatio >= 0.3f) {
                Object3d* playerObj = owner_->GetObject3d();
                if (playerObj) {
                    const Matrix4x4& mat = playerObj->GetmatWorld();
                    Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
                    Vector3 forwardVec = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
                    float fLen = std::sqrt(forwardVec.x * forwardVec.x + forwardVec.y * forwardVec.y + forwardVec.z * forwardVec.z);
                    if (fLen > 0.0001f) { forwardVec.x /= fLen; forwardVec.y /= fLen; forwardVec.z /= fLen; }

                    Vector3 targetPos = owner_->GetReticleWorldPosition();
                    Vector3 muzzlePos = { playerPos.x + forwardVec.x * 2.8f, playerPos.y + forwardVec.y * 2.8f, playerPos.z + forwardVec.z * 2.8f };

                    Vector3 dir = { targetPos.x - muzzlePos.x, targetPos.y - muzzlePos.y, targetPos.z - muzzlePos.z };
                    float len = std::sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                    if (len > 0.0001f) { dir.x /= len; dir.y /= len; dir.z /= len; }
                    else { dir = forwardVec; }

                    float bSpeed = 12.0f + 4.0f * chargeRatio;
                    Vector3 vel = { dir.x * bSpeed, dir.y * bSpeed, dir.z * bSpeed };

                    auto bullet = std::make_unique<PlayerBullet>();
                    bullet->Initialize(object3dCommon_, muzzlePos, vel, parentCamera, owner_->GetReticleHoverEnemy());

                    // フルチャージ（0.9以上）は特大貫通スナイパー弾！
                    if (chargeRatio >= 0.9f) {
                        bullet->SetDamage(6);
                        bullet->SetPenetrating(true, 10);
                        // 超特大の長大貫通レーザービーム（直径3.4m、長さ26m！）
                        bullet->SetBulletScale({ 3.4f, 3.4f, 26.0f });
                        bullet->SetBulletColor({ 1.0f, 0.95f, 0.30f, 1.0f }); // 眩いホワイトゴールド
                        SoundManager::GetInstance()->PlaySE("launcher.mp3", 1.0f);
                    } else {
                        bullet->SetDamage(2);
                        bullet->SetPenetrating(true, 2);
                        bullet->SetBulletScale({ 2.0f, 2.0f, 16.0f });
                        bullet->SetBulletColor({ 1.0f, 0.65f, 0.15f, 1.0f }); // ネオンオレンジ
                        SoundManager::GetInstance()->PlaySE("shot.mp3", 0.9f);
                    }

                    bullets.push_back(std::move(bullet));
                    ammo_--;
                }
            }

            isCharging_ = false;
            chargeTimer_ = 0.0f;
            hasPlayedMaxChargeSE_ = false;
        }
    }
}

void ChargeShotWeapon::Draw() {
    if (isCharging_) {
        if (chargeAuraObject_) {
            chargeAuraObject_->Draw();
        }
        if (chargeCoreObject_) {
            chargeCoreObject_->Draw();
        }
    }
}

void ChargeShotWeapon::DrawUI() {
}

std::string ChargeShotWeapon::GetAmmoText() const {
    if (isCharging_) {
        int pct = static_cast<int>((std::min)(1.0f, chargeTimer_ / maxChargeTime_) * 100.0f);
        return "CHARGE: " + std::to_string(ammo_) + " / " + std::to_string(maxAmmo_) + " (" + std::to_string(pct) + "%)";
    }
    return "CHARGE: " + std::to_string(ammo_) + " / " + std::to_string(maxAmmo_);
}
