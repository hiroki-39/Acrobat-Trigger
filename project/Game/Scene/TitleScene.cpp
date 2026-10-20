#define NOMINMAX
#include "TitleScene.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "KHEngine/Graphics/Resource/Descriptor/SrvManager.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/3d/Object/Object3dCommon.h"
#include "KHEngine/Graphics/3d/Particle/ParticleManager.h"
#include "KHEngine/Graphics/Billboard/Billboard.h"
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Graphics/2d/SpriteCommon.h"
#include "KHEngine/Scene/SceneManager.h"
#ifdef ENABLE_EDITOR
#include "KHEngine/Debug/Editor/EffectStudio.h"
#endif
#include "KHEngine/Graphics/PostProcess/PostProcess.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "externals/imgui/imgui.h"
#include "externals/nlohmann/json.hpp"
#include "KHEngine/Scene/LevelLoader.h"
#include "KHEngine/Core/Resource/ResourceLocator.h"
#include <limits>
#include <memory>
#include <fstream>
#include <cmath>
#include <numbers>
#include <algorithm>
#include <Windows.h>

void TitleScene::Initialize()
{
    auto services = Services();
    if (!services) return;

    auto dxCommon = services->GetDirectXCommon();
    auto object3dCommon = services->GetObject3dCommon();
    auto srvManager = services->GetSrvManager();
    auto texManager = TextureManager::GetInstance();
    if (!texManager) return;

    if (dxCommon) dxCommon->BeginTextureUploadBatch();

    // -------------------------------------------------------------
    // 3Dカメラの初期化
    // -------------------------------------------------------------
    camera_ = std::make_unique<Camera>();
    camera_->SetTranslate(cameraPos_);
    camera_->SetRotation(cameraRot_);

    debugCamera_ = std::make_unique<Camera>();
    debugCamera_->SetTranslate(cameraPos_);
    debugCamera_->SetRotation(cameraRot_);

    if (object3dCommon)
    {
        object3dCommon->SetDefaultCamera(camera_.get());
    }

    // -------------------------------------------------------------
    // スカイボックスの初期化（スターフォックス風サイバー仮想空間対応）
    // -------------------------------------------------------------
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(dxCommon, "resources/skybox.dds");
    skybox_->SetVirtualSpace(isVirtualSpace_);

    // -------------------------------------------------------------
    // ゲームプレイ用アセットの事前キャッシュ（プリロード）：
    // シーン遷移時のフリーズ（1.7秒）を解消し、瞬時遷移を実現する
    // -------------------------------------------------------------
    OutputDebugStringA("\n[Preload] Starting background preload for GamePlayScene...\n");
    auto modelManager = ModelManager::GetInstance();
    if (modelManager)
    {
        // 1. ゲームプレイシーン（template.json）用 地形・背景・建造物モデルの事前ロード
        modelManager->LoadModel("Terrain_00_SEA.obj");
        modelManager->LoadModel("Terrain_01_PLAINS.obj");
        modelManager->LoadModel("Terrain_02_HILLS.obj");
        modelManager->LoadModel("Terrain_03_CANYON.obj");
        modelManager->LoadModel("Terrain_Background_Mountains.obj");
        modelManager->LoadModel("arch_1.obj");
        modelManager->LoadModel("collapsed bridge.obj");
        modelManager->LoadModel("bridge.obj");
        modelManager->LoadModel("top_building.obj");
        modelManager->LoadModel("under_building.obj");
        modelManager->LoadModel("wall.obj");

        // 2. プレイヤー・敵・弾・障害物モデル
        modelManager->LoadModel("player.obj");
        modelManager->LoadModel("cube.obj");
        modelManager->LoadModel("plane.obj");
        modelManager->LoadModel("enemy.obj");
        modelManager->LoadModel("fixedTurret.obj");
        modelManager->LoadModel("missile.obj");
        modelManager->LoadModel("RockOn.obj");
        modelManager->LoadModel("collider_cube_player.obj");
        modelManager->LoadModel("collider_sphere_enemy.obj");
        modelManager->LoadModel("collider_cube_enemy.obj");
        modelManager->LoadModel("Ring.obj");
        modelManager->LoadModel("heal.obj");
        modelManager->LoadModel("rail.obj");
        modelManager->LoadModel("beam.obj");
    }

    // 3. スカイボックス（26.7MB）および主要テクスチャの事前ロード
    texManager->LoadTexture("resources/skybox.dds");
    texManager->LoadTexture("uvChecker.png");
    texManager->LoadTexture("circle.png");
    texManager->LoadTexture("circle2.png");
    texManager->LoadTexture("gradationLine.png");
    texManager->LoadTexture("sprites/white.png");
    texManager->LoadTexture("prticle_kira.png");
    texManager->LoadTexture("hart.png");
    texManager->LoadTexture("light.png");
    texManager->LoadTexture("sprites/UI/ringGet_icon_outline.png");
    texManager->LoadTexture("sprites/UI/ringGet_icon.png");

    // -------------------------------------------------------------
    // タイトルシーン用マップ（title.json）の読み込みと配置
    // ※テクスチャ一括アップロードの前にモデル・テクスチャをロードする
    // ※地形・建造物のシームレス無限ループ描画用スロットを生成
    // -------------------------------------------------------------
    titleMapNodes_.clear();
    flightProgressZ_ = 0.0f;

    auto levelData = LevelLoader::Load("resources/json/maps/title/title.json");

    if (levelData && object3dCommon)
    {
        uint32_t skyboxTexIdx = skybox_ ? skybox_->GetCubemapSrvIndex() : 0;
        auto modelMgr = ModelManager::GetInstance();

        for (const auto& node : levelData->objects)
        {
            if (node.type == "MESH")
            {
                std::string modelName = node.fileName;
                if (modelName.empty())
                {
                    std::string baseName = node.name;
                    size_t dotPos = baseName.rfind('.');
                    if (dotPos != std::string::npos && dotPos + 1 < baseName.size())
                    {
                        bool isNumber = true;
                        for (size_t i = dotPos + 1; i < baseName.size(); ++i)
                        {
                            if (!std::isdigit(static_cast<unsigned char>(baseName[i])))
                            {
                                isNumber = false;
                                break;
                            }
                        }
                        if (isNumber)
                        {
                            baseName = baseName.substr(0, dotPos);
                        }
                    }

                    std::string baseObjName = baseName;
                    if (baseObjName.find(".obj") == std::string::npos) baseObjName += ".obj";

                    if (!ResourceLocator::Resolve(baseObjName, ResourceLocator::AssetType::Model3D).empty())
                    {
                        modelName = baseObjName;
                    }
                    else
                    {
                        // アンダースコアをスペースに置換 (例: collapsed_bridge -> collapsed bridge)
                        std::string spaceName = baseObjName;
                        std::replace(spaceName.begin(), spaceName.end(), '_', ' ');
                        if (!ResourceLocator::Resolve(spaceName, ResourceLocator::AssetType::Model3D).empty())
                        {
                            modelName = spaceName;
                        }
                        else
                        {
                            modelName = baseObjName;
                        }
                    }
                }
                else if (modelName.find(".obj") == std::string::npos)
                {
                    modelName += ".obj";
                }

                std::string resolvedPath = ResourceLocator::Resolve(modelName, ResourceLocator::AssetType::Model3D);

                bool hasModel = false;
                if (modelMgr)
                {
                    modelMgr->LoadModel(modelName);
                    auto m = modelMgr->FindModel(modelName);
                    hasModel = (m != nullptr);
                }

                // 指定テクスチャがある場合はテクスチャもロード
                if (!node.texturePath.empty())
                {
                    std::string texName = node.texturePath;
                    size_t dotPos = texName.rfind('.');
                    if (dotPos != std::string::npos && dotPos + 1 < texName.size())
                    {
                        bool isNum = true;
                        for (size_t i = dotPos + 1; i < texName.size(); ++i)
                        {
                            if (!std::isdigit(static_cast<unsigned char>(texName[i]))) { isNum = false; break; }
                        }
                        if (isNum) { texName = texName.substr(0, dotPos); }
                    }
                    texManager->LoadTexture(texName);
                }

                Vector3 rotRad;
                rotRad.x = node.rotation.x * (std::numbers::pi_v<float> / 180.0f);
                rotRad.y = node.rotation.y * (std::numbers::pi_v<float> / 180.0f);
                rotRad.z = node.rotation.z * (std::numbers::pi_v<float> / 180.0f);

                // 全ての地形・地面・建物・橋を統一された2000m周期でシームレス無限ループ配置
                // 建物(billding)は長さ約2000mのため4スロットで前後を完全にカバーし、途切れることなく連続配置
                // 地面・山岳・橋は3スロット（手前・中央・奥）で配置
                bool isBuilding = (node.name.find("billding") != std::string::npos) ||
                                  (modelName.find("billding") != std::string::npos);
                float period = 2000.0f;
                int slotCount = isBuilding ? 4 : 3;

                for (int slot = 0; slot < slotCount; ++slot)
                {
                    auto obj = std::make_unique<Object3d>();
                    obj->Initialize(object3dCommon);
                    if (hasModel)
                    {
                        obj->SetModel(modelName);
                    }
                    obj->SetTranslate(node.translation);
                    obj->SetRotation(rotRad);
                    obj->SetScale(node.scale);
                    obj->SetEnvironmentTextureIndex(skyboxTexIdx);
                    obj->SetEnvironmentCoefficient(0.45f);
                    obj->SetEnableLighting(true);
                    if (isVirtualSpace_)
                    {
                        // サイバー仮想空間シェーダー（case 7: エメラルドグリッド地面＆ホログラム建造物）を適用
                        obj->SetSelectLightings(7);
                    }
                    obj->Update();

                    TitleMapObjectNode mapNode;
                    mapNode.object = std::move(obj);
                    mapNode.name = node.name;
                    mapNode.baseTranslation = node.translation;
                    mapNode.rotation = rotRad;
                    mapNode.scale = node.scale;
                    mapNode.loopPeriod = period;
                    mapNode.slotIndex = slot;

                    titleMapNodes_.push_back(std::move(mapNode));
                }
            }
        }
    }

    // 全テクスチャ（タイトルマップ用・プリロード用・スカイボックス用）を一括GPUアップロード
    texManager->ExecuteUploadCommands();
    texManager->ClearIntermediateResources();
    OutputDebugStringA("[Preload] Preload & Title Map texture upload completed!\n\n");

    // -------------------------------------------------------------
    // タイトル自機の生成と初期化
    // -------------------------------------------------------------
    if (object3dCommon)
    {
        playerObj_ = std::make_unique<Object3d>();
        playerObj_->Initialize(object3dCommon);
        playerObj_->SetModel("player.obj");

        // プレイヤー設定ファイルからスケールとカラーを読み込み
        std::ifstream file("resources/json/player/player_settings.json");
        if (file.is_open())
        {
            try
            {
                nlohmann::json j;
                file >> j;
                if (j.contains("playerScale"))
                {
                    auto s = j["playerScale"];
                    if (s.is_array() && s.size() == 3)
                    {
                        playerScale_ = { s[0], s[1], s[2] };
                    }
                }
                if (j.contains("color"))
                {
                    auto c = j["color"];
                    if (c.is_array() && c.size() == 4)
                    {
                        playerColor_ = { c[0], c[1], c[2], c[3] };
                    }
                }
            }
            catch (...) {}
            file.close();
        }

        playerObj_->SetScale(playerScale_);
        // 仮想空間に調和するスタイリッシュなサイバーホログラム自機マテリアル（case 8）
        playerColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
        playerObj_->SetColor(playerColor_);
        playerObj_->SetSelectLightings(8); // サイバー仮想空間自機シェーダー（ネオンリム・パネル・ポリゴン消滅ディゾルブ）
        if (skybox_)
        {
            playerObj_->SetEnvironmentTextureIndex(skybox_->GetCubemapSrvIndex());
        }
        playerObj_->SetEnvironmentCoefficient(0.0f); // ホログラム発光を純粋に保つため環境反射はオフ
        playerObj_->SetEnableLighting(true);

        // 巡航飛行の初期位置に配置
        playerPos_ = targetPos_;
        playerRot_ = { 0.0f, 0.0f, 0.0f }; // 回転はすべて 0
        playerObj_->SetTranslate(playerPos_);
        playerObj_->SetRotation(playerRot_);
        playerObj_->Update();
    }

    // -------------------------------------------------------------
    // ブースト・気流粒子・着弾スパークエフェクトの初期化
    // -------------------------------------------------------------
    if (srvManager)
    {
        ParticleManager::GetInstance()->RegisterQuad("quad", "circle2.png");
        ParticleManager::GetInstance()->RegisterCylinder("Cylinder", "resources/sprites/effect/gradationLine.png", 16, 0.5f, 0.5f, 4.0f);

        thrusterEffect_.Initialize(dxCommon, srvManager);
        thrusterEffect_.LoadFromJson("thruster.json");
        if (playerObj_)
        {
            thrusterEffect_.SetParentMatrix(&playerObj_->GetmatWorld());
        }
        thrusterEffect_.SetPosition(nozzleOffset_);

        windEffect_.Initialize(dxCommon, srvManager);
        windEffect_.LoadFromJson("wind.json");
        for (auto& node : windEffect_.GetNodes())
        {
            node->emitter.SetUseBillboard(false);
            auto param = node->emitter.GetParameter();
            param.minVelocity.z = -10.0f;
            param.maxVelocity.z = -14.0f;
            param.maxLifeTime = 0.60f;
            param.minLifeTime = 0.60f * 0.65f;
            param.maxScale.y = 5.00f;
            param.minScale.y = 5.00f * 0.45f;
            node->emitter.SetParameter(param);
        }

        randomEngine_.seed(1337);
    }

    // 3Dポリゴン破片用モデルのロード
    ModelManager::GetInstance()->LoadModel("polygon_shard.obj");

    // 演出タイマーの初期化（起動直後から手前飛び込み登場演出開始）
    isIntro_ = true;
    introTimer_ = 0.0f;
    introBlendTimer_ = 0.0f;
    idleTimer_ = 0.0f;
    isDiving_ = false;
    diveTimer_ = 0.0f;
    demoTimer_ = 0.0f;
    flightProgressZ_ = 0.0f;
    dissolveProgress_ = 0.0f;
    polyShards3D_.clear();

    // タイトルBGMの再生（ループ再生、音量0.25）
    SoundManager::GetInstance()->PlayBGM("tileBGM.mp3", 0.25f, true);
}

void TitleScene::UpdateDebugCamera(float dt)
{
    auto services = Services();
    if (!services || !debugCamera_) return;

    auto input = services->GetInput();
    if (!input) return;

    const float kRotateSpeed = 0.005f;
    const float kMoveSpeed = debugCameraMoveSpeed_;

    LONG dx = input->GetMouseMoveX();
    LONG dy = input->GetMouseMoveY();
    LONG wheel = input->GetMouseWheel();

    // マウス右ボタンドラッグでカメラ視点回転
    if (input->PushMouseButton(1))
    {
        Vector3 rot = debugCamera_->GetRotation();
        rot.y += static_cast<float>(dx) * kRotateSpeed;
        rot.x += static_cast<float>(dy) * kRotateSpeed;

        const float kMaxPitch = 1.5f;
        const float kMinPitch = -1.5f;
        rot.x = std::clamp(rot.x, kMinPitch, kMaxPitch);

        debugCamera_->SetRotation(rot);
    }

    // 移動速度（Shiftキーで高速移動）
    float currentMoveSpeed = kMoveSpeed;
    if (input->PushKey(DIK_LSHIFT) || input->PushKey(DIK_RSHIFT))
    {
        currentMoveSpeed *= 8.0f;
    }
    float moveStep = currentMoveSpeed * dt;

    Vector3 pos = debugCamera_->GetTranslate();
    Vector3 rot = debugCamera_->GetRotation();
    float yaw = rot.y;

    Vector3 forward = { std::sinf(yaw), 0.0f, std::cosf(yaw) };
    Vector3 right = { std::cosf(yaw), 0.0f, -std::sinf(yaw) };

    auto normalize = [](Vector3 v)
    {
        float len = std::sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len > 1e-6f) { v.x /= len; v.y /= len; v.z /= len; }
        return v;
    };

    forward = normalize(forward);
    right = normalize(right);

    // WASD による水平移動
    if (input->PushKey(DIK_W))
    {
        pos.x += forward.x * moveStep;
        pos.z += forward.z * moveStep;
    }
    if (input->PushKey(DIK_S))
    {
        pos.x -= forward.x * moveStep;
        pos.z -= forward.z * moveStep;
    }
    if (input->PushKey(DIK_D))
    {
        pos.x += right.x * moveStep;
        pos.z += right.z * moveStep;
    }
    if (input->PushKey(DIK_A))
    {
        pos.x -= right.x * moveStep;
        pos.z -= right.z * moveStep;
    }

    // E / Q による上下移動
    if (input->PushKey(DIK_E))
    {
        pos.y += moveStep;
    }
    if (input->PushKey(DIK_Q))
    {
        pos.y -= moveStep;
    }

    // マウスホイールによる前後移動
    if (wheel != 0)
    {
        float wheelStep = static_cast<float>(wheel) * 0.01f;
        pos.x += forward.x * wheelStep;
        pos.z += forward.z * wheelStep;
    }

    debugCamera_->SetTranslate(pos);
    debugCamera_->Update();
}

void TitleScene::SpawnShipPolygonShards(const Vector3& shipPos)
{
    auto services = Services();
    if (!services) return;
    auto object3dCommon = services->GetObject3dCommon();
    if (!object3dCommon) return;

    polyShards3D_.clear();

    std::mt19937 rng(54321);
    std::uniform_real_distribution<float> distOffset(-1.8f, 1.8f);
    std::uniform_real_distribution<float> distVel(-10.0f, 10.0f);
    std::uniform_real_distribution<float> distVelY(-6.0f, 9.0f);
    std::uniform_real_distribution<float> distVelZ(-4.0f, 16.0f);
    std::uniform_real_distribution<float> distRot(-3.14f, 3.14f);
    std::uniform_real_distribution<float> distRotSpd(-8.0f, 8.0f);
    std::uniform_real_distribution<float> distScale(0.40f, 1.10f);

    // 自機周囲から飛び散る48個の3Dポリゴン破片
    for (int i = 0; i < 48; ++i)
    {
        PolygonShard3D shard;
        shard.position = {
            shipPos.x + distOffset(rng) * 1.5f,
            shipPos.y + distOffset(rng) * 0.7f,
            shipPos.z + distOffset(rng) * 2.0f
        };
        shard.velocity = {
            distVel(rng),
            distVelY(rng) + 1.5f,
            distVelZ(rng) + forwardSpeed_
        };
        shard.rotation = { distRot(rng), distRot(rng), distRot(rng) };
        shard.rotSpeed = { distRotSpd(rng), distRotSpd(rng), distRotSpd(rng) };
        float sc = distScale(rng);
        shard.scale = { sc, sc, sc };
        shard.life = 1.0f;

        // エメラルドグリーン＆ネオンシアンの発光ポリゴンカラー
        if (i % 3 == 0)
            shard.color = { 0.10f, 1.0f, 0.85f, 1.0f };
        else if (i % 3 == 1)
            shard.color = { 0.25f, 0.95f, 1.0f, 1.0f };
        else
            shard.color = { 0.05f, 0.85f, 0.65f, 1.0f };

        shard.object = std::make_unique<Object3d>();
        shard.object->Initialize(object3dCommon);
        shard.object->SetModel("polygon_shard.obj");
        shard.object->SetSelectLightings(6); // ネオン高輝度プラズマ発光シェーダー
        shard.object->SetColor(shard.color);
        shard.object->SetScale(shard.scale);
        shard.object->SetRotation(shard.rotation);
        shard.object->SetTranslate(shard.position);
        shard.object->Update();

        polyShards3D_.push_back(std::move(shard));
    }
}

void TitleScene::TriggerStartTransition(bool isPreview)
{
    if (isDiving_) return;

    auto services = Services();
    if (services)
    {
        services->SetGamePlaying(true);
    }

    isDiving_ = true;
    isPreviewTransition_ = isPreview;
    diveTimer_ = 0.0f;
    diveStartPos_ = playerPos_;
    diveStartRot_ = playerRot_;
    diveStartCamPos_ = cameraPos_;
    diveStartCamRot_ = cameraRot_;
    diveStartProgressZ_ = flightProgressZ_;

    // 仮想空間自機ポリゴン消滅演出の状態リセット
    dissolveProgress_ = 0.0f;
    glitchStarted_ = false;
    polyShards3D_.clear();

    if (startTransitionType_ == StartTransitionType::VirtualShatter)
    {
        // タイムライン（合計 1.15秒）:
        //  0.00s〜0.45s: 決定ボタン押下 ➔ 自機がポリゴンに分解し、破片が周囲へ美しく分散（グリッチなし）
        //  0.45s〜1.15s: ポリゴンが分散したところから、激しいグリッチ振動＆カメラシェイクが発動！
        //  1.15s: 通常のシーンフェードトランジションでゲームシーンへ移行
        diveDuration_ = isPreview ? 1.35f : 1.15f;

        SpawnShipPolygonShards(diveStartPos_);
    }
    else if (startTransitionType_ == StartTransitionType::SonicAfterburner)
    {
        diveDuration_ = 1.3f;
    }
    else
    {
        diveDuration_ = 1.4f;
    }

    // 決定SE再生
    SoundManager::GetInstance()->PlaySE("select.mp3", 0.95f);
}

void TitleScene::Update()
{
    UpdateSprites();

    auto services = Services();
    if (!services) return;

    auto input = services->GetInput();
    auto object3dCommon = services->GetObject3dCommon();

#ifdef ENABLE_EDITOR
    bool isPlaying = services->IsGamePlaying();
#else
    bool isPlaying = true;
#endif
    const float dt = isPlaying ? (1.0f / 60.0f) : 0.0f;

    // -------------------------------------------------------------
    // デバッグカメラ切り替え（F2キー） / ミュート切り替え（Mキー）
    // -------------------------------------------------------------
    if (input && input->TriggerKey(DIK_F2))
    {
        isDebugCamera_ = !isDebugCamera_;
        if (isDebugCamera_ && camera_ && debugCamera_)
        {
            debugCamera_->SetTranslate(camera_->GetTranslate());
            debugCamera_->SetRotation(camera_->GetRotation());
            debugCamera_->Update();
        }
    }
    if (input && input->TriggerKey(DIK_M))
    {
        SoundManager::GetInstance()->ToggleMasterMute();
    }

    // -------------------------------------------------------------
    // 1. カメラ更新とアクティブカメラの決定（自機・描画オブジェクトより先に確定）
    // -------------------------------------------------------------
    if (camera_)
    {
        camera_->SetTranslate(cameraPos_);
        camera_->SetRotation(cameraRot_);
        camera_->Update();
    }

    if (isDebugCamera_)
    {
        // デバッグカメラは一時停止中（Pause）でも動かせるように固定デルタタイムを使用
        UpdateDebugCamera(1.0f / 60.0f);
    }

    // デバッグカメラ有効時はデバッグカメラをアクティブカメラとして使用
    Camera* activeCamera = (isDebugCamera_ && debugCamera_) ? debugCamera_.get() : camera_.get();

    // 描画エンジンにアクティブカメラを設定
    if (object3dCommon && activeCamera)
    {
        object3dCommon->SetDefaultCamera(activeCamera);
    }

    // -------------------------------------------------------------
    // 決定ボタン押下時の判定（演出開始）
    // -------------------------------------------------------------
    bool isDecisionPressed = false;

    if (input && isPlaying)
    {
        if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) ||
            input->TriggerPadButton(XINPUT_GAMEPAD_A) || input->TriggerPadButton(XINPUT_GAMEPAD_START))
        {
            isDecisionPressed = true;
        }
    }

    // デモプレイ中：自機登場演出（約1.4秒）完了後、巡航飛行をじっくり眺めてから自動突入
    if (services && services->IsDemoMode())
    {
        if (!isIntro_ && !isDiving_)
        {
            demoTimer_ += dt;
            if (demoTimer_ >= demoCruiseDuration_)
            {
                isDecisionPressed = true;
            }
        }
    }
    else
    {
        demoTimer_ = 0.0f;
    }

    if (isDecisionPressed && !isDiving_)
    {
        TriggerStartTransition(false); // 通常のゲームプレイ突入
    }

    // -------------------------------------------------------------
    // 2. 自機のアニメーション計算・更新
    // -------------------------------------------------------------
    Vector3 curPos = targetPos_;
    Vector3 curRot = playerRot_; // 基本回転はすべて 0
    float motionBlend = 0.0f; // 巡航モーションブレンド率 (0.0 -> 1.0)

    if (isDiving_)
    {
        if (prop2PausePreview_ && isPreviewTransition_)
        {
            // 一時停止（ポーズ）中はシークバーのスライダー値で時間を固定
            diveTimer_ = prop2PreviewProgress_ * diveDuration_;
        }
        else
        {
            diveTimer_ += dt;
        }

        float t = std::clamp(diveTimer_ / diveDuration_, 0.0f, 1.0f);

        // 演出完了時の処理（ポーズ中でない場合のみ）
        if (diveTimer_ >= diveDuration_ && (!prop2PausePreview_ || !isPreviewTransition_))
        {
            // ポストプロセスのグリッチをリセット
            if (auto pp = services->GetPostProcess())
            {
                pp->SetEffectActive("Random(Glitch)", false);
            }

            if (isPreviewTransition_)
            {
                // プレビュー時は自動リセットして巡航へ復帰
                isDiving_ = false;
                isPreviewTransition_ = false;
                diveTimer_ = 0.0f;
                dissolveProgress_ = 0.0f;
                polyShards3D_.clear();
                playerColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
                if (playerObj_) playerObj_->SetColor(playerColor_);
            }
            else
            {
                // 本番突入時: 設定されているトランジション（ヘキサゴンまたは四角形グリッド）でゲームシーンへ移行
                auto sceneManager = GetSceneManager();
                if (sceneManager && !sceneManager->IsTransitioning())
                {
                    float fadeDur = sceneManager->GetFadeDuration();
                    Vector4 fadeCol = sceneManager->GetFadeColor();
                    sceneManager->ChangeScene("GAMEPLAY", fadeDur, fadeCol);
                    return;
                }
            }
        }

        if (startTransitionType_ == StartTransitionType::VirtualShatter)
        {
            // =========================================================
            // 【仮想空間自機ポリゴン消滅シーケンス（合計 1.15秒）】
            //  0.00s〜0.45s: 自機がポリゴンに分解し、破片が四散して分散（この間はグリッチ＆シェイクOFFで視認性を確保）
            //  0.45s〜1.15s: ポリゴンが分散した瞬間、空間グリッチ振動＆激しいカメラシェイク発動！
            //  1.15s: 通常のフェードトランジションでゲームシーンへ移行
            // =========================================================
            flightProgressZ_ += forwardSpeed_ * dt;
            float deltaZ = flightProgressZ_ - diveStartProgressZ_;

            curPos.x = diveStartPos_.x;
            curPos.y = diveStartPos_.y;
            curPos.z = diveStartPos_.z + deltaZ;

            // 0.00s〜0.45s で自機がポリゴンディゾルブ消滅（0.45s時点で完全に透明・分解）
            dissolveProgress_ = std::clamp(diveTimer_ / 0.45f, 0.0f, 1.0f);

            // 自機のマテリアルアルファ値をディゾルブ進行度に応じて減衰（シェーダー case 8 でポリゴンブロックが分解）
            playerColor_.w = 1.0f - dissolveProgress_;
            if (playerObj_)
            {
                playerObj_->SetColor(playerColor_);
            }

            // ポリゴン分散後のグリッチ判定（0.45s以降に発動）
            bool isGlitchActive = (diveTimer_ >= 0.45f);

            if (isGlitchActive && !glitchStarted_)
            {
                glitchStarted_ = true;
            }

            // ポストプロセス（デジタルグリッチ振動演出: 0.45s以降のみ有効化）
            if (auto pp = services->GetPostProcess())
            {
                pp->SetEffectActive("Random(Glitch)", isGlitchActive);
                if (isGlitchActive)
                {
                    if (auto* d = pp->GetData())
                    {
                        float glitchPhase = (diveTimer_ - 0.45f) / 0.70f;
                        d->glitchStrength = 0.22f + glitchPhase * 0.15f;
                        d->noiseStrength = 0.12f + glitchPhase * 0.08f;
                    }
                }
            }

            // 決定時も前方に急加速・速度上昇せず、通常の巡航位置を安定維持
            dodgeThrustBoost_ = 0.0f;
            curRot = diveStartRot_;

            // カメラ制御: 0.45s以降にグリッチ振動（激しいカメラシェイク）を発動
            if (!isDebugCamera_ || !debugCamera_)
            {
                if (isGlitchActive)
                {
                    float glitchPhase = (diveTimer_ - 0.45f) / 0.70f;
                    float shakeAmp = 0.22f + glitchPhase * 0.18f;
                    cameraPos_.x = diveStartCamPos_.x + std::sinf(diveTimer_ * 95.0f) * shakeAmp;
                    cameraPos_.y = diveStartCamPos_.y + std::cosf(diveTimer_ * 115.0f) * shakeAmp * 0.8f;
                }
                else
                {
                    // 分散中（0.0s〜0.45s）はカメラを安定させ、ポリゴンの飛散を美しく見せる
                    cameraPos_.x = diveStartCamPos_.x;
                    cameraPos_.y = diveStartCamPos_.y;
                }
                cameraPos_.z = diveStartCamPos_.z + deltaZ;
                cameraRot_ = diveStartCamRot_;
            }

            // 3Dポリゴン破片の更新（機体から四散して消滅）
            for (auto& shard : polyShards3D_)
            {
                if (!shard.object) continue;
                shard.position.x += shard.velocity.x * dt;
                shard.position.y += shard.velocity.y * dt;
                shard.position.z += shard.velocity.z * dt;
                shard.rotation.x += shard.rotSpeed.x * dt;
                shard.rotation.y += shard.rotSpeed.y * dt;
                shard.rotation.z += shard.rotSpeed.z * dt;

                // 0.0sから分散開始し、1.10sにかけてフェードアウト
                float shardFade = std::clamp(1.0f - (diveTimer_ / 1.10f), 0.0f, 1.0f);
                Vector4 col = shard.color;
                col.w = shardFade;
                shard.object->SetColor(col);
                shard.object->SetCamera(activeCamera);
                shard.object->SetTranslate(shard.position);
                shard.object->SetRotation(shard.rotation);
                shard.object->SetScale(shard.scale * (1.0f + dissolveProgress_ * 0.3f));
                shard.object->Update();
            }
        }
        else if (startTransitionType_ == StartTransitionType::SonicAfterburner)
        {
            // =========================================================
            // 【案1: 超音速アフターバーナー上昇】（大空・上空へ急上昇突破）
            // =========================================================
            flightProgressZ_ += forwardSpeed_ * dt * 0.4f;

            // 0.0 ~ 0.18s: 【予兆チャージ】自機がわずかに後退・機首固定、エネルギー凝縮
            // 0.18 ~ 1.0s: 【爆裂急上昇突破】機首を大空へ向け、猛烈なアフターバーナーで上空へ突き抜ける！
            float burstProgress = 0.0f;
            float chargePull = 0.0f;

            if (t < 0.18f)
            {
                float u = t / 0.18f;
                chargePull = std::sinf(u * 3.14159265f * 0.5f) * -0.5f;
            }
            else
            {
                float u = (t - 0.18f) / 0.82f;
                burstProgress = std::powf(u, 2.2f);
            }

            // 自機位置: 高空へ向かって急上昇(+32m)しつつ前方奥(+75m)へ突破！
            curPos.x = std::lerp(diveStartPos_.x, 0.0f, t);
            curPos.y = diveStartPos_.y + (burstProgress * 32.0f);
            curPos.z = diveStartPos_.z + chargePull + (burstProgress * 75.0f);

            // 自機姿勢: 機首を大空へ向けて引き起こす（ピッチ -42度）
            float pitchUp = -0.73f * burstProgress;
            curRot.x = diveStartRot_.x + pitchUp;
            curRot.y = std::lerp(diveStartRot_.y, 0.0f, t * 2.0f);
            curRot.z = std::lerp(diveStartRot_.z, 0.0f, t * 2.0f); // ロールは安定水平

            // カメラ演出: 点火時の振動 ＋ 上空へ駆け上がる自機を見上げるダイナミックチルト
            if (!isDebugCamera_ || !debugCamera_)
            {
                float camShake = (t > 0.16f) ? (std::sinf(t * 70.0f) * 0.12f * (1.0f - t * 0.4f)) : (std::sinf(t * 40.0f) * 0.03f);
                float camPullback = burstProgress * 4.0f;
                cameraPos_.x = diveStartCamPos_.x + camShake;
                cameraPos_.y = diveStartCamPos_.y + camShake * 0.5f;
                cameraPos_.z = diveStartCamPos_.z - camPullback;
                // 自機を見上げるカメラピッチチルト
                cameraRot_.x = diveStartCamRot_.x - (burstProgress * 0.22f);
            }

            // スラスター（ブースト炎）: 白熱アフターバーナー発光
            dodgeThrustBoost_ = (t > 0.16f) ? (0.65f + burstProgress * 0.85f) : 0.35f;
        }
        else
        {
            // =========================================================
            // 【案2: 左旋回クライムブレイク】（建造物の頭上を飛び越え画面左上外へ豪快旋回離脱）
            // =========================================================
            flightProgressZ_ += forwardSpeed_ * dt * 0.35f;

            // 0.0 ~ 0.15s: 予兆チャージ＆左バンク開始
            // 0.15 ~ 1.0s: 機首をグッと引き起こしてビルの屋根を遥かに飛び越える高度(+28m)へ急上昇しつつ、
            //              深い左バンク(-70度)で大弧を描いて画面左上外へブレイク離脱！
            float turnProgress = 0.0f;
            if (t >= 0.15f)
            {
                float u = (t - 0.15f) / 0.85f;
                turnProgress = std::powf(u, 2.0f);
            }

            // 自機位置（ImGuiからリアルタイム調整可能）:
            // X: 左側へ大きく展開して画面左外へ離脱 (prop2MoveX_)
            // Y: 建造物を完全に飛び越える安全高度へクライム急上昇 (prop2MoveY_)
            // Z: 前進推進 (prop2MoveZ_)
            float arcX = turnProgress * prop2MoveX_;
            float climbY = turnProgress * prop2MoveY_;
            curPos.x = diveStartPos_.x + arcX;
            curPos.y = diveStartPos_.y + climbY;
            curPos.z = diveStartPos_.z + (t * prop2MoveZ_);

            // 自機姿勢（ImGuiでリアルタイムにピッチ・ヨー・ロールを自由調整可能）:
            // ピッチ(X): 上下引き起こし
            // ヨー(Y): 機首の左右向き
            // ロール(Z): 機体の傾き（バンク）
            curRot.x = diveStartRot_.x + (prop2Rot_.x * turnProgress);
            curRot.y = diveStartRot_.y + (prop2Rot_.y * turnProgress);
            curRot.z = std::lerp(diveStartRot_.z, prop2Rot_.z, t * 1.5f);

            // カメラ演出: 左上空へ駆け抜ける自機を追うカメラパン（左首振り）＆見上げチルト
            if (!isDebugCamera_ || !debugCamera_)
            {
                float windShock = (turnProgress > 0.1f && turnProgress < 0.9f) ? (std::sinf(t * 55.0f) * 0.15f) : 0.0f;
                cameraPos_.x = diveStartCamPos_.x + windShock - (turnProgress * 2.0f);
                cameraPos_.y = diveStartCamPos_.y + windShock * 0.5f;
                cameraPos_.z = diveStartCamPos_.z;
                cameraRot_.x = diveStartCamRot_.x - (turnProgress * 0.14f);
                cameraRot_.y = diveStartCamRot_.y - (turnProgress * 0.12f);
                cameraRot_.z = turnProgress * 0.06f;
            }

            // スラスター: 全開ブースト
            dodgeThrustBoost_ = 0.85f;
        }
    }
    else
    {
        // === 巡航・リアル前進飛行演出（前方への前進移動＋大気中の飛行感覚・左右上下のゆったりとした移動とバンク） ===
        if (isPlaying)
        {
            flightProgressZ_ += forwardSpeed_ * dt;

            // 1周目（周期 2000.0m）が終わり2周目に入るタイミングで進んだ距離をリセット（ラップアラウンド）
            // 長時間タイトルシーンに滞在しても移動距離の数値が膨大にならず、浮動小数点精度低下等のバグを防止する
            constexpr float kLoopPeriodZ = 2000.0f;
            if (flightProgressZ_ >= kLoopPeriodZ)
            {
                flightProgressZ_ -= kLoopPeriodZ;

                // ワールド座標で発生しているスピードライン（気流粒子）も同距離分シフトし、継ぎ目なく連続表示させる
                for (auto& node : windEffect_.GetNodes())
                {
                    if (node)
                    {
                        node->emitter.OffsetParticles({ 0.0f, 0.0f, -kLoopPeriodZ });
                    }
                }
            }
        }

        curPos.x = targetPos_.x;
        curPos.y = targetPos_.y;
        curPos.z = targetPos_.z + flightProgressZ_;
        curRot = playerRot_;

        float introOffsetZ = 0.0f;
        float introOffsetY = 0.0f;
        float introPitch = 0.0f;

        if (isIntro_)
        {
            // === 起動時：カメラ・地形は前進し続けながら、機体が手前画面外から勢いよく追い抜いて飛び込んでくる演出 ===
            introTimer_ += dt;
            float t = std::clamp(introTimer_ / introDuration_, 0.0f, 1.0f);

            // Cubic Ease-Out による滑らかな追い抜き・定位置への合流
            float easeOut = 1.0f - std::powf(1.0f - t, 3.0f);
            introOffsetZ = introStartOffsetZ_ * (1.0f - easeOut);
            introOffsetY = introStartOffsetY_ * (1.0f - easeOut);

            // 加速進入中は機首がわずかに上を向くリアル演出（t=1.0で綺麗に0度へ収束）
            introPitch = std::sinf(t * 3.14159265f) * -0.05f;

            if (t >= 1.0f)
            {
                isIntro_ = false;
                introBlendTimer_ = 0.0f; // イントロ終了の瞬間から巡航モーションのフェードイン開始
                idleTimer_ = 0.0f;       // 波の起点を0から開始
            }
        }
        else
        {
            // イントロ終了後、巡航タイマーとブレンドタイマーを進行
            if (isPlaying)
            {
                idleTimer_ += dt;
                // 長時間放置時の浮動小数点精度低下を防ぐため、サイン波の共通周期（40π / flightSpeed_）でラップアラウンド
                if (flightSpeed_ > 0.001f)
                {
                    const float kIdleWavePeriod = (40.0f * std::numbers::pi_v<float>) / flightSpeed_;
                    if (idleTimer_ >= kIdleWavePeriod)
                    {
                        idleTimer_ -= kIdleWavePeriod;
                    }
                }
                if (introBlendTimer_ < introBlendDuration_)
                {
                    introBlendTimer_ += dt;
                }
            }
        }

        // ---------------------------------------------------------
        // 巡航モーションのブレンド率（0.0 -> 1.0 へ SmoothStep で極めて滑らかにフェードイン）
        // ---------------------------------------------------------
        if (!isIntro_)
        {
            float bp = std::clamp(introBlendTimer_ / introBlendDuration_, 0.0f, 1.0f);
            motionBlend = bp * bp * (3.0f - 2.0f * bp); // SmoothStep
        }

        float driftX = 0.0f, driftY = 0.0f, driftZ = 0.0f;
        float rollAngle = 0.0f, pitchAngle = 0.0f, yawAngle = 0.0f;

        if (isFlightMotion_ && motionBlend > 0.0f)
        {
            // 左右のゆったりとしたS字旋回・ドリフト移動
            float tFlight = idleTimer_ * flightSpeed_;
            driftX = std::sinf(tFlight * 0.7f) * flightDriftX_ + std::sinf(tFlight * 0.35f) * (flightDriftX_ * 0.35f);

            // 上下の自然な高度調整（2つの波の合成で有機的な浮遊感）
            driftY = std::sinf(tFlight * 1.3f) * flightDriftY_ + std::cosf(tFlight * 0.65f) * (flightDriftY_ * 0.4f);

            // 前後のわずかなピッチング感
            driftZ = std::cosf(tFlight * 0.85f) * 0.08f;

            // 姿勢の連動（左右移動に伴う自然なバンク角、昇降に伴うピッチ角）
            float rollVelocity = (std::cosf(tFlight * 0.7f) * 0.7f * flightDriftX_ + std::cosf(tFlight * 0.35f) * 0.35f * (flightDriftX_ * 0.35f));
            rollAngle = rollVelocity * -flightBankAmount_;

            float pitchVelocity = (std::cosf(tFlight * 1.3f) * 1.3f * flightDriftY_);
            pitchAngle = pitchVelocity * -0.04f;

            yawAngle = rollAngle * 0.25f;
        }

        // ---------------------------------------------------------
        // ゲームプレイに即したスムーズな障害物回避マニューバ
        // 指定された5つの危険地点（500m, 700m, 950m, 1180m, 1430m）を確実にスレスレ回避
        // 派手な曲芸飛行ではなく、ゲームプレイ（レールシューター）らしい自然なレーン移動とバンクで避ける
        // ---------------------------------------------------------
        float dodgeX = 0.0f;
        float dodgeY = 0.0f;
        float dodgeRoll = 0.0f;
        float dodgePitch = 0.0f;
        float dodgeYaw = 0.0f;
        dodgeThrustBoost_ = 0.0f;

        if (isAcrobaticDodge_ && motionBlend > 0.0f)
        {
            float relZ = std::fmod(flightProgressZ_, 2000.0f);
            if (relZ < 0.0f) relZ += 2000.0f;

            // スムーズな台形型（トラペゾイド）ウェイト計算ラムダ
            // 進入（0 -> 0.35）で滑らかにレーン移動、通過中（0.35 -> 0.65）で安全に維持、脱出（0.65 -> 1.0）で滑らかに復帰
            auto calcManeuver = [](float u, float targetX, float targetY, float maxRoll, float maxPitch,
                                   float& outX, float& outY, float& outRoll, float& outPitch, float& outYaw, float& outBoost)
            {
                if (u <= 0.0f || u >= 1.0f) return;

                float w = 0.0f;
                float rollFactor = 0.0f;

                if (u < 0.35f)
                {
                    // 進入区間: SmoothStep で目標レーンへスライド
                    float p = u / 0.35f;
                    w = p * p * (3.0f - 2.0f * p);
                    // レーン移動中のバンク傾斜（微分・速度に応じた自然な傾き）
                    rollFactor = std::sinf(p * 3.14159265f);
                }
                else if (u > 0.65f)
                {
                    // 復帰区間: センターへ滑らかに戻る
                    float p = (1.0f - u) / 0.35f;
                    w = p * p * (3.0f - 2.0f * p);
                    // 戻るときの軽い当て舵（逆バンク）
                    rollFactor = -std::sinf((1.0f - p) * 3.14159265f) * 0.5f;
                }
                else
                {
                    // 通過区間: 安全なオフセットを安定維持
                    w = 1.0f;
                    rollFactor = 0.20f; // スレスレ通過中の軽い傾き
                }

                outX += w * targetX;
                outY += w * targetY;
                outRoll += rollFactor * maxRoll;
                outPitch += w * maxPitch;
                outYaw += rollFactor * (maxRoll * 0.25f);
                outBoost = w * 0.4f;
            };

            // 1. 【500m地点】（Z: 450m ~ 565m）: 450mで回避開始、中心の柱の左スレスレ(-2.8m, 90度ロール)通過後、
            // 中央で立ち止まらず右側へゆったり抜けながら、グラグラ激しい揺れを起こさず巡航走行へシームレス合流
            if (relZ >= 450.0f && relZ <= 565.0f)
            {
                float targetX = 0.0f;
                float targetRoll = 0.0f;
                float targetY = 0.0f;
                float boost = 0.0f;

                if (relZ < 486.0f)
                {
                    // 進入区間 (Z: 450m ~ 486m): 450mから左へスライドしつつ滑らかに90度ロールイン
                    float p = (relZ - 450.0f) / 36.0f;
                    float ease = p * p * (3.0f - 2.0f * p);
                    targetX = std::lerp(driftX * motionBlend, -2.8f, ease);
                    targetRoll = std::lerp(rollAngle * motionBlend, -1.5707963f, ease);
                    targetY = ease * 0.70f;
                    boost = ease * 0.5f;
                }
                else if (relZ <= 516.0f)
                {
                    // 柱の左側面通過区間 (Z: 486m ~ 516m): 柱左面スレスレ(X=-2.8m)を90度ナイフエッジで完全キープ
                    targetX = -2.8f;
                    targetRoll = -1.5707963f;
                    targetY = 0.70f;
                    boost = 0.5f;
                }
                else
                {
                    // 復帰合流区間 (Z: 516m ~ 565m):
                    // 柱を抜けた後、左(-2.8m)から中央で立ち止まらず右側へ自然に向かいながら、
                    // 左右の急激な二重反転・グラつきを起こさずに1本の滑らかなS字曲線で巡航走行へシームレス合流！
                    float p = (relZ - 516.0f) / 49.0f; // 516m -> 565m (約1.3秒のゆったりとした復帰)
                    float ease = p * p * (3.0f - 2.0f * p);

                    // 中間(535~545m)で右側へ心地よく膨らむオフセット(+0.8m)を自然に付加しつつ巡航へ着地
                    float rightBias = std::sinf(p * 3.14159265f) * 0.8f;
                    targetX = std::lerp(-2.8f, driftX * motionBlend, ease) + rightBias;

                    // ロールも-90度から振り子のような過剰揺動を起こさず、右抜けバンク(+3.5度)を経由して巡航バンクへ滑らかに復元
                    float bankBias = std::sinf(p * 3.14159265f) * 0.06f;
                    targetRoll = std::lerp(-1.5707963f, rollAngle * motionBlend, ease) + bankBias;

                    targetY = (1.0f - ease) * 0.70f;
                    boost = (1.0f - ease) * 0.5f;
                }

                dodgeX = targetX - (driftX * motionBlend);
                dodgeRoll = targetRoll - (rollAngle * motionBlend);
                dodgeY += targetY;
                dodgeYaw += (targetRoll * 0.05f);
                dodgeThrustBoost_ = boost;
            }
            // 2. 【680m地点】（Z: 680m ~ 760m）: 680mで回避開始、右側へ大きく大回り(+9.0m)かつ高空(+10.5m)へ上昇しながら豪快なバレルロールを行い、障害物のない左側クリアゾーン(X=-8.5m, Y=0.0m)へ左バンク(-40度)の姿勢のままダイレクト着地！
            else if (relZ >= 680.0f && relZ < 760.0f)
            {
                float u = (relZ - 680.0f) / 80.0f; // 0.0 -> 1.0 (680m -> 760m, 全長80m)
                u = std::clamp(u, 0.0f, 1.0f);

                // 3つ目のアクションと同じ傾き（左バンク -40度 ≈ -0.70rad）
                const float landRoll = -0.70f;

                // SmoothStep による滑らかな回転・進行補間
                float s = u * u * (3.0f - 2.0f * u);
                // バレルロール回転: 0から時計回りに一回転し、終点でちょうど左バンク(-40度)の傾きに着地！
                float roll = s * (2.0f * 3.14159265f + landRoll);

                // 着地目標位置: 障害物・地面を完全に避けた左側クリアゾーン (X = -8.5m, Y = 0.0m)
                const float landX = -8.5f;
                const float landY = 0.0f;

                // ベースパス: 巡航位置から着地目標（左側クリアゾーン）への遷移
                float baseX = std::lerp(driftX * motionBlend, landX, s);
                float baseY = std::lerp(0.0f, landY, s);

                // 両端の速度（微分）が完全にゼロになる正弦波2乗エンベロープ: 着地時の急降下激突・跳ね返りを完全追放！
                float sinVal = std::sinf(u * 3.14159265f);
                float envelope = sinVal * sinVal; // 0 -> 1 -> 0 (両端で微分ゼロ)
                float barrelX = baseX + envelope * 9.0f + std::sinf(u * 6.2831853f) * 1.0f;
                float barrelY = baseY + envelope * 10.5f; // 高空上昇(+10.5m、ワールド高度16.0m)で障害物を遥か上空から豪快に飛び越える！
                float barrelPitch = -std::sinf(u * 6.2831853f) * envelope * 0.22f;

                dodgeX = barrelX - (driftX * motionBlend);
                dodgeRoll = roll;
                dodgePitch = barrelPitch;
                dodgeY += barrelY;
                dodgeYaw += (std::sinf(u * 6.2831853f) * envelope * 0.12f) + (s * landRoll * 0.15f);
                // バレルロール中のスラスターブースト
                dodgeThrustBoost_ = std::max(envelope * 0.85f, 0.45f);
            }
            // 3. 【760m地点】（Z: 760m ~ 865m）: バレルロール着地時の左バンク(-40度)をそのままダイレクトに維持して左側クリアゾーンを高速通過！
            //    地面への接触・跳ね返りを完全に防ぐ安全巡航高度(Y=0.0m)と左側(X=-8.5m)をキープし、左バンク(-40度)で障害物の左を颯爽と通り抜ける
            else if (relZ >= 760.0f && relZ < 865.0f)
            {
                float targetX = 0.0f;
                float targetY = 0.0f;
                float targetRoll = 0.0f;
                float boost = 0.0f;

                // 通過目標位置: 左側(X=-8.5m)、地面に絶対に当たらない安全巡航高度(Y=0.0m)、深めの左バンク(-40度)
                const float avoidX = -8.5f;
                const float avoidY = 0.0f;
                const float avoidRoll = -0.70f; // 左バンク (-40度)

                if (relZ <= 820.0f)
                {
                    // 通過区間 (Z: 760m ~ 820m): バレルロール着地時の左バンク(-40度)・左側・安全巡航高度を完全維持して一気に駆け抜ける！
                    targetX = avoidX;
                    targetY = avoidY;
                    targetRoll = avoidRoll;
                    boost = 0.45f;
                }
                else
                {
                    // 復帰区間 (Z: 820m ~ 865m): 障害物を完全に抜けた後、跳ね返りのないゆったりとしたイージングで巡航飛行へ滑らかに復帰
                    float p = (relZ - 820.0f) / 45.0f;
                    float ease = p * p * (3.0f - 2.0f * p);
                    targetX = std::lerp(avoidX, driftX * motionBlend, ease);
                    targetY = std::lerp(avoidY, 0.0f, ease);
                    targetRoll = std::lerp(avoidRoll, rollAngle * motionBlend, ease);
                    boost = (1.0f - ease) * 0.45f;
                }

                dodgeX = targetX - (driftX * motionBlend);
                dodgeRoll = targetRoll - (rollAngle * motionBlend);
                dodgeY += targetY;
                dodgeYaw += (targetRoll * 0.15f);
                dodgeThrustBoost_ = boost;
            }
            // 4. 【1120m地点】（Z: 1120m ~ 1285m）: 1120mから早めに回避開始、右側へ大きく大回り(+14.5m)して中央の巨大な柱(X=-3.7m~+10m)の右側(X=+13.0m)を右90度ナイフエッジで抜け、
            //    橋下を抜けた後、位置と傾きが一体となって1本の滑らかなカーブで自然に巡航走行へ復帰！
            else if (relZ >= 1120.0f && relZ < 1285.0f)
            {
                float targetX = 0.0f;
                float targetY = 0.0f;
                float targetRoll = 0.0f;
                float targetYaw = 0.0f;
                float boost = 0.0f;

                // 通過ライン: 中央の巨大柱(X=-3.7m~+10.0m)を完全に右へかわす安全ライン(X=+13.0m)、高度+0.5m、右90度ナイフエッジ(+π/2 rad ≈ +1.57rad)
                const float avoidX = +13.0f;
                const float avoidY = +0.5f;
                const float avoidRoll = +1.5707963f; // 右90度ロール

                if (relZ < 1195.0f)
                {
                    // 進入・右大回り区間 (Z: 1120m ~ 1195m, 75m間): 柱に到達する前に余裕を持って右側へ大きく膨らみながら(+14.5m)右90度へロールイン
                    float p = (relZ - 1120.0f) / 75.0f;
                    float ease = p * p * (3.0f - 2.0f * p);
                    // 右側へ豪快に弧を描いて回り込むアプローチ（大回り成分: 最大+14.5mへ）
                    float arcX = std::sinf(p * 3.14159265f) * 3.5f;
                    targetX = std::lerp(driftX * motionBlend, avoidX, ease) + arcX;
                    targetY = std::lerp(0.0f, avoidY, ease);
                    targetRoll = std::lerp(rollAngle * motionBlend, avoidRoll, ease);
                    targetYaw = targetRoll * 0.06f;
                    boost = ease * 0.55f;
                }
                else if (relZ <= 1228.0f)
                {
                    // 橋下通過区間 (Z: 1195m ~ 1228m): 柱の右側(X=+13.0m)を右90度ナイフエッジで完全にクリアしてくぐり抜け
                    targetX = avoidX;
                    targetY = avoidY;
                    targetRoll = avoidRoll;
                    targetYaw = avoidRoll * 0.06f;
                    boost = 0.55f;
                }
                else
                {
                    // 自然な一体復帰区間 (Z: 1228m ~ 1285m, 57m間):
                    // 柱を抜けた後、位置(X)と傾き(Roll)を段階分けせず、1本の滑らかなS字曲線で一体となって同時に自然復帰！
                    // SmootherStep (両端で速度・加速度ともにゼロ) で、ゆらゆら感や急停止反動を完全解消
                    float p = (relZ - 1228.0f) / 57.0f;
                    float ease = p * p * p * (p * (p * 6.0f - 15.0f) + 10.0f);

                    targetX = std::lerp(avoidX, driftX * motionBlend, ease);
                    targetY = std::lerp(avoidY, 0.0f, ease);
                    targetRoll = std::lerp(avoidRoll, rollAngle * motionBlend, ease);
                    targetYaw = targetRoll * 0.06f * (1.0f - ease);
                    boost = (1.0f - ease) * 0.55f;
                }

                dodgeX = targetX - (driftX * motionBlend);
                dodgeRoll = targetRoll - (rollAngle * motionBlend);
                dodgeY += targetY;
                dodgeYaw += targetYaw;
                dodgeThrustBoost_ = boost;
            }
            // 5. 【1420m地点】（Z: 1420m ~ 1575m）: 1420mから回避開始、さらに右側へ大きく大回り(+18.5m)して障害物を完全にクリアしてすり抜け、
            //    通過後は位置と傾きが一体となって1本の滑らかなS字曲線で自然に巡航走行へ復帰！
            else if (relZ >= 1420.0f && relZ < 1575.0f)
            {
                float targetX = 0.0f;
                float targetY = 0.0f;
                float targetRoll = 0.0f;
                float targetYaw = 0.0f;
                float boost = 0.0f;

                // 通過ライン: さらに大きく右側大回りすり抜けライン (X = +16.5m)、高度 +1.0m、深めの右旋回バンク (+60度 ≈ +1.05rad)
                const float avoidX = +16.5f;
                const float avoidY = +1.0f;
                const float avoidRoll = +1.0471975f; // 右バンク 60度 (π/3 rad)

                if (relZ < 1470.0f)
                {
                    // 進入・右大回り展開区間 (Z: 1420m ~ 1470m, 50m間):
                    // 1420mから初動早く右側へ豪快に大きく大回り(+18.5m)して障害物を完全にクリア！
                    float p = (relZ - 1420.0f) / 50.0f;
                    float ease = std::powf(p, 1.35f);
                    float arcX = std::sinf(p * 3.14159265f) * 3.8f;
                    targetX = std::lerp(driftX * motionBlend, avoidX, ease) + arcX;
                    targetY = std::lerp(0.0f, avoidY, ease);
                    targetRoll = std::lerp(rollAngle * motionBlend, avoidRoll, ease);
                    targetYaw = targetRoll * 0.10f;
                    boost = ease * 0.70f;
                }
                else if (relZ <= 1515.0f)
                {
                    // すり抜け通過区間 (Z: 1470m ~ 1515m, 45m間):
                    // 障害物の右外側(X=+16.5m)を深い右バンク(+60度)のシャープな姿勢で完全にすり抜け
                    targetX = avoidX;
                    targetY = avoidY;
                    targetRoll = avoidRoll;
                    targetYaw = avoidRoll * 0.10f;
                    boost = 0.70f;
                }
                else
                {
                    // 自然な一体復帰区間 (Z: 1515m ~ 1575m, 60m間):
                    // 障害物を抜けた後、位置(X)と傾き(Roll)が一体となって滑らかなS字曲線で自然に巡航走行へ合流
                    // SmootherStep (両端で速度・加速度ともにゼロ) により反動やカクつきのない美しい復帰
                    float p = (relZ - 1515.0f) / 60.0f;
                    float ease = p * p * p * (p * (p * 6.0f - 15.0f) + 10.0f);

                    targetX = std::lerp(avoidX, driftX * motionBlend, ease);
                    targetY = std::lerp(avoidY, 0.0f, ease);
                    targetRoll = std::lerp(avoidRoll, rollAngle * motionBlend, ease);
                    targetYaw = targetRoll * 0.10f * (1.0f - ease);
                    boost = (1.0f - ease) * 0.70f;
                }

                dodgeX = targetX - (driftX * motionBlend);
                dodgeRoll = targetRoll - (rollAngle * motionBlend);
                dodgeY += targetY;
                dodgeYaw += targetYaw;
                dodgeThrustBoost_ = boost;
            }

            dodgeX *= dodgeIntensity_ * motionBlend;
            dodgeY *= dodgeIntensity_ * motionBlend;
            dodgeRoll *= dodgeIntensity_ * motionBlend;
            dodgePitch *= dodgeIntensity_ * motionBlend;
            dodgeYaw *= dodgeIntensity_ * motionBlend;
        }

        // 巡航モーションと回避オフセットを合成
        // ※左側の回り込み（-21m）および右側の大回り（+19m）を許容しつつ、安全範囲内にクランプ
        float totalX = (driftX * motionBlend) + dodgeX;
        totalX = std::clamp(totalX, -23.0f, 22.0f);

        curPos.x = targetPos_.x + totalX;
        curPos.y = targetPos_.y + (driftY * motionBlend) + dodgeY + introOffsetY;
        curPos.z = targetPos_.z + flightProgressZ_ + (driftZ * motionBlend) + introOffsetZ;

        curRot.x = (pitchAngle * motionBlend) + dodgePitch + introPitch;
        curRot.y = (yawAngle * motionBlend) + dodgeYaw;
        curRot.z = (rollAngle * motionBlend) + dodgeRoll;

        // 通常カメラの自機追従（ゲームプレイ視点に合わせた安定追従）
        if (!isDebugCamera_ || !debugCamera_)
        {
            if (isIntro_)
            {
                cameraPos_.x = curPos.x * 0.5f;
                cameraPos_.y = targetPos_.y + 0.8f + (curPos.y - targetPos_.y) * 0.4f;
                cameraPos_.z = targetPos_.z + flightProgressZ_ - 6.5f;
                cameraRot_ = { 0.04f, 0.0f, 0.0f };
            }
            else if (!isDiving_)
            {
                // 自機の変位差分
                float diffX = curPos.x;
                float diffY = curPos.y - targetPos_.y;

                // 1. 横方向 (X): 自機の左右大きな変位（+9m ~ -8.5m）をしっかり中央寄りに捉える
                float targetCamX = curPos.x * 0.72f;
                // 大きな変位時は少し素早く追従して見切れを防止
                float lerpFactorX = (std::abs(diffX) > 4.0f) ? 0.20f : 0.15f;
                cameraPos_.x = std::lerp(cameraPos_.x, targetCamX, lerpFactorX);

                // 2. 縦方向 (Y): 高空バレルロール（+10.5m）に力強く追従し、画面上部へのフレームアウトを完全防止
                float targetCamY = targetPos_.y + 0.8f + diffY * 0.60f;
                float lerpFactorY = (diffY > 3.0f) ? 0.22f : 0.14f;
                cameraPos_.y = std::lerp(cameraPos_.y, targetCamY, lerpFactorY);

                // 3. 距離 (Z): 高空ダイナミックアクション中はカメラが適度に引き、壮大なパノラマ構図に
                float camPullback = std::clamp(diffY * 0.16f, 0.0f, 2.2f);
                cameraPos_.z = targetPos_.z + flightProgressZ_ - (6.5f + camPullback);

                // 4. 回転 (Rot):
                // ピッチ角: 自機が急上昇したときに自然に仰角で見上げ、下降時に見下ろすダイナミックチルト
                float camPitchTilt = -diffY * 0.012f;
                cameraRot_.x = 0.04f + camPitchTilt + (curRot.x * 0.06f);

                // ヨー角: 自機とカメラの横位置差に応じて自機を捉えるパン
                float targetPanY = (curPos.x - cameraPos_.x) * 0.03f;
                cameraRot_.y = (curRot.y * 0.06f) + targetPanY;

                // ロール角: -π ~ +π に正規化して360度回転時のジャンプ・跳ね返りを防止しつつ、バンクを自然にサポート
                float normRoll = std::atan2f(std::sinf(curRot.z), std::cosf(curRot.z));
                normRoll = std::clamp(normRoll, -0.70f, 0.70f);
                cameraRot_.z = -normRoll * dodgeCameraRollAmount_;
            }

            if (camera_)
            {
                camera_->SetTranslate(cameraPos_);
                camera_->SetRotation(cameraRot_);
                camera_->Update();
            }
        }
    }

    // -------------------------------------------------------------
    // タイトルシーン用マップ（title.json）オブジェクトのシームレス無限ループ更新
    // -------------------------------------------------------------
    float camZ = activeCamera ? activeCamera->GetTranslate().z : curPos.z;

    for (auto& nodeItem : titleMapNodes_)
    {
        if (!nodeItem.object) continue;

        float period = nodeItem.loopPeriod;
        float baseZ = nodeItem.baseTranslation.z;
        // カメラから見て背後に消えた分を前方へ循環シフト
        int shift = static_cast<int>(std::floor((camZ - (baseZ - period * 0.5f)) / period));

        // 各スロットを手前側から順に敷き詰める (slotIndex - 1)
        int k = shift + nodeItem.slotIndex - 1;

        Vector3 curTrans = nodeItem.baseTranslation;
        curTrans.z = baseZ + static_cast<float>(k) * period;

        nodeItem.object->SetTranslate(curTrans);
        nodeItem.object->SetCamera(activeCamera);
        nodeItem.object->Update();
    }

    playerPos_ = curPos;

    // 自機モデルの更新（現在のアクティブカメラを直接適用して更新）
    if (playerObj_)
    {
        playerObj_->SetCamera(activeCamera);
        playerObj_->SetTranslate(curPos);
        playerObj_->SetRotation(curRot);
        playerObj_->SetScale(playerScale_);
        playerObj_->SetColor(playerColor_);
        playerObj_->SetEnvironmentCoefficient(envCoefficient_);
        playerObj_->Update();
    }

    // -------------------------------------------------------------
    // 3. スカイボックス更新
    // -------------------------------------------------------------
    if (skybox_ && activeCamera)
    {
        skybox_->SetCamera(activeCamera);
        skybox_->Update();
    }

    // -------------------------------------------------------------
    // ブースト（スラスター）＆周辺粒子（気流線）の更新
    // -------------------------------------------------------------
    if (activeCamera)
    {
        Matrix4x4 viewMatrix = activeCamera->GetViewMatrix();
        Matrix4x4 projectionMatrix = activeCamera->GetProjectionMatrix();
        Matrix4x4 billboardMatrix = Billboard::CreateFromCamera(activeCamera, true);

        // 1. スラスター（ブースト炎）
        // 自機（playerObj_）と親子関係を結び、機体の姿勢・移動・ロール回転に完全連動
        if (playerObj_)
        {
            thrusterEffect_.SetParentMatrix(&playerObj_->GetmatWorld());
        }
        thrusterEffect_.SetPosition(nozzleOffset_);

        if (isDiving_)
        {
            if (startTransitionType_ == StartTransitionType::VirtualShatter)
            {
                // 仮想空間自機ポリゴン消滅演出: エメラルドシアン〜高輝度ホワイトシアン
                thrusterEffect_.SetBaseColor(dissolveProgress_ > 0.0f ? Vector4{ 0.85f, 1.0f, 0.96f, 1.0f } : Vector4{ 0.25f, 1.0f, 0.85f, 1.0f });
            }
            else if (startTransitionType_ == StartTransitionType::SonicAfterburner)
            {
                // 超音速アフターバーナー: 白熱オレンジ〜ホワイトシアンの超巨大高エネルギー炎
                float t = diveTimer_ / diveDuration_;
                Vector4 cyanWhite = { 0.80f, 0.95f, 1.0f, 1.0f };
                Vector4 blazingOrange = { 1.0f, 0.6f, 0.1f, 1.0f };
                thrusterEffect_.SetBaseColor(t > 0.18f ? cyanWhite : blazingOrange);
            }
            else
            {
                // カメラブレイク: 鮮烈な高出力バーニングオレンジ
                thrusterEffect_.SetBaseColor({ 1.0f, 0.48f, 0.08f, 1.0f });
            }
        }
        else if (isIntro_ || motionBlend < 1.0f)
        {
            // 起動時の追い抜き飛び込み登場演出中〜巡航移行にかけて、オレンジからシアンブルーへ滑らかにカラーフェード
            Vector4 orange = { 1.0f, 0.55f, 0.1f, 1.0f };
            Vector4 cyan = { 0.5f, 0.85f, 1.0f, 1.0f };
            float tCol = isIntro_ ? 0.0f : motionBlend;
            Vector4 blendedCol = {
                std::lerp(orange.x, cyan.x, tCol),
                std::lerp(orange.y, cyan.y, tCol),
                std::lerp(orange.z, cyan.z, tCol),
                1.0f
            };
            thrusterEffect_.SetBaseColor(blendedCol);
        }
        else if (dodgeThrustBoost_ > 0.1f)
        {
            // アクロバット建物回避中は高エネルギーアフターバーナー発光（白熱シアンブースト）
            float b = std::clamp(dodgeThrustBoost_, 0.0f, 1.0f);
            Vector4 cyan = { 0.5f, 0.85f, 1.0f, 1.0f };
            Vector4 highBoost = { 0.9f, 0.95f, 1.0f, 1.0f };
            Vector4 boostedCol = {
                std::lerp(cyan.x, highBoost.x, b),
                std::lerp(cyan.y, highBoost.y, b),
                std::lerp(cyan.z, highBoost.z, b),
                1.0f
            };
            thrusterEffect_.SetBaseColor(boostedCol);
        }
        else
        {
            // 巡航中はクールな高エネルギーシアンブルー
            thrusterEffect_.SetBaseColor({ 0.5f, 0.85f, 1.0f, 1.0f });
        }
        if (isPlaying)
        {
            thrusterEffect_.Play();
        }
        thrusterEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);

        // 2. スピードライン（奥から手前へ一直線に突き抜ける光の筋）
        if (isPlaying)
        {
            int speedLineSpawnCount = (isDiving_ || isIntro_ || motionBlend < 0.5f || dodgeThrustBoost_ > 0.3f) ? 8 : 4;
            std::uniform_real_distribution<float> distOffsetX(-10.0f, 10.0f);
            std::uniform_real_distribution<float> distOffsetY(-3.5f, 4.5f);
            std::uniform_real_distribution<float> distOffsetZ(18.0f, 42.0f);

            for (int i = 0; i < speedLineSpawnCount; ++i)
            {
                float ox = distOffsetX(randomEngine_);
                if (std::abs(ox) < 1.4f) { ox = (ox >= 0.0f ? 1.4f : -1.4f); }
                float oy = distOffsetY(randomEngine_);

                Vector3 speedLineSpawnPos = {
                    curPos.x + ox,
                    curPos.y + oy,
                    curPos.z + distOffsetZ(randomEngine_)
                };

                windEffect_.SetPosition(speedLineSpawnPos);
                windEffect_.Play();
            }
        }
        windEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
    }

#ifdef ENABLE_EDITOR
    // フレーム描画スコープ外（NewFrame前）の場合はImGui描画をスキップ
    if (!ImGui::GetCurrentContext() || ImGui::GetFrameCount() <= 0)
    {
        return;
    }

    // エディターモードが無効（全画面表示時）はエディタUIを描画しない
    if (!services->GetEditorMode())
    {
        return;
    }

    // =========================================================================
    // 統合エディタUI: [左] メニュー  [右] インスペクター  [下] タイムライン
    // =========================================================================
    static int currentNavIndex = 0;
    const char* navItems[] = {
        "ゲーム・進行",
        "演出・シェーダー"
    };

    // -------------------------------------------------------------
    // 1. [左ドック用] メニュー (Category Selector & Performance)
    // -------------------------------------------------------------
    if (ImGui::Begin("メニュー"))
    {
        ImGui::TextColored(ImVec4(0.3f, 0.75f, 1.0f, 1.0f), "CATEGORY");
        ImGui::Separator();

        for (int i = 0; i < IM_ARRAYSIZE(navItems); ++i)
        {
            bool isSelected = (currentNavIndex == i);
            if (ImGui::Selectable(navItems[i], isSelected, 0, ImVec2(0, 30)))
            {
                currentNavIndex = i;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("F1: UI表示切替");
        if (services->IsGamePlaying())
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.55f, 0.15f, 1.0f));
            if (ImGui::Button("一時停止 (Pause)", ImVec2(-1, 30)))
            {
                services->SetGamePlaying(false);
            }
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.68f, 0.30f, 1.0f));
            if (ImGui::Button("ゲーム再生 (Play)", ImVec2(-1, 30)))
            {
                services->SetGamePlaying(true);
            }
            ImGui::PopStyleColor();
        }

        if (ImGui::Button("ゲームプレイ開始 (SPACE)", ImVec2(-1, 32)))
        {
            TriggerStartTransition(false);
        }

        // オーディオ・ミュートコントロール
        SoundManager::GetInstance()->DrawImGuiAudioControls();

        // 最下部にパフォーマンス情報を常時表示
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.5f, 1.0f), "PERFORMANCE");

        float fps = ImGui::GetIO().Framerate;
        float frameTime = 1000.0f / (fps > 0.0f ? fps : 1.0f);

        ImGui::Text("FPS: %.1f (%.2f ms)", fps, frameTime);

        static float fpsHistory[60] = {};
        static int historyOffset = 0;
        fpsHistory[historyOffset] = fps;
        historyOffset = (historyOffset + 1) % IM_ARRAYSIZE(fpsHistory);

        ImGui::PlotLines("##FPSMiniGraphTitle", fpsHistory, IM_ARRAYSIZE(fpsHistory), historyOffset, nullptr, 0.0f, 120.0f, ImVec2(-1, 42));
    }
    ImGui::End();

    // -------------------------------------------------------------
    // 2. [右ドック用] インスペクター (Inspector Content)
    // -------------------------------------------------------------
    if (ImGui::Begin("インスペクター"))
    {
        // 1. ゲーム・進行
        if (currentNavIndex == 0)
        {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "■ シーン・進行");
            ImGui::Separator();

            if (ImGui::CollapsingHeader("シーン切り替え (Scene Selector)", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (auto sm = GetSceneManager())
                {
                    sm->DrawSceneSelectorUI();
                    ImGui::Spacing();
                    sm->DrawTransitionSettingsUI();
                }
            }

            ImGui::Spacing();
            if (ImGui::CollapsingHeader("ゲームプレイ制御 (Playback Control)", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (services->IsGamePlaying())
                {
                    if (ImGui::Button("一時停止 (Pause)", ImVec2(130, 36)))
                    {
                        services->SetGamePlaying(false);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("全画面 (F1で復帰)", ImVec2(140, 36)))
                    {
                        services->SetEditorMode(false);
                        services->SetGamePlaying(true);
                    }
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "状態: プレイ中 (PLAYING)");
                }
                else
                {
                    if (ImGui::Button("開始 (Play)", ImVec2(130, 36)))
                    {
                        services->SetGamePlaying(true);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("全画面プレイ", ImVec2(140, 36)))
                    {
                        services->SetGamePlaying(true);
                        services->SetEditorMode(false);
                    }
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "状態: 停止中 (STOPPED - BGM/SE待機)");
                    ImGui::TextDisabled("※再生ボタンを押すまでBGMとSEは鳴りません");
                }
            }

            ImGui::Spacing();
            if (ImGui::CollapsingHeader("タイトル画面制御 (Title Control)", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("現在: タイトル画面 (TITLE SCENE)");
                ImGui::TextDisabled("SPACEキーを押すか、下のボタンでゲームを開始できます。");
                ImGui::Spacing();

                if (auto services = Services())
                {
                    bool isDemo = services->IsDemoMode();
                    if (ImGui::Checkbox("デモプレイモード (展示用ループ)##TitleDemo", &isDemo))
                    {
                        services->SetDemoMode(isDemo);
                    }
                    if (isDemo)
                    {
                        ImGui::SliderFloat("巡航飛行鑑賞時間 (秒)##DemoCruise", &demoCruiseDuration_, 3.0f, 30.0f, "%.1f 秒");
                        ImGui::ProgressBar(std::clamp(demoTimer_ / demoCruiseDuration_, 0.0f, 1.0f), ImVec2(-1, 16));
                    }
                }
                ImGui::Spacing();

                if (ImGui::Button("ゲームプレイを開始する (Start Game)", ImVec2(-1, 40)))
                {
                    TriggerStartTransition(false);
                }
            }

            ImGui::Spacing();
            if (ImGui::CollapsingHeader("3D自機・カメラ・配置設定 (Title Visual Settings)", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::Button("登場演出から飛び直す (Replay Intro)", ImVec2(-1, 30)))
                {
                    flightProgressZ_ = 0.0f;
                    isIntro_ = true;
                    introTimer_ = 0.0f;
                    introBlendTimer_ = 0.0f;
                    idleTimer_ = 0.0f;
                    isDiving_ = false;
                    diveTimer_ = 0.0f;
                    playerPos_ = targetPos_;
                    cameraPos_ = { 0.0f, 4.8f, -9.5f };
                    if (camera_) { camera_->SetTranslate(cameraPos_); }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.15f, 1.0f, 0.80f, 1.0f), "■ サイバー仮想空間ビジュアル (Star Fox Virtual Space)");
                if (ImGui::Checkbox("スターフォックス風 仮想空間シェーダー有効##VirtualSpace", &isVirtualSpace_))
                {
                    if (skybox_)
                    {
                        skybox_->SetVirtualSpace(isVirtualSpace_);
                    }
                    for (auto& nodeItem : titleMapNodes_)
                    {
                        if (nodeItem.object)
                        {
                            nodeItem.object->SetSelectLightings(isVirtualSpace_ ? 7 : 1);
                        }
                    }
                }
                ImGui::TextDisabled("※ONでエメラルドグリッド地面・ホログラム都市・オーロラ宇宙空間になります");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "■ スタート演出切り替え (Start Transition)");
                int currentType = static_cast<int>(startTransitionType_);
                const char* typeNames[] = {
                    "新演出(A案): オート4連射 ➔ 画面亀裂 ➔ 仮想空間粉砕ブレイク",
                    "案1: 超音速アフターバーナー上昇 (大空へ急上昇突破)",
                    "案2: 左旋回クライムブレイク (建造物を飛び越え左上空へ離脱)"
                };
                if (ImGui::Combo("演出タイプ##StartType", &currentType, typeNames, IM_ARRAYSIZE(typeNames)))
                {
                    startTransitionType_ = static_cast<StartTransitionType>(currentType);
                }

                if (ImGui::Button("▶ この演出をプレビュー再生 (シーン遷移なし)##PreviewBtn", ImVec2(-1, 34)))
                {
                    TriggerStartTransition(true); // プレビューモード
                }

                if (isDiving_ && isPreviewTransition_)
                {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "※ 演出プレビュー中... (完了時に自動で巡航へ復帰)");
                }
                else
                {
                    ImGui::TextDisabled("※プレビューはゲーム画面に遷移せず、何度でも見比べられます。");
                }

                // 案2選択時の詳細姿勢・移動パラメータ調整パネル
                if (startTransitionType_ == StartTransitionType::CameraBreakFlyby)
                {
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.45f, 0.65f, 0.7f));
                    if (ImGui::CollapsingHeader("▼ 案2（左旋回）機体の向き・軌道リアルタイム調整", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        // 1. ポーズ＆進行度シークバー
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "【1】 プレビュー停止・シーク操作");
                        ImGui::Checkbox("プレビューを一時停止して姿勢を確認 (Pause)##Prop2Pause", &prop2PausePreview_);
                        if (prop2PausePreview_)
                        {
                            if (!isDiving_)
                            {
                                if (ImGui::Button("ポーズ状態でプレビューを開始する##Prop2StartPause", ImVec2(-1, 28)))
                                {
                                    TriggerStartTransition(true);
                                }
                            }
                            ImGui::SliderFloat("演出進行度 (Progress)##Prop2Seek", &prop2PreviewProgress_, 0.0f, 1.0f, "%.2f (0.0=開始, 1.0=離脱)");
                            ImGui::TextDisabled("※スライダーを動かして好きな進行タイミングで機体を静止確認できます");
                        }
                        else
                        {
                            ImGui::TextDisabled("※チェックを入れると演出が静止し、角度をじっくり調整できます");
                        }

                        ImGui::Spacing();
                        ImGui::Separator();

                        // 2. 機体の向き調整（度数法表記 deg で直感操作）
                        ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "【2】 機体の向き・角度調整 (Rotation)");
                        Vector3 rotDeg = {
                            prop2Rot_.x * (180.0f / 3.14159265f),
                            prop2Rot_.y * (180.0f / 3.14159265f),
                            prop2Rot_.z * (180.0f / 3.14159265f)
                        };

                        bool rotChanged = false;
                        rotChanged |= ImGui::SliderFloat("ピッチ角 (X軸: 上下首振り)##Prop2Pitch", &rotDeg.x, -90.0f, 90.0f, "%.1f deg (負=機首上げ)");
                        rotChanged |= ImGui::SliderFloat("ヨー角 (Y軸: 左右機首振り)##Prop2Yaw", &rotDeg.y, -90.0f, 90.0f, "%.1f deg");
                        rotChanged |= ImGui::SliderFloat("ロール角 (Z軸: バンク傾斜)##Prop2Roll", &rotDeg.z, -180.0f, 180.0f, "%.1f deg (負=左バンク)");

                        if (rotChanged)
                        {
                            prop2Rot_.x = rotDeg.x * (3.14159265f / 180.0f);
                            prop2Rot_.y = rotDeg.y * (3.14159265f / 180.0f);
                            prop2Rot_.z = rotDeg.z * (3.14159265f / 180.0f);
                        }

                        ImGui::Spacing();
                        ImGui::Separator();

                        // 3. 軌道・移動量調整
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "【3】 離脱軌道・移動量調整 (Movement)");
                        ImGui::SliderFloat("横展開移動量 (X)##Prop2MoveX", &prop2MoveX_, -60.0f, 0.0f, "%.1f m (負=左へ)");
                        ImGui::SliderFloat("クライム上昇高度 (Y)##Prop2MoveY", &prop2MoveY_, 0.0f, 60.0f, "%.1f m");
                        ImGui::SliderFloat("前進推進距離 (Z)##Prop2MoveZ", &prop2MoveZ_, 0.0f, 60.0f, "%.1f m");

                        ImGui::Spacing();
                        if (ImGui::Button("初期設定値にリセット##Prop2Reset", ImVec2(-1, 26)))
                        {
                            prop2Rot_ = { -0.60f, 0.0f, -1.15f };
                            prop2MoveX_ = -25.0f;
                            prop2MoveY_ = 28.0f;
                            prop2MoveZ_ = 18.0f;
                        }
                    }
                    ImGui::PopStyleColor();
                }



                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.2f, 0.9f, 1.0f, 1.0f), "■ 巡航飛行演出 (Flight Motion)");
                ImGui::Checkbox("巡航飛行アニメーション有効", &isFlightMotion_);
                if (isFlightMotion_)
                {
                    ImGui::SliderFloat("左右ドリフト幅 (Drift X)", &flightDriftX_, 0.0f, 3.0f, "%.2f");
                    ImGui::SliderFloat("上下ドリフト幅 (Drift Y)", &flightDriftY_, 0.0f, 2.0f, "%.2f");
                    ImGui::SliderFloat("巡航周期速度 (Speed)", &flightSpeed_, 0.1f, 3.0f, "%.2f");
                    ImGui::SliderFloat("旋回バンク傾き量 (Bank)", &flightBankAmount_, 0.0f, 0.3f, "%.3f");
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.8f, 1.0f), "■ アクロバット建物回避 (Acrobatic Dodge)");
                ImGui::Checkbox("スタイリッシュ回避有効", &isAcrobaticDodge_);
                if (isAcrobaticDodge_)
                {
                    ImGui::SliderFloat("回避ダイナミック倍率", &dodgeIntensity_, 0.0f, 2.0f, "%.2f");
                    ImGui::SliderFloat("カメラバンク連動量", &dodgeCameraRollAmount_, 0.0f, 0.2f, "%.3f");
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "■ スピードライン演出 (Speed Line)");
                auto& speedNodes = windEffect_.GetNodes();
                if (!speedNodes.empty())
                {
                    auto& emitter = speedNodes[0]->emitter;
                    auto param = emitter.GetParameter();
                    float currentSpeed = -param.minVelocity.z;
                    if (ImGui::SliderFloat("ライン速度 (Speed)", &currentSpeed, 10.0f, 200.0f, "%.1f"))
                    {
                        param.minVelocity.z = -currentSpeed;
                        param.maxVelocity.z = -(currentSpeed * 1.4f);
                        emitter.SetParameter(param);
                    }
                    if (ImGui::SliderFloat("生存時間 (LifeTime)", &param.maxLifeTime, 0.3f, 2.5f, "%.2f"))
                    {
                        param.minLifeTime = param.maxLifeTime * 0.65f;
                        emitter.SetParameter(param);
                    }
                    if (ImGui::SliderFloat("ライン長さ (Length)", &param.maxScale.y, 0.5f, 5.0f, "%.2f"))
                    {
                        param.minScale.y = param.maxScale.y * 0.45f;
                        emitter.SetParameter(param);
                    }
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "■ タイトルマップ & 前進飛行設定");
                float speedKmh = forwardSpeed_ * 3.6f;
                if (ImGui::SliderFloat("前進巡航速度 (Speed)", &speedKmh, 0.0f, 300.0f, "%.1f km/h"))
                {
                    forwardSpeed_ = speedKmh / 3.6f;
                }
                ImGui::Text("現在速度: %.1f km/h (%.2f m/s)", speedKmh, forwardSpeed_);
                ImGui::Text("前進飛行距離: %.1f m / 2000.0 m (1周で自動リセット)", flightProgressZ_);
                if (ImGui::Button("飛行距離を 0 にリセット"))
                {
                    flightProgressZ_ = 0.0f;
                }
                ImGui::SameLine();
                if (ImGui::Button("title.json を再読み込み (Reload)"))
                {
                    Initialize();
                }
                ImGui::Text("ループスロット総数: %zu", titleMapNodes_.size());

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "【指定5地点 回避テスト・ワープ】");
                if (ImGui::Button("1. 500m地点: 中心の柱 (Z=430)"))
                {
                    flightProgressZ_ = 430.0f;
                }
                ImGui::SameLine();
                if (ImGui::Button("2. 680m地点: バレルロール回避 (Z=660)"))
                {
                    flightProgressZ_ = 660.0f;
                }
                if (ImGui::Button("3. 760m地点: スラブ下くぐり抜け (Z=745)"))
                {
                    flightProgressZ_ = 745.0f;
                }
                ImGui::SameLine();
                if (ImGui::Button("4. 1120m地点: 橋下90度くぐり抜け (Z=1080)"))
                {
                    flightProgressZ_ = 1080.0f;
                }
                if (ImGui::Button("5. 1420m地点: 右側大回りすり抜け (Z=1380)"))
                {
                    flightProgressZ_ = 1380.0f;
                }
                ImGui::TextDisabled("※ボタンを押すと自機とカメラが指定スポットへワープし、そこから前進飛行を続けます。");

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "■ カメラ (Title Camera)");
                ImGui::DragFloat3("通常カメラ位置", &cameraPos_.x, 0.1f, -2000.0f, 3000.0f);
                ImGui::DragFloat3("通常カメラ回転", &cameraRot_.x, 0.01f, -3.14f, 3.14f);

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.7f, 1.0f), "■ デバッグカメラ (Debug Camera)");
                if (ImGui::Checkbox("デバッグカメラ有効 (F2)", &isDebugCamera_))
                {
                    if (isDebugCamera_ && camera_ && debugCamera_)
                    {
                        debugCamera_->SetTranslate(camera_->GetTranslate());
                        debugCamera_->SetRotation(camera_->GetRotation());
                        debugCamera_->Update();
                    }
                }
                if (isDebugCamera_ && debugCamera_)
                {
                    Vector3 dbgPos = debugCamera_->GetTranslate();
                    Vector3 dbgRot = debugCamera_->GetRotation();
                    if (ImGui::DragFloat3("デバッグカメラ位置", &dbgPos.x, 0.2f, -2000.0f, 3000.0f))
                    {
                        debugCamera_->SetTranslate(dbgPos);
                    }
                    if (ImGui::DragFloat3("デバッグカメラ回転", &dbgRot.x, 0.01f, -3.14f, 3.14f))
                    {
                        debugCamera_->SetRotation(dbgRot);
                    }
                    ImGui::SliderFloat("カメラ移動速度", &debugCameraMoveSpeed_, 1.0f, 100.0f, "%.1f");
                    if (ImGui::Button("通常カメラの位置・回転にリセット"))
                    {
                        if (camera_)
                        {
                            debugCamera_->SetTranslate(camera_->GetTranslate());
                            debugCamera_->SetRotation(camera_->GetRotation());
                        }
                    }
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "操作方法:");
                    ImGui::BulletText("右ドラッグ: 視点回転");
                    ImGui::BulletText("WASD: 前後左右移動 (Shiftで8倍高速移動)");
                    ImGui::BulletText("E / Q: 上昇 / 下降");
                    ImGui::BulletText("マウスホイール: 前後移動");
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.25f, 1.0f), "■ スラスター炎の発生位置 (Thruster Offset)");
                ImGui::SliderFloat("前後位置 (Z: 手前/奥)##NozzleZ", &nozzleOffset_.z, -8.0f, 0.0f, "%.2f m (負=手前・カメラ側)");
                ImGui::SliderFloat("上下位置 (Y)##NozzleY", &nozzleOffset_.y, -2.0f, 2.0f, "%.2f m");
                ImGui::SliderFloat("左右位置 (X)##NozzleX", &nozzleOffset_.x, -2.0f, 2.0f, "%.2f m");

                ImGui::Spacing();
                if (ImGui::Button("初期設定値にリセット (上空巡航 Y=5.5)"))
                {
                    targetPos_ = { 0.0f, 5.5f, -3.0f };
                    playerRot_ = { 0.0f, 0.0f, 0.0f };
                    cameraPos_ = { 0.0f, 6.3f, -9.5f };
                    cameraRot_ = { 0.04f, 0.0f, 0.0f };
                    nozzleOffset_ = { 0.0f, -0.22f, -3.00f };
                    startTransitionType_ = StartTransitionType::VirtualShatter;
                    diveDuration_ = 1.48f;
                    isDiving_ = false;
                    isPreviewTransition_ = false;
                }

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "■ サウンド設定 (Audio)");
                float bgmVol = SoundManager::GetInstance()->GetBGMVolume();
                if (ImGui::SliderFloat("BGM音量", &bgmVol, 0.0f, 1.0f, "%.2f"))
                {
                    SoundManager::GetInstance()->SetBGMVolume(bgmVol);
                }
            }
        }
        // 2. 演出・シェーダー
        else if (currentNavIndex == 1)
        {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "■ 演出・トランジション・シェーダー");
            ImGui::Separator();

            if (ImGui::CollapsingHeader("画面遷移 (Transition Settings)", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (auto sm = GetSceneManager())
                {
                    sm->DrawTransitionSettingsUI();
                }
            }

            ImGui::Spacing();
            if (ImGui::CollapsingHeader("ポストプロセス (Post Process)", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (auto pp = EngineServices::GetInstance()->GetPostProcess())
                {
                    pp->DrawImGuiContent();
                }
            }

            ImGui::Spacing();
            if (ImGui::CollapsingHeader("パーティクルエフェクト (Particles)", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::Button("エフェクト専用画面 (Effect Studio) を開く", ImVec2(-1, 36)))
                {
                    EffectStudio::GetInstance()->GetShowViewport() = true;
                    EffectStudio::GetInstance()->GetShowEditor() = true;
                }
                ImGui::TextDisabled("※エフェクトの編集・作成・保存は「エフェクト画面」および「エフェクトエディター」で行えます。");
            }
        }
    }
    ImGui::End();

    // -------------------------------------------------------------
    // 3. [下ドック用] タイムライン (Timeline Editor)
    // -------------------------------------------------------------
    if (ImGui::Begin("タイムライン"))
    {
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "■ タイムライン (Timeline)");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("※ 現在はタイトルシーンです。レールや敵出現タイムラインの編集・確認は「ゲームプレイ画面」で行えます。");
        ImGui::Spacing();
        if (ImGui::Button("ゲームプレイ画面へ移動してタイムラインを編集##TLTitle", ImVec2(340, 36)))
        {
            auto sm = GetSceneManager();
            if (sm && !sm->IsTransitioning())
            {
                sm->ChangeScene("GAMEPLAY", 0.6f);
            }
        }
    }
    ImGui::End();
#endif // ENABLE_EDITOR
}

void TitleScene::Draw()
{
    auto services = Services();
    if (!services) return;

    auto object3dCommon = services->GetObject3dCommon();

    // 1. スカイボックス描画
    if (skybox_)
    {
        skybox_->Draw();
    }

    // 2. タイトルマップ（title.json）の地形・建物描画
    if (object3dCommon)
    {
        object3dCommon->SetCommonDrawSetting();
    }

    for (const auto& nodeItem : titleMapNodes_)
    {
        if (nodeItem.object)
        {
            nodeItem.object->Draw();
        }
    }

    // 3. 自機モデル（消滅していない場合）＆3Dポリゴン破片描画
    if (playerObj_ && playerColor_.w > 0.001f)
    {
        playerObj_->Draw();
    }

    for (const auto& shard : polyShards3D_)
    {
        if (shard.object)
        {
            shard.object->Draw();
        }
    }

    // 4. スラスターブースト＆スピードライン描画
    if (playerColor_.w > 0.1f)
    {
        thrusterEffect_.Draw();
        windEffect_.Draw();
    }
}

void TitleScene::DrawUI()
{
    BaseScene::DrawUI();
}

void TitleScene::Finalize()
{
    // ポストプロセスのグリッチ等をリセット
    if (auto services = Services())
    {
        if (auto pp = services->GetPostProcess())
        {
            pp->SetEffectActive("Random(Glitch)", false);
        }
    }

    // タイトルBGM停止
    SoundManager::GetInstance()->StopBGM();

    polyShards3D_.clear();
    titleMapNodes_.clear();
    playerObj_.reset();
    skybox_.reset();
    debugCamera_.reset();
    camera_.reset();

    BaseScene::Finalize();
}
