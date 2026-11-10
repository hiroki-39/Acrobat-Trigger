#pragma once
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "Game/Actor/Weapon/MemoryType.h"
#include <memory>

class Player;

/// <summary>
/// 敵撃破時にドロップするメモリ（データチップ）オブジェクト
/// プレイヤーが接近するとマグネット吸引され、接触すると武器形態が変化する
/// </summary>
class MemoryItem {
public:
    void Initialize(Object3dCommon* object3dCommon, const Vector3& pos, MemoryType type = MemoryType::CYBER_BIT);
    void Update(float gameSpeed, Player* player);
    void Draw();

    bool IsDead() const { return isDead_; }
    MemoryType GetMemoryType() const { return memoryType_; }
    const Vector3& GetPosition() const { return position_; }

    // デバッグ・調整用グローバル設定
    static float s_ScaleMultiplier;
    static Vector4 s_CustomBitColor;

private:
    std::unique_ptr<Object3d> object_ = nullptr;
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 baseScale_ = { 0.065f, 0.065f, 0.065f }; // 視認性を高めた拡大スケール (約2.6倍)

    MemoryType memoryType_ = MemoryType::CYBER_BIT;
    Vector4 itemColor_ = { 1.0f, 0.65f, 0.12f, 1.0f }; // 地形（シアン/青）の補色となる高輝度エレクトリックオレンジ

    float animTimer_ = 0.0f;
    float lifeTimer_ = 600.0f; // 約10秒間滞留（回収されなければ自然消滅）
    bool isDead_ = false;

    // マグネット吸引処理
    bool isAttracting_ = false;
    float attractSpeed_ = 0.0f;
    const float magnetRadius_ = 35.0f; // マグネット吸引開始距離 (m)
    const float collectRadius_ = 2.5f;  // 回収完了接触距離 (m)
};
