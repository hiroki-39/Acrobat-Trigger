#pragma once
#include <Windows.h>
#include <cstdint>

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif // USE_IMGUI


class WinApp
{
public:

	static LRESULT CALLBACK WindowProc(HWND hand, UINT msg, WPARAM wparam, LPARAM lparm);

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新処理
	/// </summary>
	void Update();

	/// <summary>
	///	終了処理
	/// </summary>
	void Finalize();

	//メッセージの処理
	bool ProcessMessage();

	//getter
	HWND GetHwnd() { return hwnd; }

	HINSTANCE GetHInstance() const { return wc.hInstance; }

	// 現在のクライアント領域サイズを取得
	uint32_t GetClientWidth() const;
	uint32_t GetClientHeight() const;

	// フルスクリーン切り替え
	void ToggleFullscreen();
	bool IsFullscreen() const { return isFullscreen_; }

public://定数

	//クライアント領域のサイズ (基準解像度)
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;

private:

	//ウィンドウのハンドル
	HWND hwnd = nullptr;

	//ウィンドウクラスの設定
	WNDCLASS wc{};

	// フルスクリーン管理
	bool isFullscreen_ = false;
	WINDOWPLACEMENT windowPlacement_ = { sizeof(windowPlacement_) };
};

