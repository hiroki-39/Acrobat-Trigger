#pragma once
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Input/Input.h"
#include "Game/Actor/Bullet/PlayerBullet.h"
#include "Game/Actor/Bullet/PlayerMissile.h"
#include <memory>
#include <list>
#include <vector>
#include <array>

class Enemy;
class ArmoredTrainBoss;
class ParticleEmitter;

class Player {
public:
    enum class WeaponType {
        NORMAL,
        MISSILE
    };

    
    
    
    void Initialize(Object3dCommon* object3dCommon, uint32_t skyboxTexIndex);

    void LoadSettings(const std::string& filepath);
    void SaveSettings(const std::string& filepath);
    void DrawUI();
    void DrawImGuiContent();

    bool IsGodMode() const { return isGodMode_; }
    void SetGodMode(bool god) { isGodMode_ = god; }

    
    
    
    bool IsRolling() const { return isRolling_; }
    float GetRollTimer() const { return rollTimer_; }
    float GetRollMaxTime() const { return rollMaxTime_; }
    
    /// <summary>
    /// カットシーン演出用オーバーライド設定
    /// </summary>
    void SetCutsceneOverride(bool active, const Vector3& localPos = { 0.0f, 0.0f, 0.0f }, const Vector3& localRot = { 0.0f, 0.0f, 0.0f }) {
        isCutsceneActive_ = active;
        cutscenePos_ = localPos;
        cutsceneRot_ = localRot;
        if (active) {
            logicalPosition_ = localPos;
            baseRotation_ = localRot;
        } else {
            // カットシーン終了・スキップ時：位置・基本姿勢・動的姿勢変数を水平巡航状態へリセット
            logicalPosition_ = { 0.0f, 0.0f, 0.0f };
            baseRotation_ = { 0.0f, 0.0f, 0.0f };
            currentPitch_ = 0.0f;
            currentYaw_ = 0.0f;
            currentBank_ = 0.0f;
            velocity_ = { 0.0f, 0.0f };
            reticlePosition_ = { 0.0f, 0.0f, dynamicReticleDistance_ };
            Update3DObjectOnly();
        }
    }
    bool IsCutsceneActive() const { return isCutsceneActive_; }

    void ResetPosture() {
        baseRotation_ = { 0.0f, 0.0f, 0.0f };
        currentPitch_ = 0.0f;
        currentYaw_ = 0.0f;
        currentBank_ = 0.0f;
        finalBank_ = 0.0f;
        velocity_ = { 0.0f, 0.0f };
        reticlePosition_ = { logicalPosition_.x, logicalPosition_.y, logicalPosition_.z + dynamicReticleDistance_ };
        Update3DObjectOnly();
    }

    void Update(std::list<std::unique_ptr<PlayerBullet>>& bullets, std::list<std::unique_ptr<PlayerMissile>>& missiles, const std::list<std::unique_ptr<Enemy>>& enemies, Object3d* parentCamera = nullptr, float gameSpeed = 1.0f, ArmoredTrainBoss* boss = nullptr);

    void Draw();

    void DrawCollider();
    
    Object3d* GetColliderObject() const { return colliderObject_.get(); }

    void Update3DObjectOnly() {
        if (isCutsceneActive_) {
            logicalPosition_ = cutscenePos_;
            baseRotation_ = cutsceneRot_;
        }
        if (object_) {
            object_->SetScale(playerScale_);
            object_->SetRotation(Vector3(
                baseRotation_.x + modelRotOffset_.x,
                baseRotation_.y + modelRotOffset_.y,
                baseRotation_.z + modelRotOffset_.z
            ));
            object_->SetTranslate(Vector3(
                logicalPosition_.x + modelPosOffset_.x, 
                logicalPosition_.y + modelPosOffset_.y, 
                logicalPosition_.z + modelPosOffset_.z
            ));
            object_->Update();
            
            if (colliderObject_) {
                colliderObject_->SetTranslate(object_->GetTranslate());
                colliderObject_->SetRotation(object_->GetRotation());
                colliderObject_->Update();
            }
            for (int i = 0; i < 4; ++i) { 
                if (mountedMissiles_[i]) mountedMissiles_[i]->Update();
                if (lockOnReticles_[i]) lockOnReticles_[i]->Update();
            }
        }
        if (accessory_) accessory_->Update();
        if (reticle_) reticle_->Update();
        if (frontReticle_) frontReticle_->Update();
        UpdateGroundMarker();
        UpdateVerniers(1.0f);
    }

    
    const Vector3& GetTranslate() const { return logicalPosition_; }
    const Vector3& GetPreviousTranslate() const { return prevLogicalPosition_; }
    const Vector3& GetColliderSize() const { return colliderSize_; }
    void SetTranslate(const Vector3& translate) { logicalPosition_ = translate; }
    void SetRotation(const Vector3& rotation) { baseRotation_ = rotation; }
    Object3d* GetObject3d() const { return object_.get(); }
    Object3d* GetReticle() const { return reticle_.get(); }
    Object3d* GetFrontReticle() const { return frontReticle_.get(); }
    Object3d* GetGroundMarkerObject() const { return groundMarkerObject_.get(); }
    Object3d* GetVernierLeft() const { return vernierLeft_.get(); }
    Object3d* GetVernierRight() const { return vernierRight_.get(); }
    
    
    void SetReticleColor(const Vector4& color);
    const Vector4& GetReticleColor() const { return reticleColor_; }
    Vector3 GetReticleWorldPosition() const;
    Vector3 GetFrontReticleWorldPosition() const;
    bool IsTargetInReticle() const { return isTargetInReticle_; }

    void SetLockOn(bool isLockOn, const Vector3& targetPos = { 0, 0, 0 }, Enemy* targetEnemy = nullptr) {
        isLockOn_ = isLockOn;
        lockOnTargetPos_ = targetPos;
        lockOnTargetEnemy_ = targetEnemy;
    }

    // ロックオン弾（ミサイル）パラメータアクセサ
    float GetLockOnCompleteTime() const { return lockOnCompleteTime_; }
    void SetLockOnCompleteTime(float time) { lockOnCompleteTime_ = time; }
    float GetLockOnInterval() const { return lockOnInterval_; }
    void SetLockOnInterval(float interval) { lockOnInterval_ = interval; }
    float GetMissileReloadTime() const { return missileReloadTime_; }
    void SetMissileReloadTime(float time) { missileReloadTime_ = time; }
    float GetLockOnMaxDistance() const { return lockOnMaxDistance_; }
    void SetLockOnMaxDistance(float dist) { lockOnMaxDistance_ = dist; }

    void SetColor(const Vector4& color) { if (object_) object_->SetColor(color); }
    void SetSelectLightings(int32_t mode) { if (object_) object_->SetSelectLightings(mode); }

    bool IsBoosting() const { return isBoosting_ || isCutsceneBoosting_; }
    void SetCutsceneBoosting(bool boosting) { isCutsceneBoosting_ = boosting; }
    bool IsCutsceneBoosting() const { return isCutsceneBoosting_; }
    float GetBoostEnergy() const { return boostEnergy_; }
    float GetMaxBoostEnergy() const { return maxBoostEnergy_; }
    float GetBoostRatio() const { return maxBoostEnergy_ > 0.0f ? (boostEnergy_ / maxBoostEnergy_) : 0.0f; }
    bool IsBoostOverheated() const { return isBoostOverheated_; }
    void SetBoostEnergy(float energy) { boostEnergy_ = std::clamp(energy, 0.0f, maxBoostEnergy_); }
    void ResetBoost() { boostEnergy_ = maxBoostEnergy_; isBoostOverheated_ = false; }

    
    bool ConsumeDodgeTrigger() {
        if (isDodgeTriggered_) {
            isDodgeTriggered_ = false;
            return true;
        }
        return false;
    }

    void OnCollision();
    bool OnTerrainCollision(const Vector3& worldNormal, float penetrationDepth, Object3d* parentCamera = nullptr, const Vector3* hitPoint = nullptr);
    OBB GetWorldOBB() const;
    void SetColliderColor(const Vector4& color) {
        if (colliderObject_ && colliderObject_->GetModel()) {
            colliderObject_->GetModel()->SetColor(color);
        }
    }
    float GetTerrainCollisionRadius() const { return terrainCollisionRadius_; }
    void SetTerrainCollisionRadius(float radius) { terrainCollisionRadius_ = radius; }
    void Heal(int amount) { hp_ += amount; if (hp_ > maxHp_) hp_ = maxHp_; }
    void PowerUp();
    void SetPowerUpLevel(int level);
    int GetPowerUpLevel() const { return powerUpLevel_; }
    bool IsDead() const { return isDead_; }
    void SetDead(bool dead) { isDead_ = dead; }
    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    size_t GetLockedEnemyCount() const { return multiLockedEnemies_.size(); }
    int GetMaxMissiles() const { return maxMissiles_; }
    static int GetMaxAllowedMissiles() { return MAX_MISSILES; }

    void SetVisible(bool visible) { isVisible_ = visible; }
    bool IsVisible() const { return isVisible_; }
    const Vector3& GetRotation() const { return baseRotation_; }
    Vector3 GetWorldPosition() const {
        if (object_) {
            const Matrix4x4& wMat = object_->GetmatWorld();
            return { wMat.m[3][0], wMat.m[3][1], wMat.m[3][2] };
        }
        return logicalPosition_;
    }

    // 動的移動制限（ソフトリミット）
    void SetTargetMoveLimits(float limitX, float limitYMin, float limitYMax) {
        targetLimitX_ = limitX;
        targetLimitYMin_ = limitYMin;
        targetLimitYMax_ = limitYMax;
    }
    void ResetMoveLimits() {
        targetLimitX_ = 35.0f;
        targetLimitYMin_ = -4.0f;
        targetLimitYMax_ = 20.0f;
    }
    float GetLimitX() const { return playerLimitX_; }
    float GetLimitYMin() const { return playerLimitYMin_; }
    float GetLimitYMax() const { return playerLimitYMax_; }

    
    void SetAssistTarget(Enemy* enemy) { assistTarget_ = enemy; }

    
    bool IsBanking() const;
    Vector3 GetLeftWingPosition() const;
    Vector3 GetRightWingPosition() const;
    float GetFinalBank() const { return finalBank_; }

private:
    
    
    void Move(float gameSpeed);

    
    
    
    void Attack(std::list<std::unique_ptr<PlayerBullet>>& bullets, std::list<std::unique_ptr<PlayerMissile>>& missiles, const std::list<std::unique_ptr<Enemy>>& enemies, Object3d* parentCamera, float gameSpeed, ArmoredTrainBoss* boss = nullptr);

private:
    std::unique_ptr<Object3d> object_ = nullptr;
    std::unique_ptr<Object3d> colliderObject_ = nullptr;
    std::unique_ptr<Object3d> reticle_ = nullptr; 
    std::unique_ptr<Object3d> frontReticle_ = nullptr; 
    std::unique_ptr<Object3d> accessory_ = nullptr; 
    std::unique_ptr<Object3d> groundMarkerObject_ = nullptr; // 接地マーカー（プロジェクションシャドウ）
    std::unique_ptr<Object3d> vernierLeft_ = nullptr;        // 左ツインバーニア噴射炎コーン
    std::unique_ptr<Object3d> vernierRight_ = nullptr;       // 右ツインバーニア噴射炎コーン

    // ロール回避残像（アフターイメージ）用データ構造
    struct RollAfterImage {
        std::unique_ptr<Object3d> object = nullptr;
        float lifeTimer = 0.0f;
        float maxLife = 12.0f; // 約0.2秒（12フレーム）
        bool active = false;
    };
    static const int MAX_AFTER_IMAGES = 6;
    std::array<RollAfterImage, MAX_AFTER_IMAGES> rollAfterImages_;
    float rollAfterImageSpawnTimer_ = 0.0f;

    void UpdateGroundMarker();
    void UpdateVerniers(float gameSpeed);
    void UpdateRollAfterImages(float gameSpeed, Object3d* parentCamera);

    Object3dCommon* object3dCommon_ = nullptr; 
    Input* input_ = nullptr;
    
    uint32_t skyboxTexIndex_ = 0; 

    Vector3 reticlePosition_ = { 0.0f, 0.0f, 30.0f }; 
    Vector4 reticleColor_ = { 1.0f, 1.0f, 1.0f, 1.0f }; 

    
    float speed_ = 0.45f;
    float reticleSpeed_ = 0.75f;
    float moveLimitX_ = 35.0f;     
    float moveLimitY_ = 20.0f;     
    float attackInterval_ = 15.0f;
    float rollMaxTime_ = 15.0f;
    float playerLimitX_ = 35.0f;   
    float playerLimitYMin_ = -4.0f;
    float playerLimitYMax_ = 20.0f;
    float targetLimitX_ = 35.0f;   
    float targetLimitYMin_ = -4.0f;
    float targetLimitYMax_ = 20.0f;
    float followSpeed_ = 0.12f;
    float bulletSpeed_ = 8.5f; // 偏差撃ち解消のため高速化 (8.5m/フレーム)
    float terrainKnockbackPower_ = 0.35f; // 壁・地面に当たった時の反発速度
    float terrainPushMargin_ = 0.05f;     // めり込み押し戻しマージン
    float terrainCollisionRadius_ = 0.8f; // 地形・障害物との衝突判定球の半径
    float terrainBlockTimer_ = 0.0f;       // 衝突直後の障害物方向への入力遮断タイマー
    Vector2 terrainBlockDir_ = { 0.0f, 0.0f }; // 遮断する方向ベクトル

    // 照準のZ深度適応・エイムアシスト・ヒット確信フィードバック用
    void UpdateReticleAndAimAssist(const std::list<std::unique_ptr<Enemy>>& enemies, ArmoredTrainBoss* boss, float gameSpeed);
    Enemy* reticleHoverEnemy_ = nullptr;
    ArmoredTrainBoss* reticleHoverBoss_ = nullptr;
    int reticleHoverCarriage_ = -1;
    Vector3 reticleHoverTargetPos_ = { 0.0f, 0.0f, 0.0f };
    bool isTargetInReticle_ = false;
    bool prevTargetInReticle_ = false;
    float dynamicReticleDistance_ = 45.0f;
    float reticleSnapAnimTimer_ = 0.0f;
    Vector3 baseReticleScale_ = { 1.6f, 1.6f, 1.6f };

    
    std::string modelName_ = "cube.obj";
    Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool reflection_ = false;
    Vector3 modelPosOffset_ = { 0.0f, 0.0f, 0.0f };
    Vector3 modelRotOffset_ = { 0.0f, 0.0f, 0.0f };
    Vector3 playerScale_ = { 0.5f, 0.5f, 0.5f };
    Vector3 colliderSize_ = { 4.0f, 4.0f, 4.0f };
    bool isGodMode_ = false;

    
    float currentPitch_ = 0.0f;
    float currentYaw_ = 0.0f;
    float currentBank_ = 0.0f;
    float finalBank_ = 0.0f;

    // プレイヤーの論理的な位置とベース回転
    Vector3 logicalPosition_ = { 0.0f, 0.0f, 0.0f }; 
    Vector3 prevLogicalPosition_ = { 0.0f, 0.0f, 0.0f }; 
    Vector3 baseRotation_ = { 0.0f, 0.0f, 0.0f };

    // 攻撃タイマー
    float attackTimer_ = 0.0f;

    
    Vector2 velocity_ = { 0.0f, 0.0f };

    
    bool isRolling_ = false;
    float rollTimer_ = 0.0f;
    float rollDirection_ = 0.0f; 
    

    
    bool isLockOn_ = false;
    Vector3 lockOnTargetPos_ = { 0, 0, 0 };
    Enemy* lockOnTargetEnemy_ = nullptr;
    
    
    Enemy* assistTarget_ = nullptr;

    
    bool isBoosting_ = false;
    float boostEnergy_ = 100.0f;
    float maxBoostEnergy_ = 100.0f;
    float boostConsumeRate_ = 1.5f;          // ブースト消費量/フレーム（約1.11秒で枯渇: さらに1/3減少）
    float boostRecoverRate_ = 0.5f;          // ブースト回復量/フレーム（約3.3秒で全回復）
    float boostRecoverDelayTimer_ = 0.0f;    // ブースト解除後の回復遅延タイマー
    const float kBoostRecoverDelay_ = 12.0f; // 約0.2秒のディレイ
    bool isBoostOverheated_ = false;         // エネルギー枯渇時のオーバーヒート状態

    
    WeaponType currentWeapon_ = WeaponType::NORMAL;
    static const int MAX_MISSILES = 8;
    int maxMissiles_ = 4;                  // 現在の最大同時ロックオン可能数（初期4、2個目の強化リングで8）
    int powerUpLevel_ = 0;                 // 強化段階 (0: 通常, 1: ダブルショット, 2: ロックオン8 & 高速化)
    std::array<std::unique_ptr<Object3d>, MAX_MISSILES> mountedMissiles_;
    std::array<std::unique_ptr<Object3d>, MAX_MISSILES> lockOnReticles_;
    struct LockOnTarget {
        Enemy* enemy = nullptr;
        ArmoredTrainBoss* boss = nullptr;
        int carriageIndex = -1;
        float lockedTime = 0.0f;
    };
    std::vector<LockOnTarget> multiLockedEnemies_;
    float missileReloadTimer_ = 0.0f;
    float missileReloadTime_ = 120.0f;     // 発射後のリロード時間 (フレーム数)
    float lockOnAnimTimer_ = 0.0f; 
    float lockOnDelayTimer_ = 0.0f;
    float lockOnCompleteTime_ = 20.0f;     // ロックオン完了所要時間 (フレーム数)
    float lockOnInterval_ = 60.0f;         // 敵1体を捕捉してから次の敵をロックするまでの間隔 (フレーム数)
    float lockOnMaxDistance_ = 350.0f;     // ロックオン最大射程 (m)
    float defaultLockOnCompleteTime_ = 20.0f;
    float defaultLockOnInterval_ = 60.0f;
    float defaultMissileReloadTime_ = 120.0f;

    
    bool isDodgeTriggered_ = false;

    
    int hp_ = 10000;
    int maxHp_ = 10000;
    float invincibilityTimer_ = 0.0f;
    bool isDead_ = false;
    bool isVisible_ = true;
    bool isDoubleShot_ = false;

    // カットシーン演出用
    bool isCutsceneActive_ = false;
    bool isCutsceneBoosting_ = false;
    Vector3 cutscenePos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 cutsceneRot_ = { 0.0f, 0.0f, 0.0f };
};

