#include "GPUParticle.hlsli"

Texture2D<float32_t4> gTexture : register(t1);
SamplerState gSampler : register(s0);

struct PixelShaderInput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
    float32_t3 normal : NORMAL0;
    float32_t lighting : LIGHTING0;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(PixelShaderInput input)
{
    PixelShaderOutput output;

    float32_t4 texColor = gTexture.Sample(gSampler, input.texcoord);

    // テクスチャが無効またはアルファゼロの場合は白テクスチャとして扱う（フェイルセーフ）
    if (texColor.a <= 0.001f)
    {
        texColor = float32_t4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // 面のエッジライン検出（ボクセルの立方体エッジをネオン発光）
    float32_t2 edgeDist = abs(input.texcoord - 0.5f) * 2.0f; // 0.0 (中心) ~ 1.0 (端)
    float32_t edgeVal = max(edgeDist.x, edgeDist.y);
    float32_t edgeGlow = smoothstep(0.70f, 0.98f, edgeVal) * 0.75f;

    // 自発光（エミッシブ）感を持たせた立体陰影
    float32_t3 baseColor = input.color.rgb * input.lighting * 1.25f;

    // エッジ部分を高輝度ホワイトシアンで強烈に発光（自機のディゾルブエッジに一致）
    float32_t3 edgeColor = float32_t3(0.85f, 1.0f, 1.0f) * edgeGlow;

    output.color.rgb = (baseColor * texColor.rgb) + edgeColor;
    output.color.a = input.color.a * texColor.a;

    return output;
}
