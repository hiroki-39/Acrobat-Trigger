#pragma once
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Math/Vector3.h"
#include "KHEngine/Math/Vector4.h"
#include <vector>
#include <list>
#include <memory>
#include <functional>

class Player;
class Camera;
class RailCameraController;
class Object3dCommon;
class ParticleEffect;
class Obstacle;
class Enemy;
class ArmoredTrainBoss;
class PlayerBullet;
class PlayerMissile;
class EnemyBullet;
class EnhanceRing;
class SceneManager;

/// <summary>
/// ゲームオーバー＆リトライ演出に必要な外部参照コンテキスト
/// </summary>
struct GameOverContext
{
	Player* player = nullptr;
	Camera* activeCamera = nullptr;
	RailCameraController* railCameraController = nullptr;
	SceneManager* sceneManager = nullptr;
	std::vector<std::unique_ptr<Object3d>>* modelInstances = nullptr;
	std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	ArmoredTrainBoss* armoredTrainBoss = nullptr;
	std::list<std::unique_ptr<PlayerBullet>>* bullets = nullptr;
	std::list<std::unique_ptr<PlayerMissile>>* missiles = nullptr;
	std::list<std::unique_ptr<EnemyBullet>>* enemyBullets = nullptr;
	std::list<std::unique_ptr<EnhanceRing>>* enhanceRings = nullptr;
	ParticleEffect* thrusterEffect = nullptr;
	ParticleEffect* windEffect = nullptr;
	float* terrainMaterializeProgress = nullptr;
	float* cameraShakeTimer = nullptr;

	// デモプレイ自動遷移用ステート
	int* demoDeathLoopCount = nullptr;
	float* demoSurvivalTimer = nullptr;
	bool* demoBulletSpawned = nullptr;
	bool* demoHitTriggered = nullptr;
	float* demoGameOverWaitTimer = nullptr;
	int* demoMenuSelectStep = nullptr;

	// リトライ実行時のコールバック
	std::function<void()> onRetry;
};

/// <summary>
/// ゲームオーバー演出およびリトライ逆マテリアライズ演出を管理するクラス
/// </summary>
class GameOverSequence
{
public:
	enum class Step
	{
		HIT_STOP = 0,             // 0.00 - 0.15s: Critical hit stop
		POLYGON_SHATTER = 1,      // 0.15 - 1.80s: プリズム四散
		VOID_DISINTEGRATE = 2,    // 1.80 - 2.30s: サイバーグリッド・スキャンライン展開
		SHOW_UI = 3,              // 2.30s+: GAME OVER モザイク崩壊テキスト、メニューUI
		RETRY_DEMATERIALIZE = 4   // リトライ時シームレス逆マテリアライズ演出
	};

	enum class MenuOption
	{
		Retry = 0,
		Title = 1
	};

	/// <summary>
	/// 状態を初期値へリセット（初期化・リトライ時用）
	/// </summary>
	void Reset(bool hideShards = false);

	/// <summary>
	/// ゲームオーバー演出を開始
	/// </summary>
	void Start(const GameOverContext& ctx);

	/// <summary>
	/// 毎フレーム更新
	/// </summary>
	void Update(float dt, const GameOverContext& ctx);

	/// <summary>
	/// 破片プリズムの3D描画
	/// </summary>
	void Draw3D(Camera* activeCamera, Object3dCommon* object3dCommon);

	/// <summary>
	/// ゲームオーバーUI・スキャンラインの描画
	/// </summary>
	void DrawUI(bool isGameOverPhase, SceneManager* sceneManager);

	bool IsRetryDematerializing() const { return gameOverStep_ == Step::RETRY_DEMATERIALIZE; }
	float GetTimer() const { return gameOverTimer_; }
	void SetRetryStartRail(float progress, int index) { retryStartProgress_ = progress; retryStartRailIndex_ = index; }

	// エディタ調整用アクセサ
	bool& DebugPreviewUI() { return debugPreviewGameOverUI_; }
	bool& BarVisible() { return gameOverBarVisible_; }
	float& BarWidth() { return gameOverBarWidth_; }
	float& BarThickness() { return gameOverBarThickness_; }
	float& BarOffsetY() { return gameOverBarOffsetY_; }
	Vector4& BarColor() { return gameOverBarColor_; }

private:
	struct GameOverShard
	{
		std::unique_ptr<Object3d> object;
		Vector3 position;
		Vector3 velocity;
		Vector3 rotation;
		Vector3 rotSpeed;
		Vector3 scale;
		Vector4 color;
		float life = 0.0f;
		float maxLife = 0.85f;
	};

	Step gameOverStep_ = Step::HIT_STOP;
	MenuOption gameOverSelectedOption_ = MenuOption::Retry;
	float gameOverTimer_ = 0.0f;
	bool debugPreviewGameOverUI_ = false;
	bool gameOverBarVisible_ = true;
	float gameOverBarWidth_ = 320.0f;
	float gameOverBarThickness_ = 2.5f;
	float gameOverBarOffsetY_ = 42.0f;
	Vector4 gameOverBarColor_ = { 0.92f, 0.12f, 0.18f, 0.86f };

	// リトライ時シームレス逆マテリアライズ用
	float retryDematerializeTimer_ = 0.0f;
	static constexpr float kRetryDematerializeDuration = 1.10f; // 0.90s消滅 + 0.20sボイド間
	Vector3 retryStartCamPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 retryStartCamTarget_ = { 0.0f, 0.0f, 0.0f };
	float retryStartProgress_ = 0.0f;
	int retryStartRailIndex_ = 0;
	Vector3 gameOverPlayerStartPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 gameOverPlayerStartRot_ = { 0.0f, 0.0f, 0.0f };

	std::vector<GameOverShard> gameOverShards_;
	std::unique_ptr<Object3d> gameOverShockwaveRing_;
	float shockwaveScale_ = 0.2f;
	float shockwaveAlpha_ = 1.0f;
};
