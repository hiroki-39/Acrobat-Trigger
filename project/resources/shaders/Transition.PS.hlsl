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


// =========================================================================
// 方式3: CRT TV 電源オフ演出 (CRTOff) - 高視認性サイバーシャットダウン仕様
// =========================================================================
PixelShaderOutput RenderCRTOff(float2 uv) {
    PixelShaderOutput output;
    float aspect = 16.0 / 9.0;
    float2 d = abs(uv - 0.5);

    // サイバーパレット (高彩度・高輝度ネオン)
    float3 black = fadeColor.rgb;
    float3 cyberCyan = float3(0.0, 1.0, 1.0);        // ネオンシアン
    float3 cyberMagenta = float3(1.0, 0.05, 0.55);   // ネオンマゼンタ（色収差エッジ外側）
    float3 plasmaWhite = float3(2.0, 2.0, 2.0);      // 純白プラズマ白熱芯

    // CRT走査線＆グリッチハッシュ
    float scanline = sin(uv.y * 500.0 * 3.14159) * 0.5 + 0.5; // クッキリ視認できる走査線
    float cellRand = Hash21(floor(uv * float2(50.0, 30.0) + progress * 50.0));

    if (isOpening == 0) {
        // =================================================================
        // ---【電源OFF (閉じる): システムシャットダウン】---
        // =================================================================
        if (progress < 0.44) {
            // [Phase 1: 上下圧縮 + CRT走査線ベール + 多層RGB色収差ネオンエッジ]
            float t = progress / 0.44;
            float ease = t * t * t; // 加速しながらギュッと潰れる
            float halfH = lerp(0.5, 0.006, ease);

            if (d.y > halfH) {
                // 外側の漆黒 ＋ エッジから漏れ出るマゼンタ/シアンのネオンオーラ
                float auraDist = d.y - halfH;
                float aura = saturate(1.0 - auraDist / 0.040);
                float3 auraCol = lerp(black, cyberMagenta * 0.6 + cyberCyan * 0.3, pow(aura, 2.0));
                output.color = float4(auraCol, 1.0);
                return output;
            } else {
                // 上下の圧縮境界: 存在感のある太い多層RGB色収差ビーム！
                float edgeDist = halfH - d.y;
                float beamWidth = 0.012 + (1.0 - halfH * 2.0) * 0.018;

                if (edgeDist < beamWidth) {
                    float factor = 1.0 - (edgeDist / beamWidth);
                    // 外側=マゼンタ, 中間=シアン, 芯=強烈なプラズマホワイト
                    float3 beamCol = lerp(cyberMagenta, cyberCyan, saturate(factor * 1.5));
                    beamCol = lerp(beamCol, plasmaWhite, pow(factor, 1.8) * 1.3);
                    output.color = float4(beamCol, factor * (0.92 + ease * 0.08));
                    return output;
                }

                // 圧縮中の可視画面: CRT走査線と水平ラスタグリッチのオーバーレイ
                float noiseBand = Hash21(floor(uv.yy * float2(24.0, 24.0) + progress * 70.0));
                float glitch = (noiseBand > 0.85) ? 0.45 : 0.0;
                float scanAlpha = lerp(0.35, 0.65, ease) * (1.0 - scanline * 0.65) + glitch;
                float3 scanColor = lerp(float3(0.01, 0.03, 0.05), cyberCyan * 0.9, glitch);
                output.color = float4(scanColor, scanAlpha);
                return output;
            }
        } else if (progress < 0.78) {
            // [Phase 2: 水平圧縮 + プラズマ波形・デジタルデータパルス]
            float t = (progress - 0.44) / 0.34;
            float ease = t * t * (3.0 - 2.0 * t); // SmoothStep
            float halfW = lerp(0.5, 0.008, ease);
            float halfH = 0.0065; // 太さをしっかり維持

            float yDist = abs(d.y);
            float lineGlow = saturate(1.0 - yDist / 0.035);

            if (d.y > halfH || d.x > halfW) {
                // 外側の漆黒 ＋ 水平ビームからのプラズマ漏れ光
                float3 glowLeak = (cyberCyan * 0.7 + cyberMagenta * 0.3) * pow(lineGlow, 2.5) * (1.0 - ease) * 0.6;
                output.color = float4(black + glowLeak, 1.0);
                return output;
            } else {
                // 激しく脈動するデジタルパルス波形
                float centerFocus = 1.0 - (d.x / halfW);
                float wave = sin(uv.x * 120.0 - progress * 110.0) * 0.5 + 0.5;
                float spark = (cellRand > 0.75) ? 1.5 : 1.0;

                float3 lineCol = lerp(cyberMagenta, cyberCyan, centerFocus);
                lineCol = lerp(lineCol, plasmaWhite, pow(centerFocus, 1.4) * wave * spark);
                output.color = float4(lineCol, 1.0);
                return output;
            }
        } else {
            // [Phase 3: 中心の光点消失 + 大判サイバーショックウェーブ拡散]
            float t = saturate((progress - 0.78) / 0.22);
            float2 aspectD = float2(d.x * aspect, d.y);
            float dist = length(aspectD);

            if (t >= 0.98) {
                output.color = float4(black, 1.0);
                return output;
            }

            // 1. 中心の光の点（スポット）
            float spotRadius = lerp(0.012, 0.0005, t);
            float spotIntensity = (1.0 - t) * smoothstep(spotRadius * 2.8, 0.0, dist);
            float3 spotCol = lerp(cyberCyan, plasmaWhite * 1.8, spotIntensity) * spotIntensity;

            // 2. 画面いっぱいに広がるサイバー・ショックウェーブ（円環パルス波紋）
            float ringRadius = t * 0.42; // 画面の広範囲に拡散
            float ringWidth = 0.008;      // クッキリ太いリング
            float ringDist = abs(dist - ringRadius);
            float ringIntensity = smoothstep(ringWidth, 0.0, ringDist) * (1.0 - t) * 2.5;
            float3 ringCol = (lerp(cyberCyan, cyberMagenta, t) + plasmaWhite * 0.4) * ringIntensity;

            float3 finalColor = black + spotCol + ringCol;
            output.color = float4(finalColor, 1.0);
            return output;
        }
    } else {
        // =================================================================
        // ---【電源ON (開く): システム再起動】---
        // =================================================================
        if (progress < 0.18) {
            // [Phase 1: 中心のサイバースポット点灯 + 逆収束リング]
            float t = progress / 0.18;
            float2 aspectD = float2(d.x * aspect, d.y);
            float dist = length(aspectD);

            float spotRadius = lerp(0.001, 0.012, t);
            float spotIntensity = t * smoothstep(spotRadius * 2.8, 0.0, dist);
            float3 spotCol = lerp(cyberCyan, plasmaWhite * 1.8, spotIntensity) * spotIntensity;

            float ringRadius = (1.0 - t) * 0.35;
            float ringWidth = 0.008;
            float ringIntensity = smoothstep(ringWidth, 0.0, abs(dist - ringRadius)) * t * 2.0;
            float3 ringCol = (cyberCyan + plasmaWhite * 0.3) * ringIntensity;

            float3 finalColor = black + spotCol + ringCol;
            output.color = float4(finalColor, 1.0);
            return output;
        } else if (progress < 0.50) {
            // [Phase 2: 水平プラズマ波形ラインの左右伸長]
            float t = (progress - 0.18) / 0.32;
            float ease = t * t * (3.0 - 2.0 * t);
            float halfW = lerp(0.008, 0.5, ease);
            float halfH = 0.0065;

            float yDist = abs(d.y);
            float lineGlow = saturate(1.0 - yDist / 0.035);

            if (d.y > halfH || d.x > halfW) {
                float3 glowLeak = cyberCyan * pow(lineGlow, 2.5) * ease * 0.5;
                output.color = float4(black + glowLeak, 1.0);
                return output;
            } else {
                float centerFocus = 1.0 - (d.x / max(halfW, 0.001));
                float wave = sin(uv.x * 120.0 + progress * 100.0) * 0.5 + 0.5;
                float3 lineCol = lerp(cyberMagenta, cyberCyan, 0.5);
                lineCol = lerp(lineCol, plasmaWhite, pow(centerFocus, 1.3) * wave);
                output.color = float4(lineCol, 1.0);
                return output;
            }
        } else {
            // [Phase 3: 上下全開 + 走査線ベールのフェードアウト]
            float t = (progress - 0.50) / 0.50;
            float ease = 1.0 - pow(1.0 - t, 2.0); // Ease-Out
            float halfH = lerp(0.0065, 0.51, ease);

            if (d.y > halfH) {
                output.color = float4(black, 1.0);
                return output;
            } else {
                float edgeDist = halfH - d.y;
                float beamWidth = 0.010;
                if (edgeDist < beamWidth && halfH < 0.48) {
                    float factor = 1.0 - (edgeDist / beamWidth);
                    float3 col = lerp(cyberCyan, plasmaWhite, factor);
                    output.color = float4(col, factor * 0.90);
                    return output;
                }

                if (t < 0.65) {
                    float veilAlpha = (0.65 - t) * 0.5 * (1.0 - scanline * 0.5);
                    output.color = float4(cyberCyan * 0.45, veilAlpha);
                    return output;
                }
                discard;
            }
        }
    }

    output.color = float4(black, 1.0);
    return output;
}

PixelShaderOutput main(VertexShaderOutput input) {
    if (transitionType == 2) {
        return RenderCRTOff(input.texcoord);
    } else if (transitionType == 1) {
        return RenderHexagon(input.texcoord);
    } else {
        return RenderSquareGrid(input.texcoord);
    }
}