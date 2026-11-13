#pragma once
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <memory>
#include <string>
#include "KHEngine/Scene/LevelLoader.h"
#include "KHEngine/Math/CollisionMath.h"

class Obstacle {
public:
    void Initialize(Object3dCommon* object3dCommon, const Vector3& pos, const Vector3& scale, const Vector3& rotation, const std::string& fileName, uint32_t skyboxTexIndex, const LevelCollider& colliderInfo, bool isDestructible = true);
    
    
    void Update();
    

    
    void Update3DObjectOnly() {
        if (object_) object_->Update();
        if (colliderObject_) colliderObject_->Update();
    }
    
    void Draw();
    void DrawCollider(); 
    void OnCollision(float damage = 1.0f); 
    void Kill(); 
    void StartShrink(); // リング用の縮小アニメーション開始

    void SetSpawnProgress(float progress) { spawnProgress_ = progress; }
    void SetTexturePath(const std::string& path);

    
    const Vector3& GetPosition() const { return position_; }
    const LevelCollider& GetCollider() const { return collider_; }
    bool IsDead() const { return isDead_; }
    Object3d* GetObject3d() const { return object_.get(); }

    
    // プレイヤー（球またはOBB）との衝突判定（法線・めり込み深さを取得）
    bool CheckCollisionWithSphere(const Sphere& sphere, CollisionResult* outResult) const;
    bool CheckCollisionWithOBB(const OBB& obb, CollisionResult* outResult, std::vector<Triangle>* outTestedTriangles = nullptr, std::vector<Triangle>* outHitTriangles = nullptr) const;

    bool CheckCollision(const Sphere& bulletSphere) const;
    bool CheckRaycast(const Ray& ray, float* outDist) const;

    bool HasMeshCollider() const { return hasMeshCollider_; }
    size_t GetMeshTriangleCount() const { EnsureTriangles(); return triangles_.size(); }

private:
    std::unique_ptr<Object3d> object_;
    std::unique_ptr<Object3d> colliderObject_; 
    Vector3 position_;
    Vector3 rotation_;
    LevelCollider collider_; 
    bool isDead_ = false;
    bool isVisible_ = true;
    bool isDestructible_ = true;
    bool isShrinking_ = false;
    float shrinkScale_ = 1.0f;
    Vector3 baseScale_;
    
    float spawnProgress_ = 0.0f;
    std::string texturePath_;
    
    // メッシュコリジョン用データ
    mutable std::vector<Triangle> triangles_;
    mutable AABB broadAABB_;
    mutable bool hasMeshCollider_ = false;
    mutable bool isTrianglesInitialized_ = false;

    void EnsureTriangles() const;
};
