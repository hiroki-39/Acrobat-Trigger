#pragma once
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <memory>

class Enemy;

class PlayerBullet {
public:
    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(Object3dCommon* object3dCommon, const Vector3& position, const Vector3& velocity, Object3d* parent, Enemy* targetEnemy = nullptr);

    /// <summary>
    /// 更新
    /// </summary>
    void Update(float gameSpeed = 1.0f);

    void Update3DObjectOnly() {
        if (object_) object_->Update();
        if (trailObject_) trailObject_->Update();
    }

    /// <summary>
    /// 描画
    /// </summary>
    void Draw();

    void DrawCollider();

    // デッドフラグ
    bool IsDead() const { return isDead_; }

    const Vector3& GetPosition() const { return object_ ? object_->GetTranslate() : velocity_; }
    const Vector3& GetPreviousPosition() const { return previousPosition_; }
    void OnCollision() { isDead_ = true; }

private:
    std::unique_ptr<Object3d> object_ = nullptr; // 高エネルギーレーザービーム本体
    std::unique_ptr<Object3d> trailObject_ = nullptr; // 光の尾（ビームトレイル）
    std::unique_ptr<Object3d> colliderObject_ = nullptr;
    Vector3 velocity_ = { 0.0f, 0.0f, 1.5f }; // 速度ベクトル
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f }; // 進行方向への向き
    Vector3 previousPosition_ = { 0.0f, 0.0f, 0.0f };
    bool isDead_ = false;
    float deathTimer_ = 180.0f; // 寿命（フレーム）
    Enemy* targetEnemy_ = nullptr;
};
