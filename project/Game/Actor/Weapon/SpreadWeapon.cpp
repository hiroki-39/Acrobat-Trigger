#include "SpreadWeapon.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/PlayerBullet.h"
#include "KHEngine/Input/Input.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include <cmath>
#include <algorithm>

void SpreadWeapon::Initialize(Player* owner, Object3dCommon* object3dCommon) {
    owner_ = owner;
    object3dCommon_ = object3dCommon;
    ammo_ = 10;
    maxAmmo_ = 10;
    attackTimer_ = 0.0f;
    attackInterval_ = 10.0f; // 小気味よい連射テンポ（秒間6射）
    bulletSpeed_ = 9.5f;
}

void SpreadWeapon::Reset() {
    ammo_ = maxAmmo_;
    attackTimer_ = 0.0f;
}

void SpreadWeapon::Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& /*enemies*/, ArmoredTrainBoss* /*boss*/, Object3d* /*parentCamera*/) {
    if (attackTimer_ > 0.0f) {
        attackTimer_ -= gameSpeed;
    }
}

void SpreadWeapon::Attack(Input* input,
                          std::list<std::unique_ptr<PlayerBullet>>& bullets,
                          std::list<std::unique_ptr<PlayerMissile>>& /*missiles*/,
                          const std::list<std::unique_ptr<Enemy>>& /*enemies*/,
                          ArmoredTrainBoss* /*boss*/,
                          Object3d* parentCamera,
                          float gameSpeed) {
    if (!input || !owner_ || ammo_ <= 0) return;

    if (input->PushKey(DIK_SPACE)) {
        if (attackTimer_ <= 0.0f) {
            attackTimer_ = attackInterval_;
            ammo_--;

            Object3d* playerObj = owner_->GetObject3d();
            if (!playerObj) return;

            const Matrix4x4& mat = playerObj->GetmatWorld();
            Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
            Vector3 forwardVec = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
            float fLen = std::sqrt(forwardVec.x * forwardVec.x + forwardVec.y * forwardVec.y + forwardVec.z * forwardVec.z);
            if (fLen > 0.0001f) { forwardVec.x /= fLen; forwardVec.y /= fLen; forwardVec.z /= fLen; }

            // 基準となる射撃方向（レティクル方向）
            Vector3 targetPos = owner_->GetReticleWorldPosition();
            Vector3 muzzlePos = { playerPos.x + forwardVec.x * 2.5f, playerPos.y + forwardVec.y * 2.5f, playerPos.z + forwardVec.z * 2.5f };

            Vector3 baseDir = { targetPos.x - muzzlePos.x, targetPos.y - muzzlePos.y, targetPos.z - muzzlePos.z };
            float baseLen = std::sqrtf(baseDir.x * baseDir.x + baseDir.y * baseDir.y + baseDir.z * baseDir.z);
            if (baseLen > 0.0001f) {
                baseDir.x /= baseLen; baseDir.y /= baseLen; baseDir.z /= baseLen;
            } else {
                baseDir = forwardVec;
            }

            // 右ベクトル（ワールドの上ベクトル(0,1,0)との外積）
            Vector3 up = { 0.0f, 1.0f, 0.0f };
            Vector3 right = {
                up.y * baseDir.z - up.z * baseDir.y,
                up.z * baseDir.x - up.x * baseDir.z,
                up.x * baseDir.y - up.y * baseDir.x
            };
            float rLen = std::sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
            if (rLen > 0.0001f) {
                right.x /= rLen; right.y /= rLen; right.z /= rLen;
            } else {
                right = { 1.0f, 0.0f, 0.0f };
            }

            // 扇状5方向の射出角度（度数法: -14, -7, 0, +7, +14）
            const float anglesDeg[5] = { 0.0f, -7.0f, 7.0f, -14.0f, 14.0f };
            const float deg2rad = 3.14159265f / 180.0f;

            for (int i = 0; i < 5; ++i) {
                float rad = anglesDeg[i] * deg2rad;
                float cosA = std::cosf(rad);
                float sinA = std::sinf(rad);

                // dir = baseDir * cosA + right * sinA
                Vector3 shootDir = {
                    baseDir.x * cosA + right.x * sinA,
                    baseDir.y * cosA + right.y * sinA,
                    baseDir.z * cosA + right.z * sinA
                };
                float sLen = std::sqrtf(shootDir.x * shootDir.x + shootDir.y * shootDir.y + shootDir.z * shootDir.z);
                if (sLen > 0.0001f) {
                    shootDir.x /= sLen; shootDir.y /= sLen; shootDir.z /= sLen;
                }

                Vector3 vel = { shootDir.x * bulletSpeed_, shootDir.y * bulletSpeed_, shootDir.z * bulletSpeed_ };

                auto bullet = std::make_unique<PlayerBullet>();
                bullet->Initialize(object3dCommon_, muzzlePos, vel, parentCamera, nullptr);
                bullet->SetDamage(1);
                bullet->SetBulletColor({ 0.2f, 1.0f, 0.35f, 1.0f }); // 鮮烈なエメラルドグリーン
                // 通常弾と同等以上の視認性を誇る大型ビーム形状（Z軸 10m！）
                bullet->SetBulletScale({ 1.25f, 1.25f, 10.0f });
                bullets.push_back(std::move(bullet));
            }

            SoundManager::GetInstance()->PlaySE("shot.mp3", 0.8f);
        }
    }
}

void SpreadWeapon::Draw() {
}

void SpreadWeapon::DrawUI() {
}

std::string SpreadWeapon::GetAmmoText() const {
    return "SPREAD: " + std::to_string(ammo_) + " / " + std::to_string(maxAmmo_);
}
