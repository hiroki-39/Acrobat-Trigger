#pragma once
#include "KHEngine/Math/MathCommon.h"
#include <cstdint>
#include <string>

/**
 * @brief ブレンドモード定義
 */
enum class BlendMode
{
    None = 0,
    Alpha,
    Additive,
    Multiply,
    PreMultiplied,
    Count
};

/**
 * @brief 単一パーティクルのデータ
 */
struct Particle
{
    Transform transform;
    Vector3 initialScale; // OverLifetime計算用
    Vector3 velocity;
    Vector3 angularVelocity; // 回転速度（rad/sec）
    Vector4 initialColor; // OverLifetime計算用
    Vector4 color;
    float lifeTime;
    float currentTime;
};

/**
 * @brief GPUへ渡す定数バッファ用構造体
 */
struct ParticleForGPU
{
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector4 color;
};

/**
 * @brief パーティクル生成パラメータ
 * 様々なエフェクトの外見や挙動を決定します
 */
struct ParticleEmitterParameter
{
    std::string name;
    
    // 生成設定
    uint32_t count = 1;      // 一度に生成する数
    float frequency = 0.1f;  // 生成間隔（0なら一度きり）

    // 寿命範囲
    float minLifeTime = 1.0f;
    float maxLifeTime = 1.0f;

    // スケール範囲
    Vector3 minScale = { 1.0f, 1.0f, 1.0f };
    Vector3 maxScale = { 1.0f, 1.0f, 1.0f };

    // 速度範囲
    Vector3 minVelocity = { -1.0f, -1.0f, -1.0f };
    Vector3 maxVelocity = { 1.0f, 1.0f, 1.0f };

    // 回転範囲
    Vector3 minRotation = { 0.0f, 0.0f, 0.0f };
    Vector3 maxRotation = { 0.0f, 0.0f, 0.0f };

    // 回転速度範囲 (rad/sec)
    Vector3 minAngularVelocity = { 0.0f, 0.0f, 0.0f };
    Vector3 maxAngularVelocity = { 0.0f, 0.0f, 0.0f };

    // 色範囲 (開始色)
    Vector4 minColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector4 maxColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    // --- 新規拡張パラメータ ---
    // 寿命による変化 (Over Lifetime)
    bool isColorOverLifetime = true; // デフォルトでフェードアウトするようにtrue
    Vector4 endColor = { 1.0f, 1.0f, 1.0f, 0.0f }; // 終了時の色 (アルファ0で消える)

    bool isScaleOverLifetime = false;
    Vector3 endScale = { 0.0f, 0.0f, 0.0f }; // 終了時のスケール

    // 物理挙動 (Physics)
    bool useGravity = false;
    float gravity = -9.8f; // 下向きの重力
    float drag = 0.0f;     // 空気抵抗 (0.0で抵抗なし、値が大きいほど急ブレーキ)
};

/**
 * @brief 加速度フィールド（重力や風などの影響範囲）
 */
struct AccelerationField
{
    Vector3 acceleration;
    struct Area {
        Vector3 min;
        Vector3 max;
    } area;

    bool IsInArea(const Vector3& point) const {
        return (point.x >= area.min.x && point.x <= area.max.x &&
                point.y >= area.min.y && point.y <= area.max.y &&
                point.z >= area.min.z && point.z <= area.max.z);
    }
};
