#pragma once
#include <vector>
#include <list>
#include <memory>

struct LevelData;
class Object3d;
class Rail;
class RailCameraController;
class Model;
class Enemy;
class Obstacle;
class EnhanceRing;
class MemoryItem;
class ArmoredTrainBoss;
class Skybox;
class Camera;
class Player;
class BossBattleController;

/// <summary>
/// レベル生成・リロード時にワールド側のコンテナやアクターへアクセスするためのコンテキスト
/// </summary>
struct LevelLoadContext
{
	std::vector<std::unique_ptr<Object3d>>* modelInstances = nullptr;
	std::vector<std::unique_ptr<Rail>>* mainRails = nullptr;
	std::unique_ptr<Rail>* bossRail = nullptr;
	std::unique_ptr<RailCameraController>* railCameraController = nullptr;
	std::vector<std::unique_ptr<Model>>* railModels = nullptr;
	std::unique_ptr<Model>* enemyRailModel = nullptr;
	std::vector<std::unique_ptr<Object3d>>* railVisualizers = nullptr;
	std::vector<std::unique_ptr<Object3d>>* enemyRailVisualizers = nullptr;
	std::list<std::unique_ptr<Enemy>>* enemies = nullptr;
	std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	std::list<std::unique_ptr<EnhanceRing>>* enhanceRings = nullptr;
	std::list<std::unique_ptr<MemoryItem>>* memoryItems = nullptr;
	std::unique_ptr<ArmoredTrainBoss>* armoredTrainBoss = nullptr;
	bool* isBossSpawned = nullptr;
	bool* hasEnemySpawned = nullptr;
	int* totalEnemiesCount = nullptr;
	int* killedEnemiesCount = nullptr;
	Skybox* skybox = nullptr;
	Camera* activeCamera = nullptr;
	Object3d* cameraObject = nullptr;
	Player* player = nullptr;
	BossBattleController* bossBattle = nullptr;
	bool isVirtualSpace = true;
};

/// <summary>
/// レベルJSONの読み込み、地形・レール・敵・障害物・ボス・ビジュアライザの生成とリロードを管理するクラス
/// </summary>
class GamePlayLevelManager
{
public:
	GamePlayLevelManager();
	~GamePlayLevelManager();

	/// <summary>
	/// ゲームプレイで使用する基本モデル・テクスチャの一括プリロード
	/// </summary>
	void PreloadResources();

	/// <summary>
	/// レベル全体をJSONから再読込し、地形・レール・アクター・ビジュアライザを構築する
	/// </summary>
	void ReloadLevel(const LevelLoadContext& ctx);

	/// <summary>
	/// 敵およびボスのみを再生成する
	/// </summary>
	void ReloadEnemiesOnly(const LevelLoadContext& ctx);

	/// <summary>
	/// キャッシュ済みレベルデータから敵・障害物・リングを即座に再生成する（シームレスリトライ用）
	/// </summary>
	void RespawnActorsFromCache(const LevelLoadContext& ctx);

	/// <summary>
	/// サイバー仮想空間ビジュアルスタイルを地形モデル・障害物・スカイボックスへ適用する
	/// </summary>
	void ApplyVirtualSpaceStyle(const std::vector<std::unique_ptr<Object3d>>& modelInstances, const std::list<std::unique_ptr<Obstacle>>& obstacles, Skybox* skybox, bool isVirtualSpace);

private:
	std::unique_ptr<LevelData> cachedLevelData_;
};
