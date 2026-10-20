#include "WinApp.h"


#pragma comment(lib, "winmm.lib")


//ウインドウプロシージャ
LRESULT WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparm)
{
#ifdef USE_IMGUI

	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparm))
	{
		return true;
	}

#endif // USE_IMGUI



	//メッセージに応じてゲーム固有の処理を行う
	switch (msg)
	{
		//ウィンドウが壊された
	case WM_DESTROY:

	//OSに対して、アプリの終了を伝える
	PostQuitMessage(0);

	return 0;
	}

	//標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparm);
}

void WinApp::Initialize()
{
	// DPI Awareness の設定 (Per-Monitor v2 でOSによる不自然なスケーリングを防止)
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// システムタイマーの分解能を上げる
	timeBeginPeriod(1);

	//ウィンドウクラスの設定
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	//ウィンドウプロシージャ
	wc.lpfnWndProc = WindowProc;
	//ウィンドウクラス名
	wc.lpszClassName = L"CG2MyWindowClass";
	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	//カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	//ウィンドウクラスの登録
	RegisterClass(&wc);

	//ウィンドサイズを表す構造体にクライアント領域のサイズを入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	//クライアント領域のサイズをウィンドウサイズに変換する
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	//ウィンドウの生成
	hwnd = CreateWindow(
		wc.lpszClassName,     //クラス名
		L"カトウ_ヒロキ_アクロバット・トリガー",       //タイトルバー名
		WS_OVERLAPPEDWINDOW,  //ウィンドウスタイル
		CW_USEDEFAULT,        //表示座X標(Windowsに任せる)
		CW_USEDEFAULT,        //表示Y座標(WindowsOSに任せる)
		wrc.right - wrc.left, //ウィンドウ横幅
		wrc.bottom - wrc.top, //ウィンドウ縦幅
		nullptr,              //親ウィンドウハンドル
		nullptr,              //メニューハンドル
		wc.hInstance,         //インスタンスハンドル
		nullptr               //オプション
	);

	//ウィンドウの表示
	ShowWindow(hwnd, SW_SHOW);
}

void WinApp::Update()
{


	/*Log(logStream, "Hello DirectX!\n");
	Log(logStream, ConvertString(std::format(L"clientSize:{},{}\n", kClientWidth, kClientHeight)));*/
}

void WinApp::Finalize()
{
	CloseWindow(hwnd);

	//COMの終了処理
	CoUninitialize();
}



bool WinApp::ProcessMessage()
{
	MSG msg{};

	//windowsにメッセージが来たら最優先で処理する（キューにあるメッセージを全て処理）
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		//もし終了メッセージが来ていたらtrueを返す
		if (msg.message == WM_QUIT)
		{
			return true;
		}
	}

	return false;
}

uint32_t WinApp::GetClientWidth() const
{
	if (!hwnd) return kClientWidth;
	RECT rect{};
	GetClientRect(hwnd, &rect);
	return static_cast<uint32_t>(rect.right - rect.left);
}

uint32_t WinApp::GetClientHeight() const
{
	if (!hwnd) return kClientHeight;
	RECT rect{};
	GetClientRect(hwnd, &rect);
	return static_cast<uint32_t>(rect.bottom - rect.top);
}

void WinApp::ToggleFullscreen()
{
	if (!hwnd) return;

	DWORD style = GetWindowLong(hwnd, GWL_STYLE);

	if (isFullscreen_)
	{
		// ウィンドウモードに戻す
		SetWindowLong(hwnd, GWL_STYLE, (style & ~WS_POPUP) | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(hwnd, &windowPlacement_);
		SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
			SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		isFullscreen_ = false;
	}
	else
	{
		// フルスクリーンにする
		MONITORINFO mi = { sizeof(mi) };
		windowPlacement_.length = sizeof(WINDOWPLACEMENT);
		if (GetWindowPlacement(hwnd, &windowPlacement_) &&
			GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi))
		{
			SetWindowLong(hwnd, GWL_STYLE, (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
			SetWindowPos(hwnd, HWND_TOP,
				mi.rcMonitor.left, mi.rcMonitor.top,
				mi.rcMonitor.right - mi.rcMonitor.left,
				mi.rcMonitor.bottom - mi.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			isFullscreen_ = true;
		}
	}
}
