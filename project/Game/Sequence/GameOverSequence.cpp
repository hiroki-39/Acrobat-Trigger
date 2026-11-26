#define NOMINMAX
#include "GameOverSequence.h"
#include "Game/System/DemoPlayController.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Obstacle/Obstacle.h"
#include "Game/Actor/Enemy/Enemy.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "Game/Actor/Bullet/PlayerBullet.h"
#include "Game/Actor/Bullet/PlayerMissile.h"
#include "Game/Actor/Bullet/EnemyBullet.h"
#include "Game/Actor/Item/EnhanceRing.h"
#include "Game/System/RailCameraController.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"
#include "KHEngine/Graphics/3d/Object/Object3dCommon.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/3d/Particle/ParticleEffect.h"
#include "KHEngine/Graphics/PostProcess/PostProcess.h"
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
#include <numbers>
#include <random>
void GameOverSequence::Start(const GameOverContext& ctx)
{
		gameOverStep_ = Step::HIT_STOP;
	gameOverSelectedOption_ = MenuOption::Retry;
	gameOverTimer_ = 0.0f;
	if (ctx.demoController)
	{
		ctx.demoController->ResetCycle();
	}
	shockwaveScale_ = 0.2f;
	shockwaveAlpha_ = 1.0f;
	gameOverShards_.clear();

	if (auto modelMgr = ModelManager::GetInstance())
	{
		modelMgr->LoadModel("Ring.obj");
		modelMgr->LoadModel("polygon_shard.obj");
	}

	if (ctx.player)
	{
		gameOverPlayerStartPos_ = ctx.player->GetTranslate();
		gameOverPlayerStartRot_ = ctx.player->GetRotation();
		ctx.player->SetCutsceneOverride(true, gameOverPlayerStartPos_, gameOverPlayerStartRot_);
		ctx.player->Update3DObjectOnly();
		ctx.player->SetDead(true);
	}

	// Keep background terrain visible
	for (auto& model : (*ctx.modelInstances))
	{
		if (model) model->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	}
	for (auto& obs : (*ctx.obstacles))
	{
		if (obs && obs->GetObject3d()) obs->GetObject3d()->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	}

	auto services = EngineServices::GetInstance();
	// スラスターおよびスピードラインパーティクルを即座に完全消去
	(*ctx.thrusterEffect).ClearParticles();
	(*ctx.windEffect).ClearParticles();

	if (ctx.railCameraController)
	{
		ctx.railCameraController->SetSpeedMultiplier(0.0f);
		retryStartProgress_ = ctx.railCameraController->GetProgress();
		retryStartRailIndex_ = ctx.railCameraController->GetCurrentRailIndex();
	}

	// HPが0になった直後〜四散完了までは画面ノイズ・グリッチを完全停止
	if (auto pp = services ? services->GetPostProcess() : nullptr)
	{
		pp->SetEffectActive("Random(Glitch)", false);
	}

	// 被弾シェイクを穏やかに（爆発感を抑える）
	(*ctx.cameraShakeTimer) = 5.0f;
}


void GameOverSequence::Update(float dt, const GameOverContext& ctx)
{
	// =========================================================================
	// 【リトライ時シームレス逆マテリアライズ演出】
	// 地形が奥から手前へデマテリアライズして消滅し、そのままオープニングへ突入！
	// =========================================================================
	if (gameOverStep_ == Step::RETRY_DEMATERIALIZE)
	{
		auto services = EngineServices::GetInstance();
		(*ctx.thrusterEffect).ClearParticles();
		(*ctx.windEffect).ClearParticles();

		retryDematerializeTimer_ += dt;
		float tTotal = std::clamp(retryDematerializeTimer_ / kRetryDematerializeDuration, 0.0f, 1.0f);

		// 【タイムライン設計: 合計 1.10秒】
		// 1. [0.00s 〜 0.90s]: 地形デマテリアライズ ＆ カメラEase-Out後退
		// 2. [0.90s 〜 1.10s]: 地形が完全に消滅した「空っぽの空間（ボイド）」の静寂の間（0.20秒間）
		const float kDematerializeDuration = 0.90f; // 地形消滅にかける時間
		const float kVoidDuration = 0.20f;          // 完全消滅後の静寂の間（0.20秒）

		if (retryDematerializeTimer_ < kDematerializeDuration)
		{
			// --- フェーズ1: 地形消滅 & カメラEase-Out着地 ---
			float tMat = retryDematerializeTimer_ / kDematerializeDuration;
			float uMat = 1.0f - (tMat * tMat * (3.0f - 2.0f * tMat)); // 1.0 -> 0.0 (SmoothStep)
			(*ctx.terrainMaterializeProgress) = uMat;

			// ① カメラ後退の終盤（最後の0.2秒程度）に「Ease-Out（滑らかな減速）」を効かせ、
			//    スタートカメラの位置へフワッと吸い込まれるように着地させる（SmootherStepカーブ）
			//    t=0からスムーズに動き始め、終盤t=1に向けて加速度・速度がゼロへとフワッと収束
			float easeCam = tMat * tMat * tMat * (tMat * (tMat * 6.0f - 15.0f) + 10.0f); // Ken Perlin's SmootherStep (C2連続)

			if (ctx.railCameraController)
			{
				ctx.railCameraController->SetProgress(retryStartProgress_);
				ctx.railCameraController->SetCurrentRailIndex(retryStartRailIndex_);

				Vector3 normalCamPos = { 0.0f, 2.5f, -8.0f };
				Vector3 normalLook = { 0.0f, 1.0f, 30.0f };
				Vector3 openingCamPos = { 2.0f, 0.45f, -12.0f };
				Vector3 openingLook = { 0.0f, 1.5f, 35.0f };

				Vector3 curCamLocal = {
					std::lerp(normalCamPos.x, openingCamPos.x, easeCam),
					std::lerp(normalCamPos.y, openingCamPos.y, easeCam),
					std::lerp(normalCamPos.z, openingCamPos.z, easeCam)
				};
				Vector3 curLookLocal = {
					std::lerp(normalLook.x, openingLook.x, easeCam),
					std::lerp(normalLook.y, openingLook.y, easeCam),
					std::lerp(normalLook.z, openingLook.z, easeCam)
				};

				ctx.railCameraController->SetCinematicCamera(true, curCamLocal, curLookLocal, 1.0f);
			}
		}
		else
		{
			// --- フェーズ2: ② 地形が完全に消え去った「空っぽの空間（ボイド）」の間（0.20秒間） ---
			(*ctx.terrainMaterializeProgress) = 0.0f; // 完全に世界が消滅した状態を維持

			// カメラは着地完了位置（オープニング初期構図）に完全に静止
			if (ctx.railCameraController)
			{
				ctx.railCameraController->SetProgress(retryStartProgress_);
				ctx.railCameraController->SetCurrentRailIndex(retryStartRailIndex_);

				Vector3 openingCamPos = { 2.0f, 0.45f, -12.0f };
				Vector3 openingLook = { 0.0f, 1.5f, 35.0f };
				ctx.railCameraController->SetCinematicCamera(true, openingCamPos, openingLook, 1.0f);
			}
		}

		// 地形オブジェクト群・障害物へ進行度を反映（消滅フェーズでは1.0->0.0、ボイドフェーズでは完全0.0）
		for (auto& model : (*ctx.modelInstances))
		{
			if (model && !model->IsBackgroundMountain())
			{
				model->SetColor({ 1.0f, 1.0f, 1.0f, (*ctx.terrainMaterializeProgress) });
			}
		}
		for (auto& obstacle : (*ctx.obstacles))
		{
			if (obstacle && obstacle->GetObject3d())
			{
				obstacle->GetObject3d()->SetColor({ 1.0f, 1.0f, 1.0f, (*ctx.terrainMaterializeProgress) });
			}
		}

		// ノイズを速やかに完全停止
		if (auto pp = services ? services->GetPostProcess() : nullptr)
		{
			pp->SetEffectActive("Random(Glitch)", false);
			if (auto* d = pp->GetData())
			{
				d->glitchStrength = 0.0f;
				d->noiseStrength = 0.0f;
			}
		}

		// 「完全消滅の静寂の間」が明けた瞬間（1.10秒到達時）
		if (tTotal >= 1.0f)
		{
			// 地形やレールを再破棄・再生成せず、超高速（1ms未満）に敵と状態をリセットし、
			// カメラを初期位置へパッと戻してスタート演出（足元からのパルス走査・地形再マテリアライズ・自機再構成）へシームレス突入！
			if (ctx.onRetry) ctx.onRetry();
			return;
		}
		return;
	}

	gameOverTimer_ += dt;

	if (ctx.railCameraController)
	{
		ctx.railCameraController->SetSpeedMultiplier(0.0f);
		retryStartProgress_ = ctx.railCameraController->GetProgress();
		retryStartRailIndex_ = ctx.railCameraController->GetCurrentRailIndex();
	}

	auto services = EngineServices::GetInstance();

	// ゲームオーバー中はスラスターパーティクルを確実にクリア
	(*ctx.thrusterEffect).ClearParticles();

	// 【ゲームオーバー演出タイムライン】
	// 1. [0.00s 〜 0.70s]: カメラは停止し、自機のみが慣性で前方へスーッと前進（カメラとの距離を離す）
	// 2. [0.70s 〜 2.35s]: 絶好の距離（約26m前方）で機体が美しく全方位へ四散（1.65秒間）
	// 3. [2.35s 〜 2.85s]: サイバーグリッド／スキャンライン展開（0.50秒間）
	// 4. [2.85s 〜]: UI表示
	const float kGlideDuration = 0.70f;
	const float kShatterDuration = 1.65f;
	const float kNoiseDuration = 0.50f;

	if (gameOverTimer_ < kGlideDuration)
	{
		gameOverStep_ = Step::HIT_STOP;

		float tGlide = gameOverTimer_ / kGlideDuration;
		// 慣性減速（最初速く、徐々に滑らかに減速する EaseOutCubic）
		float easeGlide = 1.0f - std::powf(1.0f - tGlide, 3.0f);

		// 自機をカメラから前方に 18m 進める（通常カメラ位置-8mと合わせて約26m離れ、四散エフェクト全景が画面中央に広々収まる！）
		Vector3 curGlidePos = {
			gameOverPlayerStartPos_.x,
			gameOverPlayerStartPos_.y - 0.4f * easeGlide,
			gameOverPlayerStartPos_.z + 18.0f * easeGlide
		};
		// 被弾による微小な機体姿勢変化（リアルな慣性滑空）
		Vector3 curGlideRot = {
			gameOverPlayerStartRot_.x + 0.12f * easeGlide,
			gameOverPlayerStartRot_.y,
			gameOverPlayerStartRot_.z + 0.20f * easeGlide
		};

		if (ctx.player)
		{
			ctx.player->SetCutsceneOverride(true, curGlidePos, curGlideRot);
			ctx.player->Update3DObjectOnly();
		}

		// HPが0になった瞬間（ポリゴン破片が出るまで）は画面ノイズ・グリッチを入れない
		if (auto pp = services ? services->GetPostProcess() : nullptr)
		{
			pp->SetEffectActive("Random(Glitch)", false);
		}
	}
	else if (gameOverTimer_ < kGlideDuration + kShatterDuration)
	{
		if (gameOverStep_ == Step::HIT_STOP)
		{
			gameOverStep_ = Step::POLYGON_SHATTER;

			if (ctx.player)
			{
				ctx.player->SetVisible(false);
			}

			gameOverShards_.clear();
			auto object3dCommon = services ? services->GetObject3dCommon() : nullptr;

			if (object3dCommon && ctx.player)
			{
				Vector3 shipWorldPos = ctx.player->GetWorldPosition();
				std::mt19937 rng(88888);
				std::uniform_real_distribution<float> distRot(-3.14f, 3.14f);
				std::uniform_real_distribution<float> distRotSpd(-10.0f, 10.0f);
				std::uniform_real_distribution<float> distSpeedTotal(14.0f, 26.0f);

				const Vector3 targetOffsets[64] = {
					{  0.0f,  0.02f,  2.5f }, {  0.0f, -0.06f,  2.2f }, {  0.0f,  0.10f,  1.9f }, {  0.0f, -0.10f,  1.6f },
					{ -0.15f, 0.05f,  1.8f }, {  0.15f, 0.05f,  1.8f }, { -0.12f,-0.05f,  1.5f }, {  0.12f,-0.05f,  1.5f },
					{  0.0f,  0.38f,  1.0f }, {  0.0f,  0.42f,  0.6f }, {  0.0f,  0.30f,  0.2f },
					{ -0.25f, 0.28f,  0.8f }, {  0.25f, 0.28f,  0.8f }, { -0.28f, 0.22f,  0.4f }, {  0.28f, 0.22f,  0.4f },
					{  0.0f,  0.15f,  0.0f }, {  0.0f, -0.22f,  0.0f }, {  0.0f,  0.12f, -0.5f }, {  0.0f, -0.18f, -0.5f },
					{ -0.35f, 0.02f, -0.1f }, {  0.35f, 0.02f, -0.1f }, { -0.42f, 0.05f, -0.6f }, {  0.42f, 0.05f, -0.6f },
					{ -0.85f, 0.02f,  0.4f }, { -1.45f, 0.04f,  0.1f }, { -2.10f, 0.06f, -0.3f },
					{ -2.80f, 0.09f, -0.7f }, { -3.50f, 0.12f, -1.1f }, { -3.95f, 0.14f, -1.3f },
					{ -1.80f,-0.02f, -0.7f }, { -2.60f,-0.02f, -1.1f }, { -3.30f, 0.03f, -1.5f },
					{  0.85f, 0.02f,  0.4f }, {  1.45f, 0.04f,  0.1f }, {  2.10f, 0.06f, -0.3f },
					{  2.80f, 0.09f, -0.7f }, {  3.50f, 0.12f, -1.1f }, {  3.95f, 0.14f, -1.3f },
					{  1.80f,-0.02f, -0.7f }, {  2.60f,-0.02f, -1.1f }, {  3.30f, 0.03f, -1.5f },
					{  0.0f,  0.65f, -0.8f }, {  0.0f,  1.05f, -1.2f }, {  0.0f,  1.35f, -1.6f },
					{ -0.35f, 0.75f, -1.1f }, {  0.35f, 0.75f, -1.1f }, { -0.55f, 1.15f, -1.4f }, {  0.55f, 1.15f, -1.4f },
					{ -0.65f,-0.18f,  0.3f }, {  0.65f,-0.18f,  0.3f },
					{ -0.88f,-0.08f, -0.5f }, {  0.88f,-0.08f, -0.5f },
					{ -0.52f, 0.00f, -1.8f }, {  0.52f, 0.00f, -1.8f },
					{ -0.52f,-0.05f, -2.1f }, {  0.52f,-0.05f, -2.1f },
					{  0.0f, -0.08f, -1.7f }, {  0.0f,  0.05f, -2.0f },
					{  0.0f, -0.45f, -0.6f }, {  0.0f, -0.65f, -1.1f },
					{ -0.22f,-0.40f, -0.9f }, {  0.22f,-0.40f, -0.9f }
				};

				for (int i = 0; i < 64; ++i)
				{
					GameOverShard shard;
					Vector3 offset = targetOffsets[i] * 0.5f;
					shard.position = shipWorldPos + offset;

					// フィボナッチ球面により、前後・左右・上下（四方八方・360度全球）に均等分散するベクトルを生成
					float y = 1.0f - (static_cast<float>(i) / 63.0f) * 2.0f; // -1.0 〜 +1.0 (上下)
					float radiusAtY = std::sqrt((std::max)(0.0f, 1.0f - y * y));
					const float kGoldenRatioAngle = 2.399963229728653f; // 黄金比角
					float theta = kGoldenRatioAngle * i;
					float x = std::cos(theta) * radiusAtY; // 左右
					float z = std::sin(theta) * radiusAtY; // 前後（手前・奥）

					// 機体パーツ本来の配置方向（offset）も30%ブレンドして自然な爆散形状にする
					Vector3 rawOffsetDir = offset;
					float rawLen = std::sqrt(rawOffsetDir.x * rawOffsetDir.x + rawOffsetDir.y * rawOffsetDir.y + rawOffsetDir.z * rawOffsetDir.z);
					if (rawLen > 0.001f)
					{
						rawOffsetDir.x /= rawLen;
						rawOffsetDir.y /= rawLen;
						rawOffsetDir.z /= rawLen;
					}
					else
					{
						rawOffsetDir = { 0.0f, 1.0f, 0.0f };
					}

					Vector3 finalDir = {
						x * 0.70f + rawOffsetDir.x * 0.30f,
						y * 0.70f + rawOffsetDir.y * 0.30f,
						z * 0.70f + rawOffsetDir.z * 0.30f
					};
					float finalLen = std::sqrt(finalDir.x * finalDir.x + finalDir.y * finalDir.y + finalDir.z * finalDir.z);
					if (finalLen > 0.001f)
					{
						finalDir.x /= finalLen;
						finalDir.y /= finalLen;
						finalDir.z /= finalLen;
					}

					float speed = distSpeedTotal(rng);
					shard.velocity = {
						finalDir.x * speed,
						finalDir.y * speed,
						finalDir.z * speed
					};

					shard.rotation = { distRot(rng), distRot(rng), distRot(rng) };
					shard.rotSpeed = { distRotSpd(rng), distRotSpd(rng), distRotSpd(rng) };
					float sc = 1.4f + (i % 4) * 0.35f;
					shard.scale = { sc, sc, sc };
					shard.life = 0.0f;
					shard.maxLife = 1.65f;

					// サイバーシアン／エメラルド／コアホワイトの鮮烈な破片色
					if (i % 3 == 0)
						shard.color = { 0.20f, 0.95f, 1.00f, 1.0f };
					else if (i % 3 == 1)
						shard.color = { 0.40f, 1.00f, 0.95f, 1.0f };
					else
						shard.color = { 0.95f, 1.00f, 1.00f, 1.0f };

					shard.object = std::make_unique<Object3d>();
					shard.object->Initialize(object3dCommon);
					shard.object->SetModel("polygon_shard.obj");
					shard.object->SetSelectLightings(6);
					shard.object->SetColor(shard.color);
					shard.object->SetScale(shard.scale);
					shard.object->SetRotation(shard.rotation);
					shard.object->SetTranslate(shard.position);
					shard.object->Update();

					gameOverShards_.push_back(std::move(shard));
				}
			}

			// プリズム化時のシェイクを弱めて爆発感を抑える（35.0f -> 10.0f）
			(*ctx.cameraShakeTimer) = 10.0f;
		}

		// パーティクルが四散して散らばる間はノイズを入れない（綺麗に四散する演出を長めに見せる）
		if (auto pp = services ? services->GetPostProcess() : nullptr)
		{
			pp->SetEffectActive("Random(Glitch)", false);
		}

		// 0.70s〜2.35s（1.65秒間）カメラから離れた位置でじっくりプリズムの四散全景を見せる
		float shatterPhase = std::clamp((gameOverTimer_ - kGlideDuration) / kShatterDuration, 0.0f, 1.0f);
		for (auto& shard : gameOverShards_)
		{
			if (!shard.object) continue;
			shard.position += shard.velocity * dt;
			shard.rotation += shard.rotSpeed * dt;

			float fade = std::clamp(1.0f - shatterPhase * shatterPhase, 0.0f, 1.0f);
			Vector4 col = shard.color;
			col.w = fade;

			shard.object->SetColor(col);
			shard.object->SetTranslate(shard.position);
			shard.object->SetRotation(shard.rotation);
			shard.object->SetScale(shard.scale * (1.0f + shatterPhase * 0.35f));
			shard.object->Update();
		}
	}
	else if (gameOverTimer_ < kGlideDuration + kShatterDuration + kNoiseDuration)
	{
		gameOverStep_ = Step::VOID_DISINTEGRATE;

		for (auto& shard : gameOverShards_)
		{
			if (shard.object) shard.object->SetScale({ 0.0f, 0.0f, 0.0f });
		}

		// プリズム四散直後（2.35s〜2.85s）のサイバーグリッド・スキャンライン展開フェーズ
		float noisePhase = std::clamp((gameOverTimer_ - (kGlideDuration + kShatterDuration)) / kNoiseDuration, 0.0f, 1.0f);
		if (auto pp = services ? services->GetPostProcess() : nullptr)
		{
			pp->SetEffectActive("Random(Glitch)", true);
			if (auto* d = pp->GetData())
			{
				d->glitchStrength = std::lerp(0.22f, 0.16f, noisePhase);
				d->noiseStrength = std::lerp(0.20f, 0.50f, noisePhase);
			}
		}

		// 背景地形は可視状態を維持
		for (auto& model : (*ctx.modelInstances))
		{
			if (model) model->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		}
		for (auto& obs : (*ctx.obstacles))
		{
			if (obs && obs->GetObject3d()) obs->GetObject3d()->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		}
	}
	else
	{
		gameOverStep_ = Step::SHOW_UI;

		auto sceneManager = ctx.sceneManager;
		bool isTransitioning = sceneManager && sceneManager->IsTransitioning();

		// UI表示中は NoiseStrength を 0.5 の状態で維持（シーン遷移開始後は即時リセット）
		if (auto pp = services ? services->GetPostProcess() : nullptr)
		{
			if (!isTransitioning)
			{
				pp->SetEffectActive("Random(Glitch)", true);
				if (auto* d = pp->GetData())
				{
					float glitchPulse = 0.16f + 0.06f * std::sin(gameOverTimer_ * 6.0f);
					d->glitchStrength = glitchPulse;
					d->noiseStrength = 0.50f; // ユーザー指定：0.5
				}
			}
			else
			{
				pp->SetEffectActive("Random(Glitch)", false);
				if (auto* d = pp->GetData())
				{
					d->glitchStrength = 0.0f;
					d->noiseStrength = 0.0f;
				}
			}
		}

		// 背景地形は可視状態を維持
		for (auto& model : (*ctx.modelInstances))
		{
			if (model) model->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		}
		for (auto& obs : (*ctx.obstacles))
		{
			if (obs && obs->GetObject3d()) obs->GetObject3d()->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		}

		if (sceneManager && !isTransitioning)
		{
			auto input = services ? services->GetInput() : nullptr;
			if (input)
			{
				if (input->TriggerKey(DIK_W) || input->TriggerKey(DIK_UP))
				{
					gameOverSelectedOption_ = MenuOption::Retry;
				}
				else if (input->TriggerKey(DIK_S) || input->TriggerKey(DIK_DOWN))
				{
					gameOverSelectedOption_ = MenuOption::Title;
				}

				if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) ||
					input->TriggerPadButton(XINPUT_GAMEPAD_A) || input->TriggerPadButton(XINPUT_GAMEPAD_START))
				{
					// 決定と同時にノイズを完全停止
					if (auto pp = services ? services->GetPostProcess() : nullptr)
					{
						pp->SetEffectActive("Random(Glitch)", false);
						if (auto* d = pp->GetData())
						{
							d->glitchStrength = 0.0f;
							d->noiseStrength = 0.0f;
						}
					}

					if (gameOverSelectedOption_ == MenuOption::Retry)
					{
						// シーン再ロード（画面暗転）を行わず、シームレス逆マテリアライズ演出を開始！
						gameOverStep_ = Step::RETRY_DEMATERIALIZE;
						retryDematerializeTimer_ = 0.0f;

						if (ctx.railCameraController)
						{
							retryStartProgress_ = ctx.railCameraController->GetProgress();
							retryStartRailIndex_ = ctx.railCameraController->GetCurrentRailIndex();
						}

						if (ctx.activeCamera)
						{
							retryStartCamPos_ = ctx.activeCamera->GetTranslate();
							retryStartCamTarget_ = retryStartCamPos_ + Vector3{ 0.0f, 0.0f, 30.0f };
						}

						// 敵・リング・弾・ミサイル・破片を即座に完全クリーンアップ（地形のみ残して消滅演出へ）
						(*ctx.bullets).clear();
						(*ctx.missiles).clear();
						(*ctx.enemyBullets).clear();
						(*ctx.enemies).clear();
						(*ctx.enhanceRings).clear();
						if (ctx.armoredTrainBoss)
						{
							ctx.armoredTrainBoss->SetActive(false);
						}
						for (auto& shard : gameOverShards_)
						{
							if (shard.object) shard.object->SetScale({ 0.0f, 0.0f, 0.0f });
						}
						gameOverShards_.clear();

						// BGM停止
						SoundManager::GetInstance()->StopBGM();
					}
					else if (gameOverSelectedOption_ == MenuOption::Title)
					{
						SoundManager::GetInstance()->StopBGM();
						SoundManager::GetInstance()->PlaySE("button");
						sceneManager->ChangeScene("TITLE", 0.90f, { 0.0f, 0.0f, 0.0f, 1.0f }, TransitionType::CRT_Off);
					}
				}
			}

			// デモモード中の自動決定処理（展示用ループ：プレイヤーが選んでいる感を演出）
			// サイクル1: RETRY -> (1.0s) TITLE -> (1.9s) RETRY -> (2.7s) 決定！
			// サイクル2: RETRY -> (1.0s) TITLE -> (1.8s) RETRY -> (2.5s) TITLE -> (3.3s) 決定！
			if (services && services->IsDemoMode() && ctx.demoController)
			{
				ctx.demoController->AddGameOverWaitTimer(dt);
				float waitTimer = ctx.demoController->GetGameOverWaitTimer();
				int menuStep = ctx.demoController->GetMenuSelectStep();

				if (ctx.demoController->GetDeathLoopCount() == 0)
				{
					// ==========================================
					// --- サイクル1: RETRY を選択・決定する動き ---
					// ==========================================
					// ステップ0 (1.0s経過): カーソルを TITLE へ移動（迷っている演出）
					if (waitTimer >= 1.0f && menuStep == 0)
					{
						gameOverSelectedOption_ = MenuOption::Title;
						SoundManager::GetInstance()->PlaySE("select.mp3", 0.6f);
						ctx.demoController->SetMenuSelectStep(1);
					}
					// ステップ1 (1.9s経過): カーソルを RETRY へ戻す
					else if (waitTimer >= 1.9f && menuStep == 1)
					{
						gameOverSelectedOption_ = MenuOption::Retry;
						SoundManager::GetInstance()->PlaySE("select.mp3", 0.6f);
						ctx.demoController->SetMenuSelectStep(2);
					}
					// ステップ2 (2.7s経過): RETRY を決定！
					else if (waitTimer >= 2.7f && menuStep == 2)
					{
						gameOverSelectedOption_ = MenuOption::Retry;

						if (auto pp = services->GetPostProcess())
						{
							pp->SetEffectActive("Random(Glitch)", false);
							if (auto* d = pp->GetData())
							{
								d->glitchStrength = 0.0f;
								d->noiseStrength = 0.0f;
							}
						}

						gameOverStep_ = Step::RETRY_DEMATERIALIZE;
						retryDematerializeTimer_ = 0.0f;

						if (ctx.railCameraController)
						{
							retryStartProgress_ = ctx.railCameraController->GetProgress();
							retryStartRailIndex_ = ctx.railCameraController->GetCurrentRailIndex();
						}

						if (ctx.activeCamera)
						{
							retryStartCamPos_ = ctx.activeCamera->GetTranslate();
							retryStartCamTarget_ = retryStartCamPos_ + Vector3{ 0.0f, 0.0f, 30.0f };
						}

						(*ctx.bullets).clear();
						(*ctx.missiles).clear();
						(*ctx.enemyBullets).clear();
						(*ctx.enemies).clear();
						(*ctx.enhanceRings).clear();
						if (ctx.armoredTrainBoss)
						{
							ctx.armoredTrainBoss->SetActive(false);
						}
						for (auto& shard : gameOverShards_)
						{
							if (shard.object) shard.object->SetScale({ 0.0f, 0.0f, 0.0f });
						}
						gameOverShards_.clear();

						SoundManager::GetInstance()->StopBGM();
						SoundManager::GetInstance()->PlaySE("button.mp3", 0.7f);

						ctx.demoController->SetDeathLoopCount(1); // 次回はタイトルへ
						ctx.demoController->ResetCycle();
					}
				}
				else
				{
					// ==========================================
					// --- サイクル2: TITLE を選択・決定する動き ---
					// ==========================================
					// ステップ0 (1.0s経過): カーソルを TITLE へ移動
					if (waitTimer >= 1.0f && menuStep == 0)
					{
						gameOverSelectedOption_ = MenuOption::Title;
						SoundManager::GetInstance()->PlaySE("select.mp3", 0.6f);
						ctx.demoController->SetMenuSelectStep(1);
					}
					// ステップ1 (1.8s経過): カーソルを RETRY へ一旦戻す（迷い）
					else if (waitTimer >= 1.8f && menuStep == 1)
					{
						gameOverSelectedOption_ = MenuOption::Retry;
						SoundManager::GetInstance()->PlaySE("select.mp3", 0.6f);
						ctx.demoController->SetMenuSelectStep(2);
					}
					// ステップ2 (2.5s経過): 再び TITLE へ移動！
					else if (waitTimer >= 2.5f && menuStep == 2)
					{
						gameOverSelectedOption_ = MenuOption::Title;
						SoundManager::GetInstance()->PlaySE("select.mp3", 0.6f);
						ctx.demoController->SetMenuSelectStep(3);
					}
					// ステップ3 (3.3s経過): TITLE を決定！（CRT電源オフ演出へ）
					else if (waitTimer >= 3.3f && menuStep == 3)
					{
						gameOverSelectedOption_ = MenuOption::Title;

						if (auto pp = services->GetPostProcess())
						{
							pp->SetEffectActive("Random(Glitch)", false);
							if (auto* d = pp->GetData())
							{
								d->glitchStrength = 0.0f;
								d->noiseStrength = 0.0f;
							}
						}

						SoundManager::GetInstance()->StopBGM();
						SoundManager::GetInstance()->PlaySE("button.mp3", 0.7f);
						sceneManager->ChangeScene("TITLE", 0.90f, { 0.0f, 0.0f, 0.0f, 1.0f }, TransitionType::CRT_Off);

						ctx.demoController->SetDeathLoopCount(0); // 次回はリトライから
						ctx.demoController->ResetCycle();
					}
				}
			}
		}
	}
}


void GameOverSequence::DrawUI(bool isGameOverPhase, SceneManager* sceneManager)
{
#ifdef USE_IMGUI
	if (!isGameOverPhase && !debugPreviewGameOverUI_) return;
	if (isGameOverPhase && gameOverTimer_ < 1.80f && !debugPreviewGameOverUI_) return;

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

	float transitionAlpha = 1.0f;

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

	drawList->PushClipRect(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), true);

	// リトライ逆マテリアライズ演出中はUIをスッとフェードアウト
	if (gameOverStep_ == Step::RETRY_DEMATERIALIZE)
	{
		float uiFade = std::clamp(1.0f - retryDematerializeTimer_ / 0.18f, 0.0f, 1.0f);
		if (uiFade <= 0.001f)
		{
			drawList->PopClipRect();
			return;
		}
		transitionAlpha *= uiFade;
	}

	if (transitionAlpha <= 0.001f)
	{
		drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(0, 0, 0, 255));
		drawList->PopClipRect();
		return;
	}

	float scale = std::min(screenW / 1280.0f, screenH / 720.0f);
	if (scale < 0.3f) scale = 0.3f;

	// 1.80s（プリズム四散後）からのサイバー演出の進行度
	float cyberProgress = debugPreviewGameOverUI_ ? 1.0f : std::clamp((gameOverTimer_ - 1.80f) / 0.50f, 0.0f, 1.0f);

	if (cyberProgress > 0.001f)
	{
		// ----------------------------------------------------
		// 1. サイバー空間オーバーレイ（薄いサイバーダークネイビー半透明）
		// ----------------------------------------------------
		drawList->AddRectFilled(
			viewPos,
			ImVec2(viewPos.x + screenW, viewPos.y + screenH),
			IM_COL32(2, 8, 16, static_cast<int>(145.0f * cyberProgress * transitionAlpha))
		);

		// ----------------------------------------------------
		// 2. スキャンライン（走査線）演出
		// ----------------------------------------------------
		// 4px ごとの水平スキャンライン
		int scanAlpha = static_cast<int>(18.0f * cyberProgress * transitionAlpha);
		ImU32 scanlineCol = IM_COL32(0, 220, 255, scanAlpha);
		for (float y = viewPos.y; y < viewPos.y + screenH; y += 4.0f)
		{
			drawList->AddLine(ImVec2(viewPos.x, y), ImVec2(viewPos.x + screenW, y), scanlineCol, 1.0f);
		}

		// 上から下へ流れるサイバースキャン光バー（スイープビーム）
		float sweepTimer = debugPreviewGameOverUI_ ? (float)ImGui::GetTime() : (gameOverTimer_ - 1.80f);
		float sweepY = viewPos.y + std::fmod(sweepTimer * 280.0f, screenH + 80.0f) - 40.0f;
		float beamHalfH = 32.0f * scale;
		ImU32 beamColor = IM_COL32(0, 240, 255, static_cast<int>(32.0f * cyberProgress * transitionAlpha));
		ImU32 beamCoreColor = IM_COL32(180, 255, 255, static_cast<int>(70.0f * cyberProgress * transitionAlpha));
		drawList->AddRectFilledMultiColor(
			ImVec2(viewPos.x, sweepY - beamHalfH),
			ImVec2(viewPos.x + screenW, sweepY),
			IM_COL32(0, 240, 255, 0), IM_COL32(0, 240, 255, 0),
			beamColor, beamColor
		);
		drawList->AddLine(ImVec2(viewPos.x, sweepY), ImVec2(viewPos.x + screenW, sweepY), beamCoreColor, 1.5f * scale);
		drawList->AddRectFilledMultiColor(
			ImVec2(viewPos.x, sweepY),
			ImVec2(viewPos.x + screenW, sweepY + beamHalfH),
			beamColor, beamColor,
			IM_COL32(0, 240, 255, 0), IM_COL32(0, 240, 255, 0)
		);

		// ----------------------------------------------------
		// 3. サイバーグリッド演出（上下に広がるサイバーメッシュ）
		// ----------------------------------------------------
		float gridSize = 48.0f * scale;
		int vertLines = static_cast<int>(screenW / gridSize) + 1;
		float startX = viewPos.x + std::fmod(screenW, gridSize) * 0.5f;
		for (int i = 0; i < vertLines; ++i)
		{
			float gx = startX + i * gridSize;
			ImU32 colEdge = IM_COL32(0, 210, 255, static_cast<int>(35.0f * cyberProgress * transitionAlpha));
			ImU32 colMid = IM_COL32(0, 210, 255, static_cast<int>(6.0f * cyberProgress * transitionAlpha));
			// 上半分
			drawList->AddRectFilledMultiColor(
				ImVec2(gx - 0.5f, viewPos.y),
				ImVec2(gx + 0.5f, viewPos.y + screenH * 0.5f),
				colEdge, colEdge, colMid, colMid
			);
			// 下半分
			drawList->AddRectFilledMultiColor(
				ImVec2(gx - 0.5f, viewPos.y + screenH * 0.5f),
				ImVec2(gx + 0.5f, viewPos.y + screenH),
				colMid, colMid, colEdge, colEdge
			);
		}
		int horizLines = static_cast<int>(screenH / gridSize) + 1;
		float startY = viewPos.y + std::fmod(screenH, gridSize) * 0.5f;
		for (int j = 0; j < horizLines; ++j)
		{
			float gy = startY + j * gridSize;
			float distFromCenter = std::abs((gy - (viewPos.y + screenH * 0.5f)) / (screenH * 0.5f));
			int lineAlpha = static_cast<int>((10.0f + 25.0f * distFromCenter) * cyberProgress * transitionAlpha);
			drawList->AddLine(
				ImVec2(viewPos.x, gy),
				ImVec2(viewPos.x + screenW, gy),
				IM_COL32(0, 210, 255, lineAlpha),
				1.0f
			);
		}

		// ----------------------------------------------------
		// 4. 上下のサイバーヘッダー/フッターフレーム（黒帯の代わり）
		// ----------------------------------------------------
		float edgeMarginY = 24.0f * scale;
		float frameAlpha = 220.0f * cyberProgress * transitionAlpha;
		ImU32 frameLineCol = IM_COL32(0, 235, 255, static_cast<int>(frameAlpha));
		ImU32 frameGlowCol = IM_COL32(0, 160, 220, static_cast<int>(frameAlpha * 0.45f));

		// 上部サイバーバー
		float topY = viewPos.y + edgeMarginY;
		drawList->AddLine(ImVec2(viewPos.x + 30.0f * scale, topY), ImVec2(viewPos.x + screenW - 30.0f * scale, topY), frameLineCol, 1.5f * scale);
		// 上部コーナー装飾ブラケット
		drawList->AddLine(ImVec2(viewPos.x + 20.0f * scale, topY - 8.0f * scale), ImVec2(viewPos.x + 20.0f * scale, topY + 8.0f * scale), frameLineCol, 2.0f * scale);
		drawList->AddLine(ImVec2(viewPos.x + screenW - 20.0f * scale, topY - 8.0f * scale), ImVec2(viewPos.x + screenW - 20.0f * scale, topY + 8.0f * scale), frameLineCol, 2.0f * scale);

		// 下部サイバーバー
		float bottomY = viewPos.y + screenH - edgeMarginY;
		drawList->AddLine(ImVec2(viewPos.x + 30.0f * scale, bottomY), ImVec2(viewPos.x + screenW - 30.0f * scale, bottomY), frameLineCol, 1.5f * scale);
		// 下部コーナー装飾ブラケット
		drawList->AddLine(ImVec2(viewPos.x + 20.0f * scale, bottomY - 8.0f * scale), ImVec2(viewPos.x + 20.0f * scale, bottomY + 8.0f * scale), frameLineCol, 2.0f * scale);
		drawList->AddLine(ImVec2(viewPos.x + screenW - 20.0f * scale, bottomY - 8.0f * scale), ImVec2(viewPos.x + screenW - 20.0f * scale, bottomY + 8.0f * scale), frameLineCol, 2.0f * scale);

		// 上下のサイバーステータステキスト（マイクロフォント）
		auto imguiManager = EngineServices::GetInstance()->GetImGuiManager();
		ImFont* fontMono = imguiManager ? imguiManager->GetFont(ImGuiManager::FontType::English_FiraMono) : ImGui::GetFont();
		if (!fontMono) fontMono = ImGui::GetFont();
		float subFontSize = 11.0f * scale;
		drawList->AddText(fontMono, subFontSize, ImVec2(viewPos.x + 35.0f * scale, topY + 4.0f * scale), frameGlowCol, "SYSTEM CORE: OFFLINE // EMERGENCY RECOVERY PROTOCOL");
		drawList->AddText(fontMono, subFontSize, ImVec2(viewPos.x + 35.0f * scale, bottomY - 16.0f * scale), frameGlowCol, "MEM_DUMP: 0x00A4FF01 [FAILED] // BUFFER PURGED");
	}

	// ----------------------------------------------------
	// 5. テキストおよびメニューUIの描画（2.30s〜）
	// ----------------------------------------------------
	if (gameOverTimer_ >= 2.30f || debugPreviewGameOverUI_)
	{
		auto imguiManager = EngineServices::GetInstance()->GetImGuiManager();
		auto textMgr = UITextManager::GetInstance();
		float elapsedUI = debugPreviewGameOverUI_ ? 10.0f : (gameOverTimer_ - 2.30f);

		ImFont* fontMono = imguiManager ? imguiManager->GetFont(ImGuiManager::FontType::English_FiraMono) : ImGui::GetFont();
		if (!fontMono) fontMono = ImGui::GetFont();

		// 中央ホログラムコンソールパネルおよびタイトルのフェード（0.0s〜0.4s）
		float menuAlpha = debugPreviewGameOverUI_ ? 1.0f : std::clamp(elapsedUI / 0.40f, 0.0f, 1.0f);

		float centerY = viewPos.y + screenH * 0.46f;
		float bannerH = 150.0f * scale;
		float bTop = centerY - bannerH * 0.5f;
		float bBottom = centerY + bannerH * 0.5f;

		// 中央ホログラムコンソールパネル
		float panelMarginX = screenW * 0.10f;
		float pLeft = viewPos.x + panelMarginX;
		float pRight = viewPos.x + screenW - panelMarginX;

		// 半透明サイバープレート
		drawList->AddRectFilled(
			ImVec2(pLeft, bTop),
			ImVec2(pRight, bBottom),
			IM_COL32(3, 16, 28, static_cast<int>(205.0f * menuAlpha * transitionAlpha)),
			4.0f * scale
		);

		// 上下ネオンエッジライン
		drawList->AddLine(
			ImVec2(pLeft, bTop),
			ImVec2(pRight, bTop),
			IM_COL32(0, 235, 255, static_cast<int>(190.0f * menuAlpha * transitionAlpha)),
			1.5f * scale
		);
		drawList->AddLine(
			ImVec2(pLeft, bBottom),
			ImVec2(pRight, bBottom),
			IM_COL32(0, 235, 255, static_cast<int>(190.0f * menuAlpha * transitionAlpha)),
			1.5f * scale
		);

		// パネル四隅のサイバーブラケット（[ ]）
		float bracketLen = 16.0f * scale;
		ImU32 bracketCol = IM_COL32(0, 255, 255, static_cast<int>(240.0f * menuAlpha * transitionAlpha));
		// 左上
		drawList->AddLine(ImVec2(pLeft, bTop), ImVec2(pLeft + bracketLen, bTop), bracketCol, 2.5f * scale);
		drawList->AddLine(ImVec2(pLeft, bTop), ImVec2(pLeft, bTop + bracketLen), bracketCol, 2.5f * scale);
		// 右上
		drawList->AddLine(ImVec2(pRight, bTop), ImVec2(pRight - bracketLen, bTop), bracketCol, 2.5f * scale);
		drawList->AddLine(ImVec2(pRight, bTop), ImVec2(pRight, bTop + bracketLen), bracketCol, 2.5f * scale);
		// 左下
		drawList->AddLine(ImVec2(pLeft, bBottom), ImVec2(pLeft + bracketLen, bBottom), bracketCol, 2.5f * scale);
		drawList->AddLine(ImVec2(pLeft, bBottom), ImVec2(pLeft - bracketLen, bBottom), bracketCol, 2.5f * scale);
		// 右下
		drawList->AddLine(ImVec2(pRight, bBottom), ImVec2(pRight - bracketLen, bBottom), bracketCol, 2.5f * scale);
		drawList->AddLine(ImVec2(pRight, bBottom), ImVec2(pRight, bBottom - bracketLen), bracketCol, 2.5f * scale);

		// 汎用テキスト描画ラムダ（選択肢用）
		auto renderTextItem = [&](const UITextItem* item, float alpha, float customScale = 1.0f, const std::string& overrideText = "", bool isHighlight = false) {
			if (!item || item->text.empty() || alpha <= 0.001f) return;
			ImFont* font = imguiManager ? imguiManager->GetFont(item->fontType) : ImGui::GetFont();
			if (!font) font = ImGui::GetFont();

			float fSize = item->fontSize * scale * customScale;
			const std::string& str = overrideText.empty() ? item->text : overrideText;
			ImVec2 tSize = font->CalcTextSizeA(fSize, FLT_MAX, -1.0f, str.c_str());

			float posX = viewPos.x + item->position.x * (screenW / 1280.0f);
			float posY = viewPos.y + item->position.y * (screenH / 720.0f);
			ImVec2 tPos(posX, posY);
			if (item->align == UITextAlign::Center) tPos.x -= tSize.x * 0.5f;
			else if (item->align == UITextAlign::Right) tPos.x -= tSize.x;

			if (item->hasShadow)
			{
				ImU32 shadowCol = ImColor(item->shadowColor.x, item->shadowColor.y, item->shadowColor.z, item->shadowColor.w * alpha);
				ImVec2 sPos(tPos.x + item->shadowOffset.x * scale, tPos.y + item->shadowOffset.y * scale);
				drawList->AddText(font, fSize, sPos, shadowCol, str.c_str());
			}

			if (item->hasOutline)
			{
				ImU32 outlineCol = ImColor(item->outlineColor.x, item->outlineColor.y, item->outlineColor.z, item->outlineColor.w * alpha);
				float thick = item->outlineThickness * scale;
				const float offsets[8][2] = {
					{ -thick, -thick }, { 0.0f, -thick }, { thick, -thick },
					{ -thick, 0.0f },                     { thick, 0.0f },
					{ -thick, thick },  { 0.0f, thick },  { thick, thick }
				};
				for (int k = 0; k < 8; ++k)
				{
					drawList->AddText(font, fSize, ImVec2(tPos.x + offsets[k][0], tPos.y + offsets[k][1]), outlineCol, str.c_str());
				}
			}

			Vector4 col = isHighlight ? Vector4{ 0.95f, 1.00f, 1.00f, 1.0f } : Vector4{ 0.35f, 0.78f, 0.92f, 0.85f };
			ImU32 textCol = ImColor(col.x, col.y, col.z, col.w * alpha);
			drawList->AddText(font, fSize, tPos, textCol, str.c_str());
		};

		// ========================================================
		// 【GAME OVER テキスト: モザイク／崩壊／グリッチ演出】
		// ========================================================
		const UITextItem* titleItem = textMgr ? textMgr->GetTextItem("GameOverTitle", "GAMEPLAY") : nullptr;
		if (titleItem)
		{
			float titleAlpha = menuAlpha * transitionAlpha;

			// 崩壊・グリッチ強度パラメータ
			// 出現直後（0.0s〜0.6s）は強いモザイク・崩壊、その後は定期的なサイバー破損ノイズ
			bool isPeriodicGlitch = (std::fmod(gameOverTimer_ * 3.3f, 2.6f) < 0.20f);
			float glitchIntensity = (elapsedUI < 0.60f) ? (1.0f - elapsedUI / 0.60f) : (isPeriodicGlitch ? 0.60f : 0.06f);

			// 1. 文字列のサイバーモザイク／デコードスクランブル
			std::string rawTitle = titleItem->text.empty() ? "GAME OVER" : titleItem->text;
			std::string displayTitle = rawTitle;
			static const char scrambleChars[] = "01X#_!%&/?<>[]*+-=@";
			int scrambleLen = (int)(sizeof(scrambleChars) - 1);

			if (elapsedUI < 0.50f)
			{
				// 出現時: 左から順に実体化、未確定部分はサイバー文字で激しくモザイク化
				float resolveProgress = elapsedUI / 0.50f;
				int resolvedChars = static_cast<int>(resolveProgress * (rawTitle.size() + 2));
				for (size_t ci = 0; ci < rawTitle.size(); ++ci)
				{
					if (rawTitle[ci] == ' ') continue;
					if (static_cast<int>(ci) >= resolvedChars)
					{
						int rIndex = (static_cast<int>(gameOverTimer_ * 45.0f) + ci * 7) % scrambleLen;
						displayTitle[ci] = scrambleChars[rIndex];
					}
				}
			}
			else if (isPeriodicGlitch)
			{
				// 確定後の突発的崩壊・破損
				int glitchCharIdx = static_cast<int>(gameOverTimer_ * 11.0f) % rawTitle.size();
				if (rawTitle[glitchCharIdx] != ' ')
				{
					displayTitle[glitchCharIdx] = scrambleChars[(static_cast<int>(gameOverTimer_ * 25.0f)) % scrambleLen];
				}
			}

			// テキストの座標・フォントサイズ計算
			ImFont* font = imguiManager ? imguiManager->GetFont(titleItem->fontType) : ImGui::GetFont();
			if (!font) font = ImGui::GetFont();
			float fSize = titleItem->fontSize * scale;
			ImVec2 tSize = font->CalcTextSizeA(fSize, FLT_MAX, -1.0f, displayTitle.c_str());

			float posX = viewPos.x + titleItem->position.x * (screenW / 1280.0f);
			float posY = viewPos.y + titleItem->position.y * (screenH / 720.0f);
			ImVec2 tPos(posX, posY);
			if (titleItem->align == UITextAlign::Center) tPos.x -= tSize.x * 0.5f;
			else if (titleItem->align == UITextAlign::Right) tPos.x -= tSize.x;

			// 2. RGBスプリット（赤系統色ズレ崩壊）
			if (glitchIntensity > 0.08f)
			{
				float splitDist = (5.0f * glitchIntensity) * scale;
				// ビビッドレッドゴースト（左ズレ）
				drawList->AddText(font, fSize, ImVec2(tPos.x - splitDist, tPos.y), IM_COL32(255, 20, 50, static_cast<int>(130.0f * titleAlpha * glitchIntensity)), displayTitle.c_str());
				// アンバー／オレンジゴースト（右ズレ）
				drawList->AddText(font, fSize, ImVec2(tPos.x + splitDist, tPos.y), IM_COL32(255, 130, 45, static_cast<int>(100.0f * titleAlpha * glitchIntensity)), displayTitle.c_str());
			}

			// 3. メインテキスト描画（重厚なダークワインシャドウ ＋ 鮮烈なサイバーネオンレッド発光）
			ImU32 shadowCol = IM_COL32(45, 4, 10, static_cast<int>(230.0f * titleAlpha));
			ImU32 mainCol = titleItem ? ImU32(ImColor(titleItem->color.x, titleItem->color.y, titleItem->color.z, titleItem->color.w * titleAlpha)) : IM_COL32(255, 45, 60, static_cast<int>(255.0f * titleAlpha));
			if (titleItem && (titleItem->color.x < 0.5f || (titleItem->color.y > 0.6f && titleItem->color.z > 0.6f)))
			{
				mainCol = IM_COL32(255, 45, 60, static_cast<int>(255.0f * titleAlpha));
			}

			drawList->AddText(font, fSize, ImVec2(tPos.x + 2.0f * scale, tPos.y + 2.0f * scale), shadowCol, displayTitle.c_str());
			drawList->AddText(font, fSize, tPos, mainCol, displayTitle.c_str());

			// 4. 水平スライス崩壊ライン（文字が横に引き裂かれるグリッチ）
			if (glitchIntensity > 0.12f)
			{
				int sliceSeed = static_cast<int>(gameOverTimer_ * 16.0f);
				float sliceRelY = (std::sin(sliceSeed * 1.7f) * 0.5f + 0.5f) * tSize.y;
				float sliceY = tPos.y + sliceRelY;
				float sliceH = (3.0f + 4.0f * (std::sin(sliceSeed * 2.9f) * 0.5f + 0.5f)) * scale;
				float sliceOffset = (std::sin(sliceSeed * 7.9f) * 10.0f * glitchIntensity) * scale;

				// スライスクリップ矩形で文字の一部分を横にシフトさせて崩壊感を表現
				drawList->PushClipRect(ImVec2(tPos.x - 30.0f * scale, sliceY), ImVec2(tPos.x + tSize.x + 30.0f * scale, sliceY + sliceH), true);
				drawList->AddText(font, fSize, ImVec2(tPos.x + sliceOffset, tPos.y), IM_COL32(255, 215, 220, static_cast<int>(240.0f * titleAlpha)), displayTitle.c_str());
				drawList->PopClipRect();

				// スライス走査線
				drawList->AddLine(
					ImVec2(tPos.x - 15.0f * scale, sliceY),
					ImVec2(tPos.x + tSize.x + 15.0f * scale, sliceY),
					IM_COL32(255, 50, 70, static_cast<int>(180.0f * titleAlpha * glitchIntensity)),
					1.0f * scale
				);
			}

			// 5. モザイク・ボクセル崩壊ピクセル（テキストから剥離・散乱するデジタルダスト）
			int pixelCount = static_cast<int>(18 * glitchIntensity);
			for (int pi = 0; pi < pixelCount; ++pi)
			{
				float pxRatio = std::fmod(std::sin(pi * 12.9898f + gameOverTimer_ * 3.5f) * 43758.5453f, 1.0f);
				if (pxRatio < 0.0f) pxRatio += 1.0f;
				float pyRatio = std::fmod(std::sin(pi * 78.233f + gameOverTimer_ * 5.5f) * 43758.5453f, 1.0f);
				if (pyRatio < 0.0f) pyRatio += 1.0f;

				float pX = tPos.x + pxRatio * tSize.x + (std::sin(pi * 4.3f + gameOverTimer_ * 9.0f) * 14.0f * glitchIntensity) * scale;
				float pY = tPos.y + pyRatio * tSize.y + (pyRatio * 18.0f * glitchIntensity) * scale;
				float pSize = (2.0f + (pi % 3) * 1.5f) * scale;

				ImU32 pCol = (pi % 2 == 0)
					? IM_COL32(255, 45, 65, static_cast<int>(220.0f * titleAlpha))
					: IM_COL32(255, 180, 190, static_cast<int>(230.0f * titleAlpha));
				drawList->AddRectFilled(ImVec2(pX, pY), ImVec2(pX + pSize, pY + pSize), pCol);
			}

			// 6. 発光アンダーライン（サイバークリムゾンレッド）
			bool isBarVisible = titleItem ? titleItem->hasUnderline : gameOverBarVisible_;
			if (isBarVisible)
			{
				float offsetY = titleItem ? titleItem->underlineOffsetY : gameOverBarOffsetY_;
				float barWidth = titleItem ? titleItem->underlineWidth : gameOverBarWidth_;
				float thickness = titleItem ? titleItem->underlineThickness : gameOverBarThickness_;
				float scaleX = screenW / 1280.0f;
				float scaleY = screenH / 720.0f;
				float barY = viewPos.y + (titleItem->position.y + offsetY) * scaleY;
				float barHalfW = (barWidth * 0.5f) * scale;
				float centerX = viewPos.x + titleItem->position.x * scaleX;

				ImU32 lineCol = titleItem ? ImU32(ImColor(titleItem->underlineColor.x, titleItem->underlineColor.y, titleItem->underlineColor.z, titleItem->underlineColor.w * titleAlpha)) : IM_COL32(255, 45, 65, static_cast<int>(240.0f * titleAlpha));
				drawList->AddLine(
					ImVec2(centerX - barHalfW, barY),
					ImVec2(centerX + barHalfW, barY),
					lineCol,
					thickness * scale
				);
			}
		}

		// ========================================================
		// メニュー選択肢（RETRY / TITLE）と操作ナビゲーション（0.45s〜フェードイン）
		// ========================================================
		if (elapsedUI >= 0.45f)
		{
			float optAlpha = debugPreviewGameOverUI_ ? 1.0f : std::clamp((elapsedUI - 0.45f) / 0.35f, 0.0f, 1.0f) * transitionAlpha;
			float pulse = 0.85f + 0.15f * std::sin((gameOverTimer_ - 2.0f) * 6.0f);

			const UITextItem* retryItem = textMgr ? textMgr->GetTextItem("GameOverRetry", "GAMEPLAY") : nullptr;
			if (retryItem)
			{
				bool isSelected = (gameOverSelectedOption_ == MenuOption::Retry);
				float itemAlpha = isSelected ? (optAlpha * pulse) : (optAlpha * 0.55f);
				std::string retryStr = isSelected ? ("> " + retryItem->text) : ("  " + retryItem->text);
				renderTextItem(retryItem, itemAlpha, isSelected ? 1.08f : 1.0f, retryStr, isSelected);
			}

			const UITextItem* titleNavItem = textMgr ? textMgr->GetTextItem("GameOverTitleNav", "GAMEPLAY") : nullptr;
			if (titleNavItem)
			{
				bool isSelected = (gameOverSelectedOption_ == MenuOption::Title);
				float itemAlpha = isSelected ? (optAlpha * pulse) : (optAlpha * 0.55f);
				std::string titleStr = isSelected ? ("> " + titleNavItem->text) : ("  " + titleNavItem->text);
				renderTextItem(titleNavItem, itemAlpha, isSelected ? 1.08f : 1.0f, titleStr, isSelected);
			}

			const char* navText = "[W / S] SELECT      [SPACE] DECIDE";
			float navFontSize = 17.0f * scale;
			ImVec2 navSize = fontMono->CalcTextSizeA(navFontSize, FLT_MAX, -1.0f, navText);
			ImVec2 navPos(viewPos.x + (screenW - navSize.x) * 0.5f, viewPos.y + screenH * 0.80f);
			drawList->AddText(fontMono, navFontSize, ImVec2(navPos.x + 1.5f, navPos.y + 1.5f), IM_COL32(0, 0, 0, static_cast<int>(180.0f * optAlpha)), navText);
			drawList->AddText(fontMono, navFontSize, navPos, IM_COL32(110, 220, 245, static_cast<int>(220.0f * optAlpha)), navText);
		}
	}

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

void GameOverSequence::Reset(bool hideShards)
{
	gameOverStep_ = Step::HIT_STOP;
	gameOverTimer_ = 0.0f;
	if (hideShards)
	{
		for (auto& shard : gameOverShards_)
		{
			if (shard.object) shard.object->SetScale({ 0.0f, 0.0f, 0.0f });
		}
	}
}

void GameOverSequence::Draw3D(Camera* activeCamera, Object3dCommon* object3dCommon)
{
	if (object3dCommon) object3dCommon->SetCommonDrawSetting();

	if (gameOverStep_ == Step::POLYGON_SHATTER)
	{
		for (auto& shard : gameOverShards_)
		{
			if (shard.object && shard.scale.x > 0.001f)
			{
				shard.object->SetCamera(activeCamera);
				shard.object->Draw();
			}
		}
	}
}