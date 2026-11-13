#include "NormalWeapon.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/PlayerBullet.h"
#include "KHEngine/Input/Input.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include <cmath>

void NormalWeapon::Initialize(Player* owner, Object3dCommon* object3dCommon) {
    owner_ = owner;
    object3dCommon_ = object3dCommon;
    attackTimer_ = 0.0f;
    attackInterval_ = 15.0f;
    bulletSpeed_ = 8.5f;
}

void NormalWeapon::Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& /*enemies*/, ArmoredTrainBoss* /*boss*/, Object3d* /*parentCamera*/) {
    if (attackTimer_ > 0.0f) {
        attackTimer_ -= gameSpeed;
    }
}

void NormalWeapon::Attack(Input* input,
                          std::list<std::unique_ptr<PlayerBullet>>& bullets,
                          std::list<std::unique_ptr<PlayerMissile>>& /*missiles*/,
                          const std::list<std::unique_ptr<Enemy>>& /*enemies*/,
                          ArmoredTrainBoss* /*boss*/,
                          Object3d* parentCamera,
                          float /*gameSpeed*/) {
    if (!input || !owner_) return;

    if (attackTimer_ <= 0.0f && input->PushKey(DIK_SPACE)) {
        attackTimer_ = attackInterval_;

        Object3d* playerObj = owner_->GetObject3d();
        if (!playerObj) return;

        const Matrix4x4& mat = playerObj->GetmatWorld();
        Vector3 playerPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };

        // 自機正面ベクトル
        Vector3 forwardVec = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };
        float fLen = std::sqrt(forwardVec.x * forwardVec.x + forwardVec.y * forwardVec.y + forwardVec.z * forwardVec.z);
        if (fLen > 0.0001f) {
            forwardVec.x /= fLen;
            forwardVec.y /= fLen;
            forwardVec.z /= fLen;
        }

        Vector3 targetPos = owner_->GetReticleWorldPosition();

        // エイムアシスト（プレイヤー自力でのエイミング感を最優先するため、ごく微弱な25%の引き寄せのみ）
        if (owner_->IsTargetInReticle()) {
            Vector3 hoverPos = owner_->GetReticleHoverTargetPos();
            targetPos.x += (hoverPos.x - targetPos.x) * 0.25f;
            targetPos.y += (hoverPos.y - targetPos.y) * 0.25f;
            targetPos.z += (hoverPos.z - targetPos.z) * 0.25f;
        }

        float bulletSpeed = owner_->GetBulletSpeed();
        Enemy* hoverEnemy = owner_->GetReticleHoverEnemy();

        if (owner_->IsDoubleShot()) {
            // 2連装: 左右の銃口から目標座標（奥照準）へ向けて正確に集束射出（コンバージェンス射撃）
            Vector3 rightDir = { mat.m[0][0], mat.m[0][1], mat.m[0][2] };
            float offsetAmount = 2.0f;
            Vector3 rightOffset = { playerPos.x + rightDir.x * offsetAmount + forwardVec.x * 1.5f, playerPos.y + rightDir.y * offsetAmount + forwardVec.y * 1.5f, playerPos.z + rightDir.z * offsetAmount + forwardVec.z * 1.5f };
            Vector3 leftOffset = { playerPos.x - rightDir.x * offsetAmount + forwardVec.x * 1.5f, playerPos.y - rightDir.y * offsetAmount + forwardVec.y * 1.5f, playerPos.z - rightDir.z * offsetAmount + forwardVec.z * 1.5f };

            Vector3 rightDirToTarget = { targetPos.x - rightOffset.x, targetPos.y - rightOffset.y, targetPos.z - rightOffset.z };
            float rLen = std::sqrtf(rightDirToTarget.x * rightDirToTarget.x + rightDirToTarget.y * rightDirToTarget.y + rightDirToTarget.z * rightDirToTarget.z);
            if (rLen > 0.0001f) { rightDirToTarget.x /= rLen; rightDirToTarget.y /= rLen; rightDirToTarget.z /= rLen; }
            Vector3 rightVelocity = { rightDirToTarget.x * bulletSpeed, rightDirToTarget.y * bulletSpeed, rightDirToTarget.z * bulletSpeed };

            Vector3 leftDirToTarget = { targetPos.x - leftOffset.x, targetPos.y - leftOffset.y, targetPos.z - leftOffset.z };
            float lLen = std::sqrtf(leftDirToTarget.x * leftDirToTarget.x + leftDirToTarget.y * leftDirToTarget.y + leftDirToTarget.z * leftDirToTarget.z);
            if (lLen > 0.0001f) { leftDirToTarget.x /= lLen; leftDirToTarget.y /= lLen; leftDirToTarget.z /= lLen; }
            Vector3 leftVelocity = { leftDirToTarget.x * bulletSpeed, leftDirToTarget.y * bulletSpeed, leftDirToTarget.z * bulletSpeed };

            auto rightBullet = std::make_unique<PlayerBullet>();
            rightBullet->Initialize(object3dCommon_, rightOffset, rightVelocity, parentCamera, hoverEnemy);
            bullets.push_back(std::move(rightBullet));

            auto leftBullet = std::make_unique<PlayerBullet>();
            leftBullet->Initialize(object3dCommon_, leftOffset, leftVelocity, parentCamera, hoverEnemy);
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
                direction.x * bulletSpeed,
                direction.y * bulletSpeed,
                direction.z * bulletSpeed
            };

            auto newBullet = std::make_unique<PlayerBullet>();
            newBullet->Initialize(object3dCommon_, muzzlePos, velocity, parentCamera, hoverEnemy);
            bullets.push_back(std::move(newBullet));
        }

        // 通常ショットSE再生
        SoundManager::GetInstance()->PlaySE("player_shot.mp3", 0.7f);
    }
}

void NormalWeapon::Draw() {
    // 通常弾用の追加描画オブジェクトは特になし
}

void NormalWeapon::DrawUI() {
    // 通常弾用のUI（無限表示など）
}

void NormalWeapon::Reset() {
    attackTimer_ = 0.0f;
}
