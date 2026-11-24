#pragma once
#include "KHEngine/Graphics/3d/Particle/ParticleRenderer.h"
#include "KHEngine/Graphics/3d/Particle/ParticleManager.h"
#include "KHEngine/Math/Vector3.h"
#include "KHEngine/Math/Vector4.h"
#include "KHEngine/Math/Matrix4x4.h"
#include <vector>
#include <array>
#include <deque>
#include <random>
#include <cstdint>

class DirectXCommon;
class SrvManager;

/// <summary>
/// プレイヤースラスター（バーナー）専用VFXクラス
/// ・① コアノズル（噴射口固定の幾何学3Dコーン：純白中心核＋青/オレンジ外周グロー）
/// ・② ウェイクトレイル（細い光の帯リボン＋バレルロール時の主翼端ベイパーライン）
/// ・③ デジタルスパーク（極小ドット粒子）
/// ・④ ブースト開始時のハニカム（六角形）回転衝撃波リング
/// </summary>
class PlayerThrusterVfx
{
public:
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Clear();

	/// <summary>
	/// 毎フレーム更新
	/// </summary>
	/// <param name="dt">経過時間（秒）</param>
	/// <param name="playerWorldMatrix">自機のワールド行列</param>
	/// <param name="viewMatrix">ビュー行列</param>
	/// <param name="projectionMatrix">プロジェクション行列</param>
	/// <param name="billboardMatrix">ビルボード回転行列</param>
	/// <param name="isActive">スラスター稼働中か（非稼働時は新規放出停止・ノズル非表示）</param>
	/// <param name="isBoosting">ブースト加速中か</param>
	/// <param name="isRolling">バレルロール中か</param>
	/// <param name="isWingVaporActive">主翼端ベイパーラインを発生させるか</param>
	/// <param name="randomEngine">乱数生成器</param>
	void Update(
		float dt,
		const Matrix4x4& playerWorldMatrix,
		const Matrix4x4& viewMatrix,
		const Matrix4x4& projectionMatrix,
		const Matrix4x4& billboardMatrix,
		bool isActive,
		bool isBoosting,
		bool isRolling,
		bool isWingVaporActive,
		std::mt19937& randomEngine,
		const std::vector<Vector3>* funnelPositions = nullptr,
		const std::vector<uint8_t>* funnelIsAttacking = nullptr);

	void Draw();

private:
	struct VertexData
	{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct TrailPoint
	{
		Vector3 position;
		float age = 0.0f;
		float maxLife = 0.25f;
	};

	struct DigitalSpark
	{
		Vector3 position;
		Vector3 velocity;
		Vector3 rotation = { 0.0f, 0.0f, 0.0f };
		Vector3 rotSpeed = { 6.0f, 8.0f, 5.0f };
		float scale = 0.03f;
		float age = 0.0f;
		float maxLife = 0.25f;
	};

	struct HoneycombRing
	{
		Vector3 position;
		Vector3 velocity;
		Vector3 forward;
		Vector3 right;
		Vector3 up;
		Vector4 color;
		float startScale = 0.35f;
		float endScale = 2.0f;
		float rotZ = 0.0f;
		float rotSpeed = 6.0f;
		float delay = 0.0f;
		float age = 0.0f;
		float maxLife = 0.35f;
	};

	void InitConeMesh();
	void InitTrailMesh();
	void InitSparkMesh();
	void InitHoneycombMesh();

	void UpdateTrailQueue(std::deque<TrailPoint>& trail, const Vector3& currentPos, float dt, float maxLife, bool emit);
	void BuildRibbonVertices(
		const std::deque<TrailPoint>& trail,
		const Vector3& cameraPos,
		float baseHalfWidth,
		std::vector<VertexData>& outVertices);

	static Vector3 TransformPoint(const Vector3& p, const Matrix4x4& m);
	static Vector3 ExtractNormalizedAxis(const Matrix4x4& m, int row);
	static Vector3 ExtractCameraPosition(const Matrix4x4& viewMatrix);

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// ① コアノズル（8角錐3Dコーン：内側純白コア＋外側カラーグロー × 左右）
	ParticleRenderer coneRenderer_;
	uint32_t coneInstanceCount_ = 0;
	float pulseTimer_ = 0.0f;
	float boostBlend_ = 0.0f; // 0.0=通常(青), 1.0=ブースト(オレンジ)

	// ② ウェイクトレイル（動的リボンメッシュ：中心白コア＋シアン＋マゼンタアウター）
	static constexpr uint32_t kMaxTrailPoints = 48;
	static constexpr uint32_t kMaxRibbonVertices = 4096;
	ParticleRenderer trailCoreRenderer_;
	ParticleRenderer trailGlowRenderer_;
	ParticleRenderer trailMagentaRenderer_;
	uint32_t trailCoreVertexCount_ = 0;
	uint32_t trailGlowVertexCount_ = 0;
	uint32_t trailMagentaVertexCount_ = 0;
	bool drawTrails_ = false;

	std::deque<TrailPoint> leftBurnerTrail_;
	std::deque<TrailPoint> rightBurnerTrail_;
	std::deque<TrailPoint> leftWingVaporTrail_;
	std::deque<TrailPoint> rightWingVaporTrail_;
	std::array<std::deque<TrailPoint>, 4> funnelVaporTrails_;
	std::array<bool, 4> funnelVaporAttacking_{};

	// ③ デジタルスパーク（極小ドット粒子）
	static constexpr uint32_t kMaxSparks = 128;
	ParticleRenderer sparkRenderer_;
	std::vector<DigitalSpark> sparks_;
	uint32_t sparkInstanceCount_ = 0;
	float sparkSpawnAccum_ = 0.0f;

	// ④ ブースト開始時サイバー破線衝撃波リング＆クロス閃光
	static constexpr uint32_t kMaxRings = 16;
	ParticleRenderer ringRenderer_;
	ParticleRenderer flashRenderer_;
	std::vector<HoneycombRing> rings_;
	uint32_t ringInstanceCount_ = 0;
	uint32_t flashInstanceCount_ = 0;

	bool prevBoosting_ = false;
	bool hasPrevPos_ = false;
	Vector3 prevShipPos_ = { 0.0f, 0.0f, 0.0f };
};
