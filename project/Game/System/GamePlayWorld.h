#pragma once
#include "Game/Actor/Weapon/MemoryType.h"
#include "KHEngine/Math/Vector3.h"
#include <vector>
#include <list>
#include <memory>

class Camera;
class Object3d;
class Rail;
class RailCameraController;
class Skybox;
class GamePlayVfxManager;
class Player;
class GamePlayHUD;
class PlayerBullet;
class PlayerMissile;
class Enemy;
class ArmoredTrainBoss;
class BossFinisherSequence;
class BossBattleController;
class EnemyBullet;
class CollisionManager;
class Obstacle;
class EnhanceRing;
class MemoryItem;
class OpeningCutsceneSequence;
class PauseMenu;
class GameOverSequence;

/// <summary>
/// ワールド内のアクター更新・コリジョン・3D描画に必要な外部参照コンテキスト
/// </summary>
struct GamePlayWorldContext
{
	Camera* activeCamera = nullptr;
	Object3d* cameraObject = nullptr;
	std::vector<std::unique_ptr<Object3d>>* modelInstances = nullptr;
	std::vector<std::unique_ptr<Rail>>* mainRails = nullptr;
	RailCameraController* railCameraController = nullptr;
	std::vector<std::unique_ptr<Object3d>>* railVisualizers = nullptr;
	std::vector<std::unique_ptr<Object3d>>* enemyRailVisualizers = nullptr;
	Skybox* skybox = nullptr;
	GamePlayVfxManager* vfx = nullptr;
	Player* player = nullptr;
	GamePlayHUD* hud = nullptr;
	std::list<std::unique_ptr<PlayerBullet>>* bullets = nullptr;
	std::list<std::unique_ptr<PlayerMissile>>* missiles = nullptr;
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	ArmoredTrainBoss* armoredTrainBoss = nullptr;
	BossFinisherSequence* bossFinisherSequence = nullptr;
	BossBattleController* bossBattle = nullptr;
	std::list<std::unique_ptr<EnemyBullet>>* enemyBullets = nullptr;
	CollisionManager* collisionManager = nullptr;
	std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	std::list<std::unique_ptr<EnhanceRing>>* enhanceRings = nullptr;
	std::list<std::unique_ptr<MemoryItem>>* memoryItems = nullptr;
	OpeningCutsceneSequence* openingSequence = nullptr;
	PauseMenu* pauseMenu = nullptr;
	GameOverSequence* gameOverSequence = nullptr;

	float gameSpeed = 1.0f;
	float lockOnMaxDistance = 350.0f;
	bool* isJustDodgeActive = nullptr;
	float* justDodgeTimer = nullptr;
	float justDodgeMaxTime = 60.0f;
	bool* isBossSpawned = nullptr;
	bool isDrawRail = false;
	bool isPlaying = true;
	bool isDrawCollider = false;
	bool isDrawTerrainWireframe = false;
	float* cameraShakeTimer = nullptr;
	int* enemyKillShakeFrames = nullptr;
	int* score = nullptr;
	int* killedEnemiesCount = nullptr;
	float terrainMaterializeProgress = 1.0f;

	bool isStartCutscenePhase = false;
	bool isPlayingPhase = false;
	bool isGameOverPhase = false;
	bool isClearPhase = false;
};

/// <summary>
/// ワールド内のアクター更新・コリジョン処理・3Dシーン描画を担当するクラス
/// </summary>
class GamePlayWorld
{
public:
	/// <summary>
	/// カメラ・パーティクル基盤・Skyboxの初期化
	/// </summary>
	void InitializeCamerasAndSkybox(
		std::unique_ptr<Camera>& camera,
		std::unique_ptr<Camera>& debugCamera,
		Camera*& activeCamera,
		std::unique_ptr<Skybox>& skybox,
		float& outSkyboxMs
	);

	/// <summary>
	/// プレイヤー・レールカメラ・装甲列車ボス・地形デバッガーの初期化
	/// </summary>
	void InitializeActors(
		std::unique_ptr<Object3d>& cameraObject,
		std::unique_ptr<Player>& player,
		std::unique_ptr<RailCameraController>& railCameraController,
		std::unique_ptr<ArmoredTrainBoss>& armoredTrainBoss,
		bool& isBossSpawned,
		Camera* activeCamera,
		Skybox* skybox,
		const std::vector<std::unique_ptr<Rail>>& mainRails,
		const std::vector<std::unique_ptr<Object3d>>& modelInstances
	);

	/// <summary>
	/// リトライ時のボス・レールカメラ・プレイヤー状態リセット
	/// </summary>
	void ResetActorsForRetry(
		ArmoredTrainBoss* armoredTrainBoss,
		bool& isBossSpawned,
		RailCameraController* railCameraController,
		Rail* bossRail,
		const std::vector<std::unique_ptr<Rail>>& mainRails,
		Player* player
	);

	/// <summary>
	/// プレイヤー・弾・敵・障害物・アイテムの更新および一括衝突判定
	/// </summary>
	void UpdateActorsAndCollisions(float dt, float unscaledGameSpeed, const GamePlayWorldContext& ctx);

	/// <summary>
	/// カメラFOV補間とレールカメラの追従更新
	/// </summary>
	void UpdateCameraFovAndRail(const GamePlayWorldContext& ctx, const Vector3& clearStartPlayerPos);

	/// <summary>
	/// カメラシェイク・カメラ行列・地形モデル・スカイボックス・ポーズ中オブジェクトの更新
	/// </summary>
	void UpdateCameraAndEnvironment(const GamePlayWorldContext& ctx);

	/// <summary>
	/// 3Dワールド（スカイボックス・地形・アクター・エフェクト・デバッグワイヤーフレーム）の描画
	/// </summary>
	void Draw(const GamePlayWorldContext& ctx);

	/// <summary>
	/// メモリドロップアイテムの生成
	/// </summary>
	void SpawnMemoryItem(const Vector3& pos, MemoryType type, std::list<std::unique_ptr<MemoryItem>>& memoryItems);
};
