#include "GPUParticle.hlsli"

ConstantBuffer<GPUEmitterData> gEmitter : register(b0);
ConstantBuffer<PerFrameCS> gPerFrame : register(b1);

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<uint32_t> gFreeList : register(u2);

[numthreads(1024, 1, 1)]
void main(uint32_t3 DTid : SV_DispatchThreadID)
{
    uint32_t particleIndex = DTid.x;
    if (particleIndex >= kMaxParticles)
    {
        return;
    }

    // 生存しているパーティクルのみ更新
    if (gParticles[particleIndex].lifeTime > 0.0f && gParticles[particleIndex].currentTime < gParticles[particleIndex].lifeTime)
    {
        gParticles[particleIndex].currentTime += gPerFrame.deltaTime;

        if (gParticles[particleIndex].currentTime >= gParticles[particleIndex].lifeTime)
        {
            // 寿命到達: スケールとアルファをゼロにして描画から除外
            gParticles[particleIndex].scale = float32_t3(0.0f, 0.0f, 0.0f);
            gParticles[particleIndex].color = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
            gParticles[particleIndex].lifeTime = 0.0f; // 二重返却防止

            // FreeListへインデックスを返却
            int32_t freeListIndex = 0;
            InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
            if (freeListIndex >= 0 && freeListIndex < (int32_t)kMaxParticles)
            {
                gFreeList[freeListIndex] = particleIndex;
            }
            else
            {
                InterlockedAdd(gFreeListIndex[0], -1);
            }
        }
        else
        {
            // 生存中の更新処理
            float32_t t = saturate(gParticles[particleIndex].currentTime / gParticles[particleIndex].lifeTime);

            // 重力と移動
            gParticles[particleIndex].velocity.y -= gEmitter.gravity * gPerFrame.deltaTime;
            gParticles[particleIndex].translate += gParticles[particleIndex].velocity * gPerFrame.deltaTime;

            // スケール・カラーの補間
            float32_t s = lerp(gEmitter.startScale, gEmitter.endScale, t);
            gParticles[particleIndex].scale = float32_t3(s, s, s);
            gParticles[particleIndex].color = lerp(gEmitter.startColor, gEmitter.endColor, t);
        }
    }
}
