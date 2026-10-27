#include "GPUParticle.hlsli"

Texture2D<float32_t4> gTexture : register(t1);
SamplerState gSampler : register(s0);

struct PixelShaderInput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(PixelShaderInput input)
{
    PixelShaderOutput output;

    // アルファがほぼ0なら破棄
    if (input.color.a <= 0.001f)
    {
        discard;
    }

    // テクスチャサンプリング
    float32_t4 texColor = gTexture.Sample(gSampler, input.texcoord);

    // テクスチャカラーと頂点カラーを乗算
    output.color = texColor * input.color;

    // 完全に透明ならピクセルを破棄
    if (output.color.a <= 0.001f)
    {
        discard;
    }

    return output;
}
