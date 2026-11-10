#pragma once
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <memory>
#include <string>
#include "KHEngine/Scene/LevelLoader.h"
#include "KHEngine/Math/CollisionMath.h"
#include "Game/System/Rail.h"
#include <list>

class Player;
class EnemyBullet;

class Enemy {
public:
    void Initialize(Object3dCommon* object3dCommon, const LevelObjectData& nodeData, uint32_t skyboxTexIndex);
    void Update(const Vector3& cameraPos, const Vector3& cameraForward, Player* player, std::list<std::unique_ptr<EnemyBullet>>& enemyBullets, float gameSpeed = 1.0f);
    void Update3DObjectOnly() {
        if (object_) object_->Update();
        if (colliderObject_) colliderObject_->Update();
    }
    void Draw();
    void DrawCollider(); // デバッグ描画用
    void OnCollision(float damage = 1.0f); // 弾が当たった時の処理
    void Kill(); // 即座に倒す

    void SetMovePath(std::unique_ptr<Rail> path);

    void SetSpawnProgress(float progress) { spawnProgress_ = progress; }
    void SetSpawnDelay(float delay) { spawnDelay_ = delay; }
    void SetTexturePath(const std::string& path);

    // Getter / Setter
    const Vector3& GetPosition() const { return position_; }
    Vector3 GetVisualPosition() const {
        return { position_.x + modelPosOffset_.x, position_.y + modelPosOffset_.y, position_.z + modelPosOffset_.z };
    }
    Vector3 GetColliderCenter() const {
        return { position_.x + modelPosOffset_.x + collider_.center.x, position_.y + modelPosOffset_.y + collider_.center.y, position_.z + modelPosOffset_.z + collider_.center.z };
    }
    const LevelCollider& GetCollider() const { return collider_; }
    bool IsDead() const { return isDead_; }
    bool IsActive() const { return isActive_; }
    const Rail* GetMovePath() const { return movePath_.get(); }
    float GetSpawnProgress() const { return spawnProgress_; }
    void SetRailProgress(float progress) { railProgress_ = progress; }
    float GetRailProgress() const { return (railProgress_ >= 0.0f) ? railProgress_ : spawnProgress_; }
    const Vector3& GetSpawnPos() const { return spawnPos_; }
    float GetSpawnDist() const { return spawnDist_; }
    const std::string& GetTypeName() const { return typeName_; }

    // 衝突判定用メソッド
    bool CheckCollision(const Sphere& bulletSphere) const;
    bool CheckRaycast(const Ray& ray, float* outDist) const;

private:
    std::unique_ptr<Object3d> object_;
    std::unique_ptr<Object3d> colliderObject_; // デバッグ描画用オブジェクト
    Object3dCommon* object3dCommon_ = nullptr; // 弾の発射用
    Vector3 spawnPos_; // 初期配置座標
    Vector3 position_;
    Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
    LevelCollider collider_; // コライダー情報
    float hp_ = 3.0f;
    bool isDead_ = false;
    std::string typeName_;

    // パス移動用
    std::unique_ptr<Rail> movePath_;
    float pathProgress_ = 0.0f;

    bool isActive_ = false;
    float spawnProgress_ = 0.0f;
    float railProgress_ = -1.0f;
    std::string texturePath_;
    bool isAutoAI_ = false;
    Vector3 aiOffset_ = {0.0f, 0.0f, 0.0f};

    // 拡張AI用プロパティ（新規追加分）
    std::string behavior_ = "STRAIGHT"; // 行動パターン
    float moveSpeed_ = 1.0f;            // 移動速度
    float moveAmplitude_ = 10.0f;       // 移動幅（往復の振幅）
    float shootInterval_ = 180.0f;      // 射撃間隔
    float spawnDist_ = 800.0f;          // 出現距離
    float spawnDelay_ = 0.0f;                // スポナーで生成された時の遅延（フレーム数）
    float activeTimer_ = 0.0f;               // アクティブになってからの経過時間
    bool isHomingBullet_ = false;       // 弾のホーミング有無
    float bulletSpeed_ = 2.0f;          // 弾速

    // 拡張AI用プロパティ（既存）
    Vector3 targetPos_;
    bool hasHeightLimit_ = false;
    float maxY_ = 1000.0f;
    float minY_ = -1000.0f;
    int formationId_ = -1;
    float invincibilityTimer_ = 0.0f;
    float attackTimer_ = 0.0f;
    Vector3 dashVelocity_ = {0.0f, 0.0f, 0.0f};
    Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 rotation_ = {0.0f, 3.14159265f, 0.0f};
    Vector3 modelPosOffset_ = {0.0f, 0.0f, 0.0f};
};
