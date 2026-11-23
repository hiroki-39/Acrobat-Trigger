#pragma once
#include "KHEngine/Math/Vector3.h"
#include "KHEngine/Math/Quaternion.h"
#include <list>
#include <memory>

class Player;
class RailCameraController;
class BossFinisherSequence;
class GameOverSequence;
class EnemyBullet;
class SceneManager;

/// <summary>
/// ゲームクリア演出に必要な外部参照コンテキスト
/// </summary>
struct GameClearContext
{
	Player* player = nullptr;
	RailCameraController* railCameraController = nullptr;
	BossFinisherSequence* bossFinisherSequence = nullptr;
	GameOverSequence* gameOverSequence = nullptr;
	std::list<std::unique_ptr<EnemyBullet>>* enemyBullets = nullptr;
	SceneManager* sceneManager = nullptr;
	float* cameraShakeTimer = nullptr;
	int totalEnemiesCount = 0;
	int killedEnemiesCount = 0;
	float stageElapsedTime = 0.0f;
	float bossElapsedTime = 0.0f;
};

/// <summary>
/// ゲームクリア演出（自機上昇・任務完了・リザルトパネル）を管理するクラス
/// </summary>
class GameClearSequence
{
public:
	enum class Step
	{
		ASCENDING,      // 自機が上空へ急上昇・飛び去る（カメラ停止、スラスター噴射）
		SHOW_UI,        // シアン発光ブラケット枠付き「MISSION ACCOMPLISHED / 任務完了」
		RESULT_PANEL    // 半透明ホログラムウィンドウ（リザルトパネル）展開＆SPACEでタイトル復帰
	};

	/// <summary>
	/// クリア演出を開始
	/// </summary>
	void Start(const GameClearContext& ctx);

	/// <summary>
	/// 毎フレーム更新
	/// </summary>
	void Update(float dt, const GameClearContext& ctx);

	/// <summary>
	/// クリア演出UI・リザルトパネルの描画
	/// </summary>
	void DrawUI(bool isClearPhase, const GameClearContext& ctx);

	const Vector3& GetStartPlayerPos() const { return clearStartPlayerPos_; }
	float GetTotalTimer() const { return clearTotalTimer_; }

private:
	Step clearStep_ = Step::ASCENDING;
	float clearTimer_ = 0.0f;
	float clearTotalTimer_ = 0.0f;
	Vector3 clearStartPlayerPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 clearPlayerAscentPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 clearPlayerAscentRot_ = { 0.0f, 0.0f, 0.0f };
	bool clearUseQuat_ = false;
	Quaternion clearQuat_ = Quaternion::Identity();
	Vector3 clearMoveDir_ = { 0.0f, 0.45f, 0.89f };
	float clearAscentSpeed_ = 0.0f;
};
