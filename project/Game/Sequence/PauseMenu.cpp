#define NOMINMAX
#include "PauseMenu.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Input/Input.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Scene/SceneManager.h"
#include "KHEngine/Debug/Imgui/ImGuiManager.h"
#include "externals/imgui/imgui.h"
#ifdef ENABLE_EDITOR
#include "KHEngine/Debug/Editor/EditorSystem.h"
#endif
#include <algorithm>
#include <string>

void PauseMenu::Toggle()
{
	isPaused_ = !isPaused_;
	if (isPaused_)
	{
		selectedOption_ = Option::Resume;
		SoundManager::GetInstance()->PlaySE("button.mp3", 0.6f);
	}
	else
	{
		SoundManager::GetInstance()->PlaySE("button.mp3", 0.5f);
	}
}

void PauseMenu::Update(SceneManager* sceneManager)
{
	if (sceneManager && sceneManager->IsTransitioning()) return;

	auto input = EngineServices::GetInstance()->GetInput();
	if (!input) return;

	// カーソル移動（上）
	bool upTrigger = input->TriggerKey(DIK_W) || input->TriggerKey(DIK_UP) || input->TriggerPadButton(XINPUT_GAMEPAD_DPAD_UP);
	// カーソル移動（下）
	bool downTrigger = input->TriggerKey(DIK_S) || input->TriggerKey(DIK_DOWN) || input->TriggerPadButton(XINPUT_GAMEPAD_DPAD_DOWN);

	if (upTrigger)
	{
		int opt = static_cast<int>(selectedOption_);
		opt = (opt - 1 + 3) % 3;
		selectedOption_ = static_cast<Option>(opt);
		SoundManager::GetInstance()->PlaySE("select.mp3", 0.6f);
	}
	else if (downTrigger)
	{
		int opt = static_cast<int>(selectedOption_);
		opt = (opt + 1) % 3;
		selectedOption_ = static_cast<Option>(opt);
		SoundManager::GetInstance()->PlaySE("select.mp3", 0.6f);
	}

	// 決定（SPACE / ENTER / Aボタン）
	if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) || input->TriggerPadButton(XINPUT_GAMEPAD_A))
	{
		SoundManager::GetInstance()->PlaySE("button.mp3", 0.7f);
		switch (selectedOption_)
		{
		case Option::Resume:
			isPaused_ = false;
			break;
		case Option::Retry:
			isPaused_ = false;
			if (sceneManager) sceneManager->ChangeScene("GAMEPLAY", 0.5f);
			break;
		case Option::Title:
			isPaused_ = false;
			if (sceneManager) sceneManager->ChangeScene("TITLE", 0.6f);
			break;
		}
	}
	// キャンセル／再開（Bボタン）
	else if (input->TriggerPadButton(XINPUT_GAMEPAD_B))
	{
		SoundManager::GetInstance()->PlaySE("button.mp3", 0.5f);
		isPaused_ = false;
	}
}

void PauseMenu::DrawUI()
{
#ifdef USE_IMGUI
	if (!isPaused_) return;

	if (ImGui::GetCurrentContext() == nullptr) return;
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (!viewport) return;
	ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
	if (!drawList) return;

	ImVec2 viewPos(0.0f, 0.0f);
	ImVec2 viewSize = viewport->Size;
#ifdef ENABLE_EDITOR
	if (EngineServices::GetInstance()->GetEditorMode())
	{
		viewPos = EditorSystem::GetInstance()->GetViewportPos();
		viewSize = EditorSystem::GetInstance()->GetViewportSize();
		if (viewSize.x <= 10.0f || viewSize.y <= 10.0f) return;
	}
#endif

	float screenW = viewSize.x;
	float screenH = viewSize.y;
	float scale = std::min(screenW / 1280.0f, screenH / 720.0f);
	if (scale < 0.35f) scale = 0.35f;

	drawList->PushClipRect(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), true);

	// 1. 画面全体のサイバーダーク半透明オーバーレイ
	drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(3, 10, 16, 215));

	// 2. 中央ポーズダイアログパネル
	float panelW = 440.0f * scale;
	float panelH = 340.0f * scale;
	float centerX = viewPos.x + screenW * 0.5f;
	float centerY = viewPos.y + screenH * 0.5f;
	ImVec2 panelMin(centerX - panelW * 0.5f, centerY - panelH * 0.5f);
	ImVec2 panelMax(centerX + panelW * 0.5f, centerY + panelH * 0.5f);

	// パネル背景（ディープティールグラデーション）
	drawList->AddRectFilled(panelMin, panelMax, IM_COL32(6, 20, 26, 240), 6.0f * scale);
	// パネル外枠（鮮やかなサイバーシアン）
	drawList->AddRect(panelMin, panelMax, IM_COL32(0, 230, 210, 220), 6.0f * scale, 0, 2.2f * scale);

	// 四隅のサイバーL字ブラケット
	float cornerLen = 14.0f * scale;
	float cThick = 3.0f * scale;
	ImU32 cornerCol = IM_COL32(255, 235, 45, 255); // ゴールドコーナー
	// 左上
	drawList->AddLine(panelMin, ImVec2(panelMin.x + cornerLen, panelMin.y), cornerCol, cThick);
	drawList->AddLine(panelMin, ImVec2(panelMin.x, panelMin.y + cornerLen), cornerCol, cThick);
	// 右上
	drawList->AddLine(ImVec2(panelMax.x, panelMin.y), ImVec2(panelMax.x - cornerLen, panelMin.y), cornerCol, cThick);
	drawList->AddLine(ImVec2(panelMax.x, panelMin.y), ImVec2(panelMax.x, panelMin.y + cornerLen), cornerCol, cThick);
	// 左下
	drawList->AddLine(ImVec2(panelMin.x, panelMax.y), ImVec2(panelMin.x + cornerLen, panelMax.y), cornerCol, cThick);
	drawList->AddLine(ImVec2(panelMin.x, panelMax.y), ImVec2(panelMin.x, panelMax.y - cornerLen), cornerCol, cThick);
	// 右下
	drawList->AddLine(panelMax, ImVec2(panelMax.x - cornerLen, panelMax.y), cornerCol, cThick);
	drawList->AddLine(panelMax, ImVec2(panelMax.x, panelMax.y - cornerLen), cornerCol, cThick);

	auto imguiManager = EngineServices::GetInstance()->GetImGuiManager();
	ImFont* font = imguiManager ? imguiManager->GetFont(ImGuiManager::FontType::Default) : ImGui::GetFont();
	if (!font) font = ImGui::GetFont();

	// テキスト描画ヘルパーラムダ
	auto DrawCenteredText = [&](const char* text, float y, float size, ImU32 col, bool shadow) {
		ImVec2 tSize = font->CalcTextSizeA(size, FLT_MAX, -1.0f, text);
		ImVec2 tPos(centerX - tSize.x * 0.5f, y);
		if (shadow)
		{
			float o = 1.6f * scale;
			drawList->AddText(font, size, ImVec2(tPos.x - o, tPos.y), IM_COL32(0, 0, 0, 240), text);
			drawList->AddText(font, size, ImVec2(tPos.x + o, tPos.y), IM_COL32(0, 0, 0, 240), text);
			drawList->AddText(font, size, ImVec2(tPos.x, tPos.y - o), IM_COL32(0, 0, 0, 240), text);
			drawList->AddText(font, size, ImVec2(tPos.x, tPos.y + o), IM_COL32(0, 0, 0, 240), text);
		}
		drawList->AddText(font, size, tPos, col, text);
	};

	// 3. タイトルテキスト「PAUSE」
	float titleY = panelMin.y + 24.0f * scale;
	DrawCenteredText("PAUSE", titleY, 36.0f * scale, IM_COL32(255, 255, 255, 255), true);

	// サブタイトル「MISSION SUSPENDED」
	float subY = titleY + 38.0f * scale;
	DrawCenteredText("- MISSION SUSPENDED -", subY, 14.0f * scale, IM_COL32(0, 235, 195, 200), true);

	// 区切り線
	float lineY = subY + 22.0f * scale;
	drawList->AddLine(ImVec2(panelMin.x + 30.0f * scale, lineY), ImVec2(panelMax.x - 30.0f * scale, lineY), IM_COL32(0, 180, 160, 120), 1.2f * scale);

	// 4. メニュー選択肢
	const char* options[3] = { "RESUME", "RETRY", "TITLE" };
	float menuStartY = lineY + 26.0f * scale;
	float itemSpacing = 42.0f * scale;

	for (int i = 0; i < 3; ++i)
	{
		bool isSelected = (static_cast<int>(selectedOption_) == i);
		float itemY = menuStartY + i * itemSpacing;

		if (isSelected)
		{
			// 選択中ハイライトバー
			float barW = panelW * 0.72f;
			float barH = 30.0f * scale;
			drawList->AddRectFilled(
				ImVec2(centerX - barW * 0.5f, itemY - 2.0f * scale),
				ImVec2(centerX + barW * 0.5f, itemY + barH - 2.0f * scale),
				IM_COL32(255, 215, 0, 45), 4.0f * scale
			);
			drawList->AddRect(
				ImVec2(centerX - barW * 0.5f, itemY - 2.0f * scale),
				ImVec2(centerX + barW * 0.5f, itemY + barH - 2.0f * scale),
				IM_COL32(255, 225, 45, 180), 4.0f * scale, 0, 1.4f * scale
			);

			// 選択中テキスト（ゴールド発光 ＋ インジケーター）
			std::string label = std::string(">  ") + options[i] + "  <";
			DrawCenteredText(label.c_str(), itemY, 22.0f * scale, IM_COL32(255, 235, 45, 255), true);
		}
		else
		{
			// 非選択テキスト（クールシルバー）
			DrawCenteredText(options[i], itemY, 20.0f * scale, IM_COL32(180, 205, 215, 180), true);
		}
	}

	// 5. 下部操作ガイド
	float guideY = panelMax.y - 32.0f * scale;
	DrawCenteredText("[W / S] SELECT   [SPACE / A] DECIDE   [ESC / START / B] RESUME", guideY, 11.5f * scale, IM_COL32(140, 175, 185, 170), false);

	drawList->PopClipRect();
#endif
}
