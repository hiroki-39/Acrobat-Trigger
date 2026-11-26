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

	float GetTimer() const { return demoTimer_; }
	void SetTimer(float v) { demoTimer_ = v; }
	float GetPostIntroDuration() const { return demoPostIntroDuration_; }
	void SetPostIntroDuration(float v) { demoPostIntroDuration_ = v; }
	int GetDeathLoopCount() const { return demoDeathLoopCount_; }
	void SetDeathLoopCount(int v) { demoDeathLoopCount_ = v; }
	float GetSurvivalTimer() const { return demoSurvivalTimer_; }
	void SetSurvivalTimer(float v) { demoSurvivalTimer_ = v; }
	bool IsBulletSpawned() const { return demoBulletSpawned_; }
	void SetBulletSpawned(bool v) { demoBulletSpawned_ = v; }
	bool IsHitTriggered() const { return demoHitTriggered_; }
	void SetHitTriggered(bool v) { demoHitTriggered_ = v; }
	float GetGameOverWaitTimer() const { return demoGameOverWaitTimer_; }
	void SetGameOverWaitTimer(float v) { demoGameOverWaitTimer_ = v; }
	void AddGameOverWaitTimer(float dt) { demoGameOverWaitTimer_ += dt; }
	int GetMenuSelectStep() const { return demoMenuSelectStep_; }
	void SetMenuSelectStep(int v) { demoMenuSelectStep_ = v; }

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
