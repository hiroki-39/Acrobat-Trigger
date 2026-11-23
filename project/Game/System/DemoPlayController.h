#pragma once
#include <list>
#include <memory>
#include <functional>

class EngineServices;
class Player;
class BossFinisherSequence;
class BossBattleController;
class ArmoredTrainBoss;
class PlayerBullet;

/// <summary>
/// デモプレイ時の自動操作・自動進行・自動射撃を管理するクラス
/// </summary>
class DemoPlayController
{
public:
	/// <summary>
	/// 全デモ状態のリセット（Initialize時用）
	/// </summary>
	void ResetAll();

	/// <summary>
	/// 1サイクル分のタイマー・フラグをリセット（リトライ・リロード時用）
	/// </summary>
	void ResetCycle();

	/// <summary>
	/// デモプレイの毎フレーム更新
	/// </summary>
	void Update(
		EngineServices* services,
		float dt,
		bool isPlayingPhase,
		Player* player,
		BossFinisherSequence* finisher,
		BossBattleController* bossBattle,
		ArmoredTrainBoss* boss,
		std::list<std::unique_ptr<PlayerBullet>>& bullets,
		const std::function<void()>& onStartBossWarning
	);

	float& Timer() { return demoTimer_; }
	float& PostIntroDuration() { return demoPostIntroDuration_; }
	int& DeathLoopCount() { return demoDeathLoopCount_; }
	float& SurvivalTimer() { return demoSurvivalTimer_; }
	bool& BulletSpawned() { return demoBulletSpawned_; }
	bool& HitTriggered() { return demoHitTriggered_; }
	float& GameOverWaitTimer() { return demoGameOverWaitTimer_; }
	int& MenuSelectStep() { return demoMenuSelectStep_; }

private:
	float demoTimer_ = 0.0f;
	float demoPostIntroDuration_ = 2.0f;
	int demoDeathLoopCount_ = 0;
	float demoSurvivalTimer_ = 0.0f;
	bool demoBulletSpawned_ = false;
	bool demoHitTriggered_ = false;
	float demoGameOverWaitTimer_ = 0.0f;
	int demoMenuSelectStep_ = 0;
};
