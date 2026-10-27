#include "GPUParticle.hlsli"

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

[numthreads(1024, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;
    if (particleIndex < kMaxParticles)
    {
        // パーティクル構造体をゼロクリア
        gParticles[particleIndex].translate = float32_t3(0.0f, 0.0f, 0.0f);
        gParticles[particleIndex].pad0 = 0.0f;
        gParticles[particleIndex].scale = float32_t3(0.0f, 0.0f, 0.0f);
        gParticles[particleIndex].lifeTime = 0.0f;
        gParticles[particleIndex].velocity = float32_t3(0.0f, 0.0f, 0.0f);
        gParticles[particleIndex].currentTime = 0.0f;
        gParticles[particleIndex].color = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);

        // FreeListの初期化: 0 から kMaxParticles - 1 までのインデックスを格納
        gFreeList[particleIndex] = particleIndex;
    }

    // スレッド0番がFreeListIndexの空き数を設定
    if (particleIndex == 0)
    {
        gFreeListIndex[0] = (int32_t)kMaxParticles;
    }
}
