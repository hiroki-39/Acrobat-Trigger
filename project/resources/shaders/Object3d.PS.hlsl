#include "Object3d.hlsli"
#define float32_t4 float4
#define float32_t4x4 float4x4
#define int32_t int
#define float32_t float

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t3 padding0;
    float32_t4x4 uvTransform;
    int32_t selectLightings;
    float32_t shininess;
    float32_t environmentCoefficient;
    float32_t fresnelF0;
    float32_t3 specularColor;
};

struct DirectionlLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};

struct PointLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
    float radius;
    float decry;
};

struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t3 direction;
    float32_t distance;
    float32_t decay;
    float32_t cosAngle;
    float32_t padding[2];
};

struct Camera
{
   float32_t3 worldPosition;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionlLight> gDirectionlLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLight> gPointLight : register(b3);
ConstantBuffer<SpotLight> gSpotLight : register(b4);

Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnvironmentMap : register(t1);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    float32_t3 normal = normalize(input.normal);
    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

    float32_t3 reflectLight = reflect(gDirectionlLight.direction, normal);
    float RdotE = dot(reflectLight, toEye);
    
    float32_t3 halfVector = normalize(-gDirectionlLight.direction + toEye);
    float NdotH = dot(normal, halfVector);
    float specularPow = pow(saturate(NdotH), gMaterial.shininess);
    
    float cos;
    float NdotL;
    
    switch (gMaterial.selectLightings)
    {
        case 0:
            output.color = gMaterial.color * textureColor;
            break;
        case 1:
            cos = saturate(dot(normal, -gDirectionlLight.direction));
            output.color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionlLight.color.rgb * cos * gDirectionlLight.intensity;
            output.color.a = gMaterial.color.a * textureColor.a;
            break;
        case 2:
            NdotL = dot(normal, -gDirectionlLight.direction);
            cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
            output.color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionlLight.color.rgb * cos * gDirectionlLight.intensity;
            output.color.a = gMaterial.color.a * textureColor.a;
            break;
       
        case 3:
            NdotL = dot(normal, -gDirectionlLight.direction);
            cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
            {
                float32_t3 ambient = gMaterial.color.rgb * textureColor.rgb * 0.2f; // 環境光を追加
                float32_t3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionlLight.color.rgb * cos * gDirectionlLight.intensity;
                float32_t3 specular = gDirectionlLight.color.rgb * gDirectionlLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
                output.color.rgb = ambient + diffuse + specular;
                output.color.a = gMaterial.color.a * textureColor.a;
            }
            break;
        case 4:
            {
                // DirectionalLight
                float32_t3 diffuseDirectionalLight = {0,0,0};
                float32_t3 specularDirectionalLight = {0,0,0};
                if (gDirectionlLight.intensity > 0.0f)
                {
                    NdotL = dot(normal, -gDirectionlLight.direction);
                    cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
                    diffuseDirectionalLight = gMaterial.color.rgb * textureColor.rgb * gDirectionlLight.color.rgb * cos * gDirectionlLight.intensity;
                    specularDirectionalLight = gDirectionlLight.color.rgb * gDirectionlLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
                }
                
                // ===== PointLight =====
                float32_t3 diffusePointLight = {0,0,0};
                float32_t3 specularPointLight = {0,0,0};
                float32_t distance = length(gPointLight.direction - input.worldPosition);
                
                if (gPointLight.intensity > 0.0f && distance <= gPointLight.radius)
                {
                    float32_t3 pointLightDirection = normalize(input.worldPosition - gPointLight.direction);
                    float32_t factor = pow(saturate(-distance / gPointLight.radius + 1.0), gPointLight.decry);
                    
                    // Diffuse
                    float NdotL_Point = dot(normal, -pointLightDirection);
                    float cosPoint = pow(NdotL_Point * 0.5f + 0.5f, 2.0f);
                    diffusePointLight = gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * cosPoint * gPointLight.intensity * factor;

                    // Blinn-Phong Specular
                    float32_t3 halfVectorPoint = normalize(-pointLightDirection + toEye);
                    float NdotH_Point = dot(normal, halfVectorPoint);
                    float specularPowPoint = pow(saturate(NdotH_Point), gMaterial.shininess);
                    specularPointLight = gPointLight.color.rgb * gPointLight.intensity * specularPowPoint * float32_t3(1.0f, 1.0f, 1.0f) * factor;
                }

                // ===== 全部足す =====
                output.color.rgb = diffuseDirectionalLight + specularDirectionalLight + diffusePointLight + specularPointLight;
                output.color.a = gMaterial.color.a * textureColor.a;
            }
            break;
        case 5:
            {
             // ===== SpotLight =====
                float32_t3 diffuseSpotLight = {0,0,0};
                float32_t3 specularSpotLight = {0,0,0};
                float32_t distance = length(gSpotLight.position - input.worldPosition);
                
                if (gSpotLight.intensity > 0.0f && distance <= gSpotLight.distance)
                {
                    // 表面 → 光源方向
                    float32_t3 spotLightDirection = normalize(input.worldPosition - gSpotLight.position);
                    float32_t attenuation = pow(saturate(-distance / gSpotLight.distance + 1.0f), gSpotLight.decay);

                    // 角度（Falloff）
                    float cosAngle = dot(spotLightDirection, gSpotLight.direction);
                    float falloffFactor = saturate((cosAngle - gSpotLight.cosAngle) / (1.0f - gSpotLight.cosAngle));

                    if (falloffFactor > 0.0f)
                    {
                        // ===== Diffuse =====
                        float NdotL = dot(normal, -spotLightDirection);
                        float cosDiffuse = pow(NdotL * 0.5f + 0.5f, 2.0f);
                        diffuseSpotLight = gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb * cosDiffuse * gSpotLight.intensity * attenuation * falloffFactor;

                        // ===== Specular (Blinn-Phong) =====
                        float32_t3 halfVector = normalize(-spotLightDirection + toEye);
                        float NdotH = dot(normal, halfVector);
                        float specularPow = pow(saturate(NdotH), gMaterial.shininess);
                        specularSpotLight = gSpotLight.color.rgb * gSpotLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f) * attenuation * falloffFactor;
                    }
                }

                // ===== 出力 =====
                output.color.rgb = diffuseSpotLight + specularSpotLight;
                output.color.a = gMaterial.color.a * textureColor.a;
            }
            break;
        case 6:
            {
                // ===== ビーム・高エネルギー発光（ホワイトハイライト + ネオンプラズマ） =====
                // 視線と法線の内積から、中心軸付近に鋭い光芯を入れる
                float32_t NdotV = saturate(dot(normal, toEye));
                
                // コア（超高エネルギーハイライト）：中心線を鋭く細くハイライト
                float32_t core = pow(NdotV, 4.0f);
                
                // リム（輪郭のプラズマ膜）：輪郭部にネオン光をまとう
                float32_t rim = pow(1.0f - NdotV, 2.0f);
                
                // ベースとなるネオンカラー
                float32_t3 baseColor = gMaterial.color.rgb;
                
                // 外周は鮮烈なネオンカラー、中心線にスッと細く白い光芯が通る
                float32_t3 glowColor = lerp(baseColor, float32_t3(1.0f, 1.0f, 1.0f), core * 0.70f);
                
                // 輪郭にプラズマ光を付加
                glowColor += baseColor * (rim * 0.5f);
                
                output.color.rgb = glowColor * textureColor.rgb;
                output.color.a = gMaterial.color.a * textureColor.a;
            }
            break;
        case 7:
            {
                // ===== スターフォックス風 サイバー仮想空間（ホログラム都市・デジタルグリッド地面） =====
                float32_t3 wp = input.worldPosition;
                float32_t dist = length(wp - gCamera.worldPosition);

                // カメラからの奥行きに応じた地平線エメラルドフォグ（遠方のビルがシルエット化して光に溶ける）
                float32_t fogFactor = saturate((dist - 120.0f) / 780.0f);

                // 視線と法線によるホログラム・リム発光（建物のエッジや輪郭がシアンに透けて光る）
                float32_t NdotV = saturate(abs(dot(normal, toEye)));
                float32_t rim = pow(1.0f - NdotV, 2.2f);

                // 地面判定（水平かつ高度が低いフラットフロアのみ。傾斜のある山のふもとは山岳として等高線描画）
                bool isGroundSurface = (normal.y > 0.96f && wp.y < 2.0f);

                float32_t3 holoColor = float32_t3(0.0f, 0.0f, 0.0f);
                float wireFrameMesh = 0.0f;
                float cleanWireMesh = 0.0f;

                if (isGroundSurface)
                {
                    // --- 1. 仮想空間の地面（ダークティール面 ＋ サイバーグリッド ＆ 滑走路ドット列） ---
                    float32_t3 groundBase = float32_t3(0.015f, 0.14f, 0.16f);

                    // メイングリッド（20m間隔の鋭いエメラルドライン）
                    float2 gMain = abs(frac(wp.xz / 20.0f - 0.5f) - 0.5f) / max(fwidth(wp.xz / 20.0f), 0.001f);
                    float mainLine = 1.0f - saturate(min(gMain.x, gMain.y));

                    // サブグリッド（5m間隔の繊細なデジタルメッシュ）
                    float2 gSub = abs(frac(wp.xz / 5.0f - 0.5f) - 0.5f) / max(fwidth(wp.xz / 5.0f), 0.001f);
                    float subLine = (1.0f - saturate(min(gSub.x, gSub.y))) * 0.35f;

                    wireFrameMesh = saturate(mainLine * 0.9f + subLine);

                    // [Clean Wireframe Mesh for Terrain Ground]
                    float2 gClean = abs(frac(wp.xz / 20.0f - 0.5f) - 0.5f) / max(fwidth(wp.xz / 20.0f), 0.0005f);
                    float cleanLineMain = saturate(2.2f - min(gClean.x, gClean.y));
                    float2 gCleanSub = abs(frac(wp.xz / 5.0f - 0.5f) - 0.5f) / max(fwidth(wp.xz / 5.0f), 0.0005f);
                    float cleanLineSub = saturate(1.4f - min(gCleanSub.x, gCleanSub.y)) * 0.40f;
                    cleanWireMesh = saturate(cleanLineMain + cleanLineSub) * smoothstep(750.0f, 150.0f, dist);

                    // 進行方向（Z軸）へ伸びる滑走路ガイドライン＆発光ドット列
                    float32_t runwayMask = smoothstep(2.2f, 0.2f, abs(frac(wp.x / 28.0f + 0.5f) - 0.5f) * 28.0f);
                    // 中央の列（X=0付近）のみをピンポイントで除外するマスク（左右端 X=±28m はそのまま維持）
                    float32_t centerMask = smoothstep(5.0f, 15.0f, abs(wp.x));
                    float32_t dotPattern = smoothstep(0.35f, 0.75f, sin(wp.z * 1.8f)) * smoothstep(280.0f, 30.0f, dist);
                    float32_t guideDots = runwayMask * dotPattern * centerMask * 0.85f;

                    // 遠方でグリッドのジャギーを抑えつつ美しい光面に収束
                    float32_t gridFade = smoothstep(550.0f, 60.0f, dist);
                    float32_t totalGrid = wireFrameMesh * gridFade;

                    float32_t3 neonCyan = float32_t3(0.20f, 0.98f, 0.88f);
                    float32_t3 brightWhiteCyan = float32_t3(0.65f, 1.0f, 0.96f);

                    holoColor = groundBase + neonCyan * totalGrid * 0.75f + brightWhiteCyan * guideDots;
                }
                else
                {
                    // --- 2. 仮想空間のビル群・橋・山岳（半透明感あるホログラムシルエット ＋ デジタル階層ライン） ---
                    // 高さに応じたグラデーション（根元はエメラルドの光、上層はクールなディープシアン）
                    float32_t heightGrad = saturate(wp.y / 65.0f);
                    float32_t3 bldgBase = lerp(float32_t3(0.03f, 0.24f, 0.27f), float32_t3(0.02f, 0.11f, 0.18f), heightGrad);

                    // 水平方向のホログラムスキャンライン（4m間隔の等高線・フロアライン）
                    float32_t hScan = pow(saturate(sin(wp.y * 1.6f) * 0.5f + 0.5f), 6.0f) * 0.45f;

                    // 垂直方向のエッジ・グリッドライン（建物の立体的なワイヤーフレーム感）
                    // ※山の斜面では垂直グリッドを抑え、等高線（hScan）を主役にして山の起伏を綺麗に表現
                    float2 vGrid = abs(frac(wp.xz / 8.0f - 0.5f) - 0.5f) / max(fwidth(wp.xz / 8.0f), 0.001f);
                    float vLine = (1.0f - saturate(min(vGrid.x, vGrid.y))) * smoothstep(450.0f, 80.0f, dist) * 0.40f;
                    float wallFactor = saturate(1.0f - saturate(normal.y) * 1.5f);

                    wireFrameMesh = saturate(hScan + vLine * wallFactor);

                    // [Clean Wireframe Skeleton for Buildings and Structures]
                    float2 bldgXZ = abs(frac(wp.xz / 8.0f - 0.5f) - 0.5f) / max(fwidth(wp.xz / 8.0f), 0.0005f);
                    float vWire = saturate(2.2f - min(bldgXZ.x, bldgXZ.y));

                    float bldgY = abs(frac(wp.y / 6.0f - 0.5f) - 0.5f) / max(fwidth(wp.y / 6.0f), 0.0005f);
                    float hWire = saturate(2.2f - bldgY);

                    // Normal discontinuity edge
                    float3 dNormal = fwidth(normal);
                    float sharpEdge = saturate(length(dNormal) * 4.0f);

                    float wireStructure = saturate(max(max(vWire, hWire), sharpEdge));
                    cleanWireMesh = wireStructure * smoothstep(800.0f, 150.0f, dist);

                    // 法線による面ごとの明暗コントラスト（ホログラムの多面体らしさ）
                    float32_t faceShade = 0.65f + 0.35f * abs(normal.x - normal.z * 0.5f);

                    float32_t3 rimNeon = float32_t3(0.28f, 1.0f, 0.92f) * (rim * 0.95f);
                    float32_t3 lineNeon = float32_t3(0.18f, 0.92f, 0.84f) * (hScan + vLine * wallFactor);

                    holoColor = bldgBase * faceShade + rimNeon + lineNeon;

                    // 地上付近（Y=0〜8m）の接地部が地平線の光と溶け合うボトムグロー
                    float32_t bottomGlow = exp(-max(0.0f, wp.y - 1.0f) * 0.16f);
                    holoColor += float32_t3(0.15f, 0.85f, 0.78f) * (bottomGlow * 0.32f);
                }

                // 遠方の地平線エメラルドグローへ美しくフェード（画像の奥のビル群が光の中に浮かぶ表現）
                float32_t3 horizonFogColor = float32_t3(0.12f, 0.72f, 0.68f);
                output.color.rgb = lerp(holoColor, horizonFogColor, fogFactor * 0.68f);
                output.color.a = 1.0f;

                // ===== 仮想空間マテリアライズ・スキャンウェーブ（地形形成演出） =====
                // gMaterial.color.a が 1.0 未満のとき、2段階の光の波が奔り、世界が形成される！
                // 段階 1: 完全な虚空（何もない） -> 先行波によりワイヤーフレームが形成される
                // 段階 2: 後続のスキャン波によりワイヤーフレームにテクスチャ（面）が張られて実体化する
                if (gMaterial.color.a < 0.999f)
                {
                    float materializeProgress = saturate(gMaterial.color.a); // 0.0 -> 1.0

                    // 1. Advance Wireframe Construction Wave
                    float wireT = saturate(materializeProgress * 1.82f);
                    float scanDistWire = lerp(-40.0f, 980.0f, wireT);

                    // 2. Trailing Solid Texture Scan Wave
                    float solidT = saturate((materializeProgress - 0.54f) / 0.46f);
                    float scanDistSolid = lerp(-60.0f, 850.0f, solidT);

                    float zDist = wp.z - gCamera.worldPosition.z;
                    float heightDelay = max(0.0f, wp.y) * 1.5f;
                    float scanCoord = zDist + heightDelay;

                    // [Stage 1: Void]
                    if (scanCoord > scanDistWire)
                    {
                        discard;
                    }

                    // [Stage 2: Pure Wireframe Construction Phase (No black solid surfaces!)]
                    if (scanCoord > scanDistSolid)
                    {
                        // 1. Advance construction laser wave
                        float wireFrontDelta = abs(scanCoord - scanDistWire);
                        float wireFrontPulse = smoothstep(28.0f, 0.0f, wireFrontDelta);

                        // Discard all solid faces completely so no black occlusion blocks the background!
                        if (cleanWireMesh < 0.05f && wireFrontPulse < 0.12f)
                        {
                            discard;
                        }

                        // 2. Vertical rising beam on building columns
                        float beamRise = frac(wp.y * 0.08f - materializeProgress * 6.0f);
                        float beamPulse = pow(beamRise, 4.0f) * (isGroundSurface ? 0.0f : 1.0f);

                        // 3. Photon pulse running along grid lines
                        float gridPulse = sin(wp.z * 0.25f - materializeProgress * 30.0f) * 0.5f + 0.5f;
                        float runningLight = pow(gridPulse, 3.0f);

                        // 4. Wireframe energy boost
                        float wireAge = saturate((scanDistWire - scanCoord) / 120.0f);
                        float energyBoost = lerp(3.0f, 1.4f, wireAge);

                        // Wireframe skeleton (Emerald green for ground, cyber cyan for buildings)
                        float32_t3 wireBaseColor = isGroundSurface ? float32_t3(0.08f, 1.0f, 0.72f) : float32_t3(0.12f, 0.95f, 1.0f);
                        float32_t3 wireColor = wireBaseColor * (cleanWireMesh * energyBoost);
                        wireColor += float32_t3(0.25f, 1.0f, 0.95f) * (cleanWireMesh * runningLight * 1.8f);
                        wireColor += float32_t3(0.15f, 1.0f, 0.70f) * (cleanWireMesh * beamPulse * 2.5f);

                        // Frontier laser wave (Vibrant electric cyan-emerald)
                        float32_t3 frontLaser = float32_t3(0.30f, 1.0f, 0.90f) * (wireFrontPulse * 4.0f);

                        output.color.rgb = wireColor + frontLaser;
                        output.color.a = 1.0f;
                    }
                    else
                    {
                        // [Stage 3: Solid Dissolve Transition Phase]
                        float solidDelta = scanDistSolid - scanCoord;
                        const float kSolidBandWidth = 32.0f;

                        // Laser frontier edge
                        float frontPulse = smoothstep(16.0f, 0.0f, abs(scanCoord - scanDistSolid));
                        float scanStripe = sin(scanCoord * 1.8f) * 0.5f + 0.5f;
                        float32_t3 frontGlow = float32_t3(0.22f, 0.98f, 0.92f) * (frontPulse * (3.0f + scanStripe * 1.5f));

                        if (solidDelta < kSolidBandWidth)
                        {
                            float32_t3 buildBlock = floor(wp.xyz * 0.45f);
                            float blockSeed = frac(sin(dot(buildBlock, float32_t3(12.9898f, 78.233f, 45.164f))) * 43758.5453f);
                            float buildProgress = solidDelta / kSolidBandWidth;

                            // Unmaterialized blocks: completely discard faces, wire only!
                            if (buildProgress < blockSeed)
                            {
                                if (cleanWireMesh < 0.05f && frontPulse < 0.12f)
                                {
                                    discard;
                                }
                                float32_t3 activeWire = (isGroundSurface ? float32_t3(0.08f, 1.0f, 0.72f) : float32_t3(0.15f, 0.95f, 1.0f)) * saturate(cleanWireMesh * 2.6f);
                                output.color.rgb = activeWire + frontGlow;
                                output.color.a = 1.0f;
                            }
                            else
                            {
                                float blockEdge = saturate(1.0f - abs(buildProgress - blockSeed) * 10.0f);
                                float32_t3 blockFlash = float32_t3(0.30f, 1.0f, 0.78f) * (blockEdge * 3.0f);

                                float trailPulse = exp(-solidDelta * 0.08f) * saturate(wireFrameMesh * 1.5f + 0.15f);
                                float32_t3 trailGlow = float32_t3(0.12f, 0.98f, 0.75f) * (trailPulse * 2.2f);

                                output.color.rgb = output.color.rgb + frontGlow + blockFlash + trailGlow;
                            }
                        }
                        else
                        {
                            output.color.rgb = output.color.rgb;
                        }
                    }
                }
            }
            break;
        case 8:
            {
                // ===== スターフォックス風 サイバー仮想空間 自機（ホログラム装甲・ネオンパネル・ポリゴン消滅ディゾルブ） =====
                float32_t3 wp = input.worldPosition;
                float32_t NdotV = saturate(abs(dot(normal, toEye)));
                // 輪郭のエメラルド／シアンのネオンリム発光
                float32_t rim = pow(1.0f - NdotV, 2.2f);

                // 基本装甲カラー：ハイテクなサイバーディープティール＆ネイビー
                float32_t3 cyberHull = float32_t3(0.04f, 0.20f, 0.26f);
                // 上面・側面の立体的な陰影
                float32_t shade = 0.55f + 0.45f * saturate(dot(normal, float32_t3(0.0f, 1.0f, 0.4f)));

                // 鮮やかなネオンシアン＆エメラルドの発光色
                float32_t3 neonCyan = float32_t3(0.20f, 1.0f, 0.90f);
                float32_t3 neonEmerald = float32_t3(0.05f, 0.95f, 0.72f);

                // 輪郭ネオンリム発光
                float32_t3 rimLight = neonCyan * (rim * 1.5f);

                // 機体表面の幾何学デジタルスキャンライン＆ワイヤーフレーム
                float2 uvMesh = abs(frac(input.texcoord * 18.0f - 0.5f) - 0.5f);
                float wireLine = (1.0f - saturate(min(uvMesh.x, uvMesh.y) * 12.0f)) * 0.45f;
                // 機体を流れるエナジーパルス（奥から手前へ走る光の波）
                float pulse = pow(saturate(sin(wp.z * 1.4f - wp.y * 2.5f) * 0.5f + 0.5f), 7.0f) * 0.60f;

                float32_t3 shipGlow = (cyberHull * shade) + rimLight + (neonEmerald * wireLine) + (neonCyan * pulse);

                // 【点火・結合時の強烈フラッシュ発光ブースト】：
                // gMaterial.color.rgb が 1.0 を超えているオーバーブースト時、エッジとコアを白熱超発光！
                float maxCol = max(gMaterial.color.r, max(gMaterial.color.g, gMaterial.color.b));
                if (maxCol > 1.05f)
                {
                    float flashIntensity = maxCol - 1.0f;
                    float32_t3 whiteCoreFlash = float32_t3(0.92f, 1.0f, 1.0f) * (flashIntensity * (rim * 3.5f + pulse * 2.5f + 1.2f));
                    shipGlow += whiteCoreFlash;
                }

                output.color.rgb = shipGlow * gMaterial.color.rgb;
                output.color.a = 1.0f;

                // ===== ポリゴンディゾルブ消滅（gMaterial.color.a が 1.0 未満のときに発動） =====
                if (gMaterial.color.a < 0.999f)
                {
                    float dissolveProgress = 1.0f - gMaterial.color.a; // 0.0 -> 1.0
                    
                    // 3D空間座標に基づくデジタルポリゴングリッドパターン（三角・六角ブロック状の崩壊ノイズ）
                    float32_t3 blockCoord = floor(wp.xyz * 4.5f);
                    float polyPattern = frac(sin(dot(blockCoord, float32_t3(12.9898f, 78.233f, 45.164f))) * 43758.5453f);
                    float dissolveThreshold = dissolveProgress * 1.35f - 0.15f;

                    if (polyPattern < dissolveThreshold)
                    {
                        discard;
                    }

                    // ポリゴン崩壊境界の超高輝度ネオン発光エッジ
                    float edge = saturate(1.0f - abs(polyPattern - dissolveThreshold) * 10.0f);
                    output.color.rgb += float32_t3(0.70f, 1.0f, 0.98f) * (edge * 5.0f);
                }
            }
            break;
    }

    // 環境マッピング（仮想空間シェーダー case 7, 8 時は独自のホログラム発光を維持するため除外）
    if (gMaterial.environmentCoefficient > 0.0f && gMaterial.selectLightings != 7 && gMaterial.selectLightings != 8)
    {
        // 視線ベクトル（表面からカメラへ）
        float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        // 反射ベクトル（入射ベクトルを法線で反射させる。reflectの第1引数は「光源から表面へ」なので-toEye）
        float32_t3 reflectVector = reflect(-toEye, normal);
        
        // 環境マップをサンプリング
        float32_t3 environmentColor = gEnvironmentMap.Sample(gSampler, reflectVector).rgb;
        
        // フレネル反射 (Schlick近似)
        float cosTheta = saturate(dot(toEye, normal));
        float f0 = gMaterial.fresnelF0;
        float fresnel = f0 + (1.0f - f0) * pow(1.0f - cosTheta, 5.0f);
        
        // 環境マップの色を加算（フレネルを適用）
        output.color.rgb += environmentColor * gMaterial.environmentCoefficient * fresnel;
    }    // 半透明のピクセル（アンチエイリアスのフチなど）も破棄して黒いフチを消す
    if (output.color.a <= 0.5f) {
        discard;
    }

    return output;
}
