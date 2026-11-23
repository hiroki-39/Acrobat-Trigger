#pragma once
#include "KHEngine/Graphics/3d/Particle/ParticleEffect.h"
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
	/// スラスター・ウィンド以外のエフェクトおよびボクセル粒子の更新
	/// </summary>
	void UpdateEffects(float dt, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const Matrix4x4& billboardMatrix);

	/// <summary>
	/// 自機スラスター・ウィンド・全VFXの毎フレーム更新
	/// </summary>
	void UpdatePlayerVfx(float dt, class Player* player, Camera* activeCamera, bool isPlaying, bool isPaused, bool isStartCutscene, bool isPlayingPhase, bool isGameOver, bool isClear, float cutsceneTimer, bool isThrusterReady, float gameSpeed, std::mt19937& randomEngine);

	/// <summary>
	/// ブースト時のRadialBlur・ジャスト回避時のVignette/Grayscaleポストプロセス演出を更新
	/// </summary>
	void UpdatePostProcessEffects(class PostProcess* pp, bool isBoosting, bool& isJustDodgeActive, float& justDodgeTimer, float justDodgeMaxTime);

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
	ParticleEffect explosionEffect_;
	ParticleEffect hitEffect_;
	ParticleEffect dodgeEffect_;
	ParticleEffect trailEffect_;
	ParticleEffect missileSmokeEffect_;
	ParticleEffect ringEffect_;
	ParticleEffect healRingEffect_;
	ParticleEffect windEffect_;

	std::vector<VoxelHitParticle> voxelHitParticles_;
};
