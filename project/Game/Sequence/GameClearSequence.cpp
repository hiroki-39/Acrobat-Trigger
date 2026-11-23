#define NOMINMAX
#include "GameClearSequence.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/EnemyBullet.h"
#include "Game/System/RailCameraController.h"
#include "Game/Sequence/BossFinisherSequence.h"
#include "Game/Sequence/GameOverSequence.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Scene/SceneManager.h"
#include "KHEngine/UI/UITextManager.h"
#include "KHEngine/Debug/Imgui/ImGuiManager.h"
#include "externals/imgui/imgui.h"
#ifdef ENABLE_EDITOR
#include "KHEngine/Debug/Editor/EditorSystem.h"
#endif
#include <algorithm>
#include <cmath>
#include <cstdio>
void GameClearSequence::Start(const GameClearContext& ctx)
{


	bool fromFinisher = (ctx.bossFinisherSequence && ctx.bossFinisherSequence->IsCompleted());

		// 処刑フィニッシャー演出完了時は既に自機がカメラを横切って上空へ飛び立っているため、即座にクリアUI表示へ移行
	clearStep_ = fromFinisher ? Step::SHOW_UI : Step::ASCENDING;
	clearTimer_ = 0.0f;
	clearTotalTimer_ = 0.0f;
	clearAscentSpeed_ = fromFinisher ? 18.0f : 8.0f; // 上空へ高く上昇する初速

	if (ctx.player)
	{
		clearStartPlayerPos_ = ctx.player->GetTranslate();
		clearPlayerAscentPos_ = clearStartPlayerPos_;
		clearPlayerAscentRot_ = ctx.player->GetRotation();
		if (fromFinisher && ctx.player->IsCutsceneUsingQuaternion())
		{
			// フィニッシャー演出のクォータニオン姿勢と上昇方向をそのまま引き継ぎ、切り替わり瞬間の一瞬傾きを防止
			clearUseQuat_ = true;
			clearQuat_ = ctx.player->GetCutsceneQuaternion();
			Matrix4x4 rotM = clearQuat_.ToMatrix();
			Vector3 fwd = { rotM.m[2][0], rotM.m[2][1], rotM.m[2][2] };
			clearMoveDir_ = (fwd.Length() > 0.001f) ? fwd.Normalize() : Vector3{ 0.0f, 0.45f, 0.89f };
			ctx.player->SetCutsceneOverrideQuaternion(true, clearPlayerAscentPos_, clearQuat_);
		}
		else
		{
			clearUseQuat_ = false;
			ctx.player->SetCutsceneOverride(true, clearPlayerAscentPos_, clearPlayerAscentRot_);
		}
		ctx.player->Update3DObjectOnly();
	}

	// 敵弾を全消去して安全・爽快にクリア
	(*ctx.enemyBullets).clear();

	// レールカメラの前進を即座に停止（その場にとどまる）
	if (ctx.railCameraController)
	{
		ctx.railCameraController->SetSpeedMultiplier(0.0f);
		if (!ctx.railCameraController->IsCinematicActive())
		{
			Vector3 camPos = { 0.0f, 2.5f, -8.0f };
			Vector3 lookTarget = { 0.0f, clearStartPlayerPos_.y, clearStartPlayerPos_.z };
			ctx.railCameraController->SetCinematicCamera(true, camPos, lookTarget, 1.0f);
		}
	}

	// 画面揺れ停止
	(*ctx.cameraShakeTimer) = 0.0f;
}


void GameClearSequence::Update(float dt, const GameClearContext& ctx)
{
	clearTimer_ += dt;
	clearTotalTimer_ += dt;

	bool fromFinisher = (ctx.bossFinisherSequence && ctx.bossFinisherSequence->IsCompleted());

	// カメラ前進は完全停止を維持
	if (ctx.railCameraController)
	{
		ctx.railCameraController->SetSpeedMultiplier(0.0f);
		ctx.gameOverSequence->SetRetryStartRail(ctx.railCameraController->GetProgress(), ctx.railCameraController->GetCurrentRailIndex());

	}

	// 自機の上昇・加速・姿勢制御（ある程度上昇したら上昇を止めて非表示にする）
	const float kAscentHideTime = fromFinisher ? 3.2f : 4.5f;
	if (clearTotalTimer_ >= kAscentHideTime)
	{
		if (ctx.player)
		{
			ctx.player->SetVisible(false);
		}
	}
	else
	{
		clearAscentSpeed_ += 4.5f * dt;
		if (clearUseQuat_)
		{
			clearPlayerAscentPos_ = clearPlayerAscentPos_ + clearMoveDir_ * (clearAscentSpeed_ * dt);
			if (ctx.player)
			{
				ctx.player->SetCutsceneOverrideQuaternion(true, clearPlayerAscentPos_, clearQuat_);
				ctx.player->Update3DObjectOnly();
			}
		}
		else
		{
			clearPlayerAscentRot_.x += (-0.5f - clearPlayerAscentRot_.x) * (3.0f * dt);
			clearPlayerAscentRot_.y += (0.0f - clearPlayerAscentRot_.y) * (4.0f * dt);
			clearPlayerAscentRot_.z += (0.0f - clearPlayerAscentRot_.z) * (4.0f * dt);
			clearPlayerAscentPos_.y += (clearAscentSpeed_ * 0.65f) * dt;
			clearPlayerAscentPos_.z += (clearAscentSpeed_ * 1.15f) * dt;
			if (ctx.player)
			{
				ctx.player->SetCutsceneOverride(true, clearPlayerAscentPos_, clearPlayerAscentRot_);
				ctx.player->Update3DObjectOnly();
			}
		}
	}

	if (ctx.railCameraController)
	{
		if (fromFinisher)
		{
			// 処刑フィニッシャー完了時（SHOW_UI 〜 RESULT_PANEL）：
			// 上昇していく自機をそのままのカメラ位置・向きから見送り続ける
			Vector3 curCam = ctx.railCameraController->GetCinematicLocalPos();
			Vector3 curLook = ctx.railCameraController->GetCinematicLookAt();
			ctx.railCameraController->SetCinematicCamera(true, curCam, curLook, 1.0f);
		}
		else
		{
			float camRiseT = std::clamp(clearTotalTimer_ / 1.1f, 0.0f, 1.0f);
			float smoothRise = camRiseT * camRiseT * (3.0f - 2.0f * camRiseT); // SmoothStep

			Vector3 targetCamPos = {
				0.0f,
				2.5f + 1.5f * smoothRise,
				-8.0f - 1.0f * smoothRise
			};

			float maxLookUpY = clearStartPlayerPos_.y + 9.5f;
			float maxLookZ = clearStartPlayerPos_.z + 12.0f;
			float currentLookY = (std::min)(maxLookUpY, clearStartPlayerPos_.y + (clearPlayerAscentPos_.y - clearStartPlayerPos_.y) * 0.72f);
			float currentLookZ = (std::min)(maxLookZ, clearStartPlayerPos_.z + (clearPlayerAscentPos_.z - clearStartPlayerPos_.z) * 0.35f);
			Vector3 targetLook = { 0.0f, currentLookY, currentLookZ };

			Vector3 curCam = ctx.railCameraController->GetCinematicLocalPos();
			Vector3 curLook = ctx.railCameraController->GetCinematicLookAt();
			float lerpRate = std::clamp(dt * 5.5f, 0.0f, 1.0f);
			Vector3 nextCam = curCam + (targetCamPos - curCam) * lerpRate;
			Vector3 nextLook = curLook + (targetLook - curLook) * lerpRate;

			ctx.railCameraController->SetCinematicCamera(true, nextCam, nextLook, 1.0f);
		}
	}

	switch (clearStep_)
	{
	case Step::ASCENDING:
	{
		// 1.5秒ほど上昇したら「任務完了」UI表示ステップへ
		if (clearTimer_ >= 1.5f)
		{
			clearStep_ = Step::SHOW_UI;
			clearTimer_ = 0.0f;
		}
		break;
	}
	case Step::SHOW_UI:
	{
		// フェーズ4（5.2s〜7.5s）：約2.3秒間「MISSION ACCOMPLISHED / 任務完了」を表示後、リザルトパネルへ
		if (clearTimer_ >= 2.3f)
		{
			clearStep_ = Step::RESULT_PANEL;
			clearTimer_ = 0.0f;
			SoundManager::GetInstance()->PlaySE("select.mp3", 0.75f);
		}
		break;
	}
	case Step::RESULT_PANEL:
	{
		// 総合ランク確定スタンプ演出SE（1.35sに1回再生）
		static bool s_rankStampPlayed = false;
		if (clearTimer_ < 0.1f) s_rankStampPlayed = false;
		if (clearTimer_ >= 1.35f && !s_rankStampPlayed)
		{
			s_rankStampPlayed = true;
			SoundManager::GetInstance()->PlaySE("button.mp3", 0.85f);
		}

		// 全項目＆ランク表示後、SPACE / ENTER / パッドAでシャットダウン遷移（CRT_Off）してタイトルへ復帰
		// デモプレイ時はリザルト表示後（4.2秒後）に自動でタイトルへ復帰
		auto services = EngineServices::GetInstance();
		bool isDemoAutoReturn = (services && services->IsDemoMode() && clearTimer_ >= 4.2f);
		if (clearTimer_ >= 1.65f)
		{
			auto sceneManager = ctx.sceneManager;
			if (sceneManager && !sceneManager->IsTransitioning())
			{
				auto input = services ? services->GetInput() : nullptr;
				if (isDemoAutoReturn || (input && (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) || input->TriggerPadButton(XINPUT_GAMEPAD_A))))
				{
					SoundManager::GetInstance()->StopBGM();
					SoundManager::GetInstance()->PlaySE("button.mp3", 0.7f);
					sceneManager->ChangeScene("TITLE", 0.90f, { 0.0f, 0.0f, 0.0f, 1.0f }, TransitionType::CRT_Off);
				}
			}
		}
		break;
	}
	}
}


void GameClearSequence::DrawUI(bool isClearPhase, const GameClearContext& ctx)
{
#ifdef USE_IMGUI
	if (!isClearPhase) return;

	if (ImGui::GetCurrentContext() == nullptr) return;
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (!viewport) return;
	ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
	if (!drawList) return;

	// ゲーム画面の表示領域を取得（エディタモード時はゲーム画面ビューポート内、フルスクリーン時は全画面）
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

	// トランジション（画面遷移）中のアルファ制御
	float transitionAlpha = 1.0f;
	auto sceneManager = EngineServices::GetInstance()->GetSceneManager();
	if (sceneManager && sceneManager->IsTransitioning())
	{
		auto state = sceneManager->GetTransitionState();
		float prog = sceneManager->GetTransitionProgress();
		switch (state)
		{
		case SceneManager::TransitionState::FadeOut:
			transitionAlpha = (std::max)(0.0f, 1.0f - prog);
			break;
		case SceneManager::TransitionState::FadeOutHold:
		case SceneManager::TransitionState::Loading:
		case SceneManager::TransitionState::FadeInWait:
			transitionAlpha = 0.0f;
			break;
		case SceneManager::TransitionState::FadeIn:
			transitionAlpha = (std::min)(1.0f, prog);
			break;
		default:
			transitionAlpha = 1.0f;
			break;
		}
	}

	// ゲーム画面ビューポートの枠外へはみ出さないようにクリッピング
	drawList->PushClipRect(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), true);

	// 画面遷移の完全暗転中はUIテキストを描画せず、ビューポートを真っ黒にしてテキストを完全に「画面遷移の後ろ」に隠す
	if (transitionAlpha <= 0.001f)
	{
		drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(0, 0, 0, 255));
		drawList->PopClipRect();
		return;
	}

	// スケーリング比率（1280x720 基準）
	float scale = std::min(screenW / 1280.0f, screenH / 720.0f);
	if (scale < 0.3f) scale = 0.3f;

	// === 上下の映画風黒帯（シネマスコープ・レターボックス） ===
	// ※黒帯が出ているムービー中（ASCENDING）はUIを一切表示せず、UI表示ステップ（SHOW_UI）で黒帯を収納
	float barHeightMax = screenH * 0.12f;
	float barProgress = 1.0f;
	if (clearStep_ == Step::ASCENDING)
	{
		bool fromFinisher = (ctx.bossFinisherSequence && ctx.bossFinisherSequence->IsCompleted());
		barProgress = fromFinisher ? 1.0f : std::clamp(clearTimer_ / 0.4f, 0.0f, 1.0f);
	}
	else if (clearStep_ == Step::SHOW_UI)
	{
		// 任務完了のUIが出ている間は黒帯を出したまま維持
		barProgress = 1.0f;
	}
	else if (clearStep_ == Step::RESULT_PANEL)
	{
		// リザルト画面時は黒帯なし
		barProgress = 0.0f;
	}
	float barHeight = barHeightMax * barProgress;
	if (barHeight > 0.5f)
	{
		drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + barHeight), IM_COL32(0, 0, 0, static_cast<int>(255 * transitionAlpha)));
		drawList->AddRectFilled(ImVec2(viewPos.x, viewPos.y + screenH - barHeight), ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(0, 0, 0, static_cast<int>(255 * transitionAlpha)));
	}

	auto imguiManager = EngineServices::GetInstance()->GetImGuiManager();
	ImFont* fontMplus = imguiManager ? imguiManager->GetFont(ImGuiManager::FontType::Japanese_MPLUS) : ImGui::GetFont();
	if (!fontMplus) fontMplus = ImGui::GetFont();
	ImFont* fontMono = imguiManager ? imguiManager->GetFont(ImGuiManager::FontType::English_FiraMono) : fontMplus;
	if (!fontMono) fontMono = fontMplus;

	if (clearStep_ == Step::SHOW_UI)
	{
		// =====================================================================
		// フェーズ4：任務完了UI展開 (5.2s 〜 7.5s)
		// 暗い横帯ではなく、シアン発光の角括弧（ブラケット枠 [ ] ）付きで
		// 「MISSION ACCOMPLISHED / 任務完了」をコンパクトに展開
		// =====================================================================
		float fadeIn = std::clamp(clearTimer_ / 0.30f, 0.0f, 1.0f);
		float closeT = (clearTimer_ > 1.95f) ? std::clamp((clearTimer_ - 1.95f) / 0.25f, 0.0f, 1.0f) : 0.0f;
		float easeClose = 1.0f - (1.0f - closeT) * (1.0f - closeT) * (1.0f - closeT);
		float closeAlpha = (clearTimer_ > 2.15f) ? std::clamp(1.0f - (clearTimer_ - 2.15f) / 0.12f, 0.0f, 1.0f) : 1.0f;
		float uiAlpha = fadeIn * closeAlpha * transitionAlpha;
		float textFadeOut = (clearTimer_ > 1.95f) ? std::clamp(1.0f - (clearTimer_ - 1.95f) / 0.14f, 0.0f, 1.0f) : 1.0f;
		float textAlpha = fadeIn * textFadeOut * transitionAlpha;

		if (uiAlpha > 0.001f)
		{
			float cx = viewPos.x + screenW * 0.5f;
			float cy = viewPos.y + screenH * 0.44f;

			// ブラケット枠 [ ] の展開・収納アニメーション（開く時と同じくパッと閉じる）
			float openT = std::clamp(clearTimer_ / 0.35f, 0.0f, 1.0f);
			float easeOpen = 1.0f - (1.0f - openT) * (1.0f - openT) * (1.0f - openT);
			float widthRatio = easeOpen * (1.0f - easeClose);
			float halfW = (20.0f + 255.0f * widthRatio) * scale;
			float halfH = 48.0f * scale;
			float armLen = 22.0f * scale;
			float thick = 2.8f * scale;

			// 淡いホログラム背景（横帯ではなくブラケット内部のみのコンパクトな枠）
			drawList->AddRectFilled(
				ImVec2(cx - halfW, cy - halfH),
				ImVec2(cx + halfW, cy + halfH),
				IM_COL32(4, 24, 38, static_cast<int>(115.0f * uiAlpha)),
				4.0f * scale
			);

			// シアン発光グロー＆角括弧 [ ]
			ImU32 glowCol = IM_COL32(0, 235, 255, static_cast<int>(70.0f * uiAlpha));
			ImU32 bracketCol = IM_COL32(0, 245, 255, static_cast<int>(255.0f * uiAlpha));

			// 左括弧 [
			drawList->AddLine(ImVec2(cx - halfW + armLen, cy - halfH), ImVec2(cx - halfW, cy - halfH), glowCol, thick * 2.5f);
			drawList->AddLine(ImVec2(cx - halfW, cy - halfH), ImVec2(cx - halfW, cy + halfH), glowCol, thick * 2.5f);
			drawList->AddLine(ImVec2(cx - halfW, cy + halfH), ImVec2(cx - halfW + armLen, cy + halfH), glowCol, thick * 2.5f);

			drawList->AddLine(ImVec2(cx - halfW + armLen, cy - halfH), ImVec2(cx - halfW, cy - halfH), bracketCol, thick);
			drawList->AddLine(ImVec2(cx - halfW, cy - halfH), ImVec2(cx - halfW, cy + halfH), bracketCol, thick);
			drawList->AddLine(ImVec2(cx - halfW, cy + halfH), ImVec2(cx - halfW + armLen, cy + halfH), bracketCol, thick);

			// 右括弧 ]
			drawList->AddLine(ImVec2(cx + halfW - armLen, cy - halfH), ImVec2(cx + halfW, cy - halfH), glowCol, thick * 2.5f);
			drawList->AddLine(ImVec2(cx + halfW, cy - halfH), ImVec2(cx + halfW, cy + halfH), glowCol, thick * 2.5f);
			drawList->AddLine(ImVec2(cx + halfW, cy + halfH), ImVec2(cx + halfW - armLen, cy + halfH), glowCol, thick * 2.5f);

			drawList->AddLine(ImVec2(cx + halfW - armLen, cy - halfH), ImVec2(cx + halfW, cy - halfH), bracketCol, thick);
			drawList->AddLine(ImVec2(cx + halfW, cy - halfH), ImVec2(cx + halfW, cy + halfH), bracketCol, thick);
			drawList->AddLine(ImVec2(cx + halfW, cy + halfH), ImVec2(cx + halfW - armLen, cy + halfH), bracketCol, thick);

			// テキスト描画（UITextManagerで編集された文言・位置・サイズ・色・スタイルをそのまま反映。削除・空文字の場合は描画しない）
			auto DrawManagedClearText = [&](const char* itemId) {
				if (textAlpha <= 0.001f) return;
				auto textMgr = UITextManager::GetInstance();
				UITextItem* item = textMgr ? textMgr->GetTextItem(itemId, "GAMEPLAY") : nullptr;
				if (!item || item->text.empty()) return;

				const char* txt = item->text.c_str();
				ImFont* font = fontMplus;
				if (imguiManager) {
					ImFont* f = imguiManager->GetFont(item->fontType);
					if (f) font = f;
				}
				float fSize = item->fontSize * scale;
				if (fSize < 6.0f) fSize = 6.0f;

				ImVec2 tSz = font->CalcTextSizeA(fSize, FLT_MAX, -1.0f, txt);
				float scaleX = screenW / 1280.0f;
				float scaleY = screenH / 720.0f;
				ImVec2 tPos(viewPos.x + item->position.x * scaleX, viewPos.y + item->position.y * scaleY);
				if (item->align == UITextAlign::Center) tPos.x -= tSz.x * 0.5f;
				else if (item->align == UITextAlign::Right) tPos.x -= tSz.x;

				if (item->hasShadow) {
					ImU32 shCol = ImGui::ColorConvertFloat4ToU32(ImVec4(item->shadowColor.x, item->shadowColor.y, item->shadowColor.z, item->shadowColor.w * textAlpha));
					drawList->AddText(font, fSize, ImVec2(tPos.x + item->shadowOffset.x * scale, tPos.y + item->shadowOffset.y * scale), shCol, txt);
				}
				if (item->hasOutline) {
					float thickOut = item->outlineThickness * scale;
					Vector4 oc = item->outlineColor;
					ImU32 outCol = ImGui::ColorConvertFloat4ToU32(ImVec4(oc.x, oc.y, oc.z, oc.w * textAlpha));
					drawList->AddText(font, fSize, ImVec2(tPos.x - thickOut, tPos.y), outCol, txt);
					drawList->AddText(font, fSize, ImVec2(tPos.x + thickOut, tPos.y), outCol, txt);
					drawList->AddText(font, fSize, ImVec2(tPos.x, tPos.y - thickOut), outCol, txt);
					drawList->AddText(font, fSize, ImVec2(tPos.x, tPos.y + thickOut), outCol, txt);
				}
				Vector4 mc = item->color;
				ImU32 mainCol = ImGui::ColorConvertFloat4ToU32(ImVec4(mc.x, mc.y, mc.z, mc.w * textAlpha));
				drawList->AddText(font, fSize, tPos, mainCol, txt);
			};

			DrawManagedClearText("MissionAccomplishedTitle");
			DrawManagedClearText("MissionAccomplishedSub");
		}
	}
	else if (clearStep_ == Step::RESULT_PANEL)
	{
		// =====================================================================
		// フェーズ⑦：リザルトUI展開（上昇中の自機を背景に即座に展開）
		// =====================================================================
		float resT = clearTimer_;
		float openT = std::clamp(resT / 0.35f, 0.0f, 1.0f);
		float easeOpen = 1.0f - (1.0f - openT) * (1.0f - openT) * (1.0f - openT);
		float uiAlpha = openT * transitionAlpha;

		float cx = viewPos.x + screenW * 0.5f;
		float cy = viewPos.y + screenH * 0.50f;
		float panelW = screenW;
		float panelH = (48.0f * scale) + (screenH - 48.0f * scale) * easeOpen;

		ImVec2 pMin(cx - panelW * 0.5f, cy - panelH * 0.5f);
		ImVec2 pMax(cx + panelW * 0.5f, cy + panelH * 0.5f);

		// 半透明ダークホログラム背景（背景の自機が透けて見える全画面サイズ）
		drawList->AddRectFilled(pMin, pMax, IM_COL32(4, 16, 30, static_cast<int>(180.0f * uiAlpha)), 0.0f);
		drawList->AddRect(pMin, pMax, IM_COL32(0, 225, 255, static_cast<int>(190.0f * uiAlpha)), 0.0f, 0, 2.2f * scale);

		// 内部スキャンライン（ホログラム質感）
		for (float y = pMin.y + 4.0f * scale; y < pMax.y; y += 6.0f * scale)
		{
			drawList->AddLine(ImVec2(pMin.x, y), ImVec2(pMax.x, y), IM_COL32(0, 235, 255, static_cast<int>(14.0f * uiAlpha)), 1.0f);
		}

		// 四隅のシアン発光L字アクセント
		float cornerLen = 18.0f * scale;
		ImU32 cornerCol = IM_COL32(0, 255, 255, static_cast<int>(255.0f * uiAlpha));
		float cThick = 3.0f * scale;
		drawList->AddLine(pMin, ImVec2(pMin.x + cornerLen, pMin.y), cornerCol, cThick);
		drawList->AddLine(pMin, ImVec2(pMin.x, pMin.y + cornerLen), cornerCol, cThick);
		drawList->AddLine(ImVec2(pMax.x, pMin.y), ImVec2(pMax.x - cornerLen, pMin.y), cornerCol, cThick);
		drawList->AddLine(ImVec2(pMax.x, pMin.y), ImVec2(pMax.x, pMin.y + cornerLen), cornerCol, cThick);
		drawList->AddLine(ImVec2(pMin.x, pMax.y), ImVec2(pMin.x + cornerLen, pMax.y), cornerCol, cThick);
		drawList->AddLine(ImVec2(pMin.x, pMax.y), ImVec2(pMin.x, pMax.y - cornerLen), cornerCol, cThick);
		drawList->AddLine(pMax, ImVec2(pMax.x - cornerLen, pMax.y), cornerCol, cThick);
		drawList->AddLine(pMax, ImVec2(pMax.x, pMax.y - cornerLen), cornerCol, cThick);

		if (openT > 0.65f)
		{
			float contentAlpha = std::clamp((openT - 0.65f) / 0.35f, 0.0f, 1.0f) * transitionAlpha;

			// ヘッダー
			const char* headerStr = "MISSION RESULT // 戦績評価ログ";
			float headerSize = 30.0f * scale;
			drawList->AddText(fontMplus, headerSize, ImVec2(pMin.x + 56.0f * scale, pMin.y + 32.0f * scale), IM_COL32(0, 240, 255, static_cast<int>(255.0f * contentAlpha)), headerStr);
			drawList->AddLine(ImVec2(pMin.x + 48.0f * scale, pMin.y + 82.0f * scale), ImVec2(pMax.x - 48.0f * scale, pMin.y + 82.0f * scale), IM_COL32(0, 200, 240, static_cast<int>(130.0f * contentAlpha)), 1.6f * scale);

			// 戦績データの算出
			int totalEnemies = (std::max)(1, ctx.totalEnemiesCount);
			int killedEnemies = std::clamp(ctx.killedEnemiesCount, 0, totalEnemies);
			int killPct = static_cast<int>(std::round((static_cast<float>(killedEnemies) / static_cast<float>(totalEnemies)) * 100.0f));

			int hp = ctx.player ? ctx.player->GetHp() : 100;
			int maxHp = (ctx.player && ctx.player->GetMaxHp() > 0) ? ctx.player->GetMaxHp() : 100;
			int integrityPct = std::clamp(static_cast<int>(std::round((static_cast<float>(hp) / static_cast<float>(maxHp)) * 100.0f)), 0, 100);
			int damagePct = 100 - integrityPct;

			// 総合スコアからランク算出 (S / A / B / C)
			float rankScore = integrityPct * 0.45f + killPct * 0.35f;
			if (ctx.stageElapsedTime < 120.0f) rankScore += 20.0f;
			else if (ctx.stageElapsedTime < 180.0f) rankScore += 12.0f;
			else rankScore += 5.0f;

			const char* rankChar = "C";
			ImU32 rankColor = IM_COL32(190, 210, 225, static_cast<int>(255.0f * transitionAlpha));
			if (rankScore >= 85.0f) { rankChar = "S"; rankColor = IM_COL32(255, 215, 40, static_cast<int>(255.0f * transitionAlpha)); }
			else if (rankScore >= 65.0f) { rankChar = "A"; rankColor = IM_COL32(0, 245, 255, static_cast<int>(255.0f * transitionAlpha)); }
			else if (rankScore >= 45.0f) { rankChar = "B"; rankColor = IM_COL32(80, 255, 160, static_cast<int>(255.0f * transitionAlpha)); }

			// 時間フォーマットヘルパー
			auto formatTime = [](float sec, char* outBuf, size_t bufSz) {
				if (sec < 0.0f) sec = 0.0f;
				int mins = static_cast<int>(sec) / 60;
				float rem = sec - static_cast<float>(mins * 60);
				snprintf(outBuf, bufSz, "%02d:%05.2f", mins, rem);
			};

			// 各行描画ラムダ（高速カウントアップ演出付き）
			float startX = pMin.x + 64.0f * scale;
			float rowY = pMin.y + 115.0f * scale;
			float rowStep = 112.0f * scale;

			auto drawStatRow = [&](int idx, float startDelay, const char* jpLabel, const char* enLabel, const char* valStr) {
				if (resT < startDelay) return;
				float rowT = std::clamp((resT - startDelay) / 0.25f, 0.0f, 1.0f);
				float y = rowY + idx * rowStep;

				drawList->AddText(fontMplus, 26.0f * scale, ImVec2(startX, y), IM_COL32(220, 245, 255, static_cast<int>(255.0f * rowT * transitionAlpha)), jpLabel);
				drawList->AddText(fontMono, 16.0f * scale, ImVec2(startX, y + 32.0f * scale), IM_COL32(0, 200, 235, static_cast<int>(200.0f * rowT * transitionAlpha)), enLabel);

				drawList->AddText(fontMono, 36.0f * scale, ImVec2(startX + 410.0f * scale, y + 10.0f * scale), IM_COL32(255, 255, 255, static_cast<int>(255.0f * rowT * transitionAlpha)), valStr);
				drawList->AddLine(ImVec2(startX, y + 76.0f * scale), ImVec2(startX + 740.0f * scale, y + 76.0f * scale), IM_COL32(0, 180, 220, static_cast<int>(65.0f * rowT * transitionAlpha)), 1.4f);
			};

			// 1. 敵撃破数
			{
				float countT = std::clamp((resT - 0.15f) / 0.35f, 0.0f, 1.0f);
				int dispKill = static_cast<int>(killedEnemies * countT);
				int dispPct = static_cast<int>(killPct * countT);
				char buf[64];
				snprintf(buf, sizeof(buf), "%d / %d (%d%%)", dispKill, totalEnemies, dispPct);
				drawStatRow(0, 0.15f, "敵撃破数", "ENEMIES DESTROYED", buf);
			}
			// 2. クリアタイム（ステージ全体）
			{
				float countT = std::clamp((resT - 0.45f) / 0.35f, 0.0f, 1.0f);
				char buf[64];
				formatTime(ctx.stageElapsedTime * countT, buf, sizeof(buf));
				drawStatRow(1, 0.45f, "クリアタイム（ステージ全体）", "TOTAL MISSION TIME", buf);
			}
			// 3. クリアタイム（ボス戦）
			{
				float countT = std::clamp((resT - 0.75f) / 0.35f, 0.0f, 1.0f);
				char buf[64];
				formatTime(ctx.bossElapsedTime * countT, buf, sizeof(buf));
				drawStatRow(2, 0.75f, "クリアタイム（ボス戦）", "BOSS ENGAGEMENT TIME", buf);
			}
			// 4. 被弾率 / 残存装甲率
			{
				float countT = std::clamp((resT - 1.05f) / 0.35f, 0.0f, 1.0f);
				int dispDmg = static_cast<int>(damagePct * countT);
				int dispInt = static_cast<int>(integrityPct * countT);
				char buf[64];
				snprintf(buf, sizeof(buf), "%d%% (INTEGRITY: %d%%)", dispDmg, dispInt);
				drawStatRow(3, 1.05f, "被弾率 / 残存装甲率", "DAMAGE RATE / INTEGRITY", buf);
			}

			// 右側：総合評価ランク枠 & ネオン発光スタンプ演出
			float rankBoxX = pMax.x - 350.0f * scale;
			float rankBoxY = pMin.y + 120.0f * scale;
			float rankBoxW = 290.0f * scale;
			float rankBoxH = 390.0f * scale;
			drawList->AddRectFilled(ImVec2(rankBoxX, rankBoxY), ImVec2(rankBoxX + rankBoxW, rankBoxY + rankBoxH), IM_COL32(2, 12, 24, static_cast<int>(180.0f * contentAlpha)), 6.0f * scale);
			drawList->AddRect(ImVec2(rankBoxX, rankBoxY), ImVec2(rankBoxX + rankBoxW, rankBoxY + rankBoxH), IM_COL32(0, 220, 255, static_cast<int>(130.0f * contentAlpha)), 6.0f * scale, 0, 1.6f * scale);

			const char* evalStr = "EVALUATION RANK";
			ImVec2 evSz = fontMono->CalcTextSizeA(20.0f * scale, FLT_MAX, -1.0f, evalStr);
			drawList->AddText(fontMono, 20.0f * scale, ImVec2(rankBoxX + (rankBoxW - evSz.x) * 0.5f, rankBoxY + 22.0f * scale), IM_COL32(0, 235, 255, static_cast<int>(230.0f * contentAlpha)), evalStr);

			const char* evalJp = "総合評価";
			ImVec2 evJpSz = fontMplus->CalcTextSizeA(22.0f * scale, FLT_MAX, -1.0f, evalJp);
			drawList->AddText(fontMplus, 22.0f * scale, ImVec2(rankBoxX + (rankBoxW - evJpSz.x) * 0.5f, rankBoxY + 52.0f * scale), IM_COL32(210, 240, 255, static_cast<int>(210.0f * contentAlpha)), evalJp);

			if (resT >= 1.35f)
			{
				float stampT = std::clamp((resT - 1.35f) / 0.18f, 0.0f, 1.0f);
				float stampScale = 2.2f - 1.2f * (stampT * stampT * (3.0f - 2.0f * stampT));
				float rFontSize = 190.0f * scale * stampScale;
				ImVec2 rSz = fontMplus->CalcTextSizeA(rFontSize, FLT_MAX, -1.0f, rankChar);
				ImVec2 rPos(rankBoxX + (rankBoxW - rSz.x) * 0.5f, rankBoxY + 115.0f * scale + (190.0f * scale - rSz.y) * 0.5f);

				// 発光シャドウ
				drawList->AddText(fontMplus, rFontSize, ImVec2(rPos.x + 3.0f * scale, rPos.y + 3.0f * scale), IM_COL32(0, 0, 0, static_cast<int>(220.0f * stampT * transitionAlpha)), rankChar);
				drawList->AddText(fontMplus, rFontSize, rPos, rankColor, rankChar);
			}

			// 下部：タイトル復帰ガイド（呼吸点滅）
			if (resT >= 1.65f)
			{
				float pulse = 0.65f + 0.35f * std::sin((resT - 1.65f) * 5.0f);
				const char* guideStr = "[ PRESS SPACE TO RETURN TITLE ]";
				float gFontSize = 24.0f * scale;
				ImVec2 gSz = fontMono->CalcTextSizeA(gFontSize, FLT_MAX, -1.0f, guideStr);
				drawList->AddText(fontMono, gFontSize, ImVec2(cx - gSz.x * 0.5f, pMax.y - 54.0f * scale), IM_COL32(0, 245, 255, static_cast<int>(255.0f * pulse * transitionAlpha)), guideStr);
			}
		}
	}

	// 画面遷移（トランジション）の黒カーテンをテキストの手前に被せることで、テキストを完全に「画面遷移の後ろ」に配置
	if (transitionAlpha < 0.999f)
	{
		float blackAlpha = std::clamp(1.0f - transitionAlpha, 0.0f, 1.0f);
		drawList->AddRectFilled(
			viewPos,
			ImVec2(viewPos.x + screenW, viewPos.y + screenH),
			IM_COL32(0, 0, 0, static_cast<int>(255.0f * blackAlpha))
		);
	}

	drawList->PopClipRect();
#endif
}
