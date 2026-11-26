#define NOMINMAX
#include "GamePlayScene.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Core/Utility/Log/Logger.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "KHEngine/Graphics/3d/Particle/ParticleManager.h"
#include "KHEngine/Graphics/Billboard/Billboard.h"
#include "KHEngine/Debug/Imgui/ImGuiManager.h"
#include "KHEngine/Graphics/3d/Particle/ParticleRenderer.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include <algorithm>
#include <random>
#include <memory>
#include <cmath>
#include <numbers>
#include "KHEngine/Scene/LevelLoader.h"
#include "KHEngine/Core/Resource/ResourceLocator.h"
#include "externals/imgui/imgui.h"
#ifdef ENABLE_EDITOR
#include "KHEngine/Debug/Editor/EffectStudio.h"
#include "KHEngine/Debug/Editor/EnemyStudio.h"
#include "KHEngine/Debug/Editor/EditorSystem.h"
#endif
#include "Game/Actor/Enemy/EnemyPresetManager.h"
#include <filesystem>
#include "KHEngine/Math/CollisionMath.h"
#include "KHEngine/Scene/SceneManager.h"
#include "KHEngine/UI/UITextManager.h"
#include <chrono>
#include <fstream>
#include "externals/nlohmann/json.hpp"
#include "Game/System/TerrainCollisionDebugger.h"

class GamePlayScene::StartCutscenePhaseState final : public GamePlayScene::PhaseState
{
public:
	void Update(GamePlayScene& scene, float dt, float& unscaledGameSpeed) override
	{
		scene.UpdateOpeningCutscene(dt);
		unscaledGameSpeed = scene.baseGameSpeed_;
		if (scene.railCameraController_) { scene.railCameraController_->SetSpeedMultiplier(1.0f); }
	}
	GamePhase GetPhase() const override { return GamePhase::START_CUTSCENE; }
};

class GamePlayScene::PlayingPhaseState final : public GamePlayScene::PhaseState
{
public:
	void Update(GamePlayScene& scene, float dt, float& unscaledGameSpeed) override
	{
		scene.openingSequence_.TickMissionStartText(dt);
		if (scene.bossFinisherSequence_ && scene.bossFinisherSequence_->IsMoviePhase())
		{
			unscaledGameSpeed = 0.0f;
			if (scene.railCameraController_) { scene.railCameraController_->SetSpeedMultiplier(0.0f); }
		}
		else
		{
			scene.stageElapsedTime_ += dt;
		}
	}
	GamePhase GetPhase() const override { return GamePhase::PLAYING; }
};

class GamePlayScene::GameOverPhaseState final : public GamePlayScene::PhaseState
{
public:
	void Update(GamePlayScene& scene, float dt, float& unscaledGameSpeed) override
	{
		scene.UpdateGameOverSequence(dt);
		unscaledGameSpeed = 0.0f;
		if (scene.railCameraController_) { scene.railCameraController_->SetSpeedMultiplier(0.0f); }
	}
	GamePhase GetPhase() const override { return GamePhase::GAMEOVER; }
};

class GamePlayScene::ClearPhaseState final : public GamePlayScene::PhaseState
{
public:
	void Update(GamePlayScene& scene, float dt, float& unscaledGameSpeed) override
	{
		scene.UpdateClearSequence(dt);
		unscaledGameSpeed = 0.0f;
		if (scene.railCameraController_) { scene.railCameraController_->SetSpeedMultiplier(0.0f); }
	}
	GamePhase GetPhase() const override { return GamePhase::CLEAR; }
};

GamePlayScene::GamePlayScene()
{
	phaseState_ = std::make_unique<StartCutscenePhaseState>();
}

GamePlayScene::~GamePlayScene() = default;

void GamePlayScene::ChangePhaseState(std::unique_ptr<PhaseState> nextState)
{
	phaseState_ = std::move(nextState);
}

void GamePlayScene::SetGamePhase(GamePhase phase)
{
	switch (phase)
	{
	case GamePhase::START_CUTSCENE: ChangePhaseState(std::make_unique<StartCutscenePhaseState>()); break;
	case GamePhase::PLAYING:        ChangePhaseState(std::make_unique<PlayingPhaseState>()); break;
	case GamePhase::GAMEOVER:       ChangePhaseState(std::make_unique<GameOverPhaseState>()); break;
	case GamePhase::CLEAR:          ChangePhaseState(std::make_unique<ClearPhaseState>()); break;
	}
}

void GamePlayScene::Initialize()
{
	auto initStartTime = std::chrono::high_resolution_clock::now();
	auto services = EngineServices::GetInstance();
	auto object3dCommon = services->GetObject3dCommon();
	auto dxCommon = services->GetDirectXCommon();
	auto srvManager = services->GetSrvManager();
	auto spriteCommon = services->GetSpriteCommon();

	demoController_.ResetAll();
	collisionManager_ = std::make_unique<CollisionManager>();

	float skyboxMs = 0.0f;
	world_.InitializeCamerasAndSkybox(camera, debugCamera_, activeCamera_, skybox_, skyboxMs);

	levelManager_.PreloadResources();
	auto texManager = TextureManager::GetInstance();
	uint32_t whiteTex = texManager->GetTextureIndexByFilePath("sprites/white.png");
	if (whiteTex == 0) { whiteTex = texManager->GetTextureIndexByFilePath("white.png"); }
	whiteTexIndex_ = whiteTex;
	ringIconOutlineTex_ = texManager->GetTextureIndexByFilePath("sprites/UI/ringGet_icon_outline.png");
	ringIconTex_ = texManager->GetTextureIndexByFilePath("sprites/UI/ringGet_icon.png");

	hud_.Initialize(spriteCommon, whiteTexIndex_, ringIconTex_, ringIconOutlineTex_);
	bossBattle_.Initialize(object3dCommon, spriteCommon, whiteTexIndex_);

	auto tLevel0 = std::chrono::high_resolution_clock::now();
	ReloadLevel();
	auto tLevel1 = std::chrono::high_resolution_clock::now();
	float levelMs = std::chrono::duration<float, std::milli>(tLevel1 - tLevel0).count();

	world_.InitializeActors(cameraObject_, player_, railCameraController_, armoredTrainBoss_, isBossSpawned_, activeCamera_, skybox_.get(), mainRails_, modelInstances);
	vfx_.Initialize(dxCommon, srvManager);

	auto tUp0 = std::chrono::high_resolution_clock::now();
	texManager->ExecuteUploadCommands();
	texManager->ClearIntermediateResources();
	auto tUp1 = std::chrono::high_resolution_clock::now();
	float uploadMs = std::chrono::duration<float, std::milli>(tUp1 - tUp0).count();
	lastLoadTimeMs_ = std::chrono::duration<float, std::milli>(std::chrono::high_resolution_clock::now() - initStartTime).count();

	char logBuf[512];
	snprintf(logBuf, sizeof(logBuf),
		"\n========================================\n[Load Profiler Breakdown]\n  - Skybox Load:         %8.2f ms\n  - ReloadLevel (Map):   %8.2f ms\n  - GPU Texture Upload:  %8.2f ms\n  - TOTAL Initialize:    %8.2f ms (%.3f s)\n========================================\n\n",
		skyboxMs, levelMs, uploadMs, lastLoadTimeMs_, lastLoadTimeMs_ / 1000.0f);
	OutputDebugStringA(logBuf);

	gameOverSequence_.Reset(false);
	demoController_.ResetCycle();
	if (player_) { player_->SetVisible(true); }

	StartOpeningCutscene();
	SoundManager::GetInstance()->PlayBGM("gameplayBGM.mp3", 0.25f, true);
	hud_.RegisterDefaultTexts();

	bossFinisherSequence_ = std::make_unique<BossFinisherSequence>();
	if (bossFinisherSequence_)
	{
		uint32_t skyTex = skybox_ ? skybox_->GetCubemapSrvIndex() : 0;
		bossFinisherSequence_->Initialize(object3dCommon, camera.get(), railCameraController_.get(), skyTex);
		bossFinisherSequence_->SetTerrainObjects(&modelInstances);
		bossFinisherSequence_->SetOnVoxelHit([this](const Vector3& pos, int count, float minSz, float maxSz) {
			vfx_.SpawnVoxelHitParticles(pos, activeCamera_, count, minSz, maxSz);
		});
		bossFinisherSequence_->Reset(player_.get());
	}
}

LevelLoadContext GamePlayScene::BuildLevelContext()
{
	LevelLoadContext ctx;
	ctx.modelInstances = &modelInstances;
	ctx.mainRails = &mainRails_;
	ctx.bossRail = &bossRail_;
	ctx.railCameraController = &railCameraController_;
	ctx.railModels = &railModels_;
	ctx.enemyRailModel = &enemyRailModel_;
	ctx.railVisualizers = &railVisualizers_;
	ctx.enemyRailVisualizers = &enemyRailVisualizers_;
	ctx.enemies = &enemies_;
	ctx.obstacles = &obstacles_;
	ctx.enhanceRings = &enhanceRings_;
	ctx.memoryItems = &memoryItems_;
	ctx.armoredTrainBoss = &armoredTrainBoss_;
	ctx.isBossSpawned = &isBossSpawned_;
	ctx.hasEnemySpawned = &hasEnemySpawned_;
	ctx.totalEnemiesCount = &totalEnemiesCount_;
	ctx.killedEnemiesCount = &killedEnemiesCount_;
	ctx.skybox = skybox_.get();
	ctx.activeCamera = activeCamera_;
	ctx.cameraObject = cameraObject_.get();
	ctx.player = player_.get();
	ctx.bossBattle = &bossBattle_;
	ctx.isVirtualSpace = isVirtualSpace_;
	return ctx;
}

void GamePlayScene::ReloadLevel()
{
	bullets_.clear();
	missiles_.clear();
	hud_.SetAcquiredEnhanceRingCount(0);
	stageElapsedTime_ = 0.0f;
	bossElapsedTime_ = 0.0f;
	levelManager_.ReloadLevel(BuildLevelContext());
	StartOpeningCutscene();
}

void GamePlayScene::ApplyVirtualSpaceStyle()
{
	levelManager_.ApplyVirtualSpaceStyle(modelInstances, obstacles_, skybox_.get(), isVirtualSpace_);
}

void GamePlayScene::ReloadEnemiesOnly()
{
	levelManager_.ReloadEnemiesOnly(BuildLevelContext());
}

void GamePlayScene::SeamlessResetForRetry()
{
	bullets_.clear();
	missiles_.clear();
	hud_.ClearScorePopups();
	openingSequence_.ClearShards();
	vfx_.ClearAll();
	gameOverSequence_.Reset(true);

	enemies_.clear();
	obstacles_.clear();
	enhanceRings_.clear();
	memoryItems_.clear();
	hud_.SetAcquiredEnhanceRingCount(0);

	levelManager_.RespawnActorsFromCache(BuildLevelContext());

	bossBattle_.Reset();
	world_.ResetActorsForRetry(armoredTrainBoss_.get(), isBossSpawned_, railCameraController_.get(), bossRail_.get(), mainRails_, player_.get());

	score_ = 0;
	gameOverSequence_.Reset(false);
	demoController_.ResetCycle();

	StartOpeningCutscene();
	SoundManager::GetInstance()->PlayBGM("gameplayBGM.mp3", 0.25f, true);
}

void GamePlayScene::Finalize()
{
	// BGMおよび効果音の停止
	SoundManager::GetInstance()->StopBGM();
	SoundManager::GetInstance()->StopAllSE();

	if (bossFinisherSequence_)
	{
		bossFinisherSequence_->Reset(player_.get());
	}

	ClearSprites();
	modelInstances.clear();

	vfx_.ResetPostProcess(EngineServices::GetInstance()->GetPostProcess());
}

void GamePlayScene::Update()
{
	auto services = EngineServices::GetInstance();
	auto input = services->GetInput();
	float dt = services->GetDeltaTime();

	HandleGlobalHotkeys(services, input);
	UpdateDemoPlay(services, dt);

#ifdef ENABLE_EDITOR
	// EngineServicesのゲーム再生状態と同期
	isPlaying_ = services->IsGamePlaying();
#endif

	if (isPlaying_)
	{
		activeCamera_ = camera.get();
	}
	else
	{
		activeCamera_ = debugCamera_.get();
	}
	if (auto objCommon = services->GetObject3dCommon())
	{
		objCommon->SetDefaultCamera(activeCamera_);
	}

	float unscaledGameSpeed = baseGameSpeed_;





	if (input && activeCamera_)
	{

		const float kRotateSpeed = 0.005f;
		const float kMoveSpeed = 8.0f;
		const float kZoomSpeed = 0.0015f;

		LONG dx = input->GetMouseMoveX();
		LONG dy = input->GetMouseMoveY();
		LONG wheel = input->GetMouseWheel();


		if (!isPlaying_)
		{
			if (input->PushMouseButton(1))
			{
				Vector3 rot = activeCamera_->GetRotation();

				rot.y += static_cast<float>(dx) * kRotateSpeed;
				rot.x += static_cast<float>(dy) * kRotateSpeed;


				const float kMaxPitch = 1.5f;
				const float kMinPitch = -1.5f;
				rot.x = std::clamp(rot.x, kMinPitch, kMaxPitch);

				activeCamera_->SetRotation(rot);
			}


			float currentMoveSpeed = kMoveSpeed;
			if (input->PushKey(DIK_LSHIFT) || input->PushKey(DIK_RSHIFT))
			{
				currentMoveSpeed *= 25.0f;
			}
			float moveStep = currentMoveSpeed * dt;
			Vector3 pos = activeCamera_->GetTranslate();
			Vector3 rot = activeCamera_->GetRotation();
			float yaw = rot.y;


			Vector3 forward = { std::sinf(yaw), 0.0f, std::cosf(yaw) };
			Vector3 right = { std::cosf(yaw), 0.0f, -std::sinf(yaw) };

			auto normalize = [](Vector3 v)
				{
					float len = std::sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
					if (len > 1e-6f) { v.x /= len; v.y /= len; v.z /= len; }
					return v;
				};

			forward = normalize(forward);
			right = normalize(right);

			if (input->PushKey(DIK_W))
			{
				pos.x += forward.x * moveStep;
				pos.z += forward.z * moveStep;
			}
			if (input->PushKey(DIK_S))
			{
				pos.x -= forward.x * moveStep;
				pos.z -= forward.z * moveStep;
			}
			if (input->PushKey(DIK_D))
			{
				pos.x += right.x * moveStep;
				pos.z += right.z * moveStep;
			}
			if (input->PushKey(DIK_A))
			{
				pos.x -= right.x * moveStep;
				pos.z -= right.z * moveStep;
			}
			if (input->PushKey(DIK_E))
			{
				pos.y += moveStep;
			}
			if (input->PushKey(DIK_Q))
			{
				pos.y -= moveStep;
			}

			activeCamera_->SetTranslate(pos);



		}

		// ポーズメニューの開閉判定（プレイ中、かつゲームオーバーやクリア中でない場合）
		if (isPlaying_ && GetGamePhase() != GamePhase::GAMEOVER && GetGamePhase() != GamePhase::CLEAR)
		{
			if (input)
			{
				if (input->TriggerKey(DIK_ESCAPE) || input->TriggerKey(DIK_P) || input->TriggerPadButton(XINPUT_GAMEPAD_START))
				{
					pauseMenu_.Toggle();
				}
			}
		}

	float unscaledGameSpeed = baseGameSpeed_;
		if (pauseMenu_.IsPaused())
		{
			pauseMenu_.Update(GetSceneManager());
		}
		else if (isPlaying_)
		{
			unscaledGameSpeed = baseGameSpeed_;
			UpdateRailAndCamera(services, dt, unscaledGameSpeed);
		}

	UpdateActorsAndCollisions(dt, unscaledGameSpeed);
	UpdateCameraShakeAndVfx(dt);
	}

	if (player_)
	{
		hud_.UpdateBars(dt, player_.get());
		if (isPlaying_)
		{
			CheckGameEndConditions();
		}
	}

	hud_.UpdateHudTexts(player_.get(), bossFinisherSequence_.get(), score_, GetGamePhase() == GamePhase::START_CUTSCENE, GetGamePhase() == GamePhase::GAMEOVER, GetGamePhase() == GamePhase::CLEAR, openingSequence_.GetTimer());

#ifdef ENABLE_EDITOR
	editor_.Draw(*this);
	TerrainCollisionDebugger::GetInstance()->DrawImGui();
#endif // ENABLE_EDITOR
}

void GamePlayScene::HandleGlobalHotkeys(EngineServices* services, Input* input)
{
	if (input && input->TriggerKey(DIK_0))
	{
		services->SetEditorMode(true);
	}


	if (input && input->TriggerKey(DIK_F2))
	{
		isDrawCollider_ = !isDrawCollider_;
	}

	if (input && input->TriggerKey(DIK_F3))
	{
		isDrawTerrainWireframe_ = !isDrawTerrainWireframe_;
	}

	if (input && input->TriggerKey(DIK_F5))
	{
		ReloadLevel();
	}

	if (input && input->TriggerKey(DIK_F6))
	{
		TerrainCollisionDebugger::GetInstance()->ToggleImGuiMonitor();
	}

	if (input && input->TriggerKey(DIK_M))
	{
		SoundManager::GetInstance()->ToggleMasterMute();
	}

}

void GamePlayScene::UpdateDemoPlay(EngineServices* services, float dt)
{
	demoController_.Update(services, dt, GetGamePhase() == GamePhase::PLAYING, player_.get(), bossFinisherSequence_.get(), &bossBattle_, armoredTrainBoss_.get(), bullets_, [this]() { StartBossWarningSequence(); });
}

void GamePlayScene::UpdateRailAndCamera(EngineServices* services, float dt, float& unscaledGameSpeed)
{
	auto pp = services ? services->GetPostProcess() : nullptr;
	bool isBoosting = (player_ && player_->IsBoosting());
	unscaledGameSpeed = isBoosting ? (baseGameSpeed_ * 1.5f) : baseGameSpeed_;
	if (railCameraController_)
	{
		railCameraController_->SetSpeedMultiplier(isBoosting ? 1.5f : 1.0f);
	}

	if (phaseState_)
	{
		phaseState_->Update(*this, dt, unscaledGameSpeed);
	}

	vfx_.UpdatePostProcessEffects(
		pp,
		isBoosting,
		isJustDodgeActive_,
		justDodgeTimer_,
		justDodgeMaxTime_,
		player_ ? player_->GetDamagePulseRatio() : 0.0f,
		player_ ? player_->GetHpRatio() : 1.0f,
		dt);

	gameSpeed_ = unscaledGameSpeed;
	if (isJustDodgeActive_)
	{
		gameSpeed_ *= justDodgeSlowSpeed_;
	}

	world_.UpdateCameraFovAndRail(BuildWorldContext(), clearSequence_.GetStartPlayerPos());

	if (GetGamePhase() == GamePhase::PLAYING && bossBattle_.GetStep() == BossBattleController::Step::NOT_ACTIVE && railCameraController_)
	{
		if (railCameraController_->GetProgress() >= bossBattle_.GetSpawnProgressThreshold())
		{
			StartBossWarningSequence();
		}
	}

	UpdateBossBattle(dt);
}

GamePlayWorldContext GamePlayScene::BuildWorldContext()
{
	GamePlayWorldContext ctx;
	ctx.activeCamera = activeCamera_;
	ctx.cameraObject = cameraObject_.get();
	ctx.modelInstances = &modelInstances;
	ctx.mainRails = &mainRails_;
	ctx.railCameraController = railCameraController_.get();
	ctx.railVisualizers = &railVisualizers_;
	ctx.enemyRailVisualizers = &enemyRailVisualizers_;
	ctx.skybox = skybox_.get();
	ctx.vfx = &vfx_;
	ctx.player = player_.get();
	ctx.hud = &hud_;
	ctx.bullets = &bullets_;
	ctx.missiles = &missiles_;
	ctx.enemies = &enemies_;
	ctx.armoredTrainBoss = armoredTrainBoss_.get();
	ctx.bossFinisherSequence = bossFinisherSequence_.get();
	ctx.bossBattle = &bossBattle_;
	ctx.enemyBullets = &enemyBullets_;
	ctx.collisionManager = collisionManager_.get();
	ctx.obstacles = &obstacles_;
	ctx.enhanceRings = &enhanceRings_;
	ctx.memoryItems = &memoryItems_;
	ctx.openingSequence = &openingSequence_;
	ctx.pauseMenu = &pauseMenu_;
	ctx.gameOverSequence = &gameOverSequence_;
	ctx.gameSpeed = gameSpeed_;
	ctx.lockOnMaxDistance = lockOnMaxDistance_;
	ctx.isJustDodgeActive = &isJustDodgeActive_;
	ctx.justDodgeTimer = &justDodgeTimer_;
	ctx.justDodgeMaxTime = justDodgeMaxTime_;
	ctx.isBossSpawned = &isBossSpawned_;
	ctx.isDrawRail = isDrawRail_;
	ctx.isPlaying = isPlaying_;
	ctx.isDrawCollider = isDrawCollider_;
	ctx.isDrawTerrainWireframe = isDrawTerrainWireframe_;
	ctx.cameraShakeTimer = &cameraShakeTimer_;
	ctx.enemyKillShakeFrames = &enemyKillShakeFrames_;
	ctx.score = &score_;
	ctx.killedEnemiesCount = &killedEnemiesCount_;
	ctx.terrainMaterializeProgress = terrainMaterializeProgress_;
	ctx.isStartCutscenePhase = (GetGamePhase() == GamePhase::START_CUTSCENE);
	ctx.isPlayingPhase = (GetGamePhase() == GamePhase::PLAYING);
	ctx.isGameOverPhase = (GetGamePhase() == GamePhase::GAMEOVER);
	ctx.isClearPhase = (GetGamePhase() == GamePhase::CLEAR);
	return ctx;
}

void GamePlayScene::UpdateActorsAndCollisions(float dt, float unscaledGameSpeed)
{
	world_.UpdateActorsAndCollisions(dt, unscaledGameSpeed, BuildWorldContext());
}

void GamePlayScene::UpdateCameraShakeAndVfx(float dt)
{
	if (!pauseMenu_.IsPaused())
	{
		hud_.UpdateScorePopups((1.0f / 60.0f) * gameSpeed_);
	}
	world_.UpdateCameraAndEnvironment(BuildWorldContext());
	vfx_.UpdatePlayerVfx(dt, player_.get(), activeCamera_, isPlaying_, pauseMenu_.IsPaused(), GetGamePhase() == GamePhase::START_CUTSCENE, GetGamePhase() == GamePhase::PLAYING, GetGamePhase() == GamePhase::GAMEOVER, GetGamePhase() == GamePhase::CLEAR, openingSequence_.GetTimer(), openingSequence_.IsThrusterReady(), gameSpeed_, randomEngine);
}

void GamePlayScene::CheckGameEndConditions()
{
	if (!enemies_.empty())
	{
		hasEnemySpawned_ = true;
	}

	if (player_->GetHp() <= 0)
	{
		if (GetGamePhase() == GamePhase::PLAYING)
		{
			StartGameOverSequence();
		}
	}

	// 全通常敵撃破後のクリア判定（ボス戦が控えている・戦闘中の場合はボス撃破までクリアにしない）
	if (hasEnemySpawned_ && enemies_.empty() && GetGamePhase() == GamePhase::PLAYING)
	{
		if (bossBattle_.GetStep() == BossBattleController::Step::FINISHED || (bossBattle_.GetStep() == BossBattleController::Step::NOT_ACTIVE && !armoredTrainBoss_))
		{
			StartClearSequence();
		}
	}
}

void GamePlayScene::Draw()
{
	world_.Draw(BuildWorldContext());
}

void GamePlayScene::DrawUI()
{
	auto services = EngineServices::GetInstance();
	auto spriteCommon = services->GetSpriteCommon();
	if (spriteCommon) spriteCommon->SetCommonDrawSetting();
	auto srvManager = services->GetSrvManager();
	if (srvManager) srvManager->PreDraw();

	// 通常プレイ時のみHPバー、ブーストバー、強化リングアイコンを描画（スタート演出中・ゲームオーバー・クリア時・処刑ムービー中は非表示）
	bool isFinisherMovie = (bossFinisherSequence_ && bossFinisherSequence_->IsMoviePhase());
	if (GetGamePhase() == GamePhase::PLAYING && !isFinisherMovie)
	{
		hud_.DrawSprites();
		if (IsDisplaySprite()) { for (auto& sprite : GetSprites()) if (sprite) { sprite->Update(); sprite->Draw(); } }

		// ボス専用HPバースプライト（戦闘中・撃破演出中）
		bossBattle_.DrawHpSprites(armoredTrainBoss_ ? armoredTrainBoss_->GetTotalHpRate() : 0.0f);
	}

	// 通常プレイ中のレティクル・ロックオンマーカー・スコアポップアップ描画
	hud_.DrawOverlayUI(GetGamePhase() == GamePhase::PLAYING, (bossFinisherSequence_ && bossFinisherSequence_->IsMoviePhase()), player_.get(), activeCamera_);

	// スタート演出用UIテキストの描画
	openingSequence_.DrawUI(GetGamePhase() == GamePhase::START_CUTSCENE, GetGamePhase() == GamePhase::PLAYING);

	// ボス戦用UI（WARNING警告・詳細ボスHPバー・各車両インジケーター）の描画
	bossBattle_.DrawUI(GetGamePhase() == GamePhase::START_CUTSCENE, GetGamePhase() == GamePhase::GAMEOVER, GetGamePhase() == GamePhase::CLEAR, BuildBossBattleContext());

	// ゲームオーバー演出用UIテキストの描画
	gameOverSequence_.DrawUI(GetGamePhase() == GamePhase::GAMEOVER, GetSceneManager());

	// ゲームクリア演出用UIテキストの描画
	clearSequence_.DrawUI(GetGamePhase() == GamePhase::CLEAR, BuildClearContext());

	// ポーズメニューUIの描画
	pauseMenu_.DrawUI();

	// 電脳フィニッシャー演出用UIの描画
	if (bossFinisherSequence_)
	{
		bossFinisherSequence_->DrawUI();
	}
}

OpeningCutsceneContext GamePlayScene::BuildOpeningContext()
{
	OpeningCutsceneContext ctx;
	ctx.player = player_.get();
	ctx.activeCamera = activeCamera_;
	ctx.railCameraController = railCameraController_.get();
	ctx.modelInstances = &modelInstances;
	ctx.obstacles = &obstacles_;
	ctx.thrusterEffect = vfx_.GetThrusterEffect();
	ctx.windEffect = vfx_.GetWindEffect();
	ctx.bossFinisherSequence = bossFinisherSequence_.get();
	ctx.terrainMaterializeProgress = &terrainMaterializeProgress_;
	ctx.cameraShakeTimer = &cameraShakeTimer_;
	return ctx;
}

void GamePlayScene::StartOpeningCutscene()
{
	ChangePhaseState(std::make_unique<StartCutscenePhaseState>());
	openingSequence_.Start(BuildOpeningContext());
}

void GamePlayScene::UpdateOpeningCutscene(float dt)
{
	if (openingSequence_.Update(dt, BuildOpeningContext()))
	{
		ChangePhaseState(std::make_unique<PlayingPhaseState>());
	}
}

GameOverContext GamePlayScene::BuildGameOverContext()
{
	GameOverContext ctx;
	ctx.player = player_.get();
	ctx.activeCamera = activeCamera_;
	ctx.railCameraController = railCameraController_.get();
	ctx.sceneManager = GetSceneManager();
	ctx.modelInstances = &modelInstances;
	ctx.obstacles = &obstacles_;
	ctx.enemies = &enemies_;
	ctx.armoredTrainBoss = armoredTrainBoss_.get();
	ctx.bullets = &bullets_;
	ctx.missiles = &missiles_;
	ctx.enemyBullets = &enemyBullets_;
	ctx.enhanceRings = &enhanceRings_;
	ctx.thrusterEffect = vfx_.GetThrusterEffect();
	ctx.windEffect = vfx_.GetWindEffect();
	ctx.terrainMaterializeProgress = &terrainMaterializeProgress_;
	ctx.cameraShakeTimer = &cameraShakeTimer_;
	ctx.demoController = &demoController_;
	ctx.onRetry = [this]() { SeamlessResetForRetry(); };
	return ctx;
}

void GamePlayScene::StartGameOverSequence()
{
	ChangePhaseState(std::make_unique<GameOverPhaseState>());
	gameOverSequence_.Start(BuildGameOverContext());
}

void GamePlayScene::UpdateGameOverSequence(float dt)
{
	gameOverSequence_.Update(dt, BuildGameOverContext());
}

GameClearContext GamePlayScene::BuildClearContext()
{
	GameClearContext ctx;
	ctx.player = player_.get();
	ctx.railCameraController = railCameraController_.get();
	ctx.bossFinisherSequence = bossFinisherSequence_.get();
	ctx.gameOverSequence = &gameOverSequence_;
	ctx.enemyBullets = &enemyBullets_;
	ctx.sceneManager = GetSceneManager();
	ctx.cameraShakeTimer = &cameraShakeTimer_;
	ctx.totalEnemiesCount = totalEnemiesCount_;
	ctx.killedEnemiesCount = killedEnemiesCount_;
	ctx.stageElapsedTime = stageElapsedTime_;
	ctx.bossElapsedTime = bossElapsedTime_;
	return ctx;
}

void GamePlayScene::StartClearSequence()
{
	if (GetGamePhase() != GamePhase::PLAYING) return;
	ChangePhaseState(std::make_unique<ClearPhaseState>());
	clearSequence_.Start(BuildClearContext());
}

void GamePlayScene::UpdateClearSequence(float dt)
{
	clearSequence_.Update(dt, BuildClearContext());
}

BossBattleContext GamePlayScene::BuildBossBattleContext()
{
	BossBattleContext ctx;
	ctx.mainRails = &mainRails_;
	ctx.bossRail = bossRail_.get();
	ctx.railCameraController = railCameraController_.get();
	ctx.gameSpeed = &gameSpeed_;
	ctx.explosionEffect = vfx_.GetExplosionEffect();
	ctx.player = player_.get();
	ctx.bullets = &bullets_;
	ctx.missiles = &missiles_;
	ctx.armoredTrainBoss = armoredTrainBoss_.get();
	ctx.isBossSpawned = &isBossSpawned_;
	ctx.bossFinisherSequence = bossFinisherSequence_.get();
	ctx.cameraShakeTimer = &cameraShakeTimer_;
	ctx.bossElapsedTime = &bossElapsedTime_;
	ctx.onGameOver = [this]() { if (GetGamePhase() == GamePhase::PLAYING) StartGameOverSequence(); };
	ctx.onClear = [this]() { StartClearSequence(); };
	return ctx;
}

void GamePlayScene::TriggerTimeOverCannonSequence(bool nonLethal)
{
	ChangePhaseState(std::make_unique<PlayingPhaseState>());
	bossBattle_.TriggerTimeOverCannonSequence(nonLethal, BuildBossBattleContext());
}

void GamePlayScene::StartBossWarningSequence()
{
	bossBattle_.StartWarningSequence(BuildBossBattleContext());
}

void GamePlayScene::UpdateBossBattle(float dt)
{
	bossBattle_.Update(dt, BuildBossBattleContext());
}
