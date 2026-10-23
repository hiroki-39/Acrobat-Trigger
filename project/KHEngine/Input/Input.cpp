#include "Input.h"
#include "KHEngine/Core/Services/EngineServices.h"

using namespace Microsoft::WRL;

void Input::Initialize(WinApp* winApp)
{
	/*--- 共通初期化 ---*/

	//WindowAPIのポインタを受け取る
	winApp_ = winApp;

	HRESULT result;

	//DirecctInputの初期化
	result = DirectInput8Create(winApp_->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(result));

#pragma region "キーボード操作"

	//キーボードデバイスの生成
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result));

	//入力データ形式のセット
	result = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(result));

	//排他制御レベルのセット
	result = keyboard->SetCooperativeLevel(winApp_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));

#pragma endregion

#pragma region "マウス操作"

	//マウスデバイスの生成
	result = directInput->CreateDevice(GUID_SysMouse, &mouse, nullptr);
	assert(SUCCEEDED(result));

	//入力データ形式のセット（拡張マウス）
	result = mouse->SetDataFormat(&c_dfDIMouse2);
	assert(SUCCEEDED(result));

	//排他制御レベルのセット（マウスは通常NONEXCLUSIVE）
	result = mouse->SetCooperativeLevel(winApp_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(result));

#pragma endregion

#pragma region "コントローラ初期化（XInput）"

	// XInput は特別な初期化不要。接続確認だけ行う（0番コントローラを想定）
	DWORD dwResult = XInputGetState(0, &padState);
	padConnected = (dwResult == ERROR_SUCCESS);
	// padStatePre は 0 で初期化済み

#pragma endregion
}

void Input::Update()
{
	//前回のキーの入力状態を保存
	memcpy(keyPre, key, sizeof(key));

	//キーボードの入力情報取得
	keyboard->Acquire();

	//全キーの入力情報を取得（失敗しても継続）
	keyboard->GetDeviceState(sizeof(key), key);

	//マウスの前回状態を保存
	mouseStatePre = mouseState;

	//マウス取得
	if (mouse)
	{
		mouse->Acquire();
		// 取得に失敗しても zero-clear しないと未初期化参照になることを防ぐ
		if (FAILED(mouse->GetDeviceState(sizeof(DIMOUSESTATE2), &mouseState)))
		{
			// 切断や一時的に取得できない場合はゼロにする
			memset(&mouseState, 0, sizeof(mouseState));
		}
	}

	//コントローラの状態更新（0番を確認）
	XINPUT_STATE newPadState = {};
	DWORD dwResult = XInputGetState(0, &newPadState);
	if (dwResult == ERROR_SUCCESS)
	{
		padConnected = true;
		padStatePre = padState;
		padState = newPadState;
	}
	else
	{
		// 非接続時は状態をクリア
		padConnected = false;
		padStatePre = padState;
		memset(&padState, 0, sizeof(padState));
	}

	// 1のキーでデモプレイモードの解除・トグル切り替え
	bool padStartOrA = (padState.Gamepad.wButtons & (XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_A)) &&
	                   !(padStatePre.Gamepad.wButtons & (XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_A));
	if ((key[DIK_1] && !keyPre[DIK_1]) || (padConnected && padStartOrA))
	{
		auto services = EngineServices::GetInstance();
		if (services)
		{
			services->ToggleDemoMode();
		}
	}
}

bool Input::PushKey(BYTE keyNumber)
{
	// デモプレイ中は1キー以外のキー入力を受け付けない
	auto services = EngineServices::GetInstance();
	if (services && services->IsDemoMode())
	{
		if (keyNumber != DIK_1)
		{
			return false;
		}
	}

	//キーが押されている場合はtrueを返す
	if (key[keyNumber])
	{
		return true;
	}

	//押されていない場合はfalseを返す
	return false;
}

bool Input::TriggerKey(BYTE keyNumber)
{
	// デモプレイ中は1キー以外のキー入力を受け付けない
	auto services = EngineServices::GetInstance();
	if (services && services->IsDemoMode())
	{
		if (keyNumber != DIK_1)
		{
			return false;
		}
	}

	//キーが押されていて、前回の状態が押されていない場合はトリガーと判断する
	if (key[keyNumber] && !keyPre[keyNumber])
	{
		return true;
	}

	//トリガーの条件を満たさないものはすべてfalseを返す
	return false;
}

/* --- マウス --- */
bool Input::PushMouseButton(int button)
{
	auto services = EngineServices::GetInstance();
	if (services && services->IsDemoMode()) return false;

	if (button < 0 || button >= 4) return false;
	return (mouseState.rgbButtons[button] & 0x80) != 0;
}

bool Input::TriggerMouseButton(int button)
{
	auto services = EngineServices::GetInstance();
	if (services && services->IsDemoMode()) return false;

	if (button < 0 || button >= 4) return false;
	bool now = (mouseState.rgbButtons[button] & 0x80) != 0;
	bool prev = (mouseStatePre.rgbButtons[button] & 0x80) != 0;
	return (now && !prev);
}

LONG Input::GetMouseMoveX() const
{
	auto services = EngineServices::GetInstance();
	if (services && services->IsDemoMode()) return 0;

	return mouseState.lX;
}

LONG Input::GetMouseMoveY() const
{
	auto services = EngineServices::GetInstance();
	if (services && services->IsDemoMode()) return 0;

	return mouseState.lY;
}

LONG Input::GetMouseWheel() const
{
	auto services = EngineServices::GetInstance();
	if (services && services->IsDemoMode()) return 0;

	return mouseState.lZ;
}

/* --- コントローラ（XInput） --- */
bool Input::PushPadButton(WORD buttonMask)
{
	auto services = EngineServices::GetInstance();
	if (services && services->IsDemoMode()) return false;

	if (!padConnected) return false;
	return (padState.Gamepad.wButtons & buttonMask) != 0;
}

bool Input::TriggerPadButton(WORD buttonMask)
{
	auto services = EngineServices::GetInstance();
	if (services && services->IsDemoMode()) return false;

	if (!padConnected) return false;
	bool now = (padState.Gamepad.wButtons & buttonMask) != 0;
	bool prev = (padStatePre.Gamepad.wButtons & buttonMask) != 0;
	return (now && !prev);
}