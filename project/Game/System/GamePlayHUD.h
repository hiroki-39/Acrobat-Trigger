#pragma once
#include "KHEngine/Graphics/2d/Sprite.h"
#include "KHEngine/Math/Vector3.h"
#include "KHEngine/Math/Vector4.h"
#include <array>
#include <vector>
#include <memory>

class Player;
class Camera;
class SpriteCommon;
class BossFinisherSequence;

/// <summary>
/// ゲーム中の通常HUD（HPバー・ブーストバー・強化リングアイコン・残弾/スコアテキスト・レティクル・3Dスコアポップアップ）を管理するクラス
/// </summary>
class GamePlayHUD
{
public:
	static constexpr int kMaxEnhanceRingIcons = 2;

	/// <summary>
	/// HUD用スプライト群の初期化
	/// </summary>
	void Initialize(SpriteCommon* spriteCommon, uint32_t whiteTexIndex, uint32_t ringIconTex, uint32_t ringIconOutlineTex);

	/// <summary>
	/// ゲームプレイ用UIテキスト項目の初期登録
	/// </summary>
	void RegisterDefaultTexts();

	/// <summary>
	/// 3Dスコア加算ポップアップを追加
	/// </summary>
	void AddScorePopup(const Vector3& worldPos, int score, float duration = 0.65f);

	/// <summary>
	/// ポップアップを全消去
	/// </summary>
	void ClearScorePopups() { scorePopups_.clear(); }

	/// <summary>
	/// 3Dスコアポップアップの経過時間更新
	/// </summary>
	void UpdateScorePopups(float dt);

	/// <summary>
	/// HPバー・ブーストゲージの更新
	/// </summary>
	void UpdateBars(float dt, Player* player);

	/// <summary>
	/// HUDテキスト（スコア・残弾・HP）およびフェードアルファの更新
	/// </summary>
	void UpdateHudTexts(Player* player, BossFinisherSequence* finisher, int score, bool isCutscene, bool isGameOver, bool isClear, float cutsceneTimer);

	/// <summary>
	/// HUDスプライト（強化リングアイコン・ブーストバー・HPバー）の描画
	/// </summary>
	void DrawSprites();

	/// <summary>
	/// オーバーレイUI（レティクル・ロックオンマーカー・3Dスコアポップアップ）の描画
	/// </summary>
	void DrawOverlayUI(bool isPlayingPhase, bool isFinisherMovie, Player* player, Camera* activeCamera);

	int GetAcquiredEnhanceRingCount() const { return acquiredEnhanceRingCount_; }
	void SetAcquiredEnhanceRingCount(int count) { acquiredEnhanceRingCount_ = std::clamp(count, 0, kMaxEnhanceRingIcons); }
	void IncrementEnhanceRingCount() { acquiredEnhanceRingCount_ = std::min(kMaxEnhanceRingIcons, acquiredEnhanceRingCount_ + 1); }

	const Vector4& GetRingIconColor() const { return ringIconColor_; }
	void SetRingIconColor(const Vector4& color) { ringIconColor_ = color; }
	const Vector4& GetRingOutlineColor() const { return ringOutlineColor_; }
	void SetRingOutlineColor(const Vector4& color) { ringOutlineColor_ = color; }

private:
	struct ScorePopup
	{
		Vector3 worldPos;
		int score;
		float timer;
		float duration;
	};

	std::unique_ptr<Sprite> hpBarBgSprite_;
	std::unique_ptr<Sprite> hpBarSprite_;
	std::unique_ptr<Sprite> boostBarBgSprite_;
	std::unique_ptr<Sprite> boostBarSprite_;

	std::array<std::unique_ptr<Sprite>, kMaxEnhanceRingIcons> ringGetOutlineSprites_;
	std::array<std::unique_ptr<Sprite>, kMaxEnhanceRingIcons> ringGetIconSprites_;
	int acquiredEnhanceRingCount_ = 0;
	Vector4 ringIconColor_ = { 1.0f, 0.85f, 0.2f, 1.0f };
	Vector4 ringOutlineColor_ = { 1.0f, 1.0f, 1.0f, 0.9f };

	std::vector<ScorePopup> scorePopups_;
	float overheatBlinkTimer_ = 0.0f;
};
