#include "GPUParticle.hlsli"

StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
};

// クアッドの頂点位置 (6頂点)
static const float32_t2 kQuadPositions[6] = {
    float32_t2(-0.5f, -0.5f),
    float32_t2(-0.5f,  0.5f),
    float32_t2( 0.5f, -0.5f),
    float32_t2(-0.5f,  0.5f),
    float32_t2( 0.5f,  0.5f),
    float32_t2( 0.5f, -0.5f),
};

static const float32_t2 kQuadUVs[6] = {
    float32_t2(0.0f, 1.0f),
    float32_t2(0.0f, 0.0f),
    float32_t2(1.0f, 1.0f),
    float32_t2(0.0f, 0.0f),
    float32_t2(1.0f, 0.0f),
    float32_t2(1.0f, 1.0f),
};

VertexShaderOutput main(uint32_t vertexId : SV_VertexID, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    Particle particle = gParticles[instanceId];

    // スケールまたはアルファが0以下の場合は頂点を無効化（クリッピング空間外へ）
    if (particle.scale.x <= 0.0001f || particle.color.a <= 0.0001f)
    {
        output.position = float32_t4(0.0f, 0.0f, -2.0f, 1.0f);
        output.texcoord = float32_t2(0.0f, 0.0f);
        output.color = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
        return output;
    }

    // クアッドローカル頂点
    float32_t2 localPos2D = kQuadPositions[vertexId];
    float32_t4 localPos = float32_t4(localPos2D.x, localPos2D.y, 0.0f, 1.0f);

    // ビルボード行列の構築 (資料 03_01 105039)
    float32_t4x4 worldMatrix = gPerView.billboardMatrix;
    worldMatrix[0] *= particle.scale.x;
    worldMatrix[1] *= particle.scale.y;
    worldMatrix[2] *= particle.scale.z;
    worldMatrix[3].xyz = particle.translate;

    // ワールド・ビュー・プロジェクション変換
    float32_t4 worldPos = mul(localPos, worldMatrix);
    output.position = mul(worldPos, gPerView.viewProjection);
    output.texcoord = kQuadUVs[vertexId];
    output.color = particle.color;

    return output;
}
