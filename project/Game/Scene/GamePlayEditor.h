#pragma once

class GamePlayScene;

/// <summary>
/// GamePlayScene 用の統合エディタUI（ImGui）を管理・描画するクラス
/// </summary>
class GamePlayEditor
{
public:
	/// <summary>
	/// エディタUIの描画処理
	/// </summary>
	/// <param name="scene">対象となる GamePlayScene の参照</param>
	void Draw(GamePlayScene& scene);

private:
#ifdef ENABLE_EDITOR
	int currentNavIndex_ = 0;
	float fpsHistory_[60] = {};
	int historyOffset_ = 0;
#endif
};
