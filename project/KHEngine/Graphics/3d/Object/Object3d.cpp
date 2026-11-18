#include "Object3d.h"
#include <numbers>
#include <cmath> 

static constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
static constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

void Object3d::Initialize(Object3dCommon* object3dCommon)
{
	// 引数で受け取ってメンバ変数に記録
	assert(object3dCommon != nullptr);
	this->object3dCommon = object3dCommon;

	// DirectXCommonを取得して保存
	this->dxCommon = object3dCommon->GetDirectXCommon();
	assert(this->dxCommon != nullptr);

	// 座標変換行列データの作成
	CreateTransformationMatrixResource();

	// 平行光源の作成
	CreateDirectionalLight();

	// ポイントライトの作成
	CreatePointLight();

	// スポットライトの作成
	CreateSpotLight();


	this->camera = object3dCommon->GetDefaultCamera();


	transform.translate = { 0.0f,0.0f,0.0f };
	transform.rotation = { 0.0f,0.0f,0.0f };
	transform.scale = { 1.0f,1.0f,1.0f };

	cameraTransform.translate = { 0.0f,0.0f,-15.0f };
	cameraTransform.rotation = { 0.0f,0.0f,0.0f };
	cameraTransform.scale = { 1.0f,1.0f,1.0f };
}

void Object3d::Update()
{
	Matrix4x4 worldMatrix = transform.GetWorldMatrix();
	if (parent_) {
		worldMatrix = Matrix4x4::Multiply(worldMatrix, parent_->GetmatWorld());
	}
	
	Camera* currentCamera = camera;
	if (object3dCommon && object3dCommon->GetDefaultCamera()) {
		currentCamera = object3dCommon->GetDefaultCamera();
	}

	Matrix4x4 worldViewProjectionMatrix;
	if (currentCamera)
	{
		const Matrix4x4& ViewProjectionMatrix = currentCamera->GetViewProjectionMatrix();
		worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, ViewProjectionMatrix);
	}
	else
	{
		worldViewProjectionMatrix = worldMatrix;
	}

	transformationMatrixData_->WVP = worldViewProjectionMatrix;
	transformationMatrixData_->World = worldMatrix;
	transformationMatrixData_->WorldInverseTranspose =
		Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrix));
        
    worldMatrix_ = worldMatrix;

	// 平行光源の向きの正規化
	directionalLightData_->direction = directionalLightData_->direction.Normalize();

	if (currentCamera && cameraData_)
	{
		Vector3 camPos = currentCamera->GetTranslate();
		cameraData_->worldPosition = camPos;
	}
}

void Object3d::Draw()
{
	//wvp用のCBufferの場所を設定
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());

	//平行光源用のCBufferの場所を設定
	dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResouerce_->GetGPUVirtualAddress());

	// カメラ用のCBufferの場所を設定
	if (cameraResource_)
	{
		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(4, cameraResource_->GetGPUVirtualAddress());
	}

	//ポイントライト用のCBufferの場所を設定
	if (pointLightResource_)
	{
		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(5, pointLightResource_->GetGPUVirtualAddress());
	}

	// スポットライト用のCBufferの場所を設定
	if (spotLightResource_)
	{
		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(6, spotLightResource_->GetGPUVirtualAddress());
	}

	// 環境マップのDescriptorTableを設定
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(7, environmentTextureIndex);

	//モデルの描画
	if (model)
	{
		D3D12_GPU_VIRTUAL_ADDRESS matAddr = (hasCustomMaterial_ && materialResource_) ? materialResource_->GetGPUVirtualAddress() : 0;
		model->Draw(matAddr);
	}
}

void Object3d::SetColor(const Vector4& color)
{
	if (!materialResource_ && dxCommon)
	{
		materialResource_ = dxCommon->CreateBufferResource(sizeof(Model::Material));
		materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
		if (model && model->GetMaterialData())
		{
			*materialData_ = *model->GetMaterialData();
		}
		else
		{
			materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
			materialData_->enableLighting = 1;
			materialData_->selectLightings = 2;
			materialData_->uvTransform = Matrix4x4::Identity();
			materialData_->shininess = 40.0f;
			materialData_->specularColor = { 1.0f, 1.0f, 1.0f };
			materialData_->environmentCoefficient = 0.0f;
			materialData_->fresnelF0 = 0.04f;
		}
	}
	if (materialData_)
	{
		materialData_->color = color;
		hasCustomMaterial_ = true;
	}
}

void Object3d::SetEnvironmentCoefficient(float coefficient)
{
	if (model) model->SetEnvironmentCoefficient(coefficient);
	if (materialData_)
	{
		materialData_->environmentCoefficient = coefficient;
	}
}

void Object3d::SetEnableLighting(bool enable)
{
	if (model) model->SetEnableLighting(enable);
	if (materialData_)
	{
		materialData_->enableLighting = enable ? 1 : 0;
	}
}

void Object3d::SetSelectLightings(int32_t v)
{
	if (!materialResource_ && dxCommon)
	{
		SetColor(GetColor());
	}
	if (materialData_)
	{
		materialData_->selectLightings = v;
		hasCustomMaterial_ = true;
	}
	else if (model)
	{
		model->SetSelectLightings(v);
	}
}

void Object3d::SetGlitchParams(float time, float intensity)
{
	if (!materialResource_ && dxCommon)
	{
		SetColor(GetColor());
	}
	if (materialData_)
	{
		materialData_->shininess = time;
		materialData_->fresnelF0 = intensity;
		hasCustomMaterial_ = true;
	}
}

int32_t Object3d::GetSelectLightings() const
{
	if (hasCustomMaterial_ && materialData_)
	{
		return materialData_->selectLightings;
	}
	return model ? model->GetSelectLightings() : 0;
}

Vector4 Object3d::GetColor() const
{
	if (materialData_)
	{
		return materialData_->color;
	}
	if (model && model->GetMaterialData())
	{
		return model->GetMaterialData()->color;
	}
	return { 1.0f, 1.0f, 1.0f, 1.0f };
}

void Object3d::SetModel(const std::string& filePath)
{
	// 指定ファイルが未ロードならモデルをロードする（安全策）
	if (ModelManager::GetInstance()->FindModel(filePath) == nullptr)
	{
		ModelManager::GetInstance()->LoadModel(filePath);
	}

	// モデルポインタを取得
	model = ModelManager::GetInstance()->FindModel(filePath);

	// デバッグ用にアサート（実運用ならログ出力に変更してもよい）
	assert(model != nullptr);

	// 既にカスタムマテリアルが設定されている場合、モデルのマテリアル設定（ライティング等）をベースに同期
	if (hasCustomMaterial_ && materialData_ && model && model->GetMaterialData())
	{
		Vector4 curColor = materialData_->color;
		int32_t curSelect = materialData_->selectLightings;
		*materialData_ = *model->GetMaterialData();
		materialData_->color = curColor; // 設定したカラーを保持
		materialData_->selectLightings = curSelect;
	}
}

void Object3d::CreateTransformationMatrixResource()
{
	//WVP用のリソースを作る
	transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));

	//書き込むためのアドレス取得
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	//単位行列を書き込む
	transformationMatrixData_->WVP = Matrix4x4::Identity();
	transformationMatrixData_->World = Matrix4x4::Identity();
	transformationMatrixData_->WorldInverseTranspose = Matrix4x4::Identity();

	// カメラ用CBufferを作成 
	cameraResource_ = dxCommon->CreateBufferResource(sizeof(CameraForGPU));
	cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));

	if (cameraData_)
	{
		cameraData_->worldPosition = Vector3{ 0.0f, 0.0f, 0.0f };
		cameraData_->padding = 0.0f;
	}
}

void Object3d::CreateDirectionalLight()
{
	//平行光源用のリソースを作成
	directionalLightResouerce_ = dxCommon->CreateBufferResource(sizeof(DirectionlLight));

	//書き込むためのアドレス取得
	directionalLightResouerce_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

	//ライトの色
	directionalLightData_->color = { 1.0f,1.0f,1.0f,1.0f };
	//向き
	directionalLightData_->direction = { -1.0f,-0.5f,0.5f };
	//輝度
	directionalLightData_->intensity = 1.0f;
}

void Object3d::CreatePointLight()
{
	//ポイントライト用のリソースを作成
	pointLightResource_ = dxCommon->CreateBufferResource(sizeof(PointLight));
	//書き込むためのアドレス取得
	pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));
	//ライトの色
	pointLightData_->color = { 1.0f,1.0f,1.0f,1.0f };
	//向き
	pointLightData_->direction = { 0.0f,2.0f,-5.0f };
	//輝度
	pointLightData_->intensity = 1.0f;
	// 光の届く範囲
	pointLightData_->radius = 10.0f;
	// 減衰率
	pointLightData_->decry = 1.0f;
}

void Object3d::CreateSpotLight()
{
	//スポットライト用のリソースを作成
	spotLightResource_ = dxCommon->CreateBufferResource(sizeof(SpotLight));
	//書き込むためのアドレス取得
	spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));

	spotLightData_->color = { 1.0f,1.0f,1.0f,1.0f };
	spotLightData_->position = { 2.0f,1.25f, 0.0f };
	spotLightData_->distance = 7.0f;
	spotLightData_->direction = { -1.0f,-1.0f,0.0f };
	spotLightData_->intensity = 4.0f;
	spotLightData_->decay = 2.0f;
	spotLightData_->cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
}

Vector4 Object3d::GetDirectionalLightColor() const
{
	if (directionalLightData_) return directionalLightData_->color;
	return Vector4{ 1.0f,1.0f,1.0f,1.0f };
}

Vector3 Object3d::GetDirectionalLightDirection() const
{
	if (directionalLightData_) return directionalLightData_->direction;
	return Vector3{ 1.0f,0.0f,0.0f };
}

float Object3d::GetDirectionalLightIntensity() const
{
	if (directionalLightData_) return directionalLightData_->intensity;
	return 1.0f;
}

void Object3d::SetDirectionalLightColor(const Vector4& color)
{
	if (directionalLightData_) directionalLightData_->color = color;
}

void Object3d::SetDirectionalLightDirection(const Vector3& direction)
{
	if (directionalLightData_) directionalLightData_->direction = direction;
}

void Object3d::SetDirectionalLightIntensity(float intensity)
{
	if (directionalLightData_) directionalLightData_->intensity = intensity;
}

void Object3d::SetPointLightColor(const Vector4& color)
{
	if (pointLightData_) pointLightData_->color = color;

	if (pointLightData_)
	{
		pointLightData_->color = color;
		// デバッグ出力
		char buf[128];
		sprintf_s(buf, "SetPointLightColor: (%f,%f,%f,%f)\n", color.x, color.y, color.z, color.w);
		OutputDebugStringA(buf);
	}
}

void Object3d::SetPointLightPosition(const Vector3& position)
{
	if (pointLightData_) pointLightData_->direction = position; // HLSL 側フィールド名に合わせる
}

void Object3d::SetPointLightIntensity(float intensity)
{
	if (pointLightData_) pointLightData_->intensity = intensity;
}

void Object3d::SetPointLightRadius(float radius)
{
	if (pointLightData_)
	{
		pointLightData_->radius = radius;
	}
}

void Object3d::SetPointLightDecry(float decry)
{
	if (pointLightData_)
	{
		pointLightData_->decry = decry;
	}
}

void Object3d::SetSpotLightColor(const Vector4& color)
{
	if (spotLightData_) spotLightData_->color = color;
}

void Object3d::SetSpotLightPosition(const Vector3& position)
{
	if (spotLightData_) spotLightData_->position = position;
}

void Object3d::SetSpotLightDirection(const Vector3& direction)
{
	if (!spotLightData_) return;
	// 正規化して格納
	float dx = direction.x;
	float dy = direction.y;
	float dz = direction.z;
	float len = std::sqrt(dx * dx + dy * dy + dz * dz);
	if (len > 1e-6f)
	{
		spotLightData_->direction = Vector3{ dx / len, dy / len, dz / len };
	}
}

void Object3d::SetSpotLightIntensity(float intensity)
{
	if (spotLightData_) spotLightData_->intensity = intensity;
}

void Object3d::SetSpotLightDistance(float distance)
{
	if (spotLightData_) spotLightData_->distance = distance;
}

void Object3d::SetSpotLightDecay(float decay)
{
	if (spotLightData_) spotLightData_->decay = decay;
}

void Object3d::SetSpotLightAngleDeg(float angleDeg)
{
	if (!spotLightData_) return;
	// clamp angle to sensible range [1, 90]
	if (angleDeg < 1.0f) angleDeg = 1.0f;
	if (angleDeg > 90.0f) angleDeg = 90.0f;
	spotLightData_->cosAngle = std::cos(angleDeg * kDegToRad);
}

Vector4 Object3d::GetPointLightColor() const
{
	if (pointLightData_) return pointLightData_->color;
	return Vector4{ 1.0f,1.0f,1.0f,1.0f };
}

Vector3 Object3d::GetPointLightPosition() const
{
	if (pointLightData_) return pointLightData_->direction; // HLSL 側フィールド名に合わせる
	return Vector3{ 0.0f, 0.0f, 0.0f };
}

float Object3d::GetPointLightIntensity() const
{
	if (pointLightData_) return pointLightData_->intensity;
	return 1.0f;
}

float Object3d::GetPointLightRadius() const
{
	if (pointLightData_)
	{
		return pointLightData_->radius;
	}

	return 0.0f;
}

float Object3d::GetPointLightDecry() const
{
	if (pointLightData_)
	{
		return pointLightData_->decry;
	}

	return 0.0f;
}

Vector4 Object3d::GetSpotLightColor() const
{
	if (spotLightData_) return spotLightData_->color;
	return Vector4{ 0.0f,0.0f,0.0f,1.0f };
}

Vector3 Object3d::GetSpotLightPosition() const
{
	if (spotLightData_) return spotLightData_->position;
	return Vector3{ 0.0f,0.0f,0.0f };
}

Vector3 Object3d::GetSpotLightDirection() const
{
	if (spotLightData_) return spotLightData_->direction;
	return Vector3{ 0.0f, -1.0f, 0.0f };
}

float Object3d::GetSpotLightIntensity() const
{
	if (spotLightData_) return spotLightData_->intensity;
	return 0.0f;
}

float Object3d::GetSpotLightDistance() const
{
	if (spotLightData_) return spotLightData_->distance;
	return 0.0f;
}

float Object3d::GetSpotLightDecay() const
{
	return 0.0f;
}

float Object3d::GetSpotLightAngleDeg() const
{
	return 0.0f;
}

void Object3d::EnsureTriangles() const {
	if (isTrianglesInitialized_) return;
	isTrianglesInitialized_ = true;

	if (model) {
		Matrix4x4 wMat = transform.GetWorldMatrix();
		if (parent_) {
			wMat = Matrix4x4::Multiply(wMat, parent_->GetmatWorld());
		}
		triangles_ = model->GetWorldTriangles(wMat);
		if (!triangles_.empty()) {
			hasMeshCollider_ = true;
			broadAABB_.min = { 1e9f, 1e9f, 1e9f };
			broadAABB_.max = { -1e9f, -1e9f, -1e9f };
			for (const auto& tri : triangles_) {
				for (const auto& p : { tri.p0, tri.p1, tri.p2 }) {
					broadAABB_.min.x = (std::min)(broadAABB_.min.x, p.x);
					broadAABB_.min.y = (std::min)(broadAABB_.min.y, p.y);
					broadAABB_.min.z = (std::min)(broadAABB_.min.z, p.z);
					broadAABB_.max.x = (std::max)(broadAABB_.max.x, p.x);
					broadAABB_.max.y = (std::max)(broadAABB_.max.y, p.y);
					broadAABB_.max.z = (std::max)(broadAABB_.max.z, p.z);
				}
			}
		}
	}
}

bool Object3d::CheckCollisionWithSphere(const Sphere& sphere, CollisionResult* outResult) const {
	if (!isCollisionEnabled_) return false;
	EnsureTriangles();
	if (!hasMeshCollider_) return false;

	if (!CollisionMath::IsCollision(sphere, broadAABB_)) {
		return false;
	}

	const float r = sphere.radius;
	const float sMinX = sphere.center.x - r;
	const float sMaxX = sphere.center.x + r;
	const float sMinY = sphere.center.y - r;
	const float sMaxY = sphere.center.y + r;
	const float sMinZ = sphere.center.z - r;
	const float sMaxZ = sphere.center.z + r;

	bool hitAny = false;
	CollisionResult bestResult;
	bestResult.penetrationDepth = -1.0f;

	for (const auto& tri : triangles_) {
		// 三角形のAABBと球のAABBを事前判定（高速距離カリング）
		float tMinX = (std::min)({ tri.p0.x, tri.p1.x, tri.p2.x });
		float tMaxX = (std::max)({ tri.p0.x, tri.p1.x, tri.p2.x });
		if (tMaxX < sMinX || tMinX > sMaxX) continue;

		float tMinY = (std::min)({ tri.p0.y, tri.p1.y, tri.p2.y });
		float tMaxY = (std::max)({ tri.p0.y, tri.p1.y, tri.p2.y });
		if (tMaxY < sMinY || tMinY > sMaxY) continue;

		float tMinZ = (std::min)({ tri.p0.z, tri.p1.z, tri.p2.z });
		float tMaxZ = (std::max)({ tri.p0.z, tri.p1.z, tri.p2.z });
		if (tMaxZ < sMinZ || tMinZ > sMaxZ) continue;

		CollisionResult res;
		if (CollisionMath::IsCollision(sphere, tri, &res)) {
			hitAny = true;
			if (res.penetrationDepth > bestResult.penetrationDepth) {
				bestResult = res;
			}
		}
	}

	if (hitAny) {
		if (outResult) *outResult = bestResult;
		return true;
	}
	return false;
}

bool Object3d::CheckCollisionWithOBB(const OBB& obb, CollisionResult* outResult, std::vector<Triangle>* outTestedTriangles, std::vector<Triangle>* outHitTriangles) const {
	if (!isCollisionEnabled_) return false;
	EnsureTriangles();
	if (!hasMeshCollider_) return false;

	if (!CollisionMath::IsCollision(obb, broadAABB_)) {
		return false;
	}

	// OBBを包含するAABB（外接AABB）を計算して三角形の高速カリングに利用
	float rx = obb.halfExtents.x * std::abs(obb.orientations[0].x) +
	           obb.halfExtents.y * std::abs(obb.orientations[1].x) +
	           obb.halfExtents.z * std::abs(obb.orientations[2].x);
	float ry = obb.halfExtents.x * std::abs(obb.orientations[0].y) +
	           obb.halfExtents.y * std::abs(obb.orientations[1].y) +
	           obb.halfExtents.z * std::abs(obb.orientations[2].y);
	float rz = obb.halfExtents.x * std::abs(obb.orientations[0].z) +
	           obb.halfExtents.y * std::abs(obb.orientations[1].z) +
	           obb.halfExtents.z * std::abs(obb.orientations[2].z);

	const float oMinX = obb.center.x - rx;
	const float oMaxX = obb.center.x + rx;
	const float oMinY = obb.center.y - ry;
	const float oMaxY = obb.center.y + ry;
	const float oMinZ = obb.center.z - rz;
	const float oMaxZ = obb.center.z + rz;

	bool hitAny = false;
	CollisionResult bestResult;
	bestResult.penetrationDepth = -1.0f;

	for (const auto& tri : triangles_) {
		// 三角形のAABBとOBB外接AABBの重なり判定（高速カリング）
		float tMinX = (std::min)({ tri.p0.x, tri.p1.x, tri.p2.x });
		float tMaxX = (std::max)({ tri.p0.x, tri.p1.x, tri.p2.x });
		if (tMaxX < oMinX || tMinX > oMaxX) continue;

		float tMinY = (std::min)({ tri.p0.y, tri.p1.y, tri.p2.y });
		float tMaxY = (std::max)({ tri.p0.y, tri.p1.y, tri.p2.y });
		if (tMaxY < oMinY || tMinY > oMaxY) continue;

		float tMinZ = (std::min)({ tri.p0.z, tri.p1.z, tri.p2.z });
		float tMaxZ = (std::max)({ tri.p0.z, tri.p1.z, tri.p2.z });
		if (tMaxZ < oMinZ || tMinZ > oMaxZ) continue;

		// 判定対象（Mid-Phase通過）三角形として記録
		if (outTestedTriangles) {
			outTestedTriangles->push_back(tri);
		}

		CollisionResult res;
		if (CollisionMath::IsCollision(obb, tri, &res)) {
			hitAny = true;
			if (outHitTriangles) {
				outHitTriangles->push_back(tri);
			}
			if (res.penetrationDepth > bestResult.penetrationDepth) {
				bestResult = res;
			}
		}
	}

	if (hitAny) {
		if (outResult) *outResult = bestResult;
		return true;
	}
	return false;
}

bool Object3d::RaycastDown(float x, float z, float startY, float* outGroundY, Vector3* outNormal) const {
	if (!isCollisionEnabled_) return false;
	EnsureTriangles();
	if (!hasMeshCollider_) return false;

	// AABB による XZ 範囲チェック（高速除外）
	if (x < broadAABB_.min.x || x > broadAABB_.max.x || z < broadAABB_.min.z || z > broadAABB_.max.z) {
		return false;
	}

	float highestY = -1e9f;
	Vector3 hitNormal = { 0.0f, 1.0f, 0.0f };
	bool hit = false;
	constexpr float EPS = 0.001f;

	for (const auto& tri : triangles_) {
		// 三角形のXZ境界チェック
		float minX = (std::min)({ tri.p0.x, tri.p1.x, tri.p2.x });
		float maxX = (std::max)({ tri.p0.x, tri.p1.x, tri.p2.x });
		if (x < minX - EPS || x > maxX + EPS) continue;

		float minZ = (std::min)({ tri.p0.z, tri.p1.z, tri.p2.z });
		float maxZ = (std::max)({ tri.p0.z, tri.p1.z, tri.p2.z });
		if (z < minZ - EPS || z > maxZ + EPS) continue;

		// XZ 平面上での 2D 三角形内外判定 (外積の符号チェック)
		float v0x = tri.p1.x - tri.p0.x, v0z = tri.p1.z - tri.p0.z;
		float v1x = tri.p2.x - tri.p1.x, v1z = tri.p2.z - tri.p1.z;
		float v2x = tri.p0.x - tri.p2.x, v2z = tri.p0.z - tri.p2.z;

		float c0 = (x - tri.p0.x) * v0z - (z - tri.p0.z) * v0x;
		float c1 = (x - tri.p1.x) * v1z - (z - tri.p1.z) * v1x;
		float c2 = (x - tri.p2.x) * v2z - (z - tri.p2.z) * v2x;

		if ((c0 >= -EPS && c1 >= -EPS && c2 >= -EPS) || (c0 <= EPS && c1 <= EPS && c2 <= EPS)) {
			// 平面方程式から Y を算出
			Vector3 n = tri.normal;
			if (std::abs(n.y) < 0.1f) {
				// 面が垂直に近い（急峻な崖・壁）場合は床として除外
				continue;
			}
			float curY = tri.p0.y - (n.x * (x - tri.p0.x) + n.z * (z - tri.p0.z)) / n.y;
			// startY より下で、かつこれまでの highestY より高い床を採用
			if (curY <= startY + 5.0f && curY > highestY) {
				highestY = curY;
				hitNormal = (n.y > 0.0f) ? n : Vector3(-n.x, -n.y, -n.z);
				hit = true;
			}
		}
	}

	if (hit) {
		if (outGroundY) *outGroundY = highestY;
		if (outNormal) *outNormal = hitNormal;
		return true;
	}
	return false;
}

bool Object3d::GetBoundingSphere(Vector3& outCenter, float& outRadius) const
{
	if (!model)
	{
		outCenter = { worldMatrix_.m[3][0], worldMatrix_.m[3][1], worldMatrix_.m[3][2] };
		outRadius = 1.0f;
		return false;
	}

	Vector3 localCenter = model->GetBoundingCenter();
	float localRadius = model->GetBoundingRadius();

	// ワールド座標へ変換 (行ベクトル v * M)
	outCenter.x = localCenter.x * worldMatrix_.m[0][0] + localCenter.y * worldMatrix_.m[1][0] + localCenter.z * worldMatrix_.m[2][0] + worldMatrix_.m[3][0];
	outCenter.y = localCenter.x * worldMatrix_.m[0][1] + localCenter.y * worldMatrix_.m[1][1] + localCenter.z * worldMatrix_.m[2][1] + worldMatrix_.m[3][1];
	outCenter.z = localCenter.x * worldMatrix_.m[0][2] + localCenter.y * worldMatrix_.m[1][2] + localCenter.z * worldMatrix_.m[2][2] + worldMatrix_.m[3][2];

	// 各軸のスケール長さを求めて最大値を掛ける
	float sx = std::sqrt(worldMatrix_.m[0][0] * worldMatrix_.m[0][0] + worldMatrix_.m[0][1] * worldMatrix_.m[0][1] + worldMatrix_.m[0][2] * worldMatrix_.m[0][2]);
	float sy = std::sqrt(worldMatrix_.m[1][0] * worldMatrix_.m[1][0] + worldMatrix_.m[1][1] * worldMatrix_.m[1][1] + worldMatrix_.m[1][2] * worldMatrix_.m[1][2]);
	float sz = std::sqrt(worldMatrix_.m[2][0] * worldMatrix_.m[2][0] + worldMatrix_.m[2][1] * worldMatrix_.m[2][1] + worldMatrix_.m[2][2] * worldMatrix_.m[2][2]);
	float maxScale = (std::max)({ sx, sy, sz });

	outRadius = localRadius * maxScale;
	return true;
}
