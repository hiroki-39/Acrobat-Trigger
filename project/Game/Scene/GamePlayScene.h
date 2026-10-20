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
#include "Game/Actor/Obstacle/Obstacle.h"
#include "Game/Actor/Item/EnhanceRing.h"
#include "Game/System/CollisionManager.h"
#include <vector>
#include <list>
#include <random>
#include <memory>

class GamePlayScene : public BaseScene
{
public:
    void Initialize() override;
    void Update() override;
    void Draw() override;
    void DrawUI() override;
    void Finalize() override;

    
    
    
    void ReloadLevel();

    
    
    
    void ReloadEnemiesOnly();

private:
    
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
    void ApplyVirtualSpaceStyle();
#ifdef ENABLE_EDITOR
    bool isPlaying_ = false;
#else
    bool isPlaying_ = true;
#endif

    // デモプレイ演出用タイマー（一定時間後にタイトルへ戻る）
    float demoTimer_ = 0.0f;

    
    Vector3 currentCameraRot_ = {0.0f, 0.0f, 0.0f};
    float lastCameraYaw_ = 0.0f;
    float currentCameraBank_ = 0.0f;

    
    std::random_device seedGenerator;
    std::mt19937 randomEngine{ seedGenerator() };

    
    ParticleEffect thrusterEffect_;   
    ParticleEffect explosionEffect_;  
    ParticleEffect hitEffect_;        
    ParticleEffect dodgeEffect_;      
    ParticleEffect trailEffect_;      
    ParticleEffect missileSmokeEffect_; 
    ParticleEffect ringEffect_;
    ParticleEffect healRingEffect_; // リング取得時のキラキラエフェクト
    ParticleEffect windEffect_;     // 気流線（風の筋）エフェクト

    
    int currentEditEffectIndex_ = 0;

    
    std::unique_ptr<Player> player_;

    
    std::unique_ptr<Sprite> hpBarBgSprite_;
    std::unique_ptr<Sprite> hpBarSprite_;
    std::unique_ptr<Sprite> boostBarBgSprite_;
    std::unique_ptr<Sprite> boostBarSprite_;
    uint32_t whiteTexIndex_ = 0;

    // 強化リング獲得アイコン（ブーストゲージ上部に横2つ配置）
    static const int kMaxEnhanceRingIcons = 2;
    std::array<std::unique_ptr<Sprite>, kMaxEnhanceRingIcons> ringGetOutlineSprites_;
    std::array<std::unique_ptr<Sprite>, kMaxEnhanceRingIcons> ringGetIconSprites_;
    uint32_t ringIconOutlineTex_ = 0;
    uint32_t ringIconTex_ = 0;
    int acquiredEnhanceRingCount_ = 0;
    Vector4 ringIconColor_ = { 1.0f, 0.85f, 0.2f, 1.0f };    // 中身アイコン色（視認性の高いゴールド）
    Vector4 ringOutlineColor_ = { 1.0f, 1.0f, 1.0f, 0.9f }; // 外枠アウトライン色（ホワイト）
    
    std::list<std::unique_ptr<PlayerBullet>> bullets_;
    
    std::list<std::unique_ptr<PlayerMissile>> missiles_;

    
    std::list<std::unique_ptr<Enemy>> enemies_;
    bool hasEnemySpawned_ = false; 
    
    // 装甲列車ボス
    std::unique_ptr<ArmoredTrainBoss> armoredTrainBoss_;
    bool isBossSpawned_ = false;

    // ボス戦進行ステップ
    enum class BossBattleStep {
        NOT_ACTIVE,        // 通常道中
        WARNING_ALERT,     // WARNING警告演出中
        BATTLE,            // ボス戦闘中
        DEFEATED_SEQUENCE, // 撃破演出中（連続大爆発・スロー）
        FINISHED           // 撃破完了
    };
    BossBattleStep bossBattleStep_ = BossBattleStep::NOT_ACTIVE;
    float bossSpawnProgressThreshold_ = 0.60f; // 自動出現するレール進行度閾値（60%地点）
    float bossWarningTimer_ = 0.0f;
    const float kBossWarningDuration = 2.8f;
    float bossDefeatTimer_ = 0.0f;
    const float kBossDefeatDuration = 3.5f;
    float bossDisplayedHpRate_ = 1.0f; // 遅延追従HPバー用
    float bossWarningSirenTimer_ = 0.0f;

    // ボス戦専用UIスプライト
    std::unique_ptr<Sprite> bossHpBarBgSprite_;
    std::unique_ptr<Sprite> bossHpBarDelaySprite_;
    std::unique_ptr<Sprite> bossHpBarSprite_;
    std::unique_ptr<Sprite> bossWarningBandTopSprite_;
    std::unique_ptr<Sprite> bossWarningBandBottomSprite_;
    std::unique_ptr<Sprite> bossWarningFlashSprite_;

    void StartBossWarningSequence();
    void UpdateBossBattle(float dt);
    void DrawBossUI();
    void LoadBossSettings();
    void SaveBossSettings();

    std::list<std::unique_ptr<EnemyBullet>> enemyBullets_;
    
    std::unique_ptr<CollisionManager> collisionManager_;
    std::list<std::unique_ptr<Obstacle>> obstacles_;
    std::list<std::unique_ptr<EnhanceRing>> enhanceRings_;

    
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

    // 敵撃破時の3Dスコア加算ポップ演出
    struct ScorePopup {
        Vector3 worldPos;     // 敵撃破時の3Dワールド座標
        int score;            // 加算スコア (+100 など)
        float timer;          // 経過秒数
        float duration;       // 表示持続時間
    };
    std::vector<ScorePopup> scorePopups_;

    // 進行フェーズ
    enum class GamePhase {
        START_CUTSCENE,
        PLAYING,
        GAMEOVER,
        CLEAR
    };
    GamePhase gamePhase_ = GamePhase::START_CUTSCENE;
    float cutsceneTimer_ = 0.0f;
    float missionStartTextTimer_ = 0.0f;
    const float kCutsceneDuration = 4.7f;

    // スタート演出（仮想空間地形形成 ＆ 自機復元マテリアライズ）用データ構造
    struct ReconstructShard {
        std::unique_ptr<Object3d> object = nullptr;
        Vector3 startPos = { 0.0f, 0.0f, 0.0f };
        Vector3 targetPos = { 0.0f, 0.0f, 0.0f };
        Vector3 rotation = { 0.0f, 0.0f, 0.0f };
        Vector3 rotSpeed = { 0.0f, 0.0f, 0.0f };
        Vector3 scale = { 0.20f, 0.20f, 0.20f };
        Vector4 color = { 0.3f, 1.0f, 0.9f, 1.0f };
    };
    std::vector<ReconstructShard> reconstructShards_;
    float terrainMaterializeProgress_ = 1.0f;
    float playerReconstructAlpha_ = 1.0f;

    void StartOpeningCutscene();
    void UpdateOpeningCutscene(float dt);
    void DrawCutsceneUI();

    // ポーズメニュー（Pause Menu）
    enum class PauseMenuOption {
        Resume = 0,
        Retry = 1,
        Title = 2
    };
    bool isPaused_ = false;
    PauseMenuOption pauseSelectedOption_ = PauseMenuOption::Resume;
    void UpdatePauseMenu();
    void DrawPauseUI();
    bool IsPaused() const { return isPaused_; }

    // ゲームオーバー演出（GameOver Sequence）
    enum class GameOverStep {
        FALLING,        // 被弾・地上へ落下中（カメラ停止、自機キリモミ回転、黒煙放出）
        EXPLODED,       // 地上激突・大爆発（画面揺れ、機体非表示、余韻）
        SHOW_UI         // ゲームオーバーUI表示（「GAME OVER」、リトライ/タイトル選択待ち）
    };
    enum class GameOverMenuOption {
        Retry = 0,
        Title = 1
    };
    GameOverStep gameOverStep_ = GameOverStep::FALLING;
    GameOverMenuOption gameOverSelectedOption_ = GameOverMenuOption::Retry;
    float gameOverTimer_ = 0.0f;
    Vector3 gameOverPlayerFallPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 gameOverPlayerFallRot_ = { 0.0f, 0.0f, 0.0f };
    float gameOverFallVelocity_ = 0.0f;

    void StartGameOverSequence();
    void UpdateGameOverSequence(float dt);
    void DrawGameOverUI();

    // ゲームクリア演出（GameClear Sequence）
    enum class ClearStep {
        ASCENDING,      // 自機が上空へ急上昇・飛び去る（カメラ停止、スラスター噴射）
        SHOW_UI         // 「MISSION COMPLETE」、スコア表示、SPACEでタイトルへ戻る入力待ち
    };
    ClearStep clearStep_ = ClearStep::ASCENDING;
    float clearTimer_ = 0.0f;
    float clearTotalTimer_ = 0.0f;
    Vector3 clearStartPlayerPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 clearPlayerAscentPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 clearPlayerAscentRot_ = { 0.0f, 0.0f, 0.0f };
    float clearAscentSpeed_ = 0.0f;

    void StartClearSequence();
    void UpdateClearSequence(float dt);
    void DrawClearUI();
};

