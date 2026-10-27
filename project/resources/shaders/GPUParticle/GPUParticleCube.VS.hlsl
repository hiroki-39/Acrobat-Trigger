#include "GPUParticle.hlsli"

StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
    float32_t3 normal : NORMAL0;
    float32_t lighting : LIGHTING0;
};

// 立方体の 36 頂点ローカル座標 (中心 0,0,0, 一辺 1.0)
static const float32_t3 kCubePositions[36] = {
    // 前面 (Z = -0.5)
    float32_t3(-0.5f, -0.5f, -0.5f), float32_t3(-0.5f,  0.5f, -0.5f), float32_t3( 0.5f, -0.5f, -0.5f),
    float32_t3(-0.5f,  0.5f, -0.5f), float32_t3( 0.5f,  0.5f, -0.5f), float32_t3( 0.5f, -0.5f, -0.5f),
    // 背面 (Z = +0.5)
    float32_t3( 0.5f, -0.5f,  0.5f), float32_t3( 0.5f,  0.5f,  0.5f), float32_t3(-0.5f, -0.5f,  0.5f),
    float32_t3(-0.5f, -0.5f,  0.5f), float32_t3( 0.5f,  0.5f,  0.5f), float32_t3(-0.5f,  0.5f,  0.5f),
    // 左面 (X = -0.5)
    float32_t3(-0.5f, -0.5f,  0.5f), float32_t3(-0.5f,  0.5f,  0.5f), float32_t3(-0.5f, -0.5f, -0.5f),
    float32_t3(-0.5f, -0.5f, -0.5f), float32_t3(-0.5f,  0.5f,  0.5f), float32_t3(-0.5f,  0.5f, -0.5f),
    // 右面 (X = +0.5)
    float32_t3( 0.5f, -0.5f, -0.5f), float32_t3( 0.5f,  0.5f, -0.5f), float32_t3( 0.5f, -0.5f,  0.5f),
    float32_t3( 0.5f, -0.5f,  0.5f), float32_t3( 0.5f,  0.5f, -0.5f), float32_t3( 0.5f,  0.5f,  0.5f),
    // 上面 (Y = +0.5)
    float32_t3(-0.5f,  0.5f, -0.5f), float32_t3(-0.5f,  0.5f,  0.5f), float32_t3( 0.5f,  0.5f, -0.5f),
    float32_t3( 0.5f,  0.5f, -0.5f), float32_t3(-0.5f,  0.5f,  0.5f), float32_t3( 0.5f,  0.5f,  0.5f),
    // 下面 (Y = -0.5)
    float32_t3(-0.5f, -0.5f,  0.5f), float32_t3(-0.5f, -0.5f, -0.5f), float32_t3( 0.5f, -0.5f,  0.5f),
    float32_t3( 0.5f, -0.5f,  0.5f), float32_t3(-0.5f, -0.5f, -0.5f), float32_t3( 0.5f, -0.5f, -0.5f),
};

static const float32_t3 kCubeNormals[6] = {
    float32_t3( 0.0f,  0.0f, -1.0f), // 前
    float32_t3( 0.0f,  0.0f,  1.0f), // 後
    float32_t3(-1.0f,  0.0f,  0.0f), // 左
    float32_t3( 1.0f,  0.0f,  0.0f), // 右
    float32_t3( 0.0f,  1.0f,  0.0f), // 上
    float32_t3( 0.0f, -1.0f,  0.0f), // 下
};

static const float32_t2 kFaceUVs[6] = {
    float32_t2(0.0f, 1.0f),
    float32_t2(0.0f, 0.0f),
    float32_t2(1.0f, 1.0f),
    float32_t2(0.0f, 0.0f),
    float32_t2(1.0f, 0.0f),
    float32_t2(1.0f, 1.0f),
};

// オイラー角回転行列（Y-X-Z順）の生成
float32_t3x3 CreateRotationMatrix(float32_t3 rot)
{
    float32_t sx = sin(rot.x);
    float32_t cx = cos(rot.x);
    float32_t sy = sin(rot.y);
    float32_t cy = cos(rot.y);
    float32_t sz = sin(rot.z);
    float32_t cz = cos(rot.z);

    float32_t3x3 mx = float32_t3x3(
        1.0f, 0.0f, 0.0f,
        0.0f,   cx,  -sx,
        0.0f,   sx,   cx
    );
    float32_t3x3 my = float32_t3x3(
          cy, 0.0f,   sy,
        0.0f, 1.0f, 0.0f,
         -sy, 0.0f,   cy
    );
    float32_t3x3 mz = float32_t3x3(
          cz,  -sz, 0.0f,
          sz,   cz, 0.0f,
        0.0f, 0.0f, 1.0f
    );

    return mul(mz, mul(mx, my));
}

VertexShaderOutput main(uint32_t vertexId : SV_VertexID, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    Particle particle = gParticles[instanceId];

    // 非アクティブまたはアルファがゼロのパーティクルはクリッピング
    if (particle.scale.x <= 0.0001f || particle.color.a <= 0.0001f)
    {
        output.position = float32_t4(0.0f, 0.0f, -2.0f, 1.0f);
        output.texcoord = float32_t2(0.0f, 0.0f);
        output.color = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
        output.normal = float32_t3(0.0f, 1.0f, 0.0f);
        output.lighting = 0.0f;
        return output;
    }

    uint32_t faceIndex = vertexId / 6;
    uint32_t inFaceIndex = vertexId % 6;

    float32_t3 localPos = kCubePositions[vertexId];
    float32_t3 localNormal = kCubeNormals[faceIndex];

    // インスタンスごとの固有回転角（時間とインスタンスIDから算出）
    float32_t rotSeed = (float32_t)instanceId * 0.7531f;
    float32_t3 rot = float32_t3(
        particle.currentTime * (2.8f + frac(rotSeed * 1.3f) * 3.5f) + rotSeed,
        particle.currentTime * (3.4f + frac(rotSeed * 2.7f) * 4.2f) + rotSeed * 1.7f,
        particle.currentTime * (2.2f + frac(rotSeed * 3.9f) * 3.0f) + rotSeed * 2.3f
    );
    float32_t3x3 rotMat = CreateRotationMatrix(rot);

    // スケール適用 ＆ 自転適用
    float32_t3 scaledPos = localPos * particle.scale;
    float32_t3 rotatedPos = mul(rotMat, scaledPos);
    float32_t3 rotatedNormal = normalize(mul(rotMat, localNormal));

    // ワールド位置
    float32_t3 worldPos = rotatedPos + particle.translate;

    // 平行光源による簡易陰影計算（斜め上からのライト）
    float32_t3 lightDir = normalize(float32_t3(0.4f, -0.85f, 0.35f));
    float32_t diffuse = saturate(dot(rotatedNormal, -lightDir));
    // アンビエント(0.72) + ディフューズ(0.28) で発光感を損なわずに立体陰影を形成
    float32_t lighting = diffuse * 0.28f + 0.72f;

    // ビュー・プロジェクション変換
    output.position = mul(float32_t4(worldPos, 1.0f), gPerView.viewProjection);
    output.texcoord = kFaceUVs[inFaceIndex];
    output.color = particle.color;
    output.normal = rotatedNormal;
    output.lighting = lighting;

    return output;
}
