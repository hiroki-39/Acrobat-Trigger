#pragma once
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Graphics/3d/Object/Object3dCommon.h"
#include "KHEngine/Scene/LevelLoader.h"
#include "KHEngine/Math/CollisionMath.h"
#include "Game/System/Rail.h"
#include <memory>
#include <vector>
#include <list>
#include <string>

class Player;
class EnemyBullet;

/**
 * @brief ボスの部位種別（部位破壊システム用）
 */
enum class BossPartType {
    Carriage,   // 車両本体
    Turret,     // 2連装砲台
    MissilePod  // 垂直ミサイルポッド
};

/**
 * @brief 2連装砲台マウント情報
 */
struct BossTurretMount {
    std::string name;
    Vector3 baseLocalPos = { 0.0f, 0.0f, 0.0f }; // 車両ローカル位置
    Vector3 baseLocalRot = { 0.0f, 0.0f, 0.0f }; // 初期回転
    Vector3 currentRot = { 0.0f, 0.0f, 0.0f };   // 現在の回転（自機追従）
    std::unique_ptr<Object3d> object;
    std::unique_ptr<Object3d> colliderObject;    // デバッグ用コライダー表示
    bool isDestroyed = false;
    bool canShoot = false; // 射角（左右90度以内）に入っているか
    float attackTimer = 0.0f;
    float attackInterval = 75.0f;
    int hp = 8;
    int maxHp = 8;
    float colliderRadius = 2.4f;                 // 当たり判定球半径 (m)
};

/**
 * @brief ミサイルポッドマウント情報
 */
struct BossMissilePodMount {
    std::string name;
    Vector3 baseLocalPos = { 0.0f, 0.0f, 0.0f }; // 車両ローカル位置
    Vector3 baseLocalRot = { 0.0f, 0.0f, 0.0f }; // 初期回転
    std::unique_ptr<Object3d> object;
    std::unique_ptr<Object3d> colliderObject;    // デバッグ用コライダー表示
    bool isDestroyed = false;
    float attackTimer = 0.0f;
    float attackInterval = 130.0f;
    int lockOnState = 0; // 0: 待機, 1: ロックオンチャージ中, 2: 連続発射バースト
    float lockOnTimer = 0.0f;
    int burstCount = 0;     // バースト発射カウンター
    float burstTimer = 0.0f;// バースト発射間隔タイマー
    int hp = 10;
    int maxHp = 10;
    float colliderRadius = 2.4f;                 // 当たり判定球半径 (m)
};

/**
 * @brief 3両目超巨大主砲マウント情報
 */
struct BossBigCannonMount {
    std::unique_ptr<Object3d> baseObject;   // boss_bigTurret.obj (旋回台座)
    std::unique_ptr<Object3d> barrelObject; // boss_bigTurret_1.obj (砲身)
    Vector3 baseLocalPos = { 0.0f, 0.0f, 0.0f };
    Vector3 baseRot = { 0.0f, 0.0f, 0.0f };   // 台座の旋回 (Yaw)
    Vector3 barrelRot = { 0.0f, 0.0f, 0.0f }; // 砲身の仰角 (Pitch)
    float recoilZ = 0.0f;                     // 発射時の砲身リコイル後退量
    bool isTargetingTerrain = false;
    float attackTimer = 0.0f;
};

/**
 * @brief 装甲列車の個別車両データ
 */
struct TrainCarriage {
    std::string name;             // 車両名（例: "Locomotive", "TurretCar", "CannonCar", "MissileCar"）
    std::string displayName;      // 表示名（例: "先頭装甲機関車", "多連装砲塔車", "超巨大主砲車", "垂直ミサイル車"）
    std::string modelName;        // モデル
    Vector3 scale = { 1.0f, 1.0f, 1.0f };
    Vector3 baseScale = { 1.0f, 1.0f, 1.0f };
    Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float lengthOffset = 0.0f;        // 先頭車両からの連結距離オフセット (負のZ値)
    float baseLengthOffset = 0.0f;
    
    int hp = 40;
    int maxHp = 40;
    bool isDestroyed = false;
    float attackTimer = 0.0f;
    float attackInterval = 90.0f;

    // 個別のワールド座標と回転（傾斜・カーブ姿勢）
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation = { 0.0f, 0.0f, 0.0f };

    // 当たり判定
    LevelCollider collider;
    Vector3 baseColliderCenter = { 0.0f, 0.0f, 0.0f };
    Vector3 baseColliderSize = { 1.0f, 1.0f, 1.0f };
    std::unique_ptr<Object3d> object;
    std::unique_ptr<Object3d> colliderObject;

    // 地面吸着キャッシュ（高負荷防止）
    float cachedGroundY = 0.0f;
    bool hasGroundHeight = false;

    // 車両にマウントされた武装・パーツ
    std::vector<BossTurretMount> turrets;
    std::vector<BossMissilePodMount> missilePods;
    std::unique_ptr<BossBigCannonMount> bigCannon;
};

/**
 * @brief 装甲列車（Armored Train）中ボス
 * レール上を連結走行し、多数の武装（砲塔、ミサイル、主砲）でプレイヤーを迎え撃つ巨大ボス
 */
class ArmoredTrainBoss {
public:
    ArmoredTrainBoss() = default;
    ~ArmoredTrainBoss() = default;

    void Initialize(Object3dCommon* object3dCommon, const Vector3& startPos, uint32_t skyboxTexIndex);
    void Update(const Vector3& cameraPos, const Vector3& playerForward, float playerSpeed, Player* player, std::list<std::unique_ptr<EnemyBullet>>& enemyBullets, float gameSpeed = 1.0f);
    void Draw();
    void DrawCollider();

    // 被弾判定（部位破壊システム対応）
    bool CheckCollision(const Sphere& bulletSphere, int* outCarriageIndex = nullptr, BossPartType* outPartType = nullptr, int* outPartIndex = nullptr, Vector3* outHitPos = nullptr);
    bool CheckRaycast(const Ray& ray, float* outDist, int* outCarriageIndex = nullptr, BossPartType* outPartType = nullptr, int* outPartIndex = nullptr);
    void OnDamaged(int carriageIndex, int damage = 1, BossPartType partType = BossPartType::Carriage, int partIndex = -1);

    // 進行レール設定
    void SetRail(const Rail* rail) { rail_ = rail; }
    void SetRailProgress(float progress) { railProgress_ = progress; }
    float GetRailProgress() const { return railProgress_; }

    // スケール倍率設定
    void SetScaleMultiplier(float scale);
    float GetScaleMultiplier() const { return scaleMultiplier_; }

    // プレイヤー連動チェイス設定
    void SetFollowPlayer(bool follow) { isFollowPlayer_ = follow; }
    bool IsFollowPlayer() const { return isFollowPlayer_; }
    void SetDesiredLeadDistance(float dist) { desiredLeadDistance_ = dist; targetDynamicLeadDistance_ = dist; }
    float GetDesiredLeadDistance() const { return desiredLeadDistance_; }
    float GetDynamicLeadDistance() const { return dynamicLeadDistance_; }

    // タイムオーバー時：超巨大主砲の照準旋回・発射リコイル制御
    void UpdateTimeOverCannon(const Vector3& targetPlayerPos, float timeOverTimer, float dt);
    Vector3 GetBigCannonMuzzlePosition() const;
    void SetRandomLeadDistanceEnabled(bool enabled) { isRandomLeadEnabled_ = enabled; }
    bool IsRandomLeadDistanceEnabled() const { return isRandomLeadEnabled_; }
    bool IsShortDistanceOpportunity() const { return isShortDistanceOpportunity_; }
    void SetRushSpeedBonus(float speed) { rushSpeedBonus_ = speed; }
    float GetRushSpeedBonus() const { return rushSpeedBonus_; }
    float GetCurrentSpeed() const { return currentSpeed_; }
    float GetTargetSpeed() const { return targetSpeed_; }
    float GetRelativeForwardDistance() const { return relativeForwardDist_; }
    float GetRearRelativeDistance() const {
        float totalLen = 115.0f;
        if (!carriages_.empty()) {
            totalLen = std::abs(carriages_.back().lengthOffset) + 20.0f;
        }
        return relativeForwardDist_ - totalLen;
    }
    bool IsCloseToPlayer() const { return isCloseToPlayer_; }
    bool IsOvertaking() const { return isOvertaking_; }
    void ResetChaseState();

    // 並走レーンオフセット設定（全車両の側面露出・ドッグファイト並走）
    void SetLaneOffset(float offset) { laneOffset_ = offset; }
    float GetLaneOffset() const { return laneOffset_; }
    void SetDynamicLaneChangeEnabled(bool enable) { isDynamicLaneChange_ = enable; }
    bool IsDynamicLaneChangeEnabled() const { return isDynamicLaneChange_; }

    // 状態
    bool IsActive() const { return isActive_; }
    void SetActive(bool active) { isActive_ = active; }
    bool IsCriticalHpOne() const { return isActive_ && !carriages_.empty() && carriages_[0].hp <= 1; }
    bool IsDefeated() const { return isDefeated_; }
    void SetDefeated(bool defeated) {
        isDefeated_ = defeated;
        if (defeated) {
            currentSpeed_ = 0.0f;
            targetSpeed_ = 0.0f;
        }
    }
    void Stop() {
        currentSpeed_ = 0.0f;
        targetSpeed_ = 0.0f;
    }
    float GetTotalHpRate() const;
    const Vector3& GetPosition() const { return position_; }
    const std::vector<TrainCarriage>& GetCarriages() const { return carriages_; }
    std::vector<TrainCarriage>& GetCarriagesMutable() { return carriages_; }
    void ApplyTransforms() { UpdateCarriageTransforms(); }

    // デジタル・グリッド・ディゾルブ用のアルファ値一括設定 (1.0f = 通常実体, 0.0f = 完全消滅)
    void SetDissolveAlpha(float alpha) {
        float a = std::clamp(alpha, 0.0f, 1.0f);
        for (auto& car : carriages_) {
            car.color.w = a;
            if (car.object) {
                Vector4 col = car.object->GetColor();
                col.w = a;
                car.object->SetColor(col);
            }
            for (auto& t : car.turrets) {
                if (t.object) {
                    Vector4 col = t.object->GetColor();
                    col.w = a;
                    t.object->SetColor(col);
                }
            }
            for (auto& p : car.missilePods) {
                if (p.object) {
                    Vector4 col = p.object->GetColor();
                    col.w = a;
                    p.object->SetColor(col);
                }
            }
            if (car.bigCannon) {
                if (car.bigCannon->baseObject) {
                    Vector4 col = car.bigCannon->baseObject->GetColor();
                    col.w = a;
                    car.bigCannon->baseObject->SetColor(col);
                }
                if (car.bigCannon->barrelObject) {
                    Vector4 col = car.bigCannon->barrelObject->GetColor();
                    col.w = a;
                    car.bigCannon->barrelObject->SetColor(col);
                }
            }
        }
    }

    // 先頭から後尾にかけて連鎖的に上から下へグリッド・ディゾルブさせる (totalProgress: 0.0f -> 1.0f)
    void SetSequentialDissolve(float totalProgress) {
        size_t count = carriages_.size();
        if (count == 0) return;
        for (size_t i = 0; i < count; ++i) {
            // 先頭車両(0)から後尾車両(3)へ少しずつ開始タイミングを遅らせる
            float delay = static_cast<float>(i) * 0.14f;
            float duration = 0.58f;
            float carDissolve = std::clamp((totalProgress - delay) / duration, 0.0f, 1.0f);
            float a = 1.0f - carDissolve;

            auto& car = carriages_[i];
            car.color.w = a;
            if (car.object) {
                Vector4 col = car.object->GetColor();
                col.w = a;
                car.object->SetColor(col);
            }
            for (auto& t : car.turrets) {
                if (t.object) {
                    Vector4 col = t.object->GetColor();
                    col.w = a;
                    t.object->SetColor(col);
                }
            }
            for (auto& p : car.missilePods) {
                if (p.object) {
                    Vector4 col = p.object->GetColor();
                    col.w = a;
                    p.object->SetColor(col);
                }
            }
            if (car.bigCannon) {
                if (car.bigCannon->baseObject) {
                    Vector4 col = car.bigCannon->baseObject->GetColor();
                    col.w = a;
                    car.bigCannon->baseObject->SetColor(col);
                }
                if (car.bigCannon->barrelObject) {
                    Vector4 col = car.bigCannon->barrelObject->GetColor();
                    col.w = a;
                    car.bigCannon->barrelObject->SetColor(col);
                }
            }
        }
    }

    // 処刑フィニッシャー用：ボスのHPを1（処刑可能状態）にする
    void SetHpToOneForExecution() {
        if (carriages_.empty()) return;
        isActive_ = true;
        isDefeated_ = false;
        // 先頭機関車のみHP1、他の護衛車両はHP0にしてメインコアを露出
        carriages_[0].hp = 1;
        carriages_[0].isDestroyed = false;
        carriages_[0].color = { 1.0f, 1.0f, 1.0f, 1.0f };
        if (carriages_[0].object) carriages_[0].object->SetColor(carriages_[0].color);
        for (size_t i = 1; i < carriages_.size(); ++i) {
            carriages_[i].hp = 0;
            carriages_[i].isDestroyed = true;
            carriages_[i].color = { 1.0f, 1.0f, 1.0f, 1.0f };
            if (carriages_[i].object) carriages_[i].object->SetColor(carriages_[i].color);
        }
        isCoreExposed_ = true;
        SetDissolveAlpha(1.0f);
        UpdateCarriageTransforms();
    }

    // JSONによるパーツ配置・設定の読み込み／保存
    bool LoadSettingsJson(const std::string& filepath = "resources/json/settings/boss_settings.json");
    bool SaveSettingsJson(const std::string& filepath = "resources/json/settings/boss_settings.json") const;

    // パーツ動的追加・削除
    void AddTurret(int carriageIndex, const Vector3& localPos = { 0.0f, 5.0f, 0.0f }, const Vector3& localRot = { 0.0f, 0.0f, 0.0f });
    void RemoveTurret(int carriageIndex, int turretIndex);
    void AddMissilePod(int carriageIndex, const Vector3& localPos = { 0.0f, 5.0f, 0.0f }, const Vector3& localRot = { 0.0f, 0.0f, 0.0f });
    void RemoveMissilePod(int carriageIndex, int podIndex);

    // 生存しているミサイルポッドがあるか判定
    bool HasAliveMissilePods() const {
        for (const auto& car : carriages_) {
            if (car.isDestroyed) continue;
            for (const auto& pod : car.missilePods) {
                if (!pod.isDestroyed) return true;
            }
        }
        return false;
    }

    // ミサイルポッドのロックオン警告状態（HUD警告UI連携用）
    bool IsAnyMissilePodLockingOn() const {
        if (isDefeated_ || !HasAliveMissilePods()) return false;
        return (missileSeqState_ == BossMissileSequence::LOCK_ON || missileSeqState_ == BossMissileSequence::FIRING);
    }
    float GetMissilePodLockOnProgress() const {
        if (isDefeated_ || !HasAliveMissilePods()) return 0.0f;
        if (missileSeqState_ == BossMissileSequence::LOCK_ON) {
            return std::clamp(missileSeqTimer_ / missileLockOnDuration_, 0.0f, 1.0f);
        } else if (missileSeqState_ == BossMissileSequence::FIRING) {
            return 1.0f;
        }
        return 0.0f;
    }
    bool IsMissilePodBurstFiring() const {
        if (isDefeated_ || !HasAliveMissilePods()) return false;
        return (missileSeqState_ == BossMissileSequence::FIRING);
    }

    // 各車両部位のワールド座標と破壊判定（マルチロックオン用）
    Vector3 GetCarriagePosition(int carriageIndex) const {
        if (carriageIndex >= 0 && carriageIndex < static_cast<int>(carriages_.size())) {
            return carriages_[carriageIndex].position;
        }
        return position_;
    }
    bool IsCarriageDestroyed(int carriageIndex) const {
        if (carriageIndex >= 0 && carriageIndex < static_cast<int>(carriages_.size())) {
            return carriages_[carriageIndex].isDestroyed;
        }
        return true;
    }

    // 地面吸着（レイキャスト自動接地）設定
    void SetTerrainObjects(const std::vector<std::unique_ptr<Object3d>>* terrainObjects) { terrainObjects_ = terrainObjects; }
    void SetGroundSnapEnabled(bool enable) { isGroundSnapEnabled_ = enable; }
    bool IsGroundSnapEnabled() const { return isGroundSnapEnabled_; }
    void SetGroundSnapOffset(float offset) { groundSnapOffset_ = offset; }
    float GetGroundSnapOffset() const { return groundSnapOffset_; }

private:
    bool GetGroundHeight(float x, float z, float startY, float* outGroundY, Vector3* outNormal = nullptr) const;
    void UpdateCarriageTransforms();
    void ExecuteAttacks(Player* player, std::list<std::unique_ptr<EnemyBullet>>& enemyBullets, float gameSpeed);

private:
    Object3dCommon* object3dCommon_ = nullptr;
    uint32_t skyboxTexIndex_ = 0;

    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
    const Rail* rail_ = nullptr;
    float railProgress_ = 0.0f;

    // チェイス走行制御
    bool isFollowPlayer_ = true;        // プレイヤー連動モード
    float currentSpeed_ = 50.0f;        // ボスの現在の実速度 (m/s)
    float targetSpeed_ = 50.0f;         // 目標速度 (m/s)
    float desiredLeadDistance_ = 145.0f; // 基準となるプレイヤー前方の並走距離 (m) (最後尾が自機前方約30m)
    float dynamicLeadDistance_ = 145.0f; // 現在滑らかに補間中の目標並走距離 (m)
    float targetDynamicLeadDistance_ = 145.0f; // ランダム周期で設定される目標並走距離 (m)
    bool isRandomLeadEnabled_ = true;   // ランダム並走距離短縮の有効化
    bool isShortDistanceOpportunity_ = false; // 現在接近攻撃チャンス中か
    float leadDistanceChangeTimer_ = 0.0f; // 次の距離変更までのタイマー (秒)
    float rushSpeedBonus_ = 45.0f;      // 接近・追い抜き時の追加速度 (m/s)
    float relativeForwardDist_ = 0.0f;  // プレイヤーから見た前後の相対距離 (m)
    bool isCloseToPlayer_ = false;      // プレイヤーの近場に到達したか
    bool isOvertaking_ = false;         // プレイヤーを追い抜き中か

    bool isActive_ = false;
    bool isDefeated_ = false;
    bool isCoreExposed_ = false; // 全武装破壊でメインコア露出

    float scaleMultiplier_ = 1.0f; // ボス全体スケール倍率

    // 並走レーンオフセット・動的車線変更
    float laneOffset_ = -10.0f;          // レール進行方向に対する左右並走オフセット (m)
    bool isDynamicLaneChange_ = true;    // 緩やかな車線変更により全車両の側面を露出
    float laneChangeTimer_ = 0.0f;

    // 地面吸着（レイキャスト）用メンバ
    const std::vector<std::unique_ptr<Object3d>>* terrainObjects_ = nullptr;
    bool isGroundSnapEnabled_ = true;
    float groundSnapOffset_ = 0.0f; // 車体底面からの微調整オフセット
    uint32_t groundCheckFrame_ = 0; // 車両ごとのレイキャスト分散カウンター（CPU負荷軽減）

    // ミサイル一斉射撃シーケンス管理（6発発射 ➔ 全滅監視 ➔ 再ロックオン）
    enum class BossMissileSequence {
        IDLE,           // 待機インターバル
        LOCK_ON,        // ロックオンチャージ中（約2.5秒）
        FIRING,         // 6発連続発射
        WAIT_ALL_CLEAR  // フィールド上のミサイル全滅待機
    };
    BossMissileSequence missileSeqState_ = BossMissileSequence::IDLE;
    float missileSeqTimer_ = 0.0f;
    float missileLockOnDuration_ = 160.0f;  // 約2.6秒の長めのロックオン警告猶予
    float missileIdleInterval_ = 180.0f;    // 全滅後の通常待機インターバル（約3.0秒）
    int missileFiredCount_ = 0;             // 今回のバーストで発射したミサイル数 (最大6)
    float missileFireIntervalTimer_ = 0.0f; // 1発ごとの発射間隔タイマー (約10f / 0.16s)

    std::vector<TrainCarriage> carriages_;
    float defeatTimer_ = 0.0f;
    float defeatExplosionTimer_ = 0.0f;

    // HP1瀕死時の崩壊パーティクル演出
    struct CriticalGlitchParticle {
        std::unique_ptr<Object3d> obj;
        Vector3 position;
        Vector3 velocity;
        Vector3 rotation;
        Vector3 rotSpeed;
        Vector3 scale;
        Vector4 color;
        float life = 0.0f;
        float maxLife = 0.8f;
    };
    std::vector<CriticalGlitchParticle> criticalParticles_;
    float criticalGlitchTimer_ = 0.0f;
    float criticalParticleAccum_ = 0.0f;
    void UpdateCriticalGlitchState(float dt);
};
