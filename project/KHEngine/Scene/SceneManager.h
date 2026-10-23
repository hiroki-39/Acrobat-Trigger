#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include "KHEngine/Core/Framework/BaseScene.h"
#include "KHEngine/Scene/AbstractSceneFactory.h"
#include "KHEngine/Math/Vector4.h"

#include "KHEngine/Graphics/Transition/TransitionRenderer.h"

class Sprite;

class SceneManager
{
public:
	enum class TransitionState
	{
		None,            // 通常時
		FadeOut,         // 暗転中（旧シーン稼働中 -> 左右から中央へ閉じる）
		FadeOutHold,     // 暗転完了・完全黒描画確定待ち（真っ黒を画面にPresent）
		Loading,         // 暗転中ロード実行（旧シーン破棄＆新シーン初期化）
		FadeInWait,      // ロード直後のタメ（先行更新と黒画面保持）
		FadeIn           // 明転中（新シーン稼働中 -> 中央から左右端へ開く）
	};

	SceneManager();
	~SceneManager();

	void Update();

	void Draw();
	void DrawUI();

	/// <summary>
	/// シーン変更（トランジション付き）
	/// </summary>
	/// <param name="sceneName">遷移先のシーン名</param>
	/// <param name="fadeDuration">フェードアウト/インにかける時間（秒）</param>
	/// <param name="fadeColor">フェード色（デフォルト: 仮想空間ディープティール）</param>
	void ChangeScene(const std::string& sceneName, float fadeDuration = 0.5f, const Vector4& fadeColor = { 0.012f, 0.045f, 0.080f, 1.0f });

	/// <summary>
	/// 遷移中かどうか
	/// </summary>
	bool IsTransitioning() const { return transitionState_ != TransitionState::None; }
	TransitionState GetTransitionState() const { return transitionState_; }
	float GetTransitionProgress() const { return transitionProgress_; }

	void SetSceneFactory(AbstractSceneFactory* factory) { sceneFactory_ = factory; }

	const std::string& GetCurrentSceneName() const { return currentSceneName_; }

	// トランジション調整用
	TransitionType GetTransitionType() const { return transitionType_; }
	void SetTransitionType(TransitionType type) { transitionType_ = type; }

	float GetFadeDuration() const { return fadeDuration_; }
	void SetFadeDuration(float duration) { fadeDuration_ = duration; }
	float GetEdgeSoftness() const { return edgeSoftness_; }
	void SetEdgeSoftness(float s) { edgeSoftness_ = s; }
	Vector4 GetFadeColor() const { return fadeColor_; }
	void SetFadeColor(const Vector4& c) { fadeColor_ = c; }
	Vector4 GetEdgeColor() const { return edgeColor_; }
	void SetEdgeColor(const Vector4& c) { edgeColor_ = c; }

	// Hexagon用パラメータアクセサ
	const Vector2& GetHexOrigin() const { return hexOrigin_; }
	void SetHexOrigin(const Vector2& o) { hexOrigin_ = o; }
	float GetHexTileScale() const { return hexTileScale_; }
	void SetHexTileScale(float s) { hexTileScale_ = s; }
	float GetHexBorderWidth() const { return hexBorderWidth_; }
	void SetHexBorderWidth(float w) { hexBorderWidth_ = w; }
	float GetHexGlitchIntensity() const { return hexGlitchIntensity_; }
	void SetHexGlitchIntensity(float g) { hexGlitchIntensity_ = g; }

	// ImGui用共通UI
	void DrawSceneSelectorUI();
	void DrawTransitionSettingsUI();

private:
	void InitTransition();

private:
	std::unique_ptr<BaseScene> nextScene_ = nullptr;
	std::unique_ptr<BaseScene> scene_ = nullptr;
	AbstractSceneFactory* sceneFactory_ = nullptr;

	std::string currentSceneName_ = "TITLE";

	// トランジション管理
	TransitionType transitionType_ = TransitionType::Hexagon; // デフォルト: ヘキサゴン
	TransitionState transitionState_ = TransitionState::None;
	std::string nextSceneName_;
	float fadeDuration_ = 0.9f; // Hexagonのデフォルト仕様 (片道0.9秒、往復トータル1.8秒)
	float fadeTimer_ = 0.0f;
	float transitionProgress_ = 0.0f;
	float edgeSoftness_ = 0.06f;
	int holdFrames_ = 0;

	// 四角形グリッド用カラー
	Vector4 squareFadeColor_ = { 0.012f, 0.045f, 0.080f, 1.0f }; // 仮想空間ディープサイバーティール
	Vector4 squareEdgeColor_ = { 0.20f, 1.0f, 0.88f, 0.95f };    // ネオンエメラルド/シアン境界発光

	// ヘキサゴン用パラメータ (仮想空間サイバー世界観カラー)
	Vector4 hexFadeColor_ = { 0.010f, 0.040f, 0.075f, 1.0f };   // 仮想空間ディープサイバーティール/ネイビー
	Vector4 hexGlowColor_ = { 0.15f, 1.0f, 0.88f, 1.0f };        // 鮮やかなエメラルドグリーン / ネオンシアン発光
	Vector2 hexOrigin_ = { 0.5f, 0.5f };                          // 波紋の起点 (画面中央)
	float hexTileScale_ = 14.0f;                                  // 六角形タイルの密度・サイズ (14.0: 迫力ある大ぶりサイズ)
	float hexBorderWidth_ = 0.08f;                                // 六角形輪郭線の太さ
	float hexGlitchIntensity_ = 0.25f;                            // ピーク時の明滅・乱数幅

	Vector4 fadeColor_ = hexFadeColor_;
	Vector4 edgeColor_ = hexGlowColor_;

	std::unique_ptr<TransitionRenderer> transitionRenderer_ = nullptr;
	uint32_t ruleTextureIndex_ = UINT32_MAX;
};

