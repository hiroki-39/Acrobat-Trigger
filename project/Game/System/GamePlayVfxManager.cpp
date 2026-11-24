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

	// プレイヤー被弾用：3D三角形プリズム（正四面体 4面×3頂点=12頂点）＆横長デジタルスパーク
	TextureManager::GetInstance()->LoadTexture("sprites/white.png");
	TextureManager::GetInstance()->LoadTexture("line_glow.png");
	{
		struct PrismVertex { Vector4 position; Vector2 texcoord; Vector3 normal; };
		struct PrismMatCB {
			Vector4 color;
			int32_t selectLightings;
			int32_t enableLightingAsInt;
			float padding;
			float alphaReference;
			Matrix4x4 uvTransform;
		};
		PrismMatCB mat{};
		mat.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		mat.selectLightings = 2;
		mat.enableLightingAsInt = 0;
		mat.alphaReference = 0.0f;
		mat.uvTransform = Matrix4x4::Identity();

		// 正四面体（4つの三角形面）の頂点生成
		const Vector3 v0 = {  0.000f,  1.000f,  0.000f };
		const Vector3 v1 = {  0.943f, -0.333f,  0.000f };
		const Vector3 v2 = { -0.471f, -0.333f,  0.816f };
		const Vector3 v3 = { -0.471f, -0.333f, -0.816f };
		auto addTri = [](std::vector<PrismVertex>& out, const Vector3& a, const Vector3& b, const Vector3& c) {
			Vector3 n = Vector3::Cross(b - a, c - a);
			float l = n.Length();
			if (l > 1e-4f) n = n / l; else n = { 0.0f, 1.0f, 0.0f };
			out.push_back({ { a.x, a.y, a.z, 1.0f }, { 0.5f, 0.0f }, n });
			out.push_back({ { b.x, b.y, b.z, 1.0f }, { 1.0f, 1.0f }, n });
			out.push_back({ { c.x, c.y, c.z, 1.0f }, { 0.0f, 1.0f }, n });
		};
		std::vector<PrismVertex> prismVerts;
		prismVerts.reserve(12);
		addTri(prismVerts, v0, v1, v2);
		addTri(prismVerts, v0, v2, v3);
		addTri(prismVerts, v0, v3, v1);
		addTri(prismVerts, v1, v3, v2);

		damagePrismRenderer_.Initialize(dxCommon, srvManager, kMaxDamagePrisms);
		damagePrismRenderer_.CreateVertexBuffer(prismVerts.data(), static_cast<uint32_t>(prismVerts.size()), sizeof(PrismVertex));
		damagePrismRenderer_.CreateMaterialBuffer(sizeof(PrismMatCB), &mat);

		std::vector<PrismVertex> quadVerts = {
			{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
			{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
			{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		};
		damageSparkRenderer_.Initialize(dxCommon, srvManager, kMaxDamageSparks);
		damageSparkRenderer_.CreateVertexBuffer(quadVerts.data(), static_cast<uint32_t>(quadVerts.size()), sizeof(PrismVertex));
		damageSparkRenderer_.CreateMaterialBuffer(sizeof(PrismMatCB), &mat);
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
	damagePrisms_.clear();
	damageSparks_.clear();
	damagePrismCount_ = 0;
	damageSparkCount_ = 0;
	lowHpPulsePhase_ = 0.0f;
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

		// 被弾用 3D三角形プリズム更新
		Matrix4x4 vpMatrix = viewMatrix * projectionMatrix;
		damagePrismCount_ = 0;
		auto* prismBuf = damagePrismRenderer_.GetInstancingData();
		for (auto it = damagePrisms_.begin(); it != damagePrisms_.end();)
		{
			it->life += dt;
			if (it->life >= it->maxLife)
			{
				it = damagePrisms_.erase(it);
				continue;
			}
			it->position = it->position + it->velocity * dt;
			it->velocity = it->velocity * std::max(0.0f, 1.0f - 3.2f * dt);
			it->rotation = it->rotation + it->rotSpeed * dt;

			float r = std::clamp(it->life / it->maxLife, 0.0f, 1.0f);
			float sc = it->scale * (1.0f - r * r);

			if (prismBuf && damagePrismCount_ < kMaxDamagePrisms)
			{
				Matrix4x4 world = Matrix4x4::Scale({ sc, sc, sc })
					* Matrix4x4::RotateX(it->rotation.x)
					* Matrix4x4::RotateY(it->rotation.y)
					* Matrix4x4::RotateZ(it->rotation.z)
					* Matrix4x4::Translation(it->position);
				prismBuf[damagePrismCount_].World = world;
				prismBuf[damagePrismCount_].WVP = world * vpMatrix;

				// 警告色グラデーション（白閃光 → ネオンレッド → マゼンタ減衰）
				Vector4 col;
				if (r < 0.12f) {
					col = { 1.4f, 1.4f, 1.5f, 1.0f };
				} else if (r < 0.55f) {
					float u = (r - 0.12f) / 0.43f;
					col = { 1.55f - 0.15f * u, 0.10f, 0.25f + 0.50f * u, 0.95f };
				} else {
					float u = (r - 0.55f) / 0.45f;
					col = { 1.40f, 0.04f, 0.75f, 0.95f * (1.0f - u) };
				}
				prismBuf[damagePrismCount_].color = col;
				++damagePrismCount_;
			}
			++it;
		}

		// 被弾用 横長デジタルスパーク更新
		damageSparkCount_ = 0;
		auto* sparkBuf = damageSparkRenderer_.GetInstancingData();
		for (auto it = damageSparks_.begin(); it != damageSparks_.end();)
		{
			it->life += dt;
			if (it->life >= it->maxLife)
			{
				it = damageSparks_.erase(it);
				continue;
			}
			it->position = it->position + it->velocity * dt;
			float r = std::clamp(it->life / it->maxLife, 0.0f, 1.0f);
			Vector3 sc = {
				std::lerp(it->startScale.x, it->endScale.x, r),
				std::lerp(it->startScale.y, it->endScale.y, r),
				1.0f
			};
			if (sparkBuf && damageSparkCount_ < kMaxDamageSparks)
			{
				Matrix4x4 world = Matrix4x4::Scale(sc) * billboardMatrix * Matrix4x4::Translation(it->position);
				sparkBuf[damageSparkCount_].World = world;
				sparkBuf[damageSparkCount_].WVP = world * vpMatrix;
				sparkBuf[damageSparkCount_].color = { 1.50f, 0.08f, 0.38f, (1.0f - r) * 0.95f };
				++damageSparkCount_;
			}
			++it;
		}
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
	if (damageSparkCount_ > 0)
	{
		if (auto* texMgr = TextureManager::GetInstance())
		{
			damageSparkRenderer_.Draw(damageSparkCount_, texMgr->GetSrvIndex("line_glow.png"), BlendMode::Additive);
		}
	}
	if (damagePrismCount_ > 0)
	{
		if (auto* texMgr = TextureManager::GetInstance())
		{
			damagePrismRenderer_.Draw(damagePrismCount_, texMgr->GetSrvIndex("sprites/white.png"), BlendMode::Additive);
		}
	}
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
			std::vector<Vector3> funnelPositions;
			std::vector<uint8_t> funnelIsAttacking;
			if (auto* wm = player->GetWeaponManager())
			{
				wm->GetFunnelTrailStates(funnelPositions, funnelIsAttacking);
			}
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
				randomEngine,
				&funnelPositions,
				&funnelIsAttacking);
		}
		else
		{
			// 死亡時・ゲームオーバー時はスラスターを即座に消去
			playerThrusterVfx_.Clear();
		}

		// 被弾トリガー検出 → 3D三角形プリズム＆横長デジタルスパーク発生
		Vector3 dmgHitPos{};
		if (player->ConsumeDamageVfxTrigger(dmgHitPos))
		{
			SpawnPlayerDamageParticles(dmgHitPos);
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

void GamePlayVfxManager::SpawnBulletHitVoxelParticles(const Vector3& worldPos, const Vector3& bulletVel)
{
	auto services = EngineServices::GetInstance();
	Object3dCommon* object3dCommon = services ? services->GetObject3dCommon() : nullptr;
	if (!object3dCommon) return;

	// 連射・拡散ヒット時でも重くならず画面を埋めないよう総数を制限
	if (voxelHitParticles_.size() > 120) return;

	Vector3 backDir = { 0.0f, 0.0f, -1.0f };
	float velLen = bulletVel.Length();
	if (velLen > 0.001f)
	{
		backDir = (bulletVel / velLen) * (-1.0f);
	}

	// 他エフェクト（シアン輪・ボムマゼンタ・被弾赤三角）と被らない細長エレクトリックブルーチップ
	const Vector4 kColors[3] = {
		{ 1.45f, 1.55f, 1.75f, 1.0f }, // 高輝度アイスホワイト
		{ 0.16f, 0.78f, 1.70f, 1.0f }, // クリアエレクトリックブルー
		{ 0.65f, 0.24f, 1.55f, 1.0f }  // サイバーバイオレットブルー
	};

	// 着弾点の瞬間閃光コアボクセル（遠距離でも一目で着弾が分かるサイズ）
	{
		VoxelHitParticle core;
		core.obj = std::make_unique<Object3d>();
		core.obj->Initialize(object3dCommon);
		core.obj->SetModel("cube.obj");
		core.obj->SetEnableLighting(false);
		core.obj->SetEnvironmentCoefficient(0.0f);
		core.obj->SetSelectLightings(10);
		core.position = worldPos + backDir * 0.6f;
		core.velocity = { 0.0f, 0.0f, 0.0f };
		core.rotation = { 0.4f, 0.7f, 0.2f };
		core.rotSpeed = { 8.0f, 10.0f, 6.0f };
		core.scale = { 3.10f, 3.10f, 3.10f };
		core.color = { 2.0f, 2.2f, 2.5f, 1.0f };
		core.life = 0.0f;
		core.maxLife = 0.20f;
		core.obj->SetTranslate(core.position);
		core.obj->SetRotation(core.rotation);
		core.obj->SetScale(core.scale);
		core.obj->SetColor(core.color);
		core.obj->Update();
		voxelHitParticles_.push_back(std::move(core));
	}

	const int kCount = 8;
	for (int i = 0; i < kCount; ++i)
	{
		float rx = ((std::rand() % 200) - 100) * 0.01f;
		float ry = ((std::rand() % 200) - 100) * 0.01f;
		float rz = ((std::rand() % 200) - 100) * 0.01f;
		Vector3 spread = { rx + backDir.x * 1.1f, ry + backDir.y * 1.1f, rz + backDir.z * 1.1f };
		float sLen = spread.Length();
		if (sLen > 0.001f) spread = spread / sLen;

		float speed = 28.0f + (static_cast<float>(std::rand() % 100) / 100.0f) * 24.0f;
		float size = 1.35f + (static_cast<float>(std::rand() % 100) / 100.0f) * 0.90f;

		VoxelHitParticle vp;
		vp.obj = std::make_unique<Object3d>();
		vp.obj->Initialize(object3dCommon);
		vp.obj->SetModel("cube.obj");
		vp.obj->SetEnableLighting(false);
		vp.obj->SetEnvironmentCoefficient(0.0f);
		vp.obj->SetSelectLightings(10);

		vp.position = worldPos + spread * 0.85f;
		vp.velocity = spread * speed;
		vp.rotation = {
			static_cast<float>(std::rand() % 628) * 0.01f,
			static_cast<float>(std::rand() % 628) * 0.01f,
			static_cast<float>(std::rand() % 628) * 0.01f
		};
		vp.rotSpeed = {
			((std::rand() % 100) - 50) * 0.45f,
			((std::rand() % 100) - 50) * 0.45f,
			((std::rand() % 100) - 50) * 0.45f
		};
		vp.scale = { size, size, size }; // 正方形（立方体）ボクセルブロック
		vp.color = kColors[i % 3];
		vp.life = 0.0f;
		vp.maxLife = 0.36f + (static_cast<float>(std::rand() % 100) / 100.0f) * 0.12f;

		vp.obj->SetTranslate(vp.position);
		vp.obj->SetRotation(vp.rotation);
		vp.obj->SetScale(vp.scale);
		vp.obj->SetColor(vp.color);
		vp.obj->Update();
		voxelHitParticles_.push_back(std::move(vp));
	}
}

void GamePlayVfxManager::SpawnEnemyDissolveShards(const Vector3& centerPos)
{
	auto services = EngineServices::GetInstance();
	Object3dCommon* object3dCommon = services ? services->GetObject3dCommon() : nullptr;
	if (!object3dCommon) return;

	// ボス消滅時と統一したカラー（バイオレット・インディゴ・ネオンシアン・ネオンマゼンタ）
	const Vector4 kColors[5] = {
		{ 0.76f, 0.14f, 1.00f, 1.0f }, // エレクトリックバイオレット
		{ 0.76f, 0.14f, 1.00f, 1.0f },
		{ 0.22f, 0.48f, 1.00f, 1.0f }, // サイバーインディゴ
		{ 0.05f, 0.95f, 1.00f, 1.0f }, // ネオンシアン
		{ 1.00f, 0.18f, 0.60f, 1.0f }  // ネオンマゼンタ
	};

	const int kCount = 24;
	for (int i = 0; i < kCount; ++i)
	{
		if (voxelHitParticles_.size() > 160) break;

		float sz = (i % 3 == 0)
			? (0.65f + (static_cast<float>(std::rand() % 10) * 0.05f))
			: (0.35f + (static_cast<float>(std::rand() % 10) * 0.03f));

		VoxelHitParticle vp;
		vp.obj = std::make_unique<Object3d>();
		vp.obj->Initialize(object3dCommon);
		vp.obj->SetModel("cube.obj");
		vp.obj->SetEnableLighting(false);
		vp.obj->SetEnvironmentCoefficient(0.0f);
		vp.obj->SetSelectLightings(10);

		vp.position = {
			centerPos.x + ((std::rand() % 100) - 50) * 0.07f,
			centerPos.y + ((std::rand() % 100) - 50) * 0.06f,
			centerPos.z + ((std::rand() % 100) - 50) * 0.07f
		};
		// 上方向へ立ち昇る速度
		vp.velocity = {
			((std::rand() % 100) - 50) * 0.06f,
			10.0f + (static_cast<float>(std::rand() % 100) * 0.12f),
			((std::rand() % 100) - 50) * 0.06f
		};
		vp.rotation = {
			static_cast<float>(std::rand() % 628) * 0.01f,
			static_cast<float>(std::rand() % 628) * 0.01f,
			static_cast<float>(std::rand() % 628) * 0.01f
		};
		vp.rotSpeed = {
			((std::rand() % 100) - 50) * 0.08f,
			((std::rand() % 100) - 50) * 0.08f,
			((std::rand() % 100) - 50) * 0.08f
		};
		vp.scale = { sz, sz, sz };
		vp.color = kColors[i % 5];
		vp.life = 0.0f;
		vp.maxLife = 0.65f + (static_cast<float>(std::rand() % 10) * 0.04f);

		vp.obj->SetTranslate(vp.position);
		vp.obj->SetRotation(vp.rotation);
		vp.obj->SetScale(vp.scale);
		vp.obj->SetColor(vp.color);
		vp.obj->Update();
		voxelHitParticles_.push_back(std::move(vp));
	}
}

void GamePlayVfxManager::SpawnMemoryBurstParticles(const Vector3& centerPos, const Vector4& color, int count, float speedScale)
{
	auto services = EngineServices::GetInstance();
	Object3dCommon* object3dCommon = services ? services->GetObject3dCommon() : nullptr;
	if (!object3dCommon) return;

	for (int i = 0; i < count; ++i)
	{
		if (voxelHitParticles_.size() > 160) break;

		float phi = static_cast<float>(std::rand() % 628) * 0.01f;
		float cosT = (static_cast<float>(std::rand() % 200) - 100.0f) * 0.01f;
		float sinT = std::sqrtf(std::max(0.0f, 1.0f - cosT * cosT));
		Vector3 dir = { sinT * std::cosf(phi), cosT, sinT * std::sinf(phi) };

		float spd = (6.0f + static_cast<float>(std::rand() % 100) * 0.12f) * speedScale;
		float sz = 0.22f + static_cast<float>(std::rand() % 10) * 0.025f;

		VoxelHitParticle vp;
		vp.obj = std::make_unique<Object3d>();
		vp.obj->Initialize(object3dCommon);
		vp.obj->SetModel("cube.obj");
		vp.obj->SetEnableLighting(false);
		vp.obj->SetEnvironmentCoefficient(0.0f);
		vp.obj->SetSelectLightings(10);

		vp.position = centerPos;
		vp.velocity = { dir.x * spd, dir.y * spd + 1.5f, dir.z * spd };
		vp.rotation = {
			static_cast<float>(std::rand() % 628) * 0.01f,
			static_cast<float>(std::rand() % 628) * 0.01f,
			static_cast<float>(std::rand() % 628) * 0.01f
		};
		vp.rotSpeed = {
			((std::rand() % 100) - 50) * 0.22f,
			((std::rand() % 100) - 50) * 0.22f,
			((std::rand() % 100) - 50) * 0.22f
		};
		vp.scale = { sz, sz, sz };
		// 一部をホワイトコア発光、残りをメモリカラーに設定
		vp.color = (i % 4 == 0) ? Vector4{ 1.0f, 1.0f, 1.0f, 1.0f } : color;
		vp.life = 0.0f;
		vp.maxLife = 0.34f + (static_cast<float>(std::rand() % 10) * 0.025f);

		vp.obj->SetTranslate(vp.position);
		vp.obj->SetRotation(vp.rotation);
		vp.obj->SetScale(vp.scale);
		vp.obj->SetColor(vp.color);
		vp.obj->Update();
		voxelHitParticles_.push_back(std::move(vp));
	}
}

void GamePlayVfxManager::SpawnMemoryTrailParticle(const Vector3& pos, const Vector4& color)
{
	if (voxelHitParticles_.size() > 160) return;
	auto services = EngineServices::GetInstance();
	Object3dCommon* object3dCommon = services ? services->GetObject3dCommon() : nullptr;
	if (!object3dCommon) return;

	float sz = 0.16f + static_cast<float>(std::rand() % 6) * 0.02f;
	VoxelHitParticle vp;
	vp.obj = std::make_unique<Object3d>();
	vp.obj->Initialize(object3dCommon);
	vp.obj->SetModel("cube.obj");
	vp.obj->SetEnableLighting(false);
	vp.obj->SetEnvironmentCoefficient(0.0f);
	vp.obj->SetSelectLightings(10);

	vp.position = {
		pos.x + ((std::rand() % 100) - 50) * 0.008f,
		pos.y + ((std::rand() % 100) - 50) * 0.008f,
		pos.z + ((std::rand() % 100) - 50) * 0.008f
	};
	vp.velocity = {
		((std::rand() % 100) - 50) * 0.02f,
		((std::rand() % 100) - 50) * 0.02f,
		((std::rand() % 100) - 50) * 0.02f
	};
	vp.rotation = {
		static_cast<float>(std::rand() % 628) * 0.01f,
		static_cast<float>(std::rand() % 628) * 0.01f,
		0.0f
	};
	vp.rotSpeed = { 8.0f, 10.0f, 6.0f };
	vp.scale = { sz, sz, sz };
	vp.color = color;
	vp.life = 0.0f;
	vp.maxLife = 0.18f;

	vp.obj->SetTranslate(vp.position);
	vp.obj->SetRotation(vp.rotation);
	vp.obj->SetScale(vp.scale);
	vp.obj->SetColor(vp.color);
	vp.obj->Update();
	voxelHitParticles_.push_back(std::move(vp));
}

void GamePlayVfxManager::SpawnMemoryAmbientVoxel(const Vector3& centerPos, const Vector4& color, float modelRadius)
{
	if (voxelHitParticles_.size() > 160) return;
	auto services = EngineServices::GetInstance();
	Object3dCommon* object3dCommon = services ? services->GetObject3dCommon() : nullptr;
	if (!object3dCommon) return;

	// モデル半径に沿った円周付近（少し外側含む）から下端より発生して上へ昇る
	float angle = static_cast<float>(std::rand() % 628) * 0.01f;
	float r = modelRadius * (0.65f + static_cast<float>(std::rand() % 65) * 0.01f);
	float sz = 0.18f + static_cast<float>(std::rand() % 13) * 0.01f; // 0.18m 〜 0.30m（モデルサイズに合う小〜中ボクセル）

	VoxelHitParticle vp;
	vp.obj = std::make_unique<Object3d>();
	vp.obj->Initialize(object3dCommon);
	vp.obj->SetModel("cube.obj");
	vp.obj->SetEnableLighting(false);
	vp.obj->SetEnvironmentCoefficient(0.0f);
	vp.obj->SetSelectLightings(10);

	vp.position = {
		centerPos.x + std::cosf(angle) * r,
		centerPos.y - modelRadius * 1.05f + (static_cast<float>(std::rand() % 40) * 0.02f),
		centerPos.z + std::sinf(angle) * r
	};
	vp.velocity = {
		std::cosf(angle) * 0.25f,
		4.8f + static_cast<float>(std::rand() % 100) * 0.038f, // 下から上へ立ち昇る
		std::sinf(angle) * 0.25f
	};
	vp.rotation = {
		static_cast<float>(std::rand() % 628) * 0.01f,
		static_cast<float>(std::rand() % 628) * 0.01f,
		static_cast<float>(std::rand() % 628) * 0.01f
	};
	vp.rotSpeed = {
		((std::rand() % 100) - 50) * 0.09f,
		((std::rand() % 100) - 50) * 0.09f,
		((std::rand() % 100) - 50) * 0.09f
	};
	vp.scale = { sz, sz, sz };
	// 4回に1回はmemory_1と同系色のサイバープラチナ色、それ以外はメモリ種類色
	vp.color = (std::rand() % 4 == 0) ? Vector4{ 0.78f, 0.88f, 0.98f, 1.0f } : color;
	vp.life = 0.0f;
	vp.maxLife = 0.55f + static_cast<float>(std::rand() % 10) * 0.025f;

	vp.obj->SetTranslate(vp.position);
	vp.obj->SetRotation(vp.rotation);
	vp.obj->SetScale(vp.scale);
	vp.obj->SetColor(vp.color);
	vp.obj->Update();
	voxelHitParticles_.push_back(std::move(vp));
}

void GamePlayVfxManager::SpawnPlayerDamageParticles(const Vector3& hitPos)
{
	// 1. 視認性を邪魔しない小さめの3D三角形プリズム破片を16個放出
	const int kCount = 16;
	for (int i = 0; i < kCount; ++i)
	{
		if (damagePrisms_.size() >= kMaxDamagePrisms) break;
		float phi = static_cast<float>(std::rand() % 628) * 0.01f;
		float cosT = (static_cast<float>(std::rand() % 200) - 100.0f) * 0.01f;
		float sinT = std::sqrtf(std::max(0.0f, 1.0f - cosT * cosT));
		Vector3 dir = { sinT * std::cosf(phi), cosT, sinT * std::sinf(phi) };

		float spd = 8.0f + (static_cast<float>(std::rand() % 100) / 100.0f) * 14.0f;
		DamagePrismParticle dp;
		dp.position = hitPos + dir * 0.35f;
		dp.velocity = dir * spd;
		dp.rotation = {
			static_cast<float>(std::rand() % 628) * 0.01f,
			static_cast<float>(std::rand() % 628) * 0.01f,
			static_cast<float>(std::rand() % 628) * 0.01f
		};
		dp.rotSpeed = {
			((std::rand() % 100) - 50) * 0.35f,
			((std::rand() % 100) - 50) * 0.35f,
			((std::rand() % 100) - 50) * 0.35f
		};
		// 小さめサイズ（0.13m 〜 0.22m）で視界を一切妨げない
		dp.scale = 0.13f + (static_cast<float>(std::rand() % 100) / 100.0f) * 0.09f;
		dp.life = 0.0f;
		dp.maxLife = 0.24f + (static_cast<float>(std::rand() % 100) / 100.0f) * 0.10f;
		damagePrisms_.push_back(dp);
	}

	// 2. 横長デジタルスパーク（line_glow）を左右へパチッと散らす
	for (int i = 0; i < 5; ++i)
	{
		if (damageSparks_.size() >= kMaxDamageSparks) break;
		float sign = (i % 2 == 0) ? 1.0f : -1.0f;
		DamageLineSpark sp;
		sp.position = {
			hitPos.x + sign * 0.3f,
			hitPos.y + ((std::rand() % 100) - 50) * 0.02f,
			hitPos.z + ((std::rand() % 100) - 50) * 0.01f
		};
		sp.velocity = { sign * (10.0f + (std::rand() % 10)), ((std::rand() % 100) - 50) * 0.04f, 0.0f };
		sp.startScale = { 1.2f + (std::rand() % 6) * 0.15f, 0.08f, 1.0f };
		sp.endScale = { 2.0f, 0.02f, 1.0f };
		sp.life = 0.0f;
		sp.maxLife = 0.20f;
		damageSparks_.push_back(sp);
	}
}

void GamePlayVfxManager::UpdatePostProcessEffects(PostProcess* pp, bool isBoosting, bool& isJustDodgeActive, float& justDodgeTimer, float justDodgeMaxTime, float damagePulse, float hpRatio, float dt)
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

	// サイバーパンク2077風 被弾＆HP減少時ポストエフェクト更新
	if (pp)
	{
		pp->GetData()->damagePulse = std::clamp(damagePulse, 0.0f, 1.0f);

		float lowHp = 0.0f;
		if (hpRatio < 0.99f && hpRatio > 0.0f)
		{
			// 生物的な心拍ではなく、不規則な電子スパイク／警告フリッカーを生成
			lowHpPulsePhase_ += dt;
			float tStep = std::floor(lowHpPulsePhase_ * 18.0f);
			float hash = std::fmod(std::sinf(tStep * 12.9898f) * 43758.5453f, 1.0f);
			if (hash < 0.0f) hash += 1.0f;
			float spike = (hash > 0.78f) ? 1.0f : ((hash > 0.52f) ? 0.35f : 0.0f);

			if (hpRatio <= 0.50f)
			{
				// 危機的HP：基本エラーレベル + 不規則な電子バースト
				float severity = 1.0f - (hpRatio / 0.50f);
				lowHp = (0.24f + 0.46f * severity) + (0.30f * spike);
			}
			else
			{
				// HP減少中（50%〜99%）：時折走る軽微なシステムノイズ
				float loss = (1.0f - hpRatio) / 0.50f;
				lowHp = (0.05f + 0.14f * loss) + (0.12f * spike * loss);
			}
		}
		else
		{
			lowHpPulsePhase_ = 0.0f;
		}
		pp->GetData()->lowHpIntensity = std::clamp(lowHp, 0.0f, 1.0f);
	}
}

void GamePlayVfxManager::ResetPostProcess(PostProcess* pp)
{
	if (!pp) return;
	pp->SetEffectActive("Grayscale", false);
	pp->GetData()->damagePulse = 0.0f;
	pp->GetData()->lowHpIntensity = 0.0f;
	pp->SetEffectActive("Vignetting", false);
	pp->SetEffectActive("Random(Glitch)", false);
	if (auto* d = pp->GetData())
	{
		d->glitchStrength = 0.0f;
		d->noiseStrength = 0.0f;
	}
}
