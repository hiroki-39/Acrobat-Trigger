#pragma once
#include "KHEngine/Graphics/3d/Object/Object3dCommon.h"
#include "KHEngine/Math/MathCommon.h"
#include "KHEngine/Math/CollisionMath.h"
#include "KHEngine/Graphics/3d/Model/Model.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"

class Object3d
{
public:// 構造体

	struct TransformationMatrix
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
	};


	struct DirectionlLight
	{
		Vector4 color; // ライトの色
		Vector3 direction; //ライトの向き
		float intensity; //輝度
	};

	// GPU用カメラ構造体
	struct CameraForGPU
	{
		Vector3 worldPosition;
		float padding;
	};

	struct PointLight
	{
		Vector4 color;		// ライトの色
		Vector3 direction;  //ライトの向き
		float intensity;	//輝度
		float radius;		// 光の届く範囲
		float decry;		// 減衰率
	};

	struct SpotLight
	{
		Vector4 color;		// ライトの色
		Vector3 position;	// ライトの位置
		float intensity;	// 輝度
		Vector3 direction;	// ライトの向き
		float distance;		// 光の届く範囲
		float decay;		// 減衰率
		float cosAngle;		// コサイン値(スポットライトの角度)
		float padding[2];	// パディング
	};

public://メンバ関数

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(Object3dCommon* object3dCommon);

	/// <summary>
	/// 更新処理
	/// </summary>
	void Update();

	/// <summary>
	/// 描画処理
	/// </summary>
	void Draw();

	// --- Getter ---
	const Vector3& GetScale() const { return transform.scale; }
	const Vector3& GetRotation() const { return transform.rotation; }
	const Vector3& GetTranslate() const { return transform.translate; }
	const Matrix4x4& GetmatWorld() const { return worldMatrix_; }
	Vector4 GetDirectionalLightColor() const;
	Vector3 GetDirectionalLightDirection() const;
	float GetDirectionalLightIntensity() const;

	// PointLight getters
	Vector4 GetPointLightColor() const;
	Vector3 GetPointLightPosition() const;
	float GetPointLightIntensity() const;
	float GetPointLightRadius() const;
	float GetPointLightDecry() const;

	// SpotLight getters
	Vector4 GetSpotLightColor() const;
	Vector3 GetSpotLightPosition() const;
	Vector3 GetSpotLightDirection() const;
	float GetSpotLightIntensity() const;
	float GetSpotLightDistance() const;
	float GetSpotLightDecay() const;
	float GetSpotLightAngleDeg() const;

	// --- Setter ---
	void SetModel(Model* model) { this->model = model; }
	void SetModel(const std::string& filePath);
	void SetScale(const Vector3& scale) { transform.scale = scale; }
	void SetRotation(const Vector3& rotation) { transform.rotation = rotation; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }
	void SetCamera(Camera* camera) { this->camera = camera; }
	void SetDirectionalLightColor(const Vector4& color);
	void SetDirectionalLightDirection(const Vector3& direction);
	void SetDirectionalLightIntensity(float intensity);
	
	void SetParent(const Object3d* parent) { parent_ = parent; }

	// PointLight setters
	void SetPointLightColor(const Vector4& color);
	void SetPointLightPosition(const Vector3& position);
	void SetPointLightIntensity(float intensity);
	void SetPointLightRadius(float radius);
	void SetPointLightDecry(float decry);

	// SpotLight setters
	void SetSpotLightColor(const Vector4& color);
	void SetSpotLightPosition(const Vector3& position);
	void SetSpotLightDirection(const Vector3& direction);
	void SetSpotLightIntensity(float intensity);
	void SetSpotLightDistance(float distance);
	void SetSpotLightDecay(float decay);
	void SetSpotLightAngleDeg(float angleDeg);

	
	inline Model* GetModel() const { return model; }
	void SetEnvironmentTextureIndex(uint32_t index) { this->environmentTextureIndex = index; }
	void SetEnvironmentCoefficient(float coefficient);
	void SetEnableLighting(bool enable);
	void SetSelectLightings(int32_t v);
	int32_t GetSelectLightings() const;

	// インスタンス固有マテリアルカラー設定
	void SetColor(const Vector4& color);
	Vector4 GetColor() const;

	// メッシュコリジョン判定
	bool CheckCollisionWithSphere(const Sphere& sphere, CollisionResult* outResult = nullptr) const;
	bool CheckCollisionWithOBB(const OBB& obb, CollisionResult* outResult = nullptr, std::vector<Triangle>* outTestedTriangles = nullptr, std::vector<Triangle>* outHitTriangles = nullptr) const;
	bool RaycastDown(float x, float z, float startY, float* outGroundY, Vector3* outNormal = nullptr) const;
	bool HasMeshCollider() const { return hasMeshCollider_; }
	size_t GetMeshTriangleCount() const { EnsureTriangles(); return triangles_.size(); }
	void SetCollisionEnabled(bool enable) { isCollisionEnabled_ = enable; }
	bool IsCollisionEnabled() const { return isCollisionEnabled_; }
	void SetName(const std::string& name) { name_ = name; }
	const std::string& GetName() const { return name_; }
	void SetBackgroundMountain(bool enable) { isBackgroundMountain_ = enable; }
	bool IsBackgroundMountain() const { return isBackgroundMountain_; }

	// バウンディング球の取得（カリング用）
	bool GetBoundingSphere(Vector3& outCenter, float& outRadius) const;

private://メンバ関数

	/// <summary>
	/// 座標変換行列データの作成
	///	</summary>
	void CreateTransformationMatrixResource();

	/// <summary>
	/// 平行光源の作成
	/// </summary>
	void CreateDirectionalLight();

	/// <summary>
	/// ポイントライトの作成
	/// </summary>
	void CreatePointLight();

	/// <summary>
	/// スポットライトの作成
	/// </summary>
	void CreateSpotLight();

private://メンバ変数

	// 環境マップ
	uint32_t environmentTextureIndex = 0;

	// 共通部分
	Object3dCommon* object3dCommon = nullptr;

	// DirectXCommon取得
	DirectXCommon* dxCommon = nullptr;

	WinApp* winApp_ = nullptr;

	Model* model = nullptr;

	Camera* camera = nullptr;
	
	const Object3d* parent_ = nullptr;

	// 変換行列リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	// 変換行列データの仮想アドレス
	TransformationMatrix* transformationMatrixData_ = nullptr;


	//平行光源用のリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResouerce_;
	//データを書き込む
	DirectionlLight* directionalLightData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	CameraForGPU* cameraData_ = nullptr;

	// ポイントライト用のリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
	// データを書き込む
	PointLight* pointLightData_ = nullptr;

	// スポットライト用のリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
	// データを書き込む
	SpotLight* spotLightData_ = nullptr;

	// インスタンス固有マテリアル用のリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Model::Material* materialData_ = nullptr;
	bool hasCustomMaterial_ = false;

	Transform transform;
	Matrix4x4 worldMatrix_;

	Transform cameraTransform;

	// メッシュコリジョン用キャッシュデータ
	mutable std::vector<Triangle> triangles_;
	mutable AABB broadAABB_;
	mutable bool hasMeshCollider_ = false;
	mutable bool isTrianglesInitialized_ = false;
	bool isCollisionEnabled_ = true;
	std::string name_;
	bool isBackgroundMountain_ = false;
	void EnsureTriangles() const;
};