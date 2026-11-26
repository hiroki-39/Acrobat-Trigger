#pragma once
#include "KHEngine/Math/Vector3.h"

/// <summary>
/// 全ての弾・ミサイルアクターの共通基底クラス
/// </summary>
class BaseBullet {
public:
    virtual ~BaseBullet() = default;

    virtual void Update(float gameSpeed = 1.0f) = 0;
    virtual void Update3DObjectOnly() = 0;
    virtual void Draw() = 0;
    virtual void DrawCollider() = 0;
    virtual void OnCollision() = 0;

    virtual bool IsDead() const = 0;
    virtual const Vector3& GetPosition() const = 0;
    virtual const Vector3& GetPreviousPosition() const = 0;
};
