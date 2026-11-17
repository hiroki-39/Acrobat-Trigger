#pragma once
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"
#include "KHEngine/Math/Vector3.h"
#include "KHEngine/Math/Vector4.h"
#include <vector>
#include <memory>
#include <array>
#include <string>

class Player;
class ArmoredTrainBoss;
class RailCameraController;
class Object3dCommon;

/// <summary>
/// 電脳仮想空間の世界観に統一したボス撃破（処刑）シークエンス管理クラス
/// 設計仕様書（フェーズ①〜⑤）をカプセル化し、GamePlaySceneから独立して動作する。
/// </summary>
class BossFinisherSequence {
public:
    enum class Phase {
        INACTIVE = 0,                // 通常戦闘中（非アクティブ）
        PHASE_1_TRANSITION,          // ① 処刑フェーズ移行 (HP1耐え、アーマーブレイク、赤メモリ強制吸収、武器オーバーライド)
        PHASE_2_AUTO_LOCKON,         // ② 自動ロックオン・チャージ (スペース長押し、全車両コア順次ロック、FOV拡大、エネルギー収束)
        PHASE_3_FINISHER_FIRE,       // ③ 特殊弾発射 (スペース離す/0.5s救済、処刑ミサイル一斉射、ネオンマゼンタ軌跡、0.2sヒットストップ+衝撃波)
        PHASE_4_DATA_COLLAPSE,       // ④ 処刑カメラ＆データ崩壊 (側面〜斜め前方回り込み、RGBグリッチ、ポリゴン/キューブ分解霧散)
        PHASE_5_MISSION_ACCOMPLISHED,// ⑤ 任務完了表示 (戦闘HUDパージ、自機オーロラ夜空上昇旋回、サイバー調ブラケット+「任務完了」ネオン表示)
        COMPLETED                    // シークエンス完了（リザルト画面展開へ引き渡す待機状態）
    };

    BossFinisherSequence() = default;
    ~BossFinisherSequence() = default;

    void Initialize(Object3dCommon* object3dCommon, Camera* camera, RailCameraController* railCam, uint32_t skyboxTexIndex);
    void SetTerrainObjects(const std::vector<std::unique_ptr<Object3d>>* terrainObjects) { terrainObjects_ = terrainObjects; }
    void StartSequence(ArmoredTrainBoss* boss, Player* player);
    void Reset(Player* player = nullptr);
    void Update(float dt, ArmoredTrainBoss* boss, Player* player, float& inOutGameSpeed);
    void Draw3D();
    void DrawUI();
    void DrawImGuiDebug(ArmoredTrainBoss* boss, Player* player);

    bool IsActive() const { return phase_ != Phase::INACTIVE && phase_ != Phase::COMPLETED; }
    bool IsCompleted() const { return phase_ == Phase::COMPLETED; }
    bool IsMoviePhase() const {
        return phase_ == Phase::PHASE_3_FINISHER_FIRE ||
               phase_ == Phase::PHASE_4_DATA_COLLAPSE ||
               phase_ == Phase::PHASE_5_MISSION_ACCOMPLISHED ||
               phase_ == Phase::COMPLETED;
    }
    Phase GetCurrentPhase() const { return phase_; }

    // 強制的にフェーズをジャンプ（デバッグ用）
    void SetPhase(Phase p) { phase_ = p; phaseTimer_ = 0.0f; }

private:
    // 各フェーズの個別更新処理
    void UpdatePhase1Transition(float dt, ArmoredTrainBoss* boss, Player* player);
    void UpdatePhase2AutoLockOn(float dt, ArmoredTrainBoss* boss, Player* player);
    void UpdatePhase3FinisherFire(float dt, ArmoredTrainBoss* boss, Player* player, float& inOutGameSpeed);
    void UpdatePhase4DataCollapse(float dt, ArmoredTrainBoss* boss, Player* player);
    void UpdatePhase5MissionAccomplished(float dt, ArmoredTrainBoss* boss, Player* player);

    // ヘルパー
    void SetupCores(ArmoredTrainBoss* boss);
    void FireExecutionMissiles(Player* player);
    void SpawnCollapseShards(ArmoredTrainBoss* boss);
    bool IsWorldPointClearOfTerrain(const Vector3& worldPos, float radius) const;
    Vector3 ResolveSafeCameraLocalPos(const Vector3& desiredLocal, const Vector3& anchorOrigin, const Vector3& pRight, const Vector3& pUp, const Vector3& pFwd, const Vector3& bossHeadLocal, const Vector3& bossFwd) const;

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Camera* camera_ = nullptr;
    RailCameraController* railCam_ = nullptr;
    uint32_t skyboxTexIndex_ = 0;

    Phase phase_ = Phase::INACTIVE;
    float phaseTimer_ = 0.0f;
    float defaultCameraFov_ = 0.45f;

    // --- 弱点コア情報（4両編成：各車両1箇所） ---
    struct FinisherCore {
        int carriageIndex = -1;
        Vector3 worldPos = { 0.0f, 0.0f, 0.0f };
        bool isLocked = false;
        float lockProgress = 0.0f; // 0.0 -> 1.0 (ファンネル接近・収束進行度)
        float snapTimer = 0.0f;    // ロックオン確定スナップタイマー
        float startAngle = 0.0f;   // ファンネル飛来の出撃角度
        std::unique_ptr<Object3d> markerObj; // 六角形ハニカムマーカー
        std::unique_ptr<Object3d> sphereObj; // 発光コア球体
    };
    std::vector<FinisherCore> cores_;

    // --- ① 処刑用赤データメモリ強制飛来 ---
    std::unique_ptr<Object3d> redMemoryObj_;
    Vector3 redMemoryPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 redMemoryStartPos_ = { 0.0f, 0.0f, 0.0f };
    bool isRedMemoryAbsorbed_ = false;

    // --- ② チャージ＆自動ロックオン ---
    float spaceHoldTimer_ = 0.0f;
    const float kRequiredHoldTime_ = 1.0f; // 約1.0秒の長押しで4両全ロック
    bool isAllCoresLocked_ = false;

    // 溜めエネルギー収束粒子
    struct ChargeParticle {
        std::unique_ptr<Object3d> obj;
        Vector3 offset;
        Vector3 startOffset;
        float speed;
        Vector4 color;
    };
    std::vector<ChargeParticle> chargeParticles_;

    // --- ③ 処刑用特殊弾（missile.obj） ---
    struct ExecutionMissile {
        std::unique_ptr<Object3d> obj;
        Vector3 startPos;
        Vector3 controlPos;
        Vector3 position;
        Vector3 velocity;
        Vector3 rotation;
        int targetCoreIndex = -1;
        Vector3 targetPos;
        float hitTime = 1.0f;
        bool isHit = false;
        float lifeTimer = 0.0f;
        // リボントレイル（軌跡点）
        std::vector<Vector3> trailPoints;
    };
    std::vector<ExecutionMissile> missiles_;

    // 着弾ヒットストップ＆衝撃波リング
    float hitStopTimer_ = 0.0f;
    std::vector<std::unique_ptr<Object3d>> shockwaveRings_;
    float shockwaveScale_ = 0.1f;
    float shockwaveAlpha_ = 1.0f;

    // --- ④ 処刑カメラ＆データ崩壊 ---
    struct CollapseShard {
        std::unique_ptr<Object3d> obj;
        Vector3 position;
        Vector3 velocity;
        Vector3 rotation;
        Vector3 rotSpeed;
        Vector3 scale;
        Vector4 color;
        float life = 0.0f;
        float maxLife = 2.0f;
        bool isCube = true;
    };
    std::vector<CollapseShard> collapseShards_;
    std::vector<CollapseShard> wingTrailParticles_;
    float glitchPulseTimer_ = 0.0f;
    Vector3 cinematicCamStartPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 cinematicCamTargetPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 flybyCamPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 flybyLookStart_ = { 0.0f, 0.0f, 0.0f };
    Vector3 resolvedFrontCamPos_ = { 0.0f, 0.0f, 0.0f };
    bool hasResolvedFrontCam_ = false;
    float chosenCamSide_ = 1.0f;
    bool hasDissolveStarted_ = false;
    float dissolveTimer_ = 0.0f;
    float voxelSpawnAccum_ = 0.0f;

    // --- ⑤ 任務完了表示 ---
    Vector3 playerAscentStartPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 playerAscentPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 playerAscentRot_ = { 0.0f, 0.0f, 0.0f };
    float playerAscentSpeed_ = 0.0f;

    Player* player_ = nullptr;
    ArmoredTrainBoss* boss_ = nullptr;
    const std::vector<std::unique_ptr<Object3d>>* terrainObjects_ = nullptr;
};
