#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float4> gMaskTexture : register(t1); // Dissolve等のマスク用
SamplerState gSampler : register(s0);

// C++側の PostProcessData と完全に一致させる
cbuffer PostProcessBuffer : register(b0) {
    int enableGrayscale;
    int enableSepia;
    float sepiaStrength;
    int enableVignette;

    float vignetteIntensity;
    float vignettePower;
    int enableSmoothing;
    float smoothingKernelSize;

    int enableGaussian;
    float gaussianSigma;
    int enableOutline;
    float outlineThreshold;

    int enableRadialBlur;
    float radialBlurCenterX;
    float radialBlurCenterY;
    float radialBlurIntensity;

    int enableDissolve;
    float dissolveThreshold;
    float dissolveEdgeWidth;
    float padding1;

    float3 dissolveEdgeColor;
    int enableRandom;

    float randomTime;
    float glitchStrength;
    float noiseStrength;
    float damagePulse;

    float lowHpIntensity;
    float3 padding2;
}

// 疑似乱数ジェネレーター
float random(float2 st) {
    return frac(sin(dot(st.xy, float2(12.9898, 78.233))) * 43758.5453123);
}

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    float2 uv = input.texcoord;

    // 1. Random (Glitch) - UVをずらす
    if (enableRandom != 0) {
        // 横方向へのスライスズレ
        float noiseY = random(float2(floor(uv.y * 50.0), randomTime));
        if (noiseY > 1.0 - glitchStrength) {
            uv.x += (random(float2(uv.y, randomTime)) - 0.5) * 0.1 * glitchStrength * 10.0;
        }
    }

    float4 color = gTexture.Sample(gSampler, uv);

    // Glitchの色収差(RGBずらし)
    if (enableRandom != 0 && glitchStrength > 0.0) {
        float offset = glitchStrength * 0.05 * random(float2(randomTime, randomTime));
        float r = gTexture.Sample(gSampler, uv + float2(offset, 0)).r;
        float b = gTexture.Sample(gSampler, uv - float2(offset, 0)).b;
        color.r = r;
        color.b = b;
    }

    // 2. Blur (Smoothing / Gaussian / RadialBlur)
    if (enableSmoothing != 0) {
        float3 sum = float3(0,0,0);
        float kernel = max(1.0, smoothingKernelSize);
        int k = (int)kernel;
        float weight = 1.0 / pow((k * 2.0 + 1.0), 2.0);
        float2 texelSize = float2(1.0/1280.0, 1.0/720.0);
        
        for (int y = -k; y <= k; ++y) {
            for (int x = -k; x <= k; ++x) {
                sum += gTexture.Sample(gSampler, uv + float2(x, y) * texelSize).rgb * weight;
            }
        }
        color.rgb = sum;
    }
    else if (enableGaussian != 0) {
        float3 sum = float3(0,0,0);
        float totalWeight = 0.0;
        int k = 3; // 固定カーネルサイズ
        float2 texelSize = float2(1.0/1280.0, 1.0/720.0) * gaussianSigma;

        for (int y = -k; y <= k; ++y) {
            for (int x = -k; x <= k; ++x) {
                float weight = exp(-(x*x + y*y) / (2.0 * gaussianSigma * gaussianSigma));
                sum += gTexture.Sample(gSampler, uv + float2(x, y) * texelSize).rgb * weight;
                totalWeight += weight;
            }
        }
        color.rgb = sum / totalWeight;
    }
    else if (enableRadialBlur != 0) {
        float2 center = float2(radialBlurCenterX, radialBlurCenterY);
        float2 dir = center - uv;
        float3 sum = float3(0,0,0);
        int samples = 10;
        for(int i = 0; i < samples; i++) {
            float t = (float)i / (float)samples;
            sum += gTexture.Sample(gSampler, uv + dir * t * radialBlurIntensity).rgb;
        }
        color.rgb = sum / samples;
    }

    // 3. Outline (簡易ソーベル)
    if (enableOutline != 0) {
        float2 texelSize = float2(1.0/1280.0, 1.0/720.0);
        
        float c00 = dot(gTexture.Sample(gSampler, uv + float2(-1, -1)*texelSize).rgb, float3(0.3,0.6,0.1));
        float c10 = dot(gTexture.Sample(gSampler, uv + float2( 0, -1)*texelSize).rgb, float3(0.3,0.6,0.1));
        float c20 = dot(gTexture.Sample(gSampler, uv + float2( 1, -1)*texelSize).rgb, float3(0.3,0.6,0.1));
        
        float c01 = dot(gTexture.Sample(gSampler, uv + float2(-1,  0)*texelSize).rgb, float3(0.3,0.6,0.1));
        float c21 = dot(gTexture.Sample(gSampler, uv + float2( 1,  0)*texelSize).rgb, float3(0.3,0.6,0.1));
        
        float c02 = dot(gTexture.Sample(gSampler, uv + float2(-1,  1)*texelSize).rgb, float3(0.3,0.6,0.1));
        float c12 = dot(gTexture.Sample(gSampler, uv + float2( 0,  1)*texelSize).rgb, float3(0.3,0.6,0.1));
        float c22 = dot(gTexture.Sample(gSampler, uv + float2( 1,  1)*texelSize).rgb, float3(0.3,0.6,0.1));

        float sx = -c00 + c20 - 2.0*c01 + 2.0*c21 - c02 + c22;
        float sy = -c00 - 2.0*c10 - c20 + c02 + 2.0*c12 + c22;
        float edge = sqrt(sx*sx + sy*sy);

        if (edge > outlineThreshold) {
            color.rgb = float3(0,0,0); // アウトラインは黒
        }
    }

    // 4. Color (Grayscale / Sepia)
    float gray = dot(color.rgb, float3(0.299, 0.587, 0.114));
    if (enableGrayscale != 0) {
        color.rgb = float3(gray, gray, gray);
    }
    else if (enableSepia != 0) {
        float3 sepiaColor = float3(gray * 1.0, gray * 0.8, gray * 0.5);
        color.rgb = lerp(color.rgb, sepiaColor, sepiaStrength);
    }

    // 5. Vignette
    if (enableVignette != 0) {
        float dist = distance(uv, float2(0.5, 0.5));
        float v = smoothstep(0.8, 0.2, dist * vignettePower);
        color.rgb *= lerp(1.0, v, vignetteIntensity);
    }

    // 6. Dissolve
    if (enableDissolve != 0) {
        float maskVal = gMaskTexture.Sample(gSampler, input.texcoord).r;
        
        if (maskVal < dissolveThreshold) {
            discard; // 閾値以下は透明に抜く
        } 
        else if (maskVal < dissolveThreshold + dissolveEdgeWidth) {
            color.rgb = dissolveEdgeColor; // エッジ部分の色
        }
    }

    // 7. Random (砂嵐 Noise)
    if (enableRandom != 0 && noiseStrength > 0.0) {
        float noise = random(uv * randomTime);
        color.rgb += (noise - 0.5) * noiseStrength;
    }

    // 8. サイバーパンク2077風 機械系電子ノイズ＆被弾グリッチ（心拍感のないデジタル障害）
    if (damagePulse > 0.001 || lowHpIntensity > 0.001) {
        float distNorm = length((uv - 0.5) * float2(1.6, 1.0));
        float noiseEdge = smoothstep(0.28, 0.90, distNorm);
        float2 sampleUv = uv;

        // 8-0. 矩形デジタルブロック・水平データラインの瞬間ズレ（機械的ショート）
        float activeGlitch = saturate(damagePulse * 0.9 + lowHpIntensity * 0.85);
        float tickFast = floor(randomTime * 28.0);
        float tickMed  = floor(randomTime * 14.0);

        // 細長い水平スキャンライン単位の横ズレ
        float lineId = floor(uv.y * 120.0);
        float lineRand = random(float2(lineId, tickFast));
        float lineThresh = 1.0 - (activeGlitch * 0.24 * (0.20 + 0.80 * noiseEdge));
        if (lineRand > lineThresh) {
            float shiftDir = random(float2(lineId + 11.3, tickFast)) - 0.5;
            sampleUv.x += shiftDir * 0.055 * activeGlitch * (0.20 + 0.80 * noiseEdge);
        }

        // 外周のデジタル矩形ブロック破損（ブロック単位の色抜け・UV飛び）
        float2 blockCoord = floor(uv * float2(28.0, 18.0));
        float blockRand = random(blockCoord + float2(tickMed * 3.7, tickMed * 1.9));
        float blockMask = step(1.0 - lowHpIntensity * 0.14 * noiseEdge, blockRand);
        if (blockMask > 0.5) {
            sampleUv.x += (random(blockCoord + tickFast) - 0.5) * 0.06;
        }

        // 8-1. 離散ステップ的なRGB色収差（アナログではなくデジタル信号のチャンネル分離）
        float aberrationOffset = 0.015 * damagePulse + 0.009 * lowHpIntensity;
        float rShift = gTexture.Sample(gSampler, sampleUv + float2(aberrationOffset, 0.0)).r;
        float gBase  = gTexture.Sample(gSampler, sampleUv).g;
        float bShift = gTexture.Sample(gSampler, sampleUv - float2(aberrationOffset, 0.0)).b;
        float chromaBlend = saturate(damagePulse * 0.95 + lowHpIntensity * 0.78);
        color.r = lerp(color.r, rShift, chromaBlend);
        color.g = lerp(color.g, gBase,  saturate(lowHpIntensity * 0.70));
        color.b = lerp(color.b, bShift, chromaBlend);

        // 破損ブロック部分にシアン／警告レッドの電子フラッシュを混ぜる
        if (blockMask > 0.5) {
            float isCyan = step(0.65, random(blockCoord + 5.1));
            float3 glitchTint = lerp(float3(1.0, 0.08, 0.25), float3(0.10, 0.85, 1.0), isCyan);
            color.rgb = lerp(color.rgb, glitchTint, 0.42 * lowHpIntensity);
        }

        // 8-2. 画面外周のみの保護マスク（中央の照準・敵の視界はクリアに維持）
        float combinedMask = saturate(damagePulse * 1.0 + lowHpIntensity * 0.78);
        float edgeMask = smoothstep(0.45, 0.90, distNorm) * combinedMask;

        // 8-3. 機械的インターレース走査線（矩形波的なクッキリしたスキャンライン）
        float scanWave = step(0.45, frac(uv.y * 180.0 + randomTime * 12.0));
        float3 errorRed = float3(1.0, 0.06, 0.22); // 警告ネオンレッド
        color.rgb = lerp(color.rgb, errorRed * (0.45 + 0.55 * scanWave), edgeMask * 0.72);
    }

    output.color = color;
    return output;
}
