#include "KHFramework.h"
#include <combaseapi.h>
#include "KHEngine/Core/Graphics/D3DResourceLeakChecker.h"
#include "KHEngine/Core/Utility/Log/Logger.h"
#include "KHEngine/Core/Utility/Crash/CrashDump.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Graphics/3d/Particle/ParticleManager.h"
#include "KHEngine/UI/UITextManager.h"
#ifdef ENABLE_EDITOR
#include "KHEngine/Debug/Editor/EditorSystem.cpp"
#include "KHEngine/Debug/Editor/EffectStudio.cpp"
#include "KHEngine/Debug/Editor/EnemyStudio.cpp"
#endif
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Scene/SceneManager.h"
#include <chrono>

void KHFramework::Run()
{
	
	FrameworkInitialize();

	
	Initialize();

	
	auto prevTime = std::chrono::high_resolution_clock::now();

	while (!endRequest_)
	{
		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float>(currentTime - prevTime).count();
		prevTime = currentTime;

		
		if (deltaTime > 0.1f) deltaTime = 0.1f;

		EngineServices::GetInstance()->SetDeltaTime(deltaTime);

		FrameworkUpdate(deltaTime);
		Update();

		FrameworkDrawBegin();

		Draw();

		FrameworkDrawEnd();
	}

	
	Finalize();

	
	FrameworkFinalize();
}

void KHFramework::FrameworkInitialize()
{
	D3DResourceLeakChecker leakcheck;

	
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	
	Logger::Initialize();

	
	KHEngine::Core::Utility::Crash::CrashDump::Install();

	
	winApp_ = std::make_unique<WinApp>();
	winApp_->Initialize();

	
	dxCommon_ = std::make_unique<DirectXCommon>();
	dxCommon_->Initialize(winApp_.get());

	
	input_ = std::make_unique<Input>();
	input_->Initialize(winApp_.get());

	imguiManager_ = std::make_unique<ImGuiManager>();
	imguiManager_->Initialize(dxCommon_.get(), winApp_.get());

	// UIテキスト管理システムの初期化（設定JSONの読み込み）
	UITextManager::GetInstance()->Initialize();

#ifdef ENABLE_EDITOR
	EditorSystem::GetInstance()->Initialize(dxCommon_.get());
#endif

	
	InitializeEngineSubsystems();

#ifdef ENABLE_EDITOR
	// エンジンサブシステム（SrvManager等）の初期化完了後にエフェクトスタジオおよびエネミースタジオを初期化
	EffectStudio::GetInstance()->Initialize(dxCommon_.get(), srvManager_);
	EnemyStudio::GetInstance()->Initialize(dxCommon_.get(), srvManager_, object3dCommon_.get());
#endif
}

void KHFramework::FrameworkUpdate(float deltaTime)
{
	
	if (winApp_ && winApp_->ProcessMessage())
	{
		endRequest_ = true;
		return;
	}

	// ウィンドウサイズ変更を検知してスワップチェーンを自動リサイズ
	if (winApp_ && dxCommon_)
	{
		uint32_t clientW = winApp_->GetClientWidth();
		uint32_t clientH = winApp_->GetClientHeight();
		if (clientW > 0 && clientH > 0 &&
			(clientW != dxCommon_->GetBackBufferWidth() || clientH != dxCommon_->GetBackBufferHeight()))
		{
			dxCommon_->ResizeSwapChain(clientW, clientH);
		}
	}

#ifdef ENABLE_EDITOR
	// エフェクトスタジオ・エネミースタジオの更新
	EffectStudio::GetInstance()->Update(deltaTime);
	EnemyStudio::GetInstance()->Update(deltaTime);
#endif

	
	if (input_)
	{
		input_->Update();

		// F11キー または Alt+Enter でフルスクリーン切り替え
		bool isAlt = (input_->PushKey(DIK_LALT) || input_->PushKey(DIK_RALT));
		if (input_->TriggerKey(DIK_F11) || (isAlt && input_->TriggerKey(DIK_RETURN)))
		{
			if (winApp_)
			{
				winApp_->ToggleFullscreen();
			}
		}

#ifdef ENABLE_EDITOR
		// F1キーでエディタモード（ImGui表示）をトグル
		if (input_->TriggerKey(DIK_F1))
		{
			auto services = EngineServices::GetInstance();
			if (services)
			{
				bool newMode = !services->GetEditorMode();
				services->SetEditorMode(newMode);
				// 全画面モードに入ったときはゲームを再生状態にする
				if (!newMode)
				{
					services->SetGamePlaying(true);
				}
			}
		}
#endif
	}

	// サウンドマネージャーの更新（再生終了したSEボイスの回収）
	SoundManager::GetInstance()->Update();

#ifdef USE_IMGUI
	if (imguiManager_)
	{
		imguiManager_->Begin();

		// スワップチェーンの解像度を取得
		uint32_t bbWidth = dxCommon_ ? dxCommon_->GetBackBufferWidth() : 1280;
		uint32_t bbHeight = dxCommon_ ? dxCommon_->GetBackBufferHeight() : 720;

#ifdef ENABLE_EDITOR
		if (EngineServices::GetInstance()->GetEditorMode())
		{
			EditorSystem::GetInstance()->Draw(postProcess_->GetResultSrvIndex());
		}
		else
#endif
		{
			// フルスクリーンプレイ時: ForegroundDrawListでUIテキストを描画（画面全体にフィット）
			UITextManager::GetInstance()->Draw(ImGui::GetForegroundDrawList(), ImVec2(0.0f, 0.0f), ImVec2(static_cast<float>(bbWidth), static_cast<float>(bbHeight)));
		}

		// デモプレイモード / 通常プレイモードのガイド表示（解除時も含め全シーンで常時表示）
		auto drawList = ImGui::GetForegroundDrawList();
		auto services = EngineServices::GetInstance();
		if (drawList && services)
		{
			bool isEditor = false;
#ifdef ENABLE_EDITOR
			isEditor = services->GetEditorMode();
#endif
			// エディタ表示中はメニューバー（上部約24px）の下にオフセット
			float badgeX = 16.0f;
			float badgeY = isEditor ? 32.0f : 16.0f;

			bool isDemo = services->IsDemoMode();
			const char* badgeText = isDemo ? "DEMO PLAY MODE | Press [1] to Play" : "PLAY MODE (MANUAL) | Press [1] for Demo";
			ImVec2 textSize = ImGui::CalcTextSize(badgeText);
			float badgeWidth = textSize.x + 20.0f;
			float badgeHeight = textSize.y + 12.0f;

			if (isDemo)
			{
				// デモプレイ中：サイバーブルーのスタイリッシュなバッジ
				drawList->AddRectFilled(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeWidth, badgeY + badgeHeight), IM_COL32(8, 16, 26, 220), 6.0f);
				drawList->AddRect(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeWidth, badgeY + badgeHeight), IM_COL32(0, 220, 255, 220), 6.0f, 0, 1.5f);
				drawList->AddText(ImVec2(badgeX + 10.0f, badgeY + 6.0f), IM_COL32(0, 240, 255, 255), badgeText);
			}
			else
			{
				// 通常プレイ時（解除中）：全シーンでエメラルドグリーンのバッジを表示
				drawList->AddRectFilled(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeWidth, badgeY + badgeHeight), IM_COL32(8, 22, 14, 200), 6.0f);
				drawList->AddRect(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeWidth, badgeY + badgeHeight), IM_COL32(60, 220, 130, 200), 6.0f, 0, 1.2f);
				drawList->AddText(ImVec2(badgeX + 10.0f, badgeY + 6.0f), IM_COL32(80, 255, 150, 240), badgeText);
			}
		}
	}
#endif
}

void KHFramework::FrameworkDrawBegin()
{
	if (dxCommon_)
	{
		dxCommon_->PreDraw();
	}
}

void KHFramework::FrameworkDrawEnd()
{
#ifdef ENABLE_EDITOR
	// エディターモード時、エフェクトスタジオとエネミースタジオのオフスクリーンレンダリングを実行
	if (EngineServices::GetInstance()->GetEditorMode())
	{
		EffectStudio::GetInstance()->Render();
		EnemyStudio::GetInstance()->Render();
	}
#endif

	if (dxCommon_)
	{
		dxCommon_->PreDrawSwapchain();
	}

	
	if (postProcess_)
	{
		
		postProcess_->Draw(!EngineServices::GetInstance()->GetEditorMode());
	}

	
	DrawUI();
	if (postProcess_)
	{
		postProcess_->PostDraw(!EngineServices::GetInstance()->GetEditorMode());
	}

	
	if (imguiManager_)
	{
		imguiManager_->End();
		// エディターモードON、またはエディターモードOFF時のUIテキスト描画を反映
		imguiManager_->Draw();
	}

	if (dxCommon_)
	{
		dxCommon_->PostDraw();
	}
}

void KHFramework::FrameworkFinalize()
{
	
	FinalizeEngineSubsystems();

	
	if (imguiManager_)
	{
		imguiManager_->Finalize();
		imguiManager_.reset();
	}

	
	if (input_)
	{
		input_.reset();
	}

	
	if (dxCommon_)
	{
		dxCommon_.reset();
	}

	
	if (winApp_)
	{
		winApp_->Finalize();
		winApp_.reset();
	}

	
	KHEngine::Core::Utility::Crash::CrashDump::Uninstall();

	
	Logger::Shutdown();

	
	CoUninitialize();
}

/* ------- 繝倥Ν繝代・螳溯｣・------- */

void KHFramework::InitializeEngineSubsystems()
{
	
	srvManager_ = SrvManager::GetInstance();
	if (srvManager_ && dxCommon_)
	{
		srvManager_->Initialize(dxCommon_.get());
		
		dxCommon_->RegisterSrvManager(srvManager_);
	}

	
	TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_);

	
	if (dxCommon_)
	{
		dxCommon_->BeginTextureUploadBatch();
	}

	
	ModelManager::GetInstance()->Initialize(dxCommon_.get());

	
	spriteCommon_ = std::make_unique<SpriteCommon>();
	spriteCommon_->Initialize(dxCommon_.get());

	object3dCommon_ = std::make_unique<Object3dCommon>();
	object3dCommon_->Initialize(dxCommon_.get());

	
	// トランジション用ルール画像のロード
	TextureManager::GetInstance()->LoadTexture("resources/textures/rules/rule_horizontal.png");

	postProcess_ = std::make_unique<PostProcess>();
	postProcess_->Initialize(dxCommon_.get());
	EngineServices::GetInstance()->SetPostProcess(postProcess_.get());

	// パーティクル基本プリミティブ（Quad, Ring, Cylinder）の登録
	auto particleMgr = ParticleManager::GetInstance();
	particleMgr->RegisterQuad("quad", "circle2.png");
	particleMgr->RegisterRing("ring", "resources/sprites/effect/gradationLine.png", 32, 0.5f, 1.0f);
	particleMgr->RegisterCylinder("Cylinder", "resources/sprites/effect/gradationLine.png");

	
	TextureManager::GetInstance()->ExecuteUploadCommands();
	TextureManager::GetInstance()->ClearIntermediateResources();

	
	SoundManager::GetInstance()->Initialize();
}

void KHFramework::FinalizeEngineSubsystems()
{
	
	ModelManager::GetInstance()->Finalize();

	
	TextureManager::GetInstance()->Finalize();

	
	if (spriteCommon_)
	{
		spriteCommon_.reset();
	}

	if (object3dCommon_)
	{
		object3dCommon_.reset();
	}

	if (postProcess_)
	{
		postProcess_.reset();
	}

	
	if (srvManager_)
	{
		srvManager_->Finalize();
		srvManager_ = nullptr;
	}

	
	SoundManager::GetInstance()->Finalize();
}

void KHFramework::BeginFrameCommon()
{
	
}

void KHFramework::EndFrameCommon()
{
	
}