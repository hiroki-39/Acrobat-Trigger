#pragma once
#include "KHEngine/Math/MathCommon.h"
#include "KHEngine/Math/CollisionMath.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Graphics/3d/Particle/ParticleEffect.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/PlayerBullet.h"
#include "Game/Actor/Bullet/PlayerMissile.h"
#include "Game/Actor/Bullet/EnemyBullet.h"
#include "Game/Actor/Enemy/Enemy.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "Game/Actor/Obstacle/Obstacle.h"
#include "Game/Actor/Item/EnhanceRing.h"

#include <vector>
#include <list>
#include <memory>
#include <functional>

/// <summary>
/// 当たり判定処理に必要なコンテキスト情報
/// カプセル化の原則に基づき、必要な参照のみを安全に受け渡す
/// </summary>
struct CollisionContext {
    Player* player = nullptr;
    std::list<std::unique_ptr<PlayerBullet>>* playerBullets = nullptr;
    std::list<std::unique_ptr<PlayerMissile>>* playerMissiles = nullptr;
    std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
    ArmoredTrainBoss* boss = nullptr;
    bool isBossSpawned = false;
    std::list<std::unique_ptr<EnemyBullet>>* enemyBullets = nullptr;
    std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
    std::list<std::unique_ptr<EnhanceRing>>* enhanceRings = nullptr;
    const std::vector<std::unique_ptr<Object3d>>* terrainModels = nullptr;
    Object3d* cameraObject = nullptr;

    // パーティクルエフェクト
    ParticleEffect* hitEffect = nullptr;
    ParticleEffect* explosionEffect = nullptr;
    ParticleEffect* dodgeEffect = nullptr;
    ParticleEffect* ringEffect = nullptr;
    ParticleEffect* healRingEffect = nullptr;

    // ゲーム状態参照・反映用
    int* score = nullptr;
    float* cameraShakeTimer = nullptr;
    bool* isJustDodgeActive = nullptr;
    float* justDodgeTimer = nullptr;
    float justDodgeMaxTime = 60.0f;
    int* acquiredEnhanceRingCount = nullptr;
    int maxEnhanceRingIcons = 2;

    // ボス撃破コールバック
    std::function<void()> onBossDefeated = nullptr;

    // 敵撃破コールバック（座標と加算スコアを通知して微小カメラシェイク＆スコアポップ演出を発火）
    std::function<void(const Vector3& pos, int score)> onEnemyKilled = nullptr;
};

/// <summary>
/// 衝突判定管理クラス (CollisionManager)
/// オブジェクト指向のカプセル化の原則に則り、
/// ゲーム内のすべての当たり判定データと判定操作をひとまとめに管理する。
/// </summary>
class CollisionManager {
public:
    CollisionManager() = default;
    ~CollisionManager() = default;

    /// <summary>
    /// すべての衝突判定を一括処理
    /// </summary>
    void ProcessAllCollisions(CollisionContext& ctx);

    /// <summary>
    /// プレイヤー攻撃（通常弾・ミサイル） vs ザコ敵の衝突判定
    /// </summary>
    void CheckPlayerAttacksVsEnemies(CollisionContext& ctx);

    /// <summary>
    /// プレイヤー攻撃（通常弾・ミサイル） vs ボスの衝突判定
    /// </summary>
    void CheckPlayerAttacksVsBoss(CollisionContext& ctx);

    /// <summary>
    /// 敵弾 vs 障害物・地形・自機の衝突判定（ジャスト回避含む）
    /// </summary>
    void CheckEnemyBullets(CollisionContext& ctx);

    /// <summary>
    /// 強化リング・回復リング vs 自機の衝突判定
    /// </summary>
    void CheckEnhanceRingsVsPlayer(CollisionContext& ctx);

    /// <summary>
    /// プレイヤー攻撃（通常弾・ミサイル） vs 障害物の衝突判定
    /// </summary>
    void CheckPlayerAttacksVsObstacles(CollisionContext& ctx);

    /// <summary>
    /// 自機 vs 障害物・地形のメッシュ衝突判定（接触・めり込み・はじかれ）
    /// </summary>
    void CheckPlayerVsObstaclesAndTerrain(CollisionContext& ctx);

    /// <summary>
    /// プレイヤー通常弾 vs 地形メッシュの衝突判定
    /// </summary>
    void CheckPlayerBulletsVsTerrain(CollisionContext& ctx);
};
