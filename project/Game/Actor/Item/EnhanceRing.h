#pragma once
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <memory>
#include <string>
#include "KHEngine/Scene/LevelLoader.h"
#include "KHEngine/Math/CollisionMath.h"

class Player;

enum class RingType {
    POWER_UP,
    HEAL
};

class EnhanceRing {
public:
    void Initialize(Object3dCommon* object3dCommon, const Vector3& pos, const Vector3& scale, const Vector3& rotation, const std::string& fileName, RingType type, uint32_t skyboxTexIndex, const LevelCollider& colliderInfo);
    
    void Update();
    void Draw();
    void DrawCollider();

    void StartShrink(Player* player);
    bool IsDead() const { return isDead_; }

    // プレイヤーがくぐったかどうかを判定する (コライダーベースの判定)
    bool CheckCollision(Player* player);

    RingType GetType() const { return type_; }
    const Vector3& GetPosition() const { return position_; }

private:
    std::unique_ptr<Object3d> object_;      // ring_core（強化=赤 / 回復=緑）
    std::unique_ptr<Object3d> ring1Object_; // ring_1（共通=白、回転パーツ）
    std::unique_ptr<Object3d> colliderObject_;
    Vector3 position_;
    Vector3 rotation_;
    Vector3 baseScale_;
    RingType type_;
    LevelCollider collider_;

    float animTimer_ = 0.0f;
    float ring1RotZ_ = 0.0f;
    bool isDead_ = false;
    bool isShrinking_ = false;
    float shrinkScale_ = 1.0f;
    float radius_ = 1.0f; 
    Player* targetPlayer_ = nullptr;
};
