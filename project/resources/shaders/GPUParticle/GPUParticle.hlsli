#pragma once

#define float32_t float
#define float32_t2 float2
#define float32_t3 float3
#define float32_t4 float4
#define float32_t3x3 float3x3
#define float32_t4x4 float4x4
#define uint32_t uint
#define int32_t int

static const uint32_t kMaxParticles = 10240;

// パーティクル構造体 (64バイトアライメント考慮)
struct Particle
{
    float32_t3 translate;
    float32_t pad0;
    float32_t3 scale;
    float32_t lifeTime;
    float32_t3 velocity;
    float32_t currentTime;
    float32_t4 color;
};

// エミッターパラメータ (CPU -> GPU)
struct GPUEmitterData
{
    float32_t3 translate;
    float32_t radius;
    
    float32_t3 baseVelocity;
    float32_t speed;
    
    float32_t4 startColor;
    float32_t4 endColor;
    
    float32_t startScale;
    float32_t endScale;
    float32_t lifeTime;
    float32_t spread;
    
    float32_t gravity;
    uint32_t count;
    uint32_t emit;
    uint32_t patternType; // 0: 球状散乱, 1: キューブ/ボクセル崩壊, 2: 直線ストリーム
};

// フレーム情報
struct PerFrameCS
{
    float32_t deltaTime;
    float32_t time;
    uint32_t randomSeed;
    float32_t pad;
};

// 描画用ビュー定数バッファ
struct PerView
{
    float32_t4x4 viewProjection;
    float32_t4x4 billboardMatrix;
};

// 疑似乱数生成ヘルパー関数
inline float32_t rand1dTo1d(float32_t3 value, float32_t3 dotDir)
{
    float32_t3 smallValue = sin(value);
    float32_t random = dot(smallValue, dotDir);
    random = frac(sin(random) * 143758.5453);
    return random;
}

inline float32_t3 rand3dTo3d(float32_t3 value)
{
    return float32_t3(
        rand1dTo1d(value, float32_t3(12.9898, 78.233, 37.719)),
        rand1dTo1d(value, float32_t3(39.346, 11.135, 83.155)),
        rand1dTo1d(value, float32_t3(73.156, 52.235, 9.151))
    );
}

// 疑似乱数生成クラス
struct RandomGenerator
{
    float32_t3 seed;

    void Init(float32_t3 initialSeed)
    {
        seed = initialSeed;
    }

    float32_t3 Generate3d()
    {
        seed = rand3dTo3d(seed);
        return seed;
    }

    float32_t Generate1d()
    {
        float32_t res = rand1dTo1d(seed, float32_t3(12.9898, 78.233, 37.719));
        seed.x = res;
        return res;
    }

    // -1.0 ~ 1.0 の範囲の3Dベクトル
    float32_t3 GenerateBipolar3d()
    {
        float32_t3 val = Generate3d();
        return val * 2.0f - 1.0f;
    }
};
