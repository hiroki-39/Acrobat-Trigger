#define NOMINMAX
#include "GamePlayVfxManager.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Graphics/3d/Object/Object3dCommon.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"
#include "KHEngine/Graphics/Billboard/Billboard.h"
#include "KHEngine/Graphics/PostProcess/PostProcess.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "Game/Actor/Player/Player.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

void GamePlayVfxManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	thrusterEffect_.Initialize(dxCommon, srvManager);
	thrusterEffect_.LoadFromJson("thruster.json");
	playerThrusterVfx_.Initialize(dxCommon, srvManager);

	explosionEffect_.Initialize(dxCommon, srvManager);
	explosionEffect_.LoadFromJson("explosion.json");

	hitEffect_.Initialize(dxCommon, srvManager);
	hitEffect_.LoadFromJson("hit.json");


	dodgeEffect_.Initialize(dxCommon, srvManager);
	dodgeEffect_.LoadFromJson("dodge.json");
	for (auto& node : dodgeEffect_.GetNodes())
	{
		node->emitter.SetUseBillboard(true);
	}
	TextureManager::GetInstance()->LoadTexture("digital_ring.png");

	trailEffect_.Initialize(dxCommon, srvManager);
	trailEffect_.LoadFromJson("trail.json");

	missileSmokeEffect_.Initialize(dxCommon, srvManager);
	missileSmokeEffect_.LoadFromJson("missile_smoke.json");
	ModelManager::GetInstance()->LoadModel("cube.obj");

	ringEffect_.Initialize(dxCommon, srvManager);
	ringEffect_.LoadFromJson("ring.json");

	healRingEffect_.Initialize(dxCommon, srvManager);
	healRingEffect_.LoadFromJson("heal_ring.json");

	windEffect_.Initialize(dxCommon, srvManager);
	windEffect_.LoadFromJson("wind.json");
	for (auto& node : windEffect_.GetNodes())
	{
		node->emitter.SetUseBillboard(true);
	}
}

void GamePlayVfxManager::ClearAll()
{
	thrusterEffect_.ClearParticles();
	playerThrusterVfx_.Clear();
	explosionEffect_.ClearParticles();
	voxelHitParticles_.clear();
	hitEffect_.ClearParticles();
	dodgeEffect_.ClearParticles();
	trailEffect_.ClearParticles();
	missileSmokeEffect_.ClearParticles();
	ringEffect_.ClearParticles();
	healRingEffect_.ClearParticles();
	windEffect_.ClearParticles();
}

void GamePlayVfxManager::UpdateEffects(float dt, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const Matrix4x4& billboardMatrix)
{
		explosionEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
		hitEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
		dodgeEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
		trailEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
		missileSmokeEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
		for (auto it = voxelHitParticles_.begin(); it != voxelHitParticles_.end(); )
		{
			auto& vp = *it;
			vp.life += dt;
			if (vp.life >= vp.maxLife)
			{
				it = voxelHitParticles_.erase(it);
				continue;
			}
			vp.position = vp.position + vp.velocity * dt;
			vp.rotation = vp.rotation + vp.rotSpeed * dt;
			vp.velocity.x *= (1.0f - 2.6f * dt);
			vp.velocity.y *= (1.0f - 2.6f * dt);
			vp.velocity.z *= (1.0f - 2.6f * dt);
			float lifeRatio = std::clamp(vp.life / vp.maxLife, 0.0f, 1.0f);
			// くすみを防ぐためアルファは1.0fを保ち、スケール縮小のみで消滅させる
			float scaleFade = 1.0f - lifeRatio * lifeRatio;
			vp.color.w = 1.0f;
			if (vp.obj)
			{
				vp.obj->SetTranslate(vp.position);
				vp.obj->SetRotation(vp.rotation);
				vp.obj->SetColor(vp.color);
				vp.obj->SetScale({ vp.scale.x * scaleFade, vp.scale.y * scaleFade, vp.scale.z * scaleFade });
				vp.obj->Update();
			}
			++it;
		}
		ringEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
		healRingEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
}

void GamePlayVfxManager::Draw(bool drawThruster, bool drawWind, Object3dCommon* object3dCommon)
{
	if (drawThruster)
	{
		playerThrusterVfx_.Draw();
	}
	explosionEffect_.Draw();
	hitEffect_.Draw();
	dodgeEffect_.Draw();
	trailEffect_.Draw();
	missileSmokeEffect_.Draw();
	if (!voxelHitParticles_.empty())
	{
		if (object3dCommon) object3dCommon->SetCommonDrawSetting();
		for (const auto& vp : voxelHitParticles_)
		{
			if (vp.obj && vp.color.w > 0.01f)
			{
				vp.obj->Draw();
			}
		}
	}
	ringEffect_.Draw();
	healRingEffect_.Draw();
	if (drawWind)
	{
		windEffect_.Draw();
	}
}

void GamePlayVfxManager::SpawnVoxelHitParticles(const Vector3& worldPos, Camera* activeCamera, int count, float minSize, float maxSize)
{
	auto services = EngineServices::GetInstance();
	Object3dCommon* object3dCommon = services ? services->GetObject3dCommon() : nullptr;
	if (!object3dCommon) return;

	// ボス（紫・マゼンタ・赤系）と明確に区別できるプレイヤー専用シアン／アイスブルーカラーのみを使用
	auto PickPlayerCyberColor = [](int idx) -> Vector4 {
		int p = idx % 3;
		if (p == 0) return { 0.05f, 0.95f, 1.0f, 1.0f }; // 鮮烈ネオンシアン
		if (p == 1) return { 0.40f, 0.94f, 1.0f, 1.0f }; // クリスタルアイスブルー
		return { 0.85f, 0.98f, 1.0f, 1.0f };             // アイスホワイト
	};

	// カメラ向きの基底ベクトル（ボクセルで作るリング円がカメラから正面に見えやすいようにする）
	Vector3 ringNormal = { 0.0f, 0.0f, -1.0f };
	if (activeCamera)
	{
		const Matrix4x4& cMat = activeCamera->GetWorldMatrix();
		Vector3 toCam = { cMat.m[3][0] - worldPos.x, cMat.m[3][1] - worldPos.y, cMat.m[3][2] - worldPos.z };
		float len = toCam.Length();
		if (len > 0.001f) ringNormal = toCam / len;
	}
	Vector3 upRef = (std::abs(ringNormal.y) < 0.9f) ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
	Vector3 ringRight = Vector3::Cross(upRef, ringNormal).Normalize();
	Vector3 ringUp = Vector3::Cross(ringNormal, ringRight).Normalize();

	// 1. ボクセルブロックで作る円形リング（着弾点から放射状に弾け広がる）
	int ringCount = (count >= 24) ? 18 : 12;
	float ringSpeed = (maxSize > 1.8f) ? 30.0f : 18.0f;
	for (int i = 0; i < ringCount; ++i)
	{
		if (voxelHitParticles_.size() >= 420) break;
		float theta = (6.2831853f * static_cast<float>(i)) / static_cast<float>(ringCount);
		Vector3 radialDir = (ringRight * std::cos(theta) + ringUp * std::sin(theta)).Normalize();

		VoxelHitParticle vp;
		vp.obj = std::make_unique<Object3d>();
		vp.obj->Initialize(object3dCommon);
		vp.obj->SetModel("cube.obj");
		vp.color = PickPlayerCyberColor(i);
		vp.obj->SetColor(vp.color);
		vp.obj->SetEnableLighting(false);
		vp.obj->SetEnvironmentCoefficient(0.0f);
		vp.obj->SetSelectLightings(10);

		float sz = minSize * 0.85f;
		vp.scale = { sz, sz, sz };
		vp.obj->SetScale(vp.scale);
		vp.position = worldPos + radialDir * 1.2f;
		vp.velocity = radialDir * ringSpeed;
		vp.rotation = { 0.0f, 0.0f, theta };
		vp.rotSpeed = { 4.0f, 6.0f, 4.0f };
		vp.life = 0.0f;
		vp.maxLife = 0.50f;
		vp.obj->SetTranslate(vp.position);
		vp.obj->SetRotation(vp.rotation);
		vp.obj->Update();
		voxelHitParticles_.push_back(std::move(vp));
	}

	// 2. その周りから全方向へ花火のように弾けるボクセルブロック
	int burstCount = count;
	float minSpd = (maxSize > 1.8f) ? 16.0f : 10.0f;
	float maxSpd = (maxSize > 1.8f) ? 44.0f : 26.0f;
	for (int i = 0; i < burstCount; ++i)
	{
		if (voxelHitParticles_.size() >= 420) break;
		float phi = static_cast<float>(rand() % 628) * 0.01f;
		float cosT = (static_cast<float>(rand() % 200) - 100.0f) * 0.01f;
		float sinT = std::sqrt((std::max)(0.0f, 1.0f - cosT * cosT));
		Vector3 dir = { sinT * std::cos(phi), cosT, sinT * std::sin(phi) };

		VoxelHitParticle vp;
		vp.obj = std::make_unique<Object3d>();
		vp.obj->Initialize(object3dCommon);
		vp.obj->SetModel("cube.obj");
		vp.color = PickPlayerCyberColor(rand());
		vp.obj->SetColor(vp.color);
		vp.obj->SetEnableLighting(false);
		vp.obj->SetEnvironmentCoefficient(0.0f);
		vp.obj->SetSelectLightings(10);

		float tSz = static_cast<float>(rand() % 100) / 100.0f;
		float sz = minSize + (maxSize - minSize) * tSz;
		vp.scale = { sz, sz, sz };
		vp.obj->SetScale(vp.scale);

		float spd = minSpd + (maxSpd - minSpd) * (static_cast<float>(rand() % 100) / 100.0f);
		vp.position = worldPos + dir * 0.8f;
		vp.velocity = dir * spd;
		vp.rotation = { (rand() % 628) * 0.01f, (rand() % 628) * 0.01f, (rand() % 628) * 0.01f };
		vp.rotSpeed = {
			((rand() % 100) - 50) * 0.14f,
			((rand() % 100) - 50) * 0.14f,
			((rand() % 100) - 50) * 0.14f
		};
		vp.life = 0.0f;
		vp.maxLife = 0.48f + (rand() % 10) * 0.04f;
		vp.obj->SetTranslate(vp.position);
		vp.obj->SetRotation(vp.rotation);
		vp.obj->Update();
		voxelHitParticles_.push_back(std::move(vp));
	}
}

void GamePlayVfxManager::UpdatePlayerVfx(float dt, Player* player, Camera* activeCamera, bool isPlaying, bool isPaused, bool isStartCutscene, bool isPlayingPhase, bool isGameOver, bool isClear, float cutsceneTimer, bool isThrusterReady, float gameSpeed, std::mt19937& randomEngine)
{
	Matrix4x4 viewMatrix = Matrix4x4::Identity();
	Matrix4x4 projectionMatrix = Matrix4x4::Identity();
	Matrix4x4 billboardMatrix = Matrix4x4::Identity();
	if (activeCamera)
	{
		viewMatrix = activeCamera->GetViewMatrix();
		projectionMatrix = activeCamera->GetProjectionMatrix();
		billboardMatrix = Billboard::CreateFromCamera(activeCamera, true);
	}
	if (player && player->GetObject3d())
	{
		const Matrix4x4& wMat = player->GetObject3d()->GetmatWorld();
		Vector3 worldPos = { wMat.m[3][0], wMat.m[3][1], wMat.m[3][2] };

		// タイトルシーンと完全に同一：自機マトリクスに親子付けし、機体姿勢・ロール・傾きに完全追従
		thrusterEffect_.SetParentMatrix(&player->GetObject3d()->GetmatWorld());
		thrusterEffect_.SetPosition({ 0.0f, -0.22f, -3.00f });

		bool shouldEmitThruster = false;

		if (!player->IsDead() && !isGameOver)
		{
			if (isStartCutscene)
			{
				// スタート演出中は、機体が復元完了して点火するタイミング（2.50s以降）のみ放出
				if (cutsceneTimer >= 2.50f || player->IsCutsceneBoosting())
				{
					shouldEmitThruster = true;
				}
			}
			else if (isPlayingPhase || (isClear && player->IsVisible()))
			{
				// 通常プレイ時およびクリア演出・リザルト背景飛行時
				shouldEmitThruster = true;
			}

			bool isBoostActive = player->IsBoosting() || player->IsCutsceneBoosting() || (isStartCutscene && shouldEmitThruster);
			bool isRollActive = player->IsRolling();
			bool isWingVaporActive = isRollActive || player->IsBanking();
			float vfxDt = isPaused ? 0.0f : dt;
			playerThrusterVfx_.Update(
				vfxDt,
				wMat,
				viewMatrix,
				projectionMatrix,
				billboardMatrix,
				shouldEmitThruster,
				isBoostActive,
				isRollActive,
				isWingVaporActive,
				randomEngine);
		}
		else
		{
			// 死亡時・ゲームオーバー時はスラスターを即座に消去
			playerThrusterVfx_.Clear();
		}
		UpdateEffects(dt, viewMatrix, projectionMatrix, billboardMatrix);

		// 気流線（スピードライン・風の筋）エフェクトの発生
		// プレイヤーが前進していない時（停止時・カットシーン待機・ゲームオーバー・被弾・リトライ時等）は完全消去
		bool isPlayerMoving = isPlaying && !isPaused &&
		                      (isPlayingPhase || 
		                       (isStartCutscene && isThrusterReady)) &&
		                      gameSpeed > 0.01f &&
		                      (player && !player->IsDead());

		if (isPlayerMoving)
		{
			bool isBoost = player && player->IsBoosting();
			int spawnCount = isBoost ? 4 : 2;

			// 機体の飛行進路周辺に分散させて発生（照準を遮らないよう自然な範囲に分散）
			std::uniform_real_distribution<float> distOffsetX(-8.0f, 8.0f);
			std::uniform_real_distribution<float> distOffsetY(-3.0f, 4.5f);
			std::uniform_real_distribution<float> distOffsetZ(20.0f, 45.0f);

			for (int i = 0; i < spawnCount; ++i)
			{
				Vector3 windSpawnPos = {
					worldPos.x + distOffsetX(randomEngine),
					worldPos.y + distOffsetY(randomEngine),
					worldPos.z + distOffsetZ(randomEngine)
				};
				windEffect_.SetPosition(windSpawnPos);
				windEffect_.Play();
			}
		}
		else
		{
			// プレイヤーが進んでいない時はスピードラインを即座に完全消去
			windEffect_.ClearParticles();
		}
		windEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
	}

}

void GamePlayVfxManager::UpdatePostProcessEffects(PostProcess* pp, bool isBoosting, bool& isJustDodgeActive, float& justDodgeTimer, float justDodgeMaxTime)
{
	if (isBoosting)
	{
		thrusterEffect_.SetBaseColor({ 1.0f, 0.2f, 0.0f, 1.0f });
		if (pp)
		{
			pp->SetEffectActive("RadialBlur", true);
			pp->GetData()->radialBlurIntensity = 0.08f;
		}
	}
	else
	{
		thrusterEffect_.SetBaseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		if (pp)
		{
			pp->SetEffectActive("RadialBlur", false);
		}
	}

	if (isJustDodgeActive)
	{
		justDodgeTimer -= 1.0f;
		if (pp)
		{
			float t = justDodgeTimer / justDodgeMaxTime;
			pp->GetData()->vignetteIntensity = std::sinf(t * 3.141592f) * 0.6f;
		}
		if (justDodgeTimer <= 0.0f)
		{
			isJustDodgeActive = false;
			if (pp)
			{
				pp->SetEffectActive("Grayscale", false);
				pp->SetEffectActive("Vignetting", false);
			}
		}
	}
}

void GamePlayVfxManager::ResetPostProcess(PostProcess* pp)
{
	if (!pp) return;
	pp->SetEffectActive("Grayscale", false);
	pp->SetEffectActive("Vignetting", false);
	pp->SetEffectActive("Random(Glitch)", false);
	if (auto* d = pp->GetData())
	{
		d->glitchStrength = 0.0f;
		d->noiseStrength = 0.0f;
	}
}
