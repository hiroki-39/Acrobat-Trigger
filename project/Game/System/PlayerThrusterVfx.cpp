#define NOMINMAX
#include "PlayerThrusterVfx.h"
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Graphics/Resource/Descriptor/SrvManager.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "KHEngine/Graphics/3d/Primitive/Ring.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
	struct ParticleMaterialCB
	{
		Vector4 color;
		int32_t selectLightings;
		int32_t enableLightingAsInt;
		float padding;
		float alphaReference;
		Matrix4x4 uvTransform;
	};

	Matrix4x4 MakeBasisMatrix(const Vector3& right, const Vector3& up, const Vector3& forward)
	{
		Matrix4x4 m = Matrix4x4::Identity();
		m.m[0][0] = right.x;   m.m[0][1] = right.y;   m.m[0][2] = right.z;
		m.m[1][0] = up.x;      m.m[1][1] = up.y;      m.m[1][2] = up.z;
		m.m[2][0] = forward.x; m.m[2][1] = forward.y; m.m[2][2] = forward.z;
		return m;
	}

	Vector4 LerpVec4(const Vector4& a, const Vector4& b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t
		};
	}
}

void PlayerThrusterVfx::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	InitConeMesh();
	InitTrailMesh();
	InitSparkMesh();
	InitHoneycombMesh();

	Clear();
}

void PlayerThrusterVfx::Clear()
{
	leftBurnerTrail_.clear();
	rightBurnerTrail_.clear();
	leftWingVaporTrail_.clear();
	rightWingVaporTrail_.clear();
	sparks_.clear();
	rings_.clear();

	coneInstanceCount_ = 0;
	trailCoreVertexCount_ = 0;
	trailGlowVertexCount_ = 0;
	sparkInstanceCount_ = 0;
	ringInstanceCount_ = 0;
	drawTrails_ = false;
	prevBoosting_ = false;
	hasPrevPos_ = false;
	sparkSpawnAccum_ = 0.0f;
}

void PlayerThrusterVfx::InitConeMesh()
{
	coneRenderer_.Initialize(dxCommon_, srvManager_, 8);

	// 8角錐の鋭い幾何学コーン（先端 z=0.0 で太く純白発光、後端 z=-1.0 で尖り透明へ減衰）
	constexpr uint32_t kDiv = 8;
	const float topRadius = 1.0f;
	const float tipRadius = 0.06f;
	const float length = 1.0f;
	const float step = 2.0f * std::numbers::pi_v<float> / static_cast<float>(kDiv);

	std::vector<VertexData> verts;
	verts.reserve(kDiv * 6);

	for (uint32_t i = 0; i < kDiv; ++i)
	{
		float a0 = static_cast<float>(i) * step;
		float a1 = static_cast<float>(i + 1) * step;

		float c0 = std::cos(a0), s0 = std::sin(a0);
		float c1 = std::cos(a1), s1 = std::sin(a1);

		// Particle.PS.hlsl は v を反転 (1.0 - v) して circle2.png をサンプルするため、
		// v=0.50 が中心（最大輝度）、v=0.02 が外周（完全透明）となる
		VertexData vBase0 = { { c0 * topRadius, s0 * topRadius, 0.0f, 1.0f },    { 0.50f, 0.50f }, { c0, s0, 0.0f } };
		VertexData vBase1 = { { c1 * topRadius, s1 * topRadius, 0.0f, 1.0f },    { 0.50f, 0.50f }, { c1, s1, 0.0f } };
		VertexData vTip0  = { { c0 * tipRadius, s0 * tipRadius, -length, 1.0f }, { 0.50f, 0.02f }, { c0, s0, -0.5f } };
		VertexData vTip1  = { { c1 * tipRadius, s1 * tipRadius, -length, 1.0f }, { 0.50f, 0.02f }, { c1, s1, -0.5f } };

		verts.push_back(vBase0);
		verts.push_back(vTip0);
		verts.push_back(vTip1);

		verts.push_back(vBase0);
		verts.push_back(vTip1);
		verts.push_back(vBase1);
	}

	coneRenderer_.CreateVertexBuffer(verts.data(), static_cast<uint32_t>(verts.size()), sizeof(VertexData));

	ParticleMaterialCB mat{};
	mat.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	mat.selectLightings = 2;
	mat.enableLightingAsInt = 0;
	mat.alphaReference = 0.0f;
	mat.uvTransform = Matrix4x4::Identity();
	coneRenderer_.CreateMaterialBuffer(sizeof(ParticleMaterialCB), &mat);
}

void PlayerThrusterVfx::InitTrailMesh()
{
	std::vector<VertexData> dummyVerts(kMaxRibbonVertices);

	ParticleMaterialCB mat{};
	mat.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	mat.selectLightings = 2;
	mat.enableLightingAsInt = 0;
	mat.alphaReference = 0.0f;
	mat.uvTransform = Matrix4x4::Identity();

	trailCoreRenderer_.Initialize(dxCommon_, srvManager_, 1);
	trailCoreRenderer_.CreateVertexBuffer(dummyVerts.data(), kMaxRibbonVertices, sizeof(VertexData));
	trailCoreRenderer_.CreateMaterialBuffer(sizeof(ParticleMaterialCB), &mat);

	trailGlowRenderer_.Initialize(dxCommon_, srvManager_, 1);
	trailGlowRenderer_.CreateVertexBuffer(dummyVerts.data(), kMaxRibbonVertices, sizeof(VertexData));
	trailGlowRenderer_.CreateMaterialBuffer(sizeof(ParticleMaterialCB), &mat);
}

void PlayerThrusterVfx::InitSparkMesh()
{
	sparkRenderer_.Initialize(dxCommon_, srvManager_, kMaxSparks);

	std::vector<VertexData> verts = {
		{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
		{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
		{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
		{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
	};
	sparkRenderer_.CreateVertexBuffer(verts.data(), static_cast<uint32_t>(verts.size()), sizeof(VertexData));

	ParticleMaterialCB mat{};
	mat.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	mat.selectLightings = 2;
	mat.enableLightingAsInt = 0;
	mat.alphaReference = 0.0f;
	mat.uvTransform = Matrix4x4::Identity();
	sparkRenderer_.CreateMaterialBuffer(sizeof(ParticleMaterialCB), &mat);
}

void PlayerThrusterVfx::InitHoneycombMesh()
{
	ringRenderer_.Initialize(dxCommon_, srvManager_, kMaxRings);

	// 六角形（ハニカム）リングを生成
	auto ringVerts = KHPrimitive::CreateRingVertices(6, 0.78f, 1.0f);
	std::vector<VertexData> verts;
	verts.reserve(ringVerts.size());
	for (const auto& rv : ringVerts)
	{
		verts.push_back({ rv.position, rv.texcoord, rv.normal });
	}
	ringRenderer_.CreateVertexBuffer(verts.data(), static_cast<uint32_t>(verts.size()), sizeof(VertexData));

	ParticleMaterialCB mat{};
	mat.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	mat.selectLightings = 2;
	mat.enableLightingAsInt = 0;
	mat.alphaReference = 0.0f;
	mat.uvTransform = Matrix4x4::Identity();
	ringRenderer_.CreateMaterialBuffer(sizeof(ParticleMaterialCB), &mat);
}

Vector3 PlayerThrusterVfx::TransformPoint(const Vector3& p, const Matrix4x4& m)
{
	return {
		p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
		p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
		p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2]
	};
}

Vector3 PlayerThrusterVfx::ExtractNormalizedAxis(const Matrix4x4& m, int row)
{
	Vector3 v = { m.m[row][0], m.m[row][1], m.m[row][2] };
	float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	if (len > 1e-5f)
	{
		v.x /= len;
		v.y /= len;
		v.z /= len;
	}
	else
	{
		v = (row == 0) ? Vector3{ 1.0f, 0.0f, 0.0f } : ((row == 1) ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 0.0f, 0.0f, 1.0f });
	}
	return v;
}

Vector3 PlayerThrusterVfx::ExtractCameraPosition(const Matrix4x4& viewMatrix)
{
	Matrix4x4 invView = Matrix4x4::Inverse(viewMatrix);
	return { invView.m[3][0], invView.m[3][1], invView.m[3][2] };
}

void PlayerThrusterVfx::UpdateTrailQueue(std::deque<TrailPoint>& trail, const Vector3& currentPos, float dt, float maxLife, bool emit)
{
	for (auto& pt : trail)
	{
		pt.age += dt;
	}
	while (!trail.empty() && trail.back().age >= trail.back().maxLife)
	{
		trail.pop_back();
	}

	if (emit)
	{
		// 先頭点から十分離れていれば追加（高フレームレートでも滑らかな曲線）
		if (trail.empty())
		{
			trail.push_front({ currentPos, 0.0f, maxLife });
		}
		else
		{
			Vector3 d = currentPos - trail.front().position;
			float distSq = d.x * d.x + d.y * d.y + d.z * d.z;
			if (distSq > 0.0025f)
			{
				trail.push_front({ currentPos, 0.0f, maxLife });
				if (trail.size() > kMaxTrailPoints)
				{
					trail.pop_back();
				}
			}
			else
			{
				trail.front().position = currentPos;
				trail.front().age = 0.0f;
			}
		}
	}
}

void PlayerThrusterVfx::BuildRibbonVertices(
	const std::deque<TrailPoint>& trail,
	const Vector3& cameraPos,
	float baseHalfWidth,
	std::vector<VertexData>& outVertices)
{
	if (trail.size() < 2) return;

	const size_t n = trail.size();
	std::vector<Vector3> leftPts(n);
	std::vector<Vector3> rightPts(n);
	std::vector<float> ratios(n);

	for (size_t i = 0; i < n; ++i)
	{
		const Vector3& pos = trail[i].position;
		Vector3 tangent;
		if (i == 0)
		{
			tangent = trail[0].position - trail[1].position;
		}
		else if (i == n - 1)
		{
			tangent = trail[n - 2].position - trail[n - 1].position;
		}
		else
		{
			tangent = trail[i - 1].position - trail[i + 1].position;
		}

		float tLen = tangent.Length();
		if (tLen > 1e-5f) tangent = tangent / tLen;
		else tangent = { 0.0f, 0.0f, 1.0f };

		Vector3 toCam = cameraPos - pos;
		float cLen = toCam.Length();
		if (cLen > 1e-5f) toCam = toCam / cLen;
		else toCam = { 0.0f, 0.0f, -1.0f };

		Vector3 side = Vector3::Cross(tangent, toCam);
		float sLen = side.Length();
		if (sLen > 1e-5f) side = side / sLen;
		else side = { 1.0f, 0.0f, 0.0f };

		float ratio = std::clamp(trail[i].age / trail[i].maxLife, 0.0f, 1.0f);
		// 後端に向けて急激に細く収束させ、視界を遮らないシャープな尾にする
		float taper = (1.0f - ratio) * (1.0f - ratio);
		ratios[i] = ratio;

		float w = baseHalfWidth * taper;
		leftPts[i] = pos - side * w;
		rightPts[i] = pos + side * w;
	}

	for (size_t i = 0; i + 1 < n; ++i)
	{
		if (outVertices.size() + 6 > kMaxRibbonVertices) break;

		// circle2.png は中心 (0.5, 0.5) が最大輝度、外周が 0.0
		// 後端側をより早く透明へ減衰させる
		float fade0 = 1.0f - (1.0f - ratios[i]) * (1.0f - ratios[i]);
		float fade1 = 1.0f - (1.0f - ratios[i + 1]) * (1.0f - ratios[i + 1]);
		float v0 = 0.50f - 0.48f * fade0;
		float v1 = 0.50f - 0.48f * fade1;

		VertexData l0 = { { leftPts[i].x,      leftPts[i].y,      leftPts[i].z,      1.0f }, { 0.20f, v0 }, { 0.0f, 1.0f, 0.0f } };
		VertexData r0 = { { rightPts[i].x,     rightPts[i].y,     rightPts[i].z,     1.0f }, { 0.80f, v0 }, { 0.0f, 1.0f, 0.0f } };
		VertexData l1 = { { leftPts[i + 1].x,  leftPts[i + 1].y,  leftPts[i + 1].z,  1.0f }, { 0.20f, v1 }, { 0.0f, 1.0f, 0.0f } };
		VertexData r1 = { { rightPts[i + 1].x, rightPts[i + 1].y, rightPts[i + 1].z, 1.0f }, { 0.80f, v1 }, { 0.0f, 1.0f, 0.0f } };

		outVertices.push_back(l0);
		outVertices.push_back(r0);
		outVertices.push_back(l1);

		outVertices.push_back(r0);
		outVertices.push_back(r1);
		outVertices.push_back(l1);
	}
}

void PlayerThrusterVfx::Update(
	float dt,
	const Matrix4x4& playerWorldMatrix,
	const Matrix4x4& viewMatrix,
	const Matrix4x4& projectionMatrix,
	const Matrix4x4& billboardMatrix,
	bool isActive,
	bool isBoosting,
	bool isRolling,
	bool isWingVaporActive,
	std::mt19937& randomEngine)
{
	if (dt <= 0.0f) dt = 0.0f;
	pulseTimer_ += dt;

	// 通常(紫: 0.0) <-> ブースト(オレンジ: 1.0) の滑らかなカラー補間
	float targetBoost = (isActive && isBoosting) ? 1.0f : 0.0f;
	float blendSpeed = isBoosting ? 14.0f : 8.0f;
	boostBlend_ += (targetBoost - boostBlend_) * std::clamp(blendSpeed * dt, 0.0f, 1.0f);

	const Vector4 kNormalPurpleGlow = { 0.65f, 0.18f, 1.00f, 0.44f }; // 通常時：紫色グロー（背景シアンと分離）
	const Vector4 kBoostOrangeGlow  = { 1.00f, 0.44f, 0.08f, 0.48f }; // ブースト時：オレンジグロー（低アルファ加算）
	const Vector4 kWhiteCore        = { 1.00f, 1.00f, 1.00f, 0.50f }; // 中心核：純白
	Vector4 outerGlowColor = LerpVec4(kNormalPurpleGlow, kBoostOrangeGlow, boostBlend_);

	Vector3 shipPos     = { playerWorldMatrix.m[3][0], playerWorldMatrix.m[3][1], playerWorldMatrix.m[3][2] };
	Vector3 shipRight   = ExtractNormalizedAxis(playerWorldMatrix, 0);
	Vector3 shipUp      = ExtractNormalizedAxis(playerWorldMatrix, 1);
	Vector3 shipForward = ExtractNormalizedAxis(playerWorldMatrix, 2);
	Matrix4x4 shipBasis = MakeBasisMatrix(shipRight, shipUp, shipForward);

	// 大きなワープ（シーン切り替え・リセット等）時はトレイル履歴をクリア
	Vector3 shipDelta = { 0.0f, 0.0f, 0.0f };
	if (hasPrevPos_)
	{
		Vector3 delta = shipPos - prevShipPos_;
		if (delta.Length() > 30.0f)
		{
			leftBurnerTrail_.clear();
			rightBurnerTrail_.clear();
			leftWingVaporTrail_.clear();
			rightWingVaporTrail_.clear();
		}
		else
		{
			shipDelta = delta;
		}
	}
	prevShipPos_ = shipPos;
	hasPrevPos_ = true;

	// 左右バーニア噴射口・主翼端のワールド座標（元の左右幅を維持しつつ後方へ配置）
	Vector3 leftNozzlePos  = TransformPoint({ -0.55f, -0.05f, -2.55f }, playerWorldMatrix);
	Vector3 rightNozzlePos = TransformPoint({  0.55f, -0.05f, -2.55f }, playerWorldMatrix);
	Vector3 leftWingPos    = TransformPoint({ -3.80f,  0.00f, -0.50f }, playerWorldMatrix);
	Vector3 rightWingPos   = TransformPoint({  3.80f,  0.00f, -0.50f }, playerWorldMatrix);

	Matrix4x4 vpMatrix = viewMatrix * projectionMatrix;

	// =========================================================
	// ① コアノズル（噴射口固定の幾何学3Dコーン）
	// =========================================================
	coneInstanceCount_ = 0;
	if (isActive)
	{
		// 高周波の伸縮・明滅パルス
		float flicker = 0.92f + 0.14f * std::sin(pulseTimer_ * 55.0f) + 0.06f * std::cos(pulseTimer_ * 93.0f);
		float lengthScale = std::lerp(1.25f, 2.25f, boostBlend_) * flicker; // ブースト時は約1.8倍へ鋭く伸長
		float radiusScale = std::lerp(1.0f, 1.20f, boostBlend_);

		// UVスクロールによるエネルギー流動感
		if (auto* mat = static_cast<ParticleMaterialCB*>(coneRenderer_.GetMaterialData()))
		{
			float uShift = std::sin(pulseTimer_ * 38.0f) * 0.03f;
			mat->uvTransform = Matrix4x4::Translation({ uShift, 0.0f, 0.0f });
		}

		ParticleForGPU* coneBuf = coneRenderer_.GetInstancingData();
		if (coneBuf)
		{
			const Vector3 nozzles[2] = { leftNozzlePos, rightNozzlePos };
			for (int i = 0; i < 2; ++i)
			{
				// 外周グローコーン（青 / オレンジ）
				{
					Matrix4x4 s = Matrix4x4::Scale({ 0.22f * radiusScale, 0.22f * radiusScale, lengthScale });
					Matrix4x4 t = Matrix4x4::Translation(nozzles[i]);
					Matrix4x4 world = s * shipBasis * t;
					coneBuf[coneInstanceCount_].World = world;
					coneBuf[coneInstanceCount_].WVP = world * vpMatrix;
					coneBuf[coneInstanceCount_].color = outerGlowColor;
					++coneInstanceCount_;
				}
				// 中心核コアコーン（純白・細身）
				{
					Matrix4x4 s = Matrix4x4::Scale({ 0.11f * radiusScale, 0.11f * radiusScale, lengthScale * 0.85f });
					Matrix4x4 t = Matrix4x4::Translation(nozzles[i]);
					Matrix4x4 world = s * shipBasis * t;
					coneBuf[coneInstanceCount_].World = world;
					coneBuf[coneInstanceCount_].WVP = world * vpMatrix;
					coneBuf[coneInstanceCount_].color = kWhiteCore;
					++coneInstanceCount_;
				}
			}
		}
	}

	// =========================================================
	// ② ウェイクトレイル（細い光の帯）＋ ロール時主翼端ベイパー
	// =========================================================
	// 前進速度が速いため寿命を短く抑え、機体移動時に前方視界を遮らない長さにする
	float burnerTrailLife = isRolling ? 0.13f : (isBoosting ? 0.09f : 0.06f);
	UpdateTrailQueue(leftBurnerTrail_,  leftNozzlePos,  dt, burnerTrailLife, isActive);
	UpdateTrailQueue(rightBurnerTrail_, rightNozzlePos, dt, burnerTrailLife, isActive);

	// バレルロール（Q/E）や深い旋回バンク時に主翼端から極細の光のベイパーラインを発生
	UpdateTrailQueue(leftWingVaporTrail_,  leftWingPos,  dt, 0.15f, isActive && isWingVaporActive);
	UpdateTrailQueue(rightWingVaporTrail_, rightWingPos, dt, 0.15f, isActive && isWingVaporActive);

	Vector3 cameraPos = ExtractCameraPosition(viewMatrix);
	std::vector<VertexData> coreVerts;
	std::vector<VertexData> glowVerts;
	coreVerts.reserve(kMaxRibbonVertices);
	glowVerts.reserve(kMaxRibbonVertices);

	// バーニア口径以内の極細リボン（視認性を損なわない太さ）
	BuildRibbonVertices(leftBurnerTrail_,  cameraPos, 0.038f, coreVerts);
	BuildRibbonVertices(rightBurnerTrail_, cameraPos, 0.038f, coreVerts);
	BuildRibbonVertices(leftWingVaporTrail_,  cameraPos, 0.028f, coreVerts);
	BuildRibbonVertices(rightWingVaporTrail_, cameraPos, 0.028f, coreVerts);

	BuildRibbonVertices(leftBurnerTrail_,  cameraPos, 0.095f, glowVerts);
	BuildRibbonVertices(rightBurnerTrail_, cameraPos, 0.095f, glowVerts);
	BuildRibbonVertices(leftWingVaporTrail_,  cameraPos, 0.060f, glowVerts);
	BuildRibbonVertices(rightWingVaporTrail_, cameraPos, 0.060f, glowVerts);

	trailCoreVertexCount_ = static_cast<uint32_t>(coreVerts.size());
	trailGlowVertexCount_ = static_cast<uint32_t>(glowVerts.size());
	drawTrails_ = (trailCoreVertexCount_ > 0 || trailGlowVertexCount_ > 0);

	if (trailCoreVertexCount_ > 0)
	{
		trailCoreRenderer_.UpdateVertexBuffer(coreVerts.data(), trailCoreVertexCount_);
		if (auto* buf = trailCoreRenderer_.GetInstancingData())
		{
			buf[0].World = Matrix4x4::Identity();
			buf[0].WVP = vpMatrix;
			buf[0].color = { 1.0f, 1.0f, 1.0f, 0.30f };
		}
	}
	if (trailGlowVertexCount_ > 0)
	{
		trailGlowRenderer_.UpdateVertexBuffer(glowVerts.data(), trailGlowVertexCount_);
		if (auto* buf = trailGlowRenderer_.GetInstancingData())
		{
			buf[0].World = Matrix4x4::Identity();
			buf[0].WVP = vpMatrix;
			buf[0].color = { outerGlowColor.x, outerGlowColor.y, outerGlowColor.z, 0.30f };
		}
	}

	// =========================================================
	// ③ デジタルスパーク（極小ドット粒子：現在の1/4以下・少量）
	// =========================================================
	for (auto it = sparks_.begin(); it != sparks_.end();)
	{
		it->age += dt;
		if (it->age >= it->maxLife)
		{
			it = sparks_.erase(it);
			continue;
		}
		it->position = it->position + it->velocity * dt;
		++it;
	}

	if (isActive)
	{
		float spawnRate = isBoosting ? 120.0f : 65.0f; // 視界を遮らない程度の細かい粒子を連続放出
		sparkSpawnAccum_ += dt * spawnRate;
		std::uniform_real_distribution<float> distSpread(-0.10f, 0.10f);
		std::uniform_real_distribution<float> distSpeed(5.0f, 12.0f);
		std::uniform_real_distribution<float> distScale(0.045f, 0.075f); // 視認できる極小ドット粒子
		std::uniform_real_distribution<float> distLife(0.18f, 0.30f);
		std::uniform_real_distribution<float> distWhite(0.0f, 1.0f);

		while (sparkSpawnAccum_ >= 1.0f)
		{
			sparkSpawnAccum_ -= 1.0f;
			if (sparks_.size() >= kMaxSparks) break;

			const Vector3& basePos = (sparks_.size() % 2 == 0) ? leftNozzlePos : rightNozzlePos;
			DigitalSpark sp;
			sp.position = basePos
				+ shipRight * distSpread(randomEngine)
				+ shipUp * distSpread(randomEngine)
				- shipForward * 0.25f;
			float spd = distSpeed(randomEngine) * (isBoosting ? 1.5f : 1.0f);
			sp.velocity = shipForward * (-spd)
				+ shipRight * (distSpread(randomEngine) * 5.0f)
				+ shipUp * (distSpread(randomEngine) * 5.0f);
			sp.scale = distScale(randomEngine);
			sp.age = 0.0f;
			sp.maxLife = distLife(randomEngine);

			// 白核と紫/オレンジを混ぜ、加算合成でキラッと光らせる
			bool isWhite = (distWhite(randomEngine) < 0.35f);
			sp.color = isWhite
				? Vector4{ 1.0f, 1.0f, 1.0f, 0.68f }
				: Vector4{ outerGlowColor.x, outerGlowColor.y, outerGlowColor.z, 0.62f };
			sparks_.push_back(sp);
		}
	}

	sparkInstanceCount_ = 0;
	if (auto* sparkBuf = sparkRenderer_.GetInstancingData())
	{
		for (const auto& sp : sparks_)
		{
			if (sparkInstanceCount_ >= kMaxSparks) break;
			float ratio = std::clamp(sp.age / sp.maxLife, 0.0f, 1.0f);
			float fade = 1.0f - ratio;
			float sc = sp.scale * (0.5f + 0.5f * fade);

			Matrix4x4 s = Matrix4x4::Scale({ sc, sc, sc });
			Matrix4x4 t = Matrix4x4::Translation(sp.position);
			Matrix4x4 world = s * billboardMatrix * t;

			sparkBuf[sparkInstanceCount_].World = world;
			sparkBuf[sparkInstanceCount_].WVP = world * vpMatrix;
			sparkBuf[sparkInstanceCount_].color = { sp.color.x, sp.color.y, sp.color.z, sp.color.w * fade };
			++sparkInstanceCount_;
		}
	}

	// =========================================================
	// ④ ブースト開始時のハニカム（六角形）回転衝撃波リング
	// =========================================================
	if (isActive && isBoosting && !prevBoosting_)
	{
		// ブースト開始の瞬間に3枚の六角形リングを連続生成
		Vector3 emitCenter = (leftNozzlePos + rightNozzlePos) * 0.5f;
		for (int i = 0; i < 3; ++i)
		{
			if (rings_.size() >= kMaxRings) break;
			HoneycombRing ring;
			ring.position = emitCenter - shipForward * (0.20f + 0.35f * static_cast<float>(i));
			ring.velocity = shipForward * (-(3.5f + 2.0f * static_cast<float>(i)));
			ring.forward = shipForward;
			ring.right = shipRight;
			ring.up = shipUp;
			ring.startScale = 0.45f + 0.12f * static_cast<float>(i);
			ring.endScale = 2.10f + 0.35f * static_cast<float>(i);
			ring.rotZ = static_cast<float>(i) * 0.52f; // 約30度ずつ位相をずらす
			ring.rotSpeed = (i % 2 == 0) ? 5.0f : -4.2f;
			ring.delay = 0.05f * static_cast<float>(i);
			ring.age = 0.0f;
			ring.maxLife = 0.45f + 0.07f * static_cast<float>(i);
			ring.color = (i == 0)
				? Vector4{ 1.0f, 0.90f, 0.65f, 0.46f }  // 1枚目は白熱オレンジ
				: Vector4{ 1.0f, 0.48f, 0.10f, 0.40f }; // 2〜3枚目はオレンジ
			rings_.push_back(ring);
		}
	}
	prevBoosting_ = (isActive && isBoosting);

	for (auto it = rings_.begin(); it != rings_.end();)
	{
		if (it->delay > 0.0f)
		{
			it->delay -= dt;
			++it;
			continue;
		}
		it->age += dt;
		if (it->age >= it->maxLife)
		{
			it = rings_.erase(it);
			continue;
		}
		// 自機の前進に追従しつつゆっくり後方へ抜けることで、高速飛行中でもはっきり視認できるようにする
		it->position = it->position + shipDelta + it->velocity * dt;
		it->rotZ += it->rotSpeed * dt;
		++it;
	}

	ringInstanceCount_ = 0;
	if (auto* ringBuf = ringRenderer_.GetInstancingData())
	{
		for (const auto& r : rings_)
		{
			if (r.delay > 0.0f) continue;
			if (ringInstanceCount_ >= kMaxRings) break;

			float t = std::clamp(r.age / r.maxLife, 0.0f, 1.0f);
			float easeOut = 1.0f - (1.0f - t) * (1.0f - t);
			float sc = std::lerp(r.startScale, r.endScale, easeOut);
			float alpha = r.color.w * (1.0f - t);

			Matrix4x4 s = Matrix4x4::Scale({ sc, sc, sc });
			Matrix4x4 rot = Matrix4x4::RotateZ(r.rotZ);
			Matrix4x4 basis = MakeBasisMatrix(r.right, r.up, r.forward);
			Matrix4x4 trans = Matrix4x4::Translation(r.position);
			Matrix4x4 world = s * rot * basis * trans;

			ringBuf[ringInstanceCount_].World = world;
			ringBuf[ringInstanceCount_].WVP = world * vpMatrix;
			ringBuf[ringInstanceCount_].color = { r.color.x, r.color.y, r.color.z, alpha };
			++ringInstanceCount_;
		}
	}
}

void PlayerThrusterVfx::Draw()
{
	auto texMgr = TextureManager::GetInstance();
	if (!texMgr) return;

	uint32_t circleTex = texMgr->GetSrvIndex("circle2.png");
	uint32_t sparkTex = texMgr->GetSrvIndex("prticle_kira.png");

	// 1. ウェイクトレイル（外周グロー -> 中心白コア）
	if (drawTrails_)
	{
		if (trailGlowVertexCount_ > 0)
		{
			trailGlowRenderer_.Draw(1, circleTex, BlendMode::Additive);
		}
		if (trailCoreVertexCount_ > 0)
		{
			trailCoreRenderer_.Draw(1, circleTex, BlendMode::Additive);
		}
	}

	// 2. コアノズル（噴射口3D幾何学コーン）
	if (coneInstanceCount_ > 0)
	{
		coneRenderer_.Draw(coneInstanceCount_, circleTex, BlendMode::Additive);
	}

	// 3. ハニカム衝撃波リング
	if (ringInstanceCount_ > 0)
	{
		ringRenderer_.Draw(ringInstanceCount_, circleTex, BlendMode::Additive);
	}

	// 4. デジタルスパーク（極小ドット粒子）
	if (sparkInstanceCount_ > 0)
	{
		sparkRenderer_.Draw(sparkInstanceCount_, sparkTex, BlendMode::Additive);
	}
}
