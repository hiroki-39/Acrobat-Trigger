#pragma once
#include "KHEngine/Graphics/3d/Particle/ParticleEffect.h"
#include "Game/System/PlayerThrusterVfx.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Math/Vector3.h"
#include "KHEngine/Math/Vector4.h"
#include "KHEngine/Math/Matrix4x4.h"
#include <vector>
#include <memory>
#include <random>

class DirectXCommon;
class SrvManager;
class Object3dCommon;
class Camera;

/// <summary>
/// ゲームプレイ中のパーティクルエフェクト群およびボクセル着弾粒子を管理するクラス
/// </summary>
class GamePlayVfxManager
{
public:
	/// <summary>
	/// 全パーティクルエフェクトの初期化とJSON読み込み
	/// </summary>
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	/// <summary>
	/// 全エフェクトおよびボクセル粒子の消去（リセット・リトライ時用）
	/// </summary>
	void ClearAll();

	/// <summary>
	/// ミサイル着弾時のサイバーボクセルブロック粒子を生成
	/// </summary>
	void SpawnVoxelHitParticles(const Vector3& worldPos, Camera* activeCamera, int count = 28, float minSize = 1.2f, float maxSize = 2.5f);

	/// <summary>
	/// プレイヤー弾ヒット専用の極小・細長マイクロチップ型ボクセル粒子を生成（他エフェクトと重複・肥大化しない）
	/// </summary>
	void SpawnBulletHitVoxelParticles(const Vector3& worldPos, const Vector3& bulletVel);

	/// <summary>
	/// 通常敵撃破時：ボス撃破ディゾルブと同系色の上昇ボクセル粒子を生成
	/// </summary>
	void SpawnEnemyDissolveShards(const Vector3& centerPos);

	/// <summary>
	/// メモリ出現・回収時のサイバーボクセル粒子バーストを生成
	/// </summary>
	void SpawnMemoryBurstParticles(const Vector3& centerPos, const Vector4& color, int count = 14, float speedScale = 1.0f);

	/// <summary>
	/// メモリ吸引中のデータ光トレイル粒子を生成
	/// </summary>
	void SpawnMemoryTrailParticle(const Vector3& pos, const Vector4& color);

	/// <summary>
	/// メモリドロップ浮遊中に周囲で下から上へ昇るボクセルブロックを生成
	/// </summary>
	void SpawnMemoryAmbientVoxel(const Vector3& centerPos, const Vector4& color, float modelRadius);

	/// <summary>
	/// 強化リング・回復リング通過時のサイバーボクセル＆リングスパークを生成
	/// </summary>
	void SpawnRingPassParticles(const Vector3& centerPos, bool isHeal);

	/// <summary>
	/// スラスター・ウィンド以外のエフェクトおよびボクセル粒子の更新
	/// </summary>
	void UpdateEffects(float dt, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const Matrix4x4& billboardMatrix);

	/// <summary>
	/// 自機スラスター・ウィンド・全VFXの毎フレーム更新
	/// </summary>
	void UpdatePlayerVfx(float dt, class Player* player, Camera* activeCamera, bool isPlaying, bool isPaused, bool isStartCutscene, bool isPlayingPhase, bool isGameOver, bool isClear, float cutsceneTimer, bool isThrusterReady, float gameSpeed, std::mt19937& randomEngine);

	/// <summary>
	/// プレイヤー被弾時の3D三角形プリズム＆デジタルスパーク粒子を生成
	/// </summary>
	void SpawnPlayerDamageParticles(const Vector3& hitPos);

	/// <summary>
	/// ブースト時のRadialBlur・ジャスト回避時のVignette/Grayscale・サイバーパンク被弾＆低HPポストプロセスを更新
	/// </summary>
	void UpdatePostProcessEffects(class PostProcess* pp, bool isBoosting, bool& isJustDodgeActive, float& justDodgeTimer, float justDodgeMaxTime, float damagePulse = 0.0f, float hpRatio = 1.0f, float dt = 1.0f / 60.0f);

	/// <summary>
	/// シーン終了時のポストプロセスエフェクトリセット
	/// </summary>
	void ResetPostProcess(class PostProcess* pp);

	/// <summary>
	/// 全エフェクトおよびボクセル粒子の描画
	/// </summary>
	void Draw(bool drawThruster, bool drawWind, Object3dCommon* object3dCommon);

	ParticleEffect& Thruster() { return thrusterEffect_; }
	ParticleEffect& Explosion() { return explosionEffect_; }
	ParticleEffect& Hit() { return hitEffect_; }
	ParticleEffect& Dodge() { return dodgeEffect_; }
	ParticleEffect& Trail() { return trailEffect_; }
	ParticleEffect& MissileSmoke() { return missileSmokeEffect_; }
	ParticleEffect& Ring() { return ringEffect_; }
	ParticleEffect& HealRing() { return healRingEffect_; }
	ParticleEffect& Wind() { return windEffect_; }

private:
	struct VoxelHitParticle
	{
		std::unique_ptr<Object3d> obj;
		Vector3 position;
		Vector3 velocity;
		Vector3 rotation;
		Vector3 rotSpeed;
		Vector3 scale;
		Vector4 color;
		float life = 0.0f;
		float maxLife = 0.6f;
	};

	ParticleEffect thrusterEffect_;
	PlayerThrusterVfx playerThrusterVfx_;
	ParticleEffect explosionEffect_;
	ParticleEffect hitEffect_;
	ParticleEffect dodgeEffect_;
	ParticleEffect trailEffect_;
	ParticleEffect missileSmokeEffect_;
	ParticleEffect ringEffect_;
	ParticleEffect healRingEffect_;
	ParticleEffect windEffect_;

	std::vector<VoxelHitParticle> voxelHitParticles_;

	// プレイヤー被弾時 3D三角形プリズム＆横長デジタルスパーク
	struct DamagePrismParticle
	{
		Vector3 position;
		Vector3 velocity;
		Vector3 rotation;
		Vector3 rotSpeed;
		float scale = 0.18f;
		float life = 0.0f;
		float maxLife = 0.30f;
	};
	struct DamageLineSpark
	{
		Vector3 position;
		Vector3 velocity;
		Vector3 startScale;
		Vector3 endScale;
		float life = 0.0f;
		float maxLife = 0.22f;
	};
	static constexpr uint32_t kMaxDamagePrisms = 64;
	static constexpr uint32_t kMaxDamageSparks = 32;
	ParticleRenderer damagePrismRenderer_;
	ParticleRenderer damageSparkRenderer_;
	std::vector<DamagePrismParticle> damagePrisms_;
	std::vector<DamageLineSpark> damageSparks_;
	uint32_t damagePrismCount_ = 0;
	uint32_t damageSparkCount_ = 0;
	float lowHpPulsePhase_ = 0.0f;
};
