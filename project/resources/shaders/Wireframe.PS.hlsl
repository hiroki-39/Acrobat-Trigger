#include "Object3d.hlsli"

#define float32_t4 float4
#define float32_t3 float3
#define float32_t4x4 float4x4
#define int32_t int

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t3 padding0;
    float32_t4x4 uvTransform;
    int32_t selectLightings;
    float shininess;
    float environmentCoefficient;
    float fresnelF0;
    float32_t3 specularColor;
};

ConstantBuffer<Material> gMaterial : register(b0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // マテリアルカラーが白（デフォルト）の場合は鮮やかな蛍光グリーンで発光表示
    // マテリアルカラーが指定されている場合（コライダーの水色や残像など）はその色を採用
    float3 col = gMaterial.color.rgb;
    float alpha = gMaterial.color.a;
    if (col.r > 0.85f && col.g > 0.85f && col.b > 0.85f)
    {
        output.color = float32_t4(0.1f, 1.0f, 0.35f, alpha); // 鮮やかな蛍光ライムグリーン
    }
    else
    {
        output.color = float32_t4(col, alpha);
    }
    
    return output;
}
