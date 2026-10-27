#pragma once
#include "KHEngine/Math/MathCommon.h"
#include "KHEngine/Debug/Imgui/ImGuiManager.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// UIテキスト揃え方向
enum class UITextAlign
{
	Left = 0,
	Center,
	Right
};

// UIテキスト単体データ
struct UITextItem
{
	std::string id;                                     // 一意な識別キー (例: "Score")
	std::string text;                                   // 表示文字列
	Vector2 position = { 0.0f, 0.0f };                  // 基準解像度 (1280x720) 上の座標
	float fontSize = 24.0f;                             // フォントサイズ (px)
	ImGuiManager::FontType fontType = ImGuiManager::FontType::Japanese_MPLUS; // フォント種類
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };         // 文字色 (RGBA)
	
	bool hasOutline = true;                             // 縁取り (袋文字)
	Vector4 outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };  // 縁取り色
	float outlineThickness = 2.0f;                      // 縁取り太さ

	bool hasShadow = false;                             // ドロップシャドウ
	Vector4 shadowColor = { 0.0f, 0.0f, 0.0f, 0.7f };   // 影の色
	Vector2 shadowOffset = { 2.0f, 2.0f };              // 影のオフセット

	bool hasUnderline = false;                           // 下線 (アンダーバー)
	Vector4 underlineColor = { 0.92f, 0.12f, 0.18f, 0.86f }; // 下線の色 (RGBA)
	float underlineWidth = 320.0f;                       // 下線の幅・長さ (px)
	float underlineThickness = 2.5f;                     // 下線の太さ (px)
	float underlineOffsetY = 42.0f;                      // 下線のY位置オフセット (px)

	UITextAlign align = UITextAlign::Left;              // 配置揃え
	bool isVisible = true;                              // 表示フラグ
};

// シーンごとのテキストデータコンテナ
struct SceneTextContainer
{
	std::vector<UITextItem> items;
	std::unordered_map<std::string, size_t> itemMap;
};

/// <summary>
/// ゲーム内テキスト管理・エディタ・描画システム（シーン別対応）
/// </summary>
class UITextManager
{
public:
	static UITextManager* GetInstance();

	/// <summary>
	/// 初期化 (設定JSONの読み込み、デフォルト項目セットアップ)
	/// </summary>
	void Initialize(const std::string& settingsFilePath = "resources/json/ui_text_settings.json");

	/// <summary>
	/// 現在アクティブなシーンを設定（シーン遷移時に呼び出し）
	/// </summary>
	void SetCurrentScene(const std::string& sceneName);

	/// <summary>
	/// 現在アクティブなシーン名を取得
	/// </summary>
	const std::string& GetCurrentScene() const { return currentScene_; }

	/// <summary>
	/// エディタで編集対象とするシーン名を設定
	/// </summary>
	void SetEditingScene(const std::string& sceneName);

	/// <summary>
	/// エディタで編集対象のシーン名を取得
	/// </summary>
	const std::string& GetEditingScene() const { return editingScene_; }

	/// <summary>
	/// 設定の保存 (JSON)
	/// </summary>
	void SaveSettings();

	/// <summary>
	/// 設定の再読み込み (JSON)
	/// </summary>
	void LoadSettings();

	/// <summary>
	/// テキストの登録 (既に同IDがある場合は上書きせずプロパティ保持)
	/// </summary>
	void RegisterText(const UITextItem& item, const std::string& sceneName = "");

	/// <summary>
	/// テキスト文字列の更新 (ゲームロジック用)
	/// </summary>
	void SetText(const std::string& id, const std::string& text, const std::string& sceneName = "");

	/// <summary>
	/// 表示・非表示の切り替え
	/// </summary>
	void SetVisible(const std::string& id, bool isVisible, const std::string& sceneName = "");

	/// <summary>
	/// 座標の更新
	/// </summary>
	void SetPosition(const std::string& id, const Vector2& position, const std::string& sceneName = "");

	/// <summary>
	/// カラーの更新
	/// </summary>
	void SetColor(const std::string& id, const Vector4& color, const std::string& sceneName = "");

	/// <summary>
	/// テキスト項目の取得
	/// </summary>
	UITextItem* GetTextItem(const std::string& id, const std::string& sceneName = "");

	/// <summary>
	/// 存在チェック
	/// </summary>
	bool HasTextItem(const std::string& id, const std::string& sceneName = "") const;

#ifdef USE_IMGUI
	/// <summary>
	/// ImGui インスペクター用エディタGUI
	/// </summary>
	void DrawImGuiEditor();

	/// <summary>
	/// 指定領域 (ビューポート等) 内へのテキスト描画
	/// </summary>
	/// <param name="drawList">ImDrawList ポインタ</param>
	/// <param name="screenOffset">描画領域の左上スクリーン座標</param>
	/// <param name="screenSize">描画領域の画面サイズ</param>
	void Draw(ImDrawList* drawList, const ImVec2& screenOffset, const ImVec2& screenSize);

	/// <summary>
	/// ゲーム内通常HUD全体の表示アルファを設定（カットシーン中・演出中の一括フェード・非表示用）
	/// </summary>
	void SetHudAlpha(float alpha) { hudAlpha_ = alpha; }
	float GetHudAlpha() const { return hudAlpha_; }
#endif

private:
	UITextManager() = default;
	~UITextManager() = default;
	UITextManager(const UITextManager&) = delete;
	UITextManager& operator=(const UITextManager&) = delete;

	void SetupDefaultScenes();
	SceneTextContainer& GetOrCreateScene(const std::string& sceneName);

	std::string settingsFilePath_ = "resources/json/ui_text_settings.json";
	std::string currentScene_ = "GAMEPLAY";
	std::string editingScene_ = "GAMEPLAY";
	bool followActiveScene_ = true;

	std::unordered_map<std::string, SceneTextContainer> scenes_;
	std::vector<std::string> knownScenes_ = { "TITLE", "GAMEPLAY", "GAMECLEAR", "GAMEOVER" };

	int selectedItemIndex_ = -1;
	bool showGizmo_ = false;
	float hudAlpha_ = 1.0f;
};
