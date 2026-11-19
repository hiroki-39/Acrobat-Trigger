#pragma once
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Math/Vector3.h"
#include "KHEngine/Math/Vector4.h"
#include <vector>
#include <list>
#include <memory>

class Player;
class Camera;
class RailCameraController;
class Object3dCommon;
class ParticleEffect;
class Obstacle;
class BossFinisherSequence;

/// <summary>
/// オープニングカットシーン（地形形成・自機復元マテリアライズ・発進演出）に必要な外部参照
/// </summary>
struct OpeningCutsceneContext
{
	Player* player = nullptr;
	Camera* activeCamera = nullptr;
	RailCameraController* railCameraController = nullptr;
	std::vector<std::unique_ptr<Object3d>>* modelInstances = nullptr;
	std::list<std::unique_ptr<Obstacle>>* obstacles = nullptr;
	ParticleEffect* thrusterEffect = nullptr;
	ParticleEffect* windEffect = nullptr;
	BossFinisherSequence* bossFinisherSequence = nullptr;
	float* terrainMaterializeProgress = nullptr;
	float* cameraShakeTimer = nullptr;
};

/// <summary>
/// ゲーム開始時のオープニングカットシーン演出を管理するクラス
/// </summary>
class OpeningCutsceneSequence
{
public:
	static constexpr float kCutsceneDuration = 4.7f;

	/// <summary>
	/// 演出開始時の初期化
	/// </summary>
	void Start(const OpeningCutsceneContext& ctx);

	/// <summary>
	/// 演出の毎フレーム更新（演出が完了したら true を返す）
	/// </summary>
	bool Update(float dt, const OpeningCutsceneContext& ctx);

	/// <summary>
	/// 自機復元ポリゴン破片の3D描画
	/// </summary>
	void Draw3D(Camera* activeCamera, Object3dCommon* object3dCommon);

	/// <summary>
	/// カットシーン用UI（シネマティック黒帯・システム起動テキスト等）の描画
	/// </summary>
	void DrawUI(bool isCutscenePhase, bool isPlayingPhase);

	/// <summary>
	/// プレイ開始後の MISSION START 表示タイマーを減算
	/// </summary>
	void TickMissionStartText(float dt);

	/// <summary>
	/// 破片オブジェクトをクリア
	/// </summary>
	void ClearShards() { reconstructShards_.clear(); }

	/// <summary>
	/// 演出状態を即座に完了状態へリセット（デバッグ・テスト用）
	/// </summary>
	void ForceComplete(float& terrainMaterializeProgress);

	float GetTimer() const { return cutsceneTimer_; }
	float GetDuration() const { return kCutsceneDuration; }
	float GetPlayerReconstructAlpha() const { return playerReconstructAlpha_; }
	bool IsThrusterReady() const { return cutsceneTimer_ >= 3.05f; }

private:
	struct ReconstructShard
	{
		std::unique_ptr<Object3d> object = nullptr;
		Vector3 startPos = { 0.0f, 0.0f, 0.0f };
		Vector3 targetPos = { 0.0f, 0.0f, 0.0f };
		Vector3 rotation = { 0.0f, 0.0f, 0.0f };
		Vector3 rotSpeed = { 0.0f, 0.0f, 0.0f };
		Vector3 scale = { 0.20f, 0.20f, 0.20f };
		Vector4 color = { 0.3f, 1.0f, 0.9f, 1.0f };
	};

	std::vector<ReconstructShard> reconstructShards_;
	float cutsceneTimer_ = 0.0f;
	float missionStartTextTimer_ = 0.0f;
	float playerReconstructAlpha_ = 1.0f;
};
