#pragma once

class SceneManager;

/// <summary>
/// ゲーム中のポーズメニューの入力・状態管理・UI描画を担当するクラス
/// </summary>
class PauseMenu
{
public:
	enum class Option
	{
		Resume = 0,
		Retry = 1,
		Title = 2
	};

	/// <summary>
	/// ポーズの開閉を切り替え
	/// </summary>
	void Toggle();

	/// <summary>
	/// ポーズメニューの入力更新
	/// </summary>
	void Update(SceneManager* sceneManager);

	/// <summary>
	/// ポーズメニューUIの描画
	/// </summary>
	void DrawUI();

	bool IsPaused() const { return isPaused_; }
	void SetPaused(bool paused) { isPaused_ = paused; }

private:
	bool isPaused_ = false;
	Option selectedOption_ = Option::Resume;
};
