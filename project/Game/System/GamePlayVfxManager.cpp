#define NOMINMAX
#include "GamePlayVfxManager.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Graphics/3d/Object/Object3dCommon.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

void GamePlayVfxManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	thrusterEffect_.Initialize(dxCommon, srvManager);
	thrusterEffect_.LoadFromJson("thruster.json");

	explosionEffect_.Initialize(dxCommon, srvManager);
	explosionEffect_.LoadFromJson("explosion.json");

	hitEffect_.Initialize(dxCommon, srvManager);
	hitEffect_.LoadFromJson("hit.json");


	dodgeEffect_.Initialize(dxCommon, srvManager);
	dodgeEffect_.LoadFromJson("dodge.json");

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
}

void GamePlayVfxManager::ClearAll()
{
	thrusterEffect_.ClearParticles();
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
		thrusterEffect_.Draw();
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
