#pragma once
#include "KHEngine/Graphics/2d/Sprite.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Sound/Core/Sound.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"
#include "KHEngine/Graphics/3d/Particle/ParticleRenderer.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Graphics/3d/Particle/Particle.h"
#include "KHEngine/Graphics/3d/Particle/ParticleManager.h"
#include "KHEngine/Math/Matrix4x4.h"
#include "KHEngine/Graphics/Billboard/Billboard.h"
#include "KHEngine/Core/Framework/BaseScene.h"
#include "KHEngine/Graphics/3d/Skybox/Skybox.h"
#include "KHEngine/Graphics/3d/Particle/ParticleEffect.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/PlayerBullet.h"
#include "Game/Actor/Bullet/PlayerMissile.h"
#include "Game/Actor/Bullet/EnemyBullet.h"
#include "Game/System/Rail.h"
#include "Game/System/RailCameraController.h"
#include "Game/Actor/Enemy/Enemy.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "Game/Sequence/BossFinisherSequence.h"
#include "Game/Sequence/OpeningCutsceneSequence.h"
#include "Game/Sequence/GameOverSequence.h"
#include "Game/Sequence/GameClearSequence.h"
#include "Game/Sequence/PauseMenu.h"
#include "Game/Actor/Obstacle/Obstacle.h"
#include "Game/Actor/Item/EnhanceRing.h"
#include "Game/Actor/Item/MemoryItem.h"
#include "Game/System/CollisionManager.h"
#include "Game/System/BossBattleController.h"
#include "Game/System/GamePlayHUD.h"
#include "Game/System/GamePlayVfxManager.h"
#include "Game/System/GamePlayLevelManager.h"
#include "Game/System/GamePlayWorld.h"
#include "Game/System/DemoPlayController.h"
#include "Game/Scene/GamePlayEditor.h"
#include <vector>
#include <list>
#include <random>
#include <memory>

class GamePlayScene : public BaseScene
{
public:
    enum class GamePhase {
        START_CUTSCENE,
        PLAYING,
        GAMEOVER,
        CLEAR
    };

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DrawUI() override;
    void Finalize() override;

    
    
    
    void ReloadLevel();

    
    
    
    void ReloadEnemiesOnly();
    void SeamlessResetForRetry();

    // エディタ・外部連携用アクセサ
    void ApplyVirtualSpaceStyle();
    void SpawnMemoryItem(const Vector3& pos, MemoryType type = MemoryType::CYBER_BIT) { world_.SpawnMemoryItem(pos, type, memoryItems_); }
    void ClearMemoryItems() { memoryItems_.clear(); }
    void TriggerTimeOverCannonSequence(bool nonLethal = true);
    void StartBossWarningSequence();
    void StartOpeningCutscene();
    void StartGameOverSequence();
    void StartClearSequence();

    Player* GetPlayer() const { return player_.get(); }
    SceneManager* GetSceneManager() const { return BaseScene::GetSceneManager(); }
    Camera* GetMainCamera() const { return camera.get(); }
    RailCameraController* GetRailCameraController() const { return railCameraController_.get(); }
    ArmoredTrainBoss* GetArmoredTrainBoss() const { return armoredTrainBoss_.get(); }
    BossBattleController* GetBossBattleController() { return &bossBattle_; }
    GamePlayHUD* GetHUD() { return &hud_; }
    DemoPlayController* GetDemoController() { return &demoController_; }
    GameOverSequence* GetGameOverSequence() { return &gameOverSequence_; }
    GameClearSequence* GetClearSequence() { return &clearSequence_; }
    OpeningCutsceneSequence* GetOpeningSequence() { return &openingSequence_; }
    BossFinisherSequence* GetBossFinisherSequence() const { return bossFinisherSequence_.get(); }
    const std::vector<std::unique_ptr<Rail>>& GetMainRails() const { return mainRails_; }
    Rail* GetBossRail() const { return bossRail_.get(); }
    const std::vector<std::unique_ptr<Object3d>>& GetModelInstances() const { return modelInstances; }
    const std::list<std::unique_ptr<Enemy>>& GetEnemies() const { return enemies_; }
    const std::list<std::unique_ptr<Obstacle>>& GetObstacles() const { return obstacles_; }
    const std::list<std::unique_ptr<MemoryItem>>& GetMemoryItems() const { return memoryItems_; }

    bool IsPlaying() const { return isPlaying_; }
    void SetPlaying(bool playing) { isPlaying_ = playing; }
    bool IsBossSpawned() const { return isBossSpawned_; }
    void SetBossSpawned(bool spawned) { isBossSpawned_ = spawned; }
    bool IsDrawRail() const { return isDrawRail_; }
    void SetDrawRail(bool draw) { isDrawRail_ = draw; }
    bool IsDrawCollider() const { return isDrawCollider_; }
    void SetDrawCollider(bool draw) { isDrawCollider_ = draw; }
    bool IsDrawTerrainWireframe() const { return isDrawTerrainWireframe_; }
    void SetDrawTerrainWireframe(bool draw) { isDrawTerrainWireframe_ = draw; }
    bool IsVirtualSpace() const { return isVirtualSpace_; }
    void SetVirtualSpace(bool v) { isVirtualSpace_ = v; }
    float GetBaseGameSpeed() const { return baseGameSpeed_; }
    void SetBaseGameSpeed(float speed) { baseGameSpeed_ = speed; }
    float GetTerrainMaterializeProgress() const { return terrainMaterializeProgress_; }
    void SetTerrainMaterializeProgress(float p) { terrainMaterializeProgress_ = p; }
    float GetLastLoadTimeMs() const { return lastLoadTimeMs_; }
    GamePhase GetGamePhase() const { return gamePhase_; }
    void SetGamePhase(GamePhase phase) { gamePhase_ = phase; }
    void ResetCameraState(const Vector3& rot = {0.0f, 0.0f, 0.0f}, float yaw = 0.0f, float bank = 0.0f)
    {
        currentCameraRot_ = rot;
        lastCameraYaw_ = yaw;
        currentCameraBank_ = bank;
    }

private:
    GamePlayLevelManager levelManager_;
    LevelLoadContext BuildLevelContext();
    GamePlayWorld world_;
    GamePlayWorldContext BuildWorldContext();
    std::vector<std::unique_ptr<Object3d>> modelInstances;

    std::unique_ptr<Camera> camera;
    std::unique_ptr<Camera> debugCamera_;
    Camera* activeCamera_ = nullptr;
    std::unique_ptr<Object3d> cameraObject_;
    
    
    std::vector<std::unique_ptr<Rail>> mainRails_;
    std::unique_ptr<Rail> bossRail_; // ボス戦専用レール (BossRail)
    std::unique_ptr<RailCameraController> railCameraController_;
    float baseGameSpeed_ = 1.0f; 
    float gameSpeed_ = 1.0f;     
    float lockOnMaxDistance_ = 350.0f; // ロックオン最大射程距離     
    
    
    bool isJustDodgeActive_ = false;
    float justDodgeTimer_ = 0.0f;
    float justDodgeMaxTime_ = 60.0f; 
    float justDodgeSlowSpeed_ = 0.2f; 
    std::vector<std::unique_ptr<Model>> railModels_;
    std::unique_ptr<Model> enemyRailModel_;
    std::vector<std::unique_ptr<Object3d>> railVisualizers_;
    std::vector<std::unique_ptr<Object3d>> enemyRailVisualizers_;
    bool isDrawRail_ = false;
    std::unique_ptr<Skybox> skybox_;
    bool isVirtualSpace_ = true; // スターフォックス風サイバー仮想空間ビジュアル有効フラグ
#ifdef ENABLE_EDITOR
    bool isPlaying_ = false;
#else
    bool isPlaying_ = true;
#endif

    DemoPlayController demoController_;

    
    Vector3 currentCameraRot_ = {0.0f, 0.0f, 0.0f};
    float lastCameraYaw_ = 0.0f;
    float currentCameraBank_ = 0.0f;

    
    std::random_device seedGenerator;
    std::mt19937 randomEngine{ seedGenerator() };

    
    GamePlayVfxManager vfx_;

    int currentEditEffectIndex_ = 0;

    
    std::unique_ptr<Player> player_;

    GamePlayHUD hud_;
    uint32_t whiteTexIndex_ = 0;
    uint32_t ringIconOutlineTex_ = 0;
    uint32_t ringIconTex_ = 0;

    std::list<std::unique_ptr<PlayerBullet>> bullets_;
    
    std::list<std::unique_ptr<PlayerMissile>> missiles_;

    
    std::list<std::unique_ptr<Enemy>> enemies_;
    bool hasEnemySpawned_ = false; 
    
    // 装甲列車ボス
    std::unique_ptr<ArmoredTrainBoss> armoredTrainBoss_;
    bool isBossSpawned_ = false;

    // 電脳フィニッシャー（クリア演出シークエンス）
    std::unique_ptr<BossFinisherSequence> bossFinisherSequence_;

    // ボス戦進行・タイムオーバー演出・専用UI・設定I/O管理
    BossBattleController bossBattle_;
    BossBattleContext BuildBossBattleContext();
    void UpdateBossBattle(float dt);

    std::list<std::unique_ptr<EnemyBullet>> enemyBullets_;
    
    std::unique_ptr<CollisionManager> collisionManager_;
    std::list<std::unique_ptr<Obstacle>> obstacles_;
    std::list<std::unique_ptr<EnhanceRing>> enhanceRings_;
    std::list<std::unique_ptr<MemoryItem>> memoryItems_;

    
#ifdef USE_IMGUI
    bool isDrawCollider_ = false; // デバッグ用コライダーはデフォルトOFF
#else
    bool isDrawCollider_ = false;
#endif
    bool isDrawTerrainWireframe_ = false; // 判定計算が行われているポリゴンのみワイヤーフレーム表示（F3キー）
    float cameraShakeTimer_ = 0.0f;
    int enemyKillShakeFrames_ = 0; // 敵撃破時の微小カメラシェイク（1〜2フレーム）
    float lastLoadTimeMs_ = 0.0f;
    int score_ = 0;

    // 戦績評価（リザルトパネル）用トラッキングデータ
    int totalEnemiesCount_ = 0;
    int killedEnemiesCount_ = 0;
    float stageElapsedTime_ = 0.0f;
    float bossElapsedTime_ = 0.0f;

    GamePhase gamePhase_ = GamePhase::START_CUTSCENE;

    // スタート演出（仮想空間地形形成 ＆ 自機復元マテリアライズ）用進行度（ゲームオーバー演出でも共有）
    float terrainMaterializeProgress_ = 1.0f;

    OpeningCutsceneSequence openingSequence_;
    OpeningCutsceneContext BuildOpeningContext();

    void UpdateOpeningCutscene(float dt);

    // ポーズメニュー（Pause Menu）
    PauseMenu pauseMenu_;
    bool IsPaused() const { return pauseMenu_.IsPaused(); }

    // ゲームオーバー演出（GameOver Sequence）
    GameOverSequence gameOverSequence_;
    GameOverContext BuildGameOverContext();
    void UpdateGameOverSequence(float dt);

    // ゲームクリア演出（GameClear Sequence）
    GameClearSequence clearSequence_;
    GameClearContext BuildClearContext();
    void UpdateClearSequence(float dt);

    // Update() 内部フェーズ分割ヘルパー
    void HandleGlobalHotkeys(class EngineServices* services, class Input* input);
    void UpdateDemoPlay(class EngineServices* services, float dt);
    void UpdateRailAndCamera(class EngineServices* services, float dt, float& unscaledGameSpeed);
    void UpdateActorsAndCollisions(float dt, float unscaledGameSpeed);
    void UpdateCameraShakeAndVfx(float dt);
    void CheckGameEndConditions();

#ifdef ENABLE_EDITOR
    GamePlayEditor editor_;
#endif
};

