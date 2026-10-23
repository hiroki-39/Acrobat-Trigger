struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

Texture2D<float4> gRuleTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer TransitionBuffer : register(b0) {
    float4 fadeColor;      // トランジションベース色
    float progress;        // 進行度 (0.0: 完全透明 〜 1.0: 完全に埋まる)
    float edgeSoftness;    // 境界アンチエイリアス幅
    int isOpening;         // 0 = 閉じる(イン), 1 = 開く(アウト)
    int transitionType;    // 0 = SquareGrid, 1 = Hexagon
    float4 edgeColor;      // タイルの枠線/エッジ色 (Hexagon時はGlowColor)
    float2 originCoord;    // 波紋の起点 (0.5, 0.5)
    float hexTileScale;    // 六角形タイルの密度 (14.0)
    float borderWidth;     // 六角形輪郭線の太さ (0.08)
    float glitchIntensity; // ピーク時の明滅・乱数幅 (0.25)
    float3 padding0;
};

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

// 疑似乱数生成
float Hash21(float2 p) {
    p = frac(p * float2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return frac(p.x * p.y);
}

// =========================================================================
// 正六角形タイリング関数 (数学的周期タイリング: 負数・境界完全安全)
// =========================================================================
float4 HexCoords(float2 p) {
    const float2 s = float2(1.0, 1.732050807568877); // (1.0, sqrt(3))
    const float2 h = s * 0.5;

    // floor を用いた正確な周期格子
    float2 a = p - s * floor(p / s) - h;
    float2 b = (p - h) - s * floor((p - h) / s) - h;

    float2 gv;
    float2 id;
    if (dot(a, a) < dot(b, b)) {
        gv = a;
        id = p - a;
    } else {
        gv = b;
        id = p - b;
    }

    return float4(gv, id);
}

// 六角形の中心からの正規化距離 (中心 0.0 〜 六角形外周エッジで 1.0)
float HexNormalizedDistance(float2 p) {
    p = abs(p);
    float c = dot(p, normalize(float2(1.0, 1.732050807568877)));
    float d = max(c, p.x);
    // 内接円半径が 0.5 なので、2.0 を掛けると境界で正確に 1.0 になる
    return d * 2.0;
}

// =========================================================================
// 方式1: 四角形サイバーグリッド（Square Grid - すっきりフラットタイル）
// =========================================================================
PixelShaderOutput RenderSquareGrid(float2 uv) {
    PixelShaderOutput output;
    float2 grid = float2(32.0, 18.0);
    float2 cellIndex = floor(uv * grid);
    float2 localUv = frac(uv * grid) - 0.5;
    float2 cellCenter = (cellIndex + 0.5) / grid;
    float distFromEdge = 1.0 - abs(cellCenter.x - 0.5) * 2.0;

    float tileScale = 0.0;
    float spread = 0.35;

    if (isOpening == 0) {
        float start = distFromEdge * (1.0 - spread);
        float t = saturate((progress - start) / spread);
        tileScale = t * t * (3.0 - 2.0 * t);
    } else {
        float distFromCenter = abs(cellCenter.x - 0.5) * 2.0;
        float start = distFromCenter * (1.0 - spread);
        float t = saturate((progress - start) / spread);
        float ease = t * t * (3.0 - 2.0 * t);
        tileScale = 1.0 - ease;
    }

    if (tileScale <= 0.001) {
        discard;
    }

    float halfSize = tileScale * 0.505;
    float distToEdge = max(abs(localUv.x), abs(localUv.y));

    if (distToEdge >= halfSize) {
        discard;
    }

    float aa = 0.015;
    float alpha = smoothstep(halfSize, halfSize - aa, distToEdge);
    if (alpha <= 0.01) {
        discard;
    }

    float3 finalColor = fadeColor.rgb;
    float border = 0.025;
    if (distToEdge >= halfSize - border && tileScale > 0.05 && tileScale < 0.95) {
        finalColor = lerp(finalColor, edgeColor.rgb, edgeColor.a);
    }

    output.color = float4(finalColor, alpha * fadeColor.a);
    return output;
}

// =========================================================================
// 方式2: ヘキサゴン・トランジション（Hexagon）
// =========================================================================
PixelShaderOutput RenderHexagon(float2 uv) {
    PixelShaderOutput output;

    // アスペクト比補正 (16:9 で縦横比の歪みのない正六角形を形成)
    float aspect = 16.0 / 9.0;
    float2 aspectUV = float2(uv.x * aspect, uv.y);
    float2 aspectOrigin = float2(originCoord.x * aspect, originCoord.y);

    // 六角形タイリング
    float scale = max(hexTileScale, 2.0);
    float2 p = aspectUV * scale;
    float4 hex = HexCoords(p);
    float2 localP = hex.xy;          // セル内ローカル座標
    float2 centerP = hex.zw / scale; // セル中心座標 (aspectUV 空間)

    // 起点 (画面中央) からの距離
    float distFromOrigin = length(centerP - aspectOrigin);

    // 起点から最も遠い四隅までの距離
    float maxCornerX = max(originCoord.x, 1.0 - originCoord.x) * aspect;
    float maxCornerY = max(originCoord.y, 1.0 - originCoord.y);
    float maxDist = max(length(float2(maxCornerX, maxCornerY)) * 1.10, 0.1);
    float normDist = saturate(distFromOrigin / maxDist);

    // 波紋の厚み
    float spread = 0.35;

    float tileScale = 0.0;
    if (isOpening == 0) {
        // 【Phase 1: 発生・伝播 (閉じる時)】中央から波紋状に四方へ六角形が拡大
        float start = normDist * (1.0 - spread);
        float t = saturate((progress - start) / spread);
        tileScale = t * t * (3.0 - 2.0 * t);

        // 終盤は完全遮蔽
        if (progress >= 0.98) {
            tileScale = 1.0;
        }
    } else {
        // 【Phase 3: 収束・定着 (開く時)】中央から波紋状に四方へ六角形が縮小
        float start = normDist * (1.0 - spread);
        float t = saturate((progress - start) / spread);
        float ease = t * t * (3.0 - 2.0 * t);
        tileScale = 1.0 - ease;

        if (progress <= 0.02) {
            tileScale = 1.0;
        }
    }

    // タイルがまだ成長していない領域は破棄して背景を表示
    if (tileScale <= 0.005) {
        discard;
    }

    // 六角形の正規化距離 (0.0: 中心, 1.0: 六角形外枠エッジ)
    float hexDist = HexNormalizedDistance(localP);

    // タイルが tileScale の大きさで拡大
    // 最大時は 1.01 で隣接タイル同士の隙間なくピッタリ接続
    float maxRadius = tileScale * 1.02;
    if (hexDist >= maxRadius) {
        discard;
    }

    // アンチエイリアス境界
    float aa = 0.04;
    float alpha = smoothstep(maxRadius, maxRadius - aa, hexDist);
    if (alpha <= 0.01) {
        discard;
    }

    // セルごとの乱数ハッシュ
    float cellRand = Hash21(floor(hex.zw * 10.0));

    // 六角形外枠のネオン枠線幅 (しっかり視認できる太さ 0.14)
    float edgeThickness = max(borderWidth * 1.8, 0.10);
    bool isBorder = (hexDist >= (maxRadius - edgeThickness));

    // 【波面ハイライト】波が広がる最先端のタイルは、枠線が白熱ネオン発光！
    float waveFront = (tileScale > 0.05 && tileScale < 0.95) ? 2.2 : 1.0;

    // 【Phase 2: ピーク時】タイルの明滅・グリッチ
    float peakFactor = sin(saturate(progress) * 3.14159265);
    float flicker = 1.0;
    if (peakFactor > 0.25) {
        bool isFlashing = (frac(cellRand + progress * 24.0) > (1.0 - glitchIntensity * peakFactor * 0.75));
        flicker = isFlashing ? 1.8 : 1.0;
    }

    // ネオン発光色（#FF1E44 等）
    float3 glowCol = edgeColor.rgb * (flicker * waveFront);

    // 回路ライン同期走行（プラズマシアン色の回路ラインが同期走行）
    float circuitWave = sin(distFromOrigin * 24.0 - progress * 35.0 + cellRand * 6.28);
    if (circuitWave > 0.58 && peakFactor > 0.15) {
        float3 cyanCircuit = float3(0.60, 1.0, 0.98);
        glowCol = lerp(glowCol, cyanCircuit, saturate((circuitWave - 0.58) * 3.5));
    }

    // タイルのベース色（仮想空間ディープサイバーティール/ネイビー）
    float3 baseBg = fadeColor.rgb;

    // タイル内部のサイバーグラデーション（セルの形がはっきり分かるよう、外枠から内側へグローを付与）
    float innerGlow = pow(saturate(hexDist / max(maxRadius, 0.01)), 2.5) * 0.50;

    float3 finalColor;
    if (isBorder && tileScale > 0.03) {
        // 外枠は強烈なネオン発光！
        finalColor = glowCol;
    } else {
        // 内部も六角形の形が美しく浮き彫りになるよう、ネオングローをブレンド
        finalColor = lerp(baseBg, edgeColor.rgb * 0.7, innerGlow);
    }

    output.color = float4(finalColor, alpha * fadeColor.a);
    return output;
}

PixelShaderOutput main(VertexShaderOutput input) {
    if (transitionType == 1) {
        return RenderHexagon(input.texcoord);
    } else {
        return RenderSquareGrid(input.texcoord);
    }
}
