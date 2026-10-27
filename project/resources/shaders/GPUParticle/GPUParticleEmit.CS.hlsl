#include "GPUParticle.hlsli"

ConstantBuffer<GPUEmitterData> gEmitter : register(b0);
ConstantBuffer<PerFrameCS> gPerFrame : register(b1);

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

[numthreads(256, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    // エミット要求がない、またはスレッド番号が発生指定数以上の場合は終了
    if (gEmitter.emit == 0 || DTid.x >= gEmitter.count)
    {
        return;
    }

    // FreeListからインデックスをアトミックに取得
    int32_t freeListIndex = 0;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

    // インデックスが有効範囲内かチェック (FreeListの末尾側から消費される)
    int32_t targetIndex = freeListIndex - 1;
    if (targetIndex >= 0 && targetIndex < (int32_t)kMaxParticles)
    {
        uint32_t particleIndex = gFreeList[targetIndex];

        // 乱数生成器の初期化
        RandomGenerator rng;
        float32_t3 seed = float32_t3(
            (float32_t)DTid.x * 17.13f + gPerFrame.time * 31.4f,
            (float32_t)particleIndex * 13.7f + (float32_t)gPerFrame.randomSeed,
            gPerFrame.time * 19.3f + (float32_t)DTid.x * 0.73f
        );
        rng.Init(seed);

        // 基本位置（エミッター位置）
        float32_t3 pos = gEmitter.translate;
        float32_t3 vel = gEmitter.baseVelocity;

        // パターン別の発生挙動
        if (gEmitter.patternType == 0)
        {
            // 球状散乱 (Explosion / Burst)
            float32_t3 dir = normalize(rng.GenerateBipolar3d() + float32_t3(0.001f, 0.001f, 0.001f));
            pos += dir * (rng.Generate1d() * gEmitter.radius);
            vel += dir * (gEmitter.speed * (0.5f + rng.Generate1d() * 0.5f));
        }
        else if (gEmitter.patternType == 1)
        {
            // ボクセル・キューブ分解 (Voxel Break)
            float32_t3 cubeOffset = rng.GenerateBipolar3d() * gEmitter.radius;
            // 仮想空間グリッド（0.1刻み等）にスナップさせることでボクセル感を出す
            cubeOffset = floor(cubeOffset * 10.0f) * 0.1f;
            pos += cubeOffset;
            float32_t3 dir = normalize(cubeOffset + float32_t3(0.001f, 0.001f, 0.001f));
            vel = dir * gEmitter.speed + rng.GenerateBipolar3d() * (gEmitter.spread * 0.5f);
        }
        else if (gEmitter.patternType == 2)
        {
            // 直線データストリーム (Stream / Warp Flow)
            float32_t3 diskOffset = rng.GenerateBipolar3d();
            diskOffset.z = 0.0f; // 円盤状に散布
            pos += diskOffset * gEmitter.radius;
            vel = gEmitter.baseVelocity * (gEmitter.speed * (0.8f + rng.Generate1d() * 0.4f));
            vel += rng.GenerateBipolar3d() * gEmitter.spread;
        }
        else if (gEmitter.patternType == 3)
        {
            // 自機ボクセル昇華ストリーム (Ship Voxel Ascend)
            // 自機の機体形状（翼スパン ±3.5m, 前後 ±2.5m, 厚み ±0.6m）に沿ってグリッド配置
            float32_t3 rnd = rng.GenerateBipolar3d();
            float32_t3 offset;
            offset.x = rnd.x * (gEmitter.radius * 3.5f);
            offset.y = rnd.y * (gEmitter.radius * 0.7f);
            offset.z = rnd.z * (gEmitter.radius * 2.2f);
            // グリッドスナップ
            offset = floor(offset * 5.0f) * 0.20f;
            pos += offset;

            // 上方向への力強い上昇速度 + 機体中心からの外向き拡散 + 気流ゆらぎ
            float32_t riseSpeed = gEmitter.speed * (0.85f + rng.Generate1d() * 0.50f);
            float32_t3 spreadDir = float32_t3(offset.x * 0.25f, 0.0f, offset.z * 0.15f);
            vel = float32_t3(
                spreadDir.x + rng.GenerateBipolar3d().x * gEmitter.spread,
                riseSpeed,
                spreadDir.z + rng.GenerateBipolar3d().z * gEmitter.spread
            );
        }
        else
        {
            // デジタルスパーク / ノイズ
            float32_t3 dir = normalize(rng.GenerateBipolar3d());
            pos += dir * (rng.Generate1d() * gEmitter.radius);
            vel = dir * (gEmitter.speed * (0.7f + rng.Generate1d() * 0.6f));
        }

        // パーティクルの各属性を設定
        gParticles[particleIndex].translate = pos;
        gParticles[particleIndex].velocity = vel;
        gParticles[particleIndex].scale = float32_t3(gEmitter.startScale, gEmitter.startScale, gEmitter.startScale);
        gParticles[particleIndex].currentTime = 0.0f;
        // 寿命に少しランダム幅を持たせる
        gParticles[particleIndex].lifeTime = max(0.05f, gEmitter.lifeTime * (0.8f + rng.Generate1d() * 0.4f));
        gParticles[particleIndex].color = gEmitter.startColor;
    }
    else
    {
        // 空きがなかった場合は消費分をもとに戻す
        InterlockedAdd(gFreeListIndex[0], 1);
    }
}
