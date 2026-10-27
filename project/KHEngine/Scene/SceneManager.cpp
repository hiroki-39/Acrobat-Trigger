#include "SceneManager.h"
#include <cassert>
#include <algorithm>
#include "KHEngine/Graphics/Transition/TransitionRenderer.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "KHEngine/Core/OS/WinApp.h"
#include "KHEngine/UI/UITextManager.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace
{
	// スムーズステップ (3t^2 - 2t^3): 加減速が滑らかで映画のような美しいトランジションを実現
	inline float SmoothStep(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}
}

SceneManager::SceneManager() = default;

SceneManager::~SceneManager()
{
	if (scene_)
	{
		scene_->Finalize();
		scene_.reset();
	}
	if (nextScene_)
	{
		nextScene_->Finalize();
		nextScene_.reset();
	}
	transitionRenderer_.reset();
}

void SceneManager::InitTransition()
{
	if (transitionRenderer_) return;

	auto dxCommon = EngineServices::GetInstance()->GetDirectXCommon();
	if (!dxCommon) return;

	transitionRenderer_ = std::make_unique<TransitionRenderer>();
	transitionRenderer_->Initialize(dxCommon);

	// ルールテクスチャのインデックス取得
	const std::string rulePath = "resources/textures/rules/rule_horizontal.png";
	auto texManager = TextureManager::GetInstance();
	ruleTextureIndex_ = texManager->GetTextureIndexByFilePath(rulePath);
	if (ruleTextureIndex_ == UINT32_MAX || ruleTextureIndex_ == 0)
	{
		texManager->LoadTexture(rulePath);
		ruleTextureIndex_ = texManager->GetTextureIndexByFilePath(rulePath);
	}
}

void SceneManager::Update()
{
	float dt = EngineServices::GetInstance()->GetDeltaTime();
	if (dt <= 0.0f)
	{
		dt = 1.0f / 60.0f;
	}

	switch (transitionState_)
	{
	case TransitionState::None:
		if (scene_)
		{
			scene_->Update();
		}
		break;

	case TransitionState::FadeOut:
		// 旧シーンを通常更新（アニメーションや演出が止まらない）
		if (scene_)
		{
			scene_->Update();
		}

		fadeTimer_ += dt;
		{
			float t = std::clamp(fadeTimer_ / fadeDuration_, 0.0f, 1.0f);
			transitionProgress_ = SmoothStep(t);
		}

		if (fadeTimer_ >= fadeDuration_)
		{
			// 左右端から中央まで閉じた（完全暗転）
			transitionProgress_ = 1.0f;
			transitionState_ = TransitionState::FadeOutHold;
			holdFrames_ = 0;
		}
		break;

	case TransitionState::FadeOutHold:
		// 完全な黒画面が画面（フロントバッファ・バックバッファ）に描画・表示されるのを待つ
		transitionProgress_ = 1.0f;

		holdFrames_++;
		if (holdFrames_ >= 2)
		{
			// 画面が完全に閉じた状態で、ロードステートへ
			transitionState_ = TransitionState::Loading;
		}
		break;

	case TransitionState::Loading:
		// 画面は100%真っ黒。この裏で新シーンを生成・初期化
		transitionProgress_ = 1.0f;

		if (scene_)
		{
			scene_->Finalize();
			scene_.reset();
		}

		assert(sceneFactory_);
		scene_ = sceneFactory_->CreateScene(nextSceneName_);
		if (scene_)
		{
			currentSceneName_ = nextSceneName_;
			scene_->SetSceneManager(this);

#ifdef ENABLE_EDITOR
			// エディタモード時は新シーン移行時に停止状態にし、再生ボタンが押されるまでBGM/SEを待機
			if (EngineServices::GetInstance()->GetEditorMode())
			{
				EngineServices::GetInstance()->SetGamePlaying(false);
			}
#endif

			scene_->Initialize();
			UITextManager::GetInstance()->SetCurrentScene(currentSceneName_);
		}

		// ロード完了：ロード時の重いフレーム（0.6秒など）によるdeltaTime急増を吸収するため、ウェイト状態へ
		transitionState_ = TransitionState::FadeInWait;
		holdFrames_ = 0;
		break;

	case TransitionState::FadeInWait:
		// 暗転中に新シーンの初期配置（行列計算・カメラ更新等）を1フレーム先行更新
		transitionProgress_ = 0.0f; // アウト（開く）の開始点: 0.0（全閉）から中央が開く

		if (scene_)
		{
			scene_->Update();
		}

		holdFrames_++;
		if (holdFrames_ >= 2)
		{
			// アウト（明転・開く）開始
			transitionState_ = TransitionState::FadeIn;
			fadeTimer_ = 0.0f;
			transitionProgress_ = 0.0f;
		}
		break;

	case TransitionState::FadeIn:
		// フェードイン中は新シーンを更新
		if (scene_)
		{
			scene_->Update();
		}

		fadeTimer_ += dt;
		{
			float t = std::clamp(fadeTimer_ / fadeDuration_, 0.0f, 1.0f);
			transitionProgress_ = SmoothStep(t);
		}

		if (fadeTimer_ >= fadeDuration_)
		{
			// 中央から左右端へ完全に開いた（遷移完了）
			transitionState_ = TransitionState::None;
			transitionProgress_ = 0.0f;
			transitionType_ = defaultTransitionType_;
		}
		break;
	}
}

void SceneManager::Draw()
{
	if (scene_)
	{
		scene_->Draw();
	}
}

void SceneManager::DrawUI()
{
	if (scene_)
	{
		scene_->DrawUI();
	}

#ifdef USE_IMGUI
	// GAMEPLAYおよびTITLE以外のシーンでもエディタモード時にシーン移動できるようにする
	if (currentSceneName_ != "GAMEPLAY" && currentSceneName_ != "TITLE" && EngineServices::GetInstance()->GetEditorMode())
	{
		ImGui::SetNextWindowSize(ImVec2(380, 260), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Scene Debug (F1: Toggle)", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			DrawSceneSelectorUI();
			ImGui::Spacing();
			if (ImGui::CollapsingHeader("トランジション設定"))
			{
				DrawTransitionSettingsUI();
			}
		}
		ImGui::End();
	}
#endif

}

void SceneManager::DrawTransition()
{
// ルール画像トランジション（最前面）描画
	if (transitionState_ != TransitionState::None && transitionRenderer_ && ruleTextureIndex_ != UINT32_MAX)
	{
		bool isOpening = (transitionState_ == TransitionState::FadeIn || transitionState_ == TransitionState::FadeInWait);
		float prog = transitionProgress_;

		// 完全暗転ホールド中またはロード中は確実に全閉（prog = 1.0f, isOpening = false）
		if (transitionState_ == TransitionState::FadeOutHold || transitionState_ == TransitionState::Loading)
		{
			prog = 1.0f;
			isOpening = false;
		}

		transitionRenderer_->Draw(ruleTextureIndex_, prog, isOpening,
			transitionType_, fadeColor_, edgeSoftness_, edgeColor_,
			hexOrigin_, hexTileScale_, hexBorderWidth_, hexGlitchIntensity_);
	}
}

void SceneManager::ChangeScene(const std::string& sceneName, float fadeDuration, const Vector4& fadeColor, TransitionType type)
{
	assert(sceneFactory_);

	// 既に遷移中の場合は二重リクエストを無視
	if (transitionState_ != TransitionState::None)
	{
		return;
	}

	// 初回読み込み（起動時等、まだシーンが存在しない場合）
	if (!scene_)
	{
		scene_ = sceneFactory_->CreateScene(sceneName);
		if (scene_)
		{
			currentSceneName_ = sceneName;
			scene_->SetSceneManager(this);
			scene_->Initialize();
			UITextManager::GetInstance()->SetCurrentScene(currentSceneName_);
		}
		transitionState_ = TransitionState::None;
		transitionProgress_ = 0.0f;
		return;
	}

	// 即時切り替え（フェード時間0以下）
	if (fadeDuration <= 0.0f)
	{
		if (scene_)
		{
			scene_->Finalize();
			scene_.reset();
		}
		scene_ = sceneFactory_->CreateScene(sceneName);
		if (scene_)
		{
			currentSceneName_ = sceneName;
			scene_->SetSceneManager(this);
			scene_->Initialize();
			UITextManager::GetInstance()->SetCurrentScene(currentSceneName_);
		}
		transitionState_ = TransitionState::None;
		transitionProgress_ = 0.0f;
		return;
	}

	// イン（トランジション）開始
	nextSceneName_ = sceneName;
	fadeDuration_ = fadeDuration;
	transitionType_ = type;
	if (transitionType_ == TransitionType::Hexagon)
	{
		fadeColor_ = hexFadeColor_;
		edgeColor_ = hexGlowColor_;
	}
	else if (transitionType_ == TransitionType::CRT_Off)
	{
		fadeColor_ = { 0.0f, 0.0f, 0.0f, 1.0f };
		edgeColor_ = { 0.35f, 0.90f, 1.0f, 1.0f };
	}
	else
	{
		fadeColor_ = fadeColor;
	}
	fadeTimer_ = 0.0f;
	transitionProgress_ = 0.0f;
	holdFrames_ = 0;
	transitionState_ = TransitionState::FadeOut;

	InitTransition();
}

void SceneManager::DrawSceneSelectorUI()
{
#ifdef USE_IMGUI
	ImGui::Text("現在のシーン: ");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "%s", currentSceneName_.c_str());

	static bool immediateChange = false;
	ImGui::Checkbox("即時切り替え (トランジション省略)", &immediateChange);

	float dur = immediateChange ? 0.0f : fadeDuration_;

	const char* scenes[] = { "TITLE", "GAMEPLAY", "GAMEOVER", "GAMECLEAR" };
	const char* sceneLabels[] = { "タイトル (TITLE)", "ゲームプレイ (GAMEPLAY)", "ゲームオーバー (GAMEOVER)", "ゲームクリア (GAMECLEAR)" };

	for (int i = 0; i < 4; ++i)
	{
		bool isCurrent = (currentSceneName_ == scenes[i]);
		if (isCurrent)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.45f, 0.25f, 0.9f));
		}

		if (ImGui::Button(sceneLabels[i], ImVec2(160, 32)))
		{
			ChangeScene(scenes[i], dur, fadeColor_);
		}

		if (isCurrent)
		{
			ImGui::PopStyleColor();
		}

		if (i % 2 == 0)
		{
			ImGui::SameLine();
		}
	}
#endif
}

void SceneManager::DrawTransitionSettingsUI()
{
#ifdef USE_IMGUI
	ImGui::TextColored(ImVec4(0.2f, 0.9f, 1.0f, 1.0f), "■ 画面遷移演出（トランジション）設定");
	
	// トランジション方式の切り替え
	int currentType = static_cast<int>(transitionType_);
	ImGui::Text("方式切り替え:");
	if (ImGui::RadioButton("ヘキサゴン (Hexagon)", &currentType, 1))
	{
		transitionType_ = TransitionType::Hexagon;
		fadeDuration_ = 0.9f;
		fadeColor_ = hexFadeColor_;
		edgeColor_ = hexGlowColor_;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("四角形グリッド (Square Grid)", &currentType, 0))
	{
		transitionType_ = TransitionType::SquareGrid;
		fadeDuration_ = 0.5f;
		fadeColor_ = squareFadeColor_;
		edgeColor_ = squareEdgeColor_;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("CRT 電源オフ (CRT TV Off)", &currentType, 2))
	{
		transitionType_ = TransitionType::CRT_Off;
		fadeDuration_ = 0.5f;
		fadeColor_ = { 0.0f, 0.0f, 0.0f, 1.0f };
		edgeColor_ = { 0.35f, 0.90f, 1.0f, 1.0f };
	}

	ImGui::Separator();
	ImGui::SliderFloat("遷移時間 (秒)", &fadeDuration_, 0.2f, 3.0f, "%.2f s");

	if (transitionType_ == TransitionType::Hexagon)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.5f, 1.0f), "【ヘキサゴン・トランジション パラメータ】");
		
		float origin[2] = { hexOrigin_.x, hexOrigin_.y };
		if (ImGui::SliderFloat2("波紋起点 (OriginCoord)", origin, 0.0f, 1.0f, "%.2f"))
		{
			hexOrigin_ = { origin[0], origin[1] };
		}
		ImGui::SliderFloat("六角形サイズ (HexTileScale: 小さいほど巨大)", &hexTileScale_, 4.0f, 40.0f, "%.1f");
		ImGui::SliderFloat("輪郭線太さ (BorderWidth)", &hexBorderWidth_, 0.02f, 0.25f, "%.3f");
		ImGui::SliderFloat("グリッチ明滅 (GlitchIntensity)", &hexGlitchIntensity_, 0.0f, 1.0f, "%.2f");

		float ec[4] = { edgeColor_.x, edgeColor_.y, edgeColor_.z, edgeColor_.w };
		if (ImGui::ColorEdit4("ネオン発光色 (GlowColorHex)", ec))
		{
			edgeColor_ = { ec[0], ec[1], ec[2], ec[3] };
			hexGlowColor_ = edgeColor_;
		}

		float fc[4] = { fadeColor_.x, fadeColor_.y, fadeColor_.z, fadeColor_.w };
		if (ImGui::ColorEdit4("背景ベース色 (FadeColor)", fc))
		{
			fadeColor_ = { fc[0], fc[1], fc[2], fc[3] };
			hexFadeColor_ = fadeColor_;
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.9f, 1.0f), "【四角形グリッド パラメータ】");
		ImGui::SliderFloat("境界ぼかし (Softness)", &edgeSoftness_, 0.001f, 0.3f, "%.3f");

		float fc[4] = { fadeColor_.x, fadeColor_.y, fadeColor_.z, fadeColor_.w };
		if (ImGui::ColorEdit4("トランジション色", fc))
		{
			fadeColor_ = { fc[0], fc[1], fc[2], fc[3] };
			squareFadeColor_ = fadeColor_;
		}

		float ec[4] = { edgeColor_.x, edgeColor_.y, edgeColor_.z, edgeColor_.w };
		if (ImGui::ColorEdit4("エッジ発光色", ec))
		{
			edgeColor_ = { ec[0], ec[1], ec[2], ec[3] };
			squareEdgeColor_ = edgeColor_;
		}
	}

	ImGui::Spacing();
	if (ImGui::Button("テスト再生 (現シーン再読込)", ImVec2(220, 32)))
	{
		ChangeScene(currentSceneName_, fadeDuration_, fadeColor_);
	}
#endif
}
