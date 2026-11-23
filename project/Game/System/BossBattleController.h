#pragma once
#include "KHEngine/Graphics/2d/Sprite.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <vector>
#include <list>
#include <memory>
#include <functional>

class Rail;
class RailCameraController;
class ParticleEffect;
class Player;
class PlayerBullet;
class PlayerMissile;
class ArmoredTrainBoss;
class BossFinisherSequence;
class Object3dCommon;
class SpriteCommon;

/// <summary>
/// ボス戦進行・タイムオーバー演出に必要な外部参照コンテキスト
/// </summary>
struct BossBattleContext
{
	std::vector<std::unique_ptr<Rail>>* mainRails = nullptr;
	Rail* bossRail = nullptr;
	RailCameraController* railCameraController = nullptr;
	float* gameSpeed = nullptr;
	ParticleEffect* explosionEffect = nullptr;
	Player* player = nullptr;
	std::list<std::unique_ptr<PlayerBullet>>* bullets = nullptr;
	std::list<std::unique_ptr<PlayerMissile>>* missiles = nullptr;
	ArmoredTrainBoss* armoredTrainBoss = nullptr;
	bool* isBossSpawned = nullptr;
	BossFinisherSequence* bossFinisherSequence = nullptr;
	float* cameraShakeTimer = nullptr;
	float* bossElapsedTime = nullptr;
	std::function<void()> onGameOver;
	std::function<void()> onClear;
};

/// <summary>
/// 装甲列車ボス戦の進行（WARNING警告・戦闘・制限時間・タイムオーバー大砲・専用UI・設定I/O）を管理するクラス
/// </summary>
class BossBattleController
{
	friend class GamePlayEditor;

public:
	enum class Step
	{
		NOT_ACTIVE,        // 通常道中
		WARNING_ALERT,     // WARNING警告演出中
		BATTLE,            // ボス戦闘中
		DEFEATED_SEQUENCE, // 撃破演出中（連続大爆発・スロー）
		TIME_OVER_ATTACK,  // タイムオーバー即死攻撃中
		FINISHED           // 撃破完了
	};

	/// <summary>
	/// タイムオーバー大砲用3Dオブジェクトとボス専用UIスプライトの初期化
	/// </summary>
	void Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, uint32_t whiteTexIndex);

	/// <summary>
	/// 戦闘状態のリセット（リロード・リトライ時用）
	/// </summary>
	void Reset();

	/// <summary>
	/// タイムオーバー大砲演出を即座に開始
	/// </summary>
	void TriggerTimeOverCannonSequence(bool nonLethal, const BossBattleContext& ctx);

	/// <summary>
	/// WARNING警告演出を開始
	/// </summary>
	void StartWarningSequence(const BossBattleContext& ctx);

	/// <summary>
	/// ボス戦進行の毎フレーム更新
	/// </summary>
	void Update(float dt, const BossBattleContext& ctx);

	/// <summary>
	/// タイムオーバー大砲3Dオブジェクトの描画
	/// </summary>
	void Draw3D();

	/// <summary>
	/// ボスHPバースプライトの描画
	/// </summary>
	void DrawHpSprites(float hpRate);

	/// <summary>
	/// ボス戦専用UI（WARNING帯・タイマー・詳細HPバー等）の描画
	/// </summary>
	void DrawUI(bool isStartCutscene, bool isGameOver, bool isClear, const BossBattleContext& ctx);

	/// <summary>
	/// ボス設定の読み込み / 保存
	/// </summary>
	void LoadSettings(ArmoredTrainBoss* boss);
	void SaveSettings(ArmoredTrainBoss* boss);

	Step GetStep() const { return bossBattleStep_; }
	void SetDefeated() { bossBattleStep_ = Step::DEFEATED_SEQUENCE; bossDefeatTimer_ = 0.0f; }
	float& SpawnProgressThreshold() { return bossSpawnProgressThreshold_; }

private:
	Step bossBattleStep_ = Step::NOT_ACTIVE;
	float bossSpawnProgressThreshold_ = 0.60f; // 自動出現するレール進行度閾値（60%地点）
	float bossWarningTimer_ = 0.0f;
	static constexpr float kBossWarningDuration = 2.8f;
	float bossDefeatTimer_ = 0.0f;
	static constexpr float kBossDefeatDuration = 3.5f;
	float bossDisplayedHpRate_ = 1.0f; // 遅延追従HPバー用
	float bossWarningSirenTimer_ = 0.0f;

	// ボス戦制限時間 & タイムオーバー即死攻撃用
	float bossTimeLimit_ = 120.0f;       // ボス戦制限時間（秒）: 2分
	float bossRemainingTime_ = 120.0f;   // ボス戦残り時間（秒）: 2分
	float bossTimeOverTimer_ = 0.0f;     // 即死攻撃シークエンス用タイマー
	static constexpr float kBossTimeOverDuration = 4.0f; // 即死攻撃シークエンス時間
	bool isBossTimeOverFired_ = false;   // 即死攻撃発射フラグ
	bool isTimeOverDebugNonLethal_ = true;
	bool isTimeOverDebugLoop_ = false;

	std::unique_ptr<Object3d> bossTimeOverChargeSphere_; // タイムオーバー大砲チャージエネルギー球
	std::unique_ptr<Object3d> bossTimeOverBeam_;         // タイムオーバー大砲の極大砲撃ビーム

	// ボス戦専用UIスプライト
	std::unique_ptr<Sprite> bossHpBarBgSprite_;
	std::unique_ptr<Sprite> bossHpBarDelaySprite_;
	std::unique_ptr<Sprite> bossHpBarSprite_;
	std::unique_ptr<Sprite> bossWarningBandTopSprite_;
	std::unique_ptr<Sprite> bossWarningBandBottomSprite_;
	std::unique_ptr<Sprite> bossWarningFlashSprite_;
};
