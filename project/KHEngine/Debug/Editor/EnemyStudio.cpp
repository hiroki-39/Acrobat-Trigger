#include "EnemyStudio.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "externals/imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

EnemyStudio* EnemyStudio::GetInstance() {
    static EnemyStudio instance;
    return &instance;
}

void EnemyStudio::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Object3dCommon* object3dCommon) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    object3dCommon_ = object3dCommon;

    // 1. オフスクリーンレンダーターゲットの作成
    CreateRenderTargets();

    // 2. プレビューカメラの初期化
    camera_.SetFovY(0.785398f);
    camera_.SetAspectRatio(static_cast<float>(kWidth_) / static_cast<float>(kHeight_));
    camera_.SetNearClip(0.1f);
    camera_.SetFarClip(1000.0f);

    // 3. プレイヤーモデルのロードと初期化
    ModelManager::GetInstance()->LoadModel("player.obj");
    playerObj_ = std::make_unique<Object3d>();
    playerObj_->Initialize(object3dCommon_);
    playerObj_->SetModel("player.obj");
    playerObj_->SetScale({ 1.0f, 1.0f, 1.0f });

    // 4. デバッグ用コライダーモデルのロードと初期化
    ModelManager::GetInstance()->LoadModel("collider_sphere_enemy.obj");
    ModelManager::GetInstance()->LoadModel("collider_cube_enemy.obj");

    colliderSphereObj_ = std::make_unique<Object3d>();
    colliderSphereObj_->Initialize(object3dCommon_);
    colliderSphereObj_->SetModel("collider_sphere_enemy.obj");
    colliderSphereObj_->GetModel()->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f });

    colliderBoxObj_ = std::make_unique<Object3d>();
    colliderBoxObj_->Initialize(object3dCommon_);
    colliderBoxObj_->SetModel("collider_cube_enemy.obj");
    colliderBoxObj_->GetModel()->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f });

    // 5. 陣形用エネミーオブジェクト群の初期化（最大10機）
    enemyObjs_.resize(10);
    for (size_t i = 0; i < enemyObjs_.size(); ++i) {
        enemyObjs_[i] = std::make_unique<Object3d>();
        enemyObjs_[i]->Initialize(object3dCommon_);
    }

    // 6. 利用可能モデル一覧のスキャン
    RefreshAvailableModels();

    // 7. プリセットマネージャーの初期化
    EnemyPresetManager::GetInstance()->Initialize();
    const auto& names = EnemyPresetManager::GetInstance()->GetPresetNames();
    if (!names.empty()) {
        currentPresetKey_ = names[0];
    }

    // 8. 装甲列車ボスのプレビュー用インスタンス初期化
    previewBoss_ = std::make_unique<ArmoredTrainBoss>();
    previewBoss_->Initialize(object3dCommon_, { 0.0f, 0.0f, 0.0f }, 0);
    previewBoss_->SetFollowPlayer(false);
    previewBoss_->SetGroundSnapEnabled(false);
    previewBoss_->SetActive(true);
}

void EnemyStudio::CreateRenderTargets() {
    auto device = dxCommon_->GetDevice();

    // 1. カラーテクスチャリソース作成
    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = kWidth_;
    texDesc.Height = kHeight_;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearVal{};
    clearVal.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    clearVal.Color[0] = backgroundColor_.x;
    clearVal.Color[1] = backgroundColor_.y;
    clearVal.Color[2] = backgroundColor_.z;
    clearVal.Color[3] = backgroundColor_.w;

    device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &texDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearVal, IID_PPV_ARGS(&renderTextureResource_));
    currentResourceState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    // 2. 深度ステンシルリソース作成
    D3D12_RESOURCE_DESC depthDesc = texDesc;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthClearVal{};
    depthClearVal.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthClearVal.DepthStencil.Depth = 1.0f;
    depthClearVal.DepthStencil.Stencil = 0;

    device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearVal, IID_PPV_ARGS(&depthStencilResource_));

    // 3. RTV デスクリプタヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap_));
    rtvHandle_ = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(renderTextureResource_.Get(), &rtvDesc, rtvHandle_);

    // 4. DSV デスクリプタヒープ作成
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvDescriptorHeap_));
    dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvHandle_);

    // 5. ImGui 用 SRV
    srvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVforTexture2D(srvIndex_, renderTextureResource_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);

    viewport_.Width = static_cast<float>(kWidth_);
    viewport_.Height = static_cast<float>(kHeight_);
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = kWidth_;
    scissorRect_.bottom = kHeight_;
}

void EnemyStudio::RefreshAvailableModels() {
    availableModels_.clear();
    std::filesystem::path modelsDir("resources/3dModels");
    if (std::filesystem::exists(modelsDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(modelsDir)) {
            if (entry.is_directory()) {
                std::string folderName = entry.path().filename().string();
                std::string objName = folderName + ".obj";
                if (std::filesystem::exists(entry.path() / objName)) {
                    availableModels_.push_back(objName);
                }
            }
        }
    }
    std::sort(availableModels_.begin(), availableModels_.end());
}

void EnemyStudio::Update(float dt) {
    if (statusMessageTimer_ > 0.0f) {
        statusMessageTimer_ -= dt;
        if (statusMessageTimer_ <= 0.0f) statusMessage_ = "";
    }

    if (playPreviewAnim_) {
        previewAnimTimer_ += dt;
    }

    // カメラ座標計算 (オービット)
    float cosPitch = std::cos(cameraPitch_);
    Vector3 camPos = {
        cameraTarget_.x + cameraDistance_ * cosPitch * std::sin(cameraYaw_),
        cameraTarget_.y + cameraDistance_ * std::sin(cameraPitch_),
        cameraTarget_.z - cameraDistance_ * cosPitch * std::cos(cameraYaw_)
    };
    camera_.SetTranslate(camPos);
    camera_.SetRotation({ cameraPitch_, cameraYaw_, 0.0f });
    camera_.Update();

    // プレイヤーオブジェクトの更新
    if (playerObj_) {
        playerObj_->SetTranslate(playerPosition_);
        playerObj_->SetRotation({ 0.0f, 0.0f, 0.0f });
        playerObj_->Update();
    }

    // 装甲列車ボスモード時はボスのトランスフォームを更新して完了
    if (editorMode_ == EditorMode::BOSS_TRAIN) {
        if (previewBoss_) {
            previewBoss_->ApplyTransforms();
        }
        return;
    }

    // 選択中プリセットの取得と敵プレビューオブジェクト更新
    const auto* preset = EnemyPresetManager::GetInstance()->GetPreset(currentPresetKey_);
    if (!preset) return;

    // モデルの読み込み確認
    ModelManager::GetInstance()->LoadModel(preset->modelName);

    // 陣形オフセット計算（または装甲列車編成プレビュー）
    if (preset->behavior == "TRAIN_LOCO" && showFormation_) {
        // 装甲列車・4両編成プレビュー
        struct TrainPreviewPart {
            Vector3 scale;
            Vector4 color;
            float zOffset;
        };
        TrainPreviewPart parts[4] = {
            { preset->scale, preset->color, 0.0f },                                        // 0: 機関車
            { { 4.0f, 2.8f, 10.5f }, { 0.42f, 0.45f, 0.48f, 1.0f }, 13.5f },             // 1: 旋回砲塔車
            { { 4.0f, 3.2f, 10.5f }, { 0.48f, 0.38f, 0.38f, 1.0f }, 25.5f },             // 2: ミサイルコンテナ車
            { { 3.8f, 2.6f, 9.5f },  { 0.32f, 0.45f, 0.55f, 1.0f }, 37.0f }              // 3: 動力ジェネレーター車
        };

        for (int i = 0; i < 4 && i < static_cast<int>(enemyObjs_.size()); ++i) {
            enemyObjs_[i]->SetModel("cube.obj");
            enemyObjs_[i]->SetScale(parts[i].scale);
            enemyObjs_[i]->SetTranslate({ 0.0f, 0.0f, parts[i].zOffset });
            enemyObjs_[i]->SetRotation({ 0.0f, 3.14159265f, 0.0f });
            enemyObjs_[i]->SetColor(parts[i].color);
            enemyObjs_[i]->Update();
        }

        // コライダープレビューは先頭機関車
        Vector3 colCenter = { preset->colliderCenter.x, preset->colliderCenter.y, preset->colliderCenter.z };
        if (colliderBoxObj_) {
            colliderBoxObj_->SetTranslate(colCenter);
            colliderBoxObj_->SetScale(preset->colliderSize);
            colliderBoxObj_->Update();
        }
    } else {
        int activeCount = (showFormation_ && preset->formationType != "NONE") ? preset->formationCount : 1;
        activeCount = std::clamp(activeCount, 1, static_cast<int>(enemyObjs_.size()));
        auto offsets = EnemyPresetManager::CalculateFormationOffsets(preset->formationType, activeCount, preset->formationSpacing);

        // 行動パターンに応じたプレビューアニメーション移動
        Vector3 animOffset = { 0.0f, 0.0f, 0.0f };
        if (playPreviewAnim_) {
            float speed = (preset->moveSpeed > 0.0f) ? preset->moveSpeed : 1.0f;
            float amp = (preset->moveAmplitude > 0.0f) ? preset->moveAmplitude : 10.0f;

            if (preset->behavior == "PATROL_H") {
                animOffset.x = std::sin(previewAnimTimer_ * speed * 2.0f) * amp;
            } else if (preset->behavior == "PATROL_V") {
                animOffset.y = std::sin(previewAnimTimer_ * speed * 2.0f) * amp;
            } else if (preset->behavior == "HOVER_SHOOT" || preset->behavior == "HOVER") {
                animOffset.x = std::sin(previewAnimTimer_ * 1.5f) * (amp * 0.3f);
                animOffset.y = std::sin(previewAnimTimer_ * 3.0f) * (amp * 0.2f);
            } else if (preset->behavior == "SIN_WAVE") {
                animOffset.x = std::sin(previewAnimTimer_ * speed * 2.5f) * amp;
            }
        }

        for (int i = 0; i < activeCount; ++i) {
            if (!enemyObjs_[i]) continue;
            enemyObjs_[i]->SetModel(preset->modelName);
            enemyObjs_[i]->SetScale(preset->scale);
            Vector3 pos = {
                offsets[i].x + preset->modelPosOffset.x + animOffset.x,
                offsets[i].y + preset->modelPosOffset.y + animOffset.y,
                offsets[i].z + preset->modelPosOffset.z + animOffset.z
            };
            enemyObjs_[i]->SetTranslate(pos);
            enemyObjs_[i]->SetRotation({ 0.0f, 3.14159265f, 0.0f }); // プレイヤーに向ける
            enemyObjs_[i]->SetColor(preset->color);
            enemyObjs_[i]->Update();
        }

        // コライダープレビューオブジェクトの更新（代表として中央の機体に合わせる）
        Vector3 colCenter = {
            offsets[0].x + preset->modelPosOffset.x + preset->colliderCenter.x + animOffset.x,
            offsets[0].y + preset->modelPosOffset.y + preset->colliderCenter.y + animOffset.y,
            offsets[0].z + preset->modelPosOffset.z + preset->colliderCenter.z + animOffset.z
        };
        if (preset->colliderType == "SPHERE") {
            if (colliderSphereObj_) {
                colliderSphereObj_->SetTranslate(colCenter);
                colliderSphereObj_->SetScale({ preset->colliderRadius, preset->colliderRadius, preset->colliderRadius });
                colliderSphereObj_->Update();
            }
        } else {
            if (colliderBoxObj_) {
                colliderBoxObj_->SetTranslate(colCenter);
                colliderBoxObj_->SetScale(preset->colliderSize);
                colliderBoxObj_->Update();
            }
        }
    }
}

void EnemyStudio::Render() {
    if (!renderTextureResource_ || !dxCommon_ || !srvManager_ || !object3dCommon_) return;

    auto commandList = dxCommon_->GetCommandList();

    // 1. バリア遷移: Target へ
    if (currentResourceState_ != D3D12_RESOURCE_STATE_RENDER_TARGET) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = renderTextureResource_.Get();
        barrier.Transition.StateBefore = currentResourceState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        currentResourceState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    // 2. レンダーターゲットセット & クリア
    commandList->OMSetRenderTargets(1, &rtvHandle_, false, &dsvHandle_);
    float clearColor[4] = { backgroundColor_.x, backgroundColor_.y, backgroundColor_.z, backgroundColor_.w };
    commandList->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);

    srvManager_->PreDraw();

    // 3. カメラセット
    object3dCommon_->SetDefaultCamera(&camera_);

    // 4. 通常モデル描画
    object3dCommon_->SetCommonDrawSetting();

    if (editorMode_ == EditorMode::BOSS_TRAIN) {
        if (previewBoss_) {
            previewBoss_->Draw();
            if (showCollider_) {
                previewBoss_->DrawCollider();
            }
        }
    } else {
        // プレイヤー描画
        if (showPlayer_ && playerObj_) {
            playerObj_->Draw();
        }

        // 敵機体群描画
        const auto* preset = EnemyPresetManager::GetInstance()->GetPreset(currentPresetKey_);
        if (preset) {
            int activeCount = (showFormation_ && preset->formationType != "NONE") ? preset->formationCount : 1;
            activeCount = std::clamp(activeCount, 1, static_cast<int>(enemyObjs_.size()));
            for (int i = 0; i < activeCount; ++i) {
                if (enemyObjs_[i]) {
                    enemyObjs_[i]->Draw();
                }
            }
        }

        // 5. コライダーワイヤーフレーム描画
        if (showCollider_ && preset) {
            object3dCommon_->SetWireframeDrawSetting();
            if (preset->colliderType == "SPHERE") {
                if (colliderSphereObj_) colliderSphereObj_->Draw();
            } else {
                if (colliderBoxObj_) colliderBoxObj_->Draw();
            }
            object3dCommon_->SetCommonDrawSetting();
        }
    }

    // 6. バリア遷移: SRV へ
    if (currentResourceState_ != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = renderTextureResource_.Get();
        barrier.Transition.StateBefore = currentResourceState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        currentResourceState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
}

void EnemyStudio::DrawViewportWindow() {
    if (!showViewport_) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("エネミー画面", &showViewport_)) {
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = srvManager_->GetSRVGPUDescriptorHandle(srvIndex_);
        ImVec2 imagePos = ImGui::GetCursorScreenPos();
        ImGui::Image((ImTextureID)srvHandle.ptr, contentSize);

        // カメラ操作
        if (ImGui::IsItemHovered()) {
            HandleCameraInput(contentSize.x, contentSize.y);
        }

        // 3D グリッド描画オーバーレイ
        if (showGrid_) {
            DrawGridOverlay(imagePos, contentSize);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void EnemyStudio::HandleCameraInput(float contentWidth, float contentHeight) {
    ImGuiIO& io = ImGui::GetIO();

    bool isShift = io.KeyShift;
    bool isCtrl = io.KeyCtrl;
    bool isAlt = io.KeyAlt;

    // カメラの各方向ベクトル（ワールド座標）を精密計算
    float cosYaw = std::cos(cameraYaw_);
    float sinYaw = std::sin(cameraYaw_);
    float cosPitch = std::cos(cameraPitch_);
    float sinPitch = std::sin(cameraPitch_);

    // 画面右方向 (Right)
    Vector3 camRight = { cosYaw, 0.0f, sinYaw };
    // 画面上方向 (Up)
    Vector3 camUp = { -sinPitch * sinYaw, cosPitch, sinPitch * cosYaw };

    // --- 1. パン（平行移動）：中ボタンドラッグ または Shift + 右ドラッグ または Alt + 中ドラッグ ---
    bool isPanDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
                         (isShift && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) ||
                         (isAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Middle));

    if (isPanDragging) {
        ImVec2 delta = io.MouseDelta;
        // マウスの移動量に画面比と距離を乗じて自然な追従係数を算出
        float panFactor = cameraDistance_ * 0.0016f;

        // マウスの移動方向に合わせて注視点をスムーズにシフト
        cameraTarget_.x -= (camRight.x * delta.x - camUp.x * delta.y) * panFactor;
        cameraTarget_.y -= (camRight.y * delta.x - camUp.y * delta.y) * panFactor;
        cameraTarget_.z -= (camRight.z * delta.x - camUp.z * delta.y) * panFactor;
    }
    // --- 2. オービット回転：右ボタンドラッグ（Shiftなし） または Alt + 左ドラッグ ---
    else if ((!isShift && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) ||
             (isAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left))) {
        ImVec2 delta = io.MouseDelta;
        float rotSpeed = 0.0055f;
        cameraYaw_ += delta.x * rotSpeed;
        cameraPitch_ += delta.y * rotSpeed;
        // 真上・真下付近でのジンバルロック防止
        cameraPitch_ = std::clamp(cameraPitch_, -1.52f, 1.52f);
    }

    // --- 3. ズーム：マウスホイール または Ctrl + 右ドラッグ ---
    if (std::abs(io.MouseWheel) > 0.01f) {
        // 指数関数的なズーム（近距離では細かく、遠距離では一気に移動）
        cameraDistance_ *= (1.0f - io.MouseWheel * 0.12f);
        cameraDistance_ = std::clamp(cameraDistance_, 1.5f, 350.0f);
    } else if (isCtrl && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        ImVec2 delta = io.MouseDelta;
        cameraDistance_ *= (1.0f - delta.y * 0.01f);
        cameraDistance_ = std::clamp(cameraDistance_, 1.5f, 350.0f);
    }

    // --- 4. キーボードによるカメラパン (W, A, S, D, Q, E) ---
    if (ImGui::IsWindowFocused() || ImGui::IsItemHovered()) {
        float keyMoveSpeed = cameraDistance_ * 0.02f;
        if (io.KeyShift) keyMoveSpeed *= 2.5f;

        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            // 前方（Z-forward or Pitch込み）
            cameraTarget_.x -= sinYaw * keyMoveSpeed;
            cameraTarget_.z += cosYaw * keyMoveSpeed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S)) {
            cameraTarget_.x += sinYaw * keyMoveSpeed;
            cameraTarget_.z -= cosYaw * keyMoveSpeed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            cameraTarget_.x -= camRight.x * keyMoveSpeed;
            cameraTarget_.z -= camRight.z * keyMoveSpeed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_D)) {
            cameraTarget_.x += camRight.x * keyMoveSpeed;
            cameraTarget_.z += camRight.z * keyMoveSpeed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_E)) {
            cameraTarget_.y += keyMoveSpeed;
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q)) {
            cameraTarget_.y -= keyMoveSpeed;
        }
    }

    // --- 5. ダブルクリック: 現在のモードに応じた視点リセット ---
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (editorMode_ == EditorMode::BOSS_TRAIN) {
            cameraTarget_ = { 0.0f, 6.0f, -50.0f };
            cameraDistance_ = 100.0f;
            cameraYaw_ = 0.5f;
            cameraPitch_ = 0.35f;
        } else {
            cameraTarget_ = { 0.0f, 0.0f, 0.0f };
            cameraDistance_ = 18.0f;
            cameraYaw_ = 0.0f;
            cameraPitch_ = 0.35f;
        }
    }
}

void EnemyStudio::DrawControlWindow() {
    if (!showEditor_) return;

    if (ImGui::Begin("エネミーエディター", &showEditor_)) {
        // モード切り替えタブバー
        if (ImGui::BeginTabBar("EnemyStudioModeTabs")) {
            if (ImGui::BeginTabItem("通常エネミー編集")) {
                if (editorMode_ != EditorMode::NORMAL_ENEMY) {
                    editorMode_ = EditorMode::NORMAL_ENEMY;
                    cameraTarget_ = { 0.0f, 0.0f, 0.0f };
                    cameraDistance_ = 18.0f;
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("装甲列車ボス編集")) {
                if (editorMode_ != EditorMode::BOSS_TRAIN) {
                    editorMode_ = EditorMode::BOSS_TRAIN;
                    cameraTarget_ = { 0.0f, 6.0f, -50.0f };
                    cameraDistance_ = 100.0f;
                    cameraYaw_ = 0.5f;
                    cameraPitch_ = 0.35f;
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        // 装甲列車ボス編集モード
        if (editorMode_ == EditorMode::BOSS_TRAIN) {
            DrawBossControlUI();
            ImGui::End();
            return;
        }

        auto presetMgr = EnemyPresetManager::GetInstance();
        const auto& names = presetMgr->GetPresetNames();

        // 1. プリセット選択
        if (ImGui::CollapsingHeader("プリセット管理 (Presets)", ImGuiTreeNodeFlags_DefaultOpen)) {
            std::string currentLabel = currentPresetKey_;
            const auto* curPreset = presetMgr->GetPreset(currentPresetKey_);
            if (curPreset && !curPreset->displayName.empty()) {
                currentLabel = curPreset->displayName;
            }

            if (ImGui::BeginCombo("選択中の敵タイプ", currentLabel.c_str())) {
                for (const auto& name : names) {
                    bool isSelected = (currentPresetKey_ == name);
                    const auto* p = presetMgr->GetPreset(name);
                    std::string itemLabel = (p && !p->displayName.empty()) ? p->displayName : name;
                    if (ImGui::Selectable(itemLabel.c_str(), isSelected)) {
                        currentPresetKey_ = name;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::InputText("新規タイプ名", newPresetKeyInput_, sizeof(newPresetKeyInput_));
            ImGui::SameLine();
            if (ImGui::Button("新規作成")) {
                std::string newKey(newPresetKeyInput_);
                if (!newKey.empty() && !presetMgr->GetPreset(newKey)) {
                    EnemyPresetData newData;
                    newData.typeName = newKey;
                    newData.displayName = newKey;
                    presetMgr->SetPreset(newKey, newData);
                    currentPresetKey_ = newKey;
                    statusMessage_ = "プリセット '" + newKey + "' を作成しました";
                    statusMessageTimer_ = 3.0f;
                }
            }

            if (ImGui::Button("現在のプリセットを複製")) {
                std::string copyKey = currentPresetKey_ + "_COPY";
                const auto* cur = presetMgr->GetPreset(currentPresetKey_);
                if (cur) {
                    EnemyPresetData copyData = *cur;
                    copyData.typeName = copyKey;
                    copyData.displayName += " (コピー)";
                    presetMgr->SetPreset(copyKey, copyData);
                    currentPresetKey_ = copyKey;
                    statusMessage_ = "プリセットを複製しました: " + copyKey;
                    statusMessageTimer_ = 3.0f;
                }
            }
            ImGui::SameLine();
            if (names.size() > 1 && ImGui::Button("削除")) {
                presetMgr->DeletePreset(currentPresetKey_);
                const auto& newNames = presetMgr->GetPresetNames();
                if (!newNames.empty()) currentPresetKey_ = newNames[0];
                statusMessage_ = "プリセットを削除しました";
                statusMessageTimer_ = 3.0f;
            }
        }

        EnemyPresetData* preset = presetMgr->GetPreset(currentPresetKey_);
        if (preset) {
            // 2. 基本ステータス設定
            if (ImGui::CollapsingHeader("基本プロパティ (Properties)", ImGuiTreeNodeFlags_DefaultOpen)) {
                char dispBuf[64];
                strncpy_s(dispBuf, preset->displayName.c_str(), sizeof(dispBuf));
                if (ImGui::InputText("表示名", dispBuf, sizeof(dispBuf))) {
                    preset->displayName = dispBuf;
                }

                // モデル選択
                if (ImGui::BeginCombo("使用3Dモデル", preset->modelName.c_str())) {
                    for (const auto& model : availableModels_) {
                        bool isSel = (preset->modelName == model);
                        if (ImGui::Selectable(model.c_str(), isSel)) {
                            preset->modelName = model;
                        }
                        if (isSel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                float scale[3] = { preset->scale.x, preset->scale.y, preset->scale.z };
                if (ImGui::DragFloat3("スケール (Scale)", scale, 0.05f, 0.1f, 50.0f)) {
                    preset->scale = { scale[0], scale[1], scale[2] };
                }

                float posOffset[3] = { preset->modelPosOffset.x, preset->modelPosOffset.y, preset->modelPosOffset.z };
                if (ImGui::DragFloat3("位置オフセット (Pos Offset)", posOffset, 0.05f, -20.0f, 20.0f, "%.2f")) {
                    preset->modelPosOffset = { posOffset[0], posOffset[1], posOffset[2] };
                }

                float col[4] = { preset->color.x, preset->color.y, preset->color.z, preset->color.w };
                if (ImGui::ColorEdit4("カラー乗数 (Color)", col)) {
                    preset->color = { col[0], col[1], col[2], col[3] };
                }

                ImGui::DragInt("耐久値 (HP)", &preset->hp, 1, 1, 1000);
                ImGui::DragFloat("移動速度 (Speed)", &preset->moveSpeed, 0.05f, 0.0f, 10.0f);

                // 行動パターン
                struct BehaviorOption {
                    const char* key;
                    const char* label;
                };
                const BehaviorOption behaviors[] = {
                    { "PATROL_H", "左右往復 (PATROL_H)" },
                    { "PATROL_V", "上下往復 (PATROL_V)" },
                    { "HOVER_SHOOT", "滞空浮遊射撃 (HOVER_SHOOT)" },
                    { "SIN_WAVE", "蛇行前進 (SIN_WAVE)" },
                    { "STRAIGHT", "直進突撃 (STRAIGHT)" },
                    { "TURRET", "固定砲台 (TURRET)" },
                    { "TRAIN_LOCO", "装甲列車・機関車" },
                    { "TRAIN_TURRET", "装甲列車・砲塔車" },
                    { "TRAIN_MISSILE", "装甲列車・ミサイル車" }
                };
                int currentBehaviorIdx = 0;
                for (int i = 0; i < IM_ARRAYSIZE(behaviors); ++i) {
                    if (preset->behavior == behaviors[i].key) {
                        currentBehaviorIdx = i;
                        break;
                    }
                }
                if (ImGui::BeginCombo("行動AIタイプ", behaviors[currentBehaviorIdx].label)) {
                    for (int i = 0; i < IM_ARRAYSIZE(behaviors); ++i) {
                        bool isSel = (currentBehaviorIdx == i);
                        if (ImGui::Selectable(behaviors[i].label, isSel)) {
                            preset->behavior = behaviors[i].key;
                        }
                        if (isSel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                // 往復やサイン波、滞空などの場合に移動幅を設定可能に
                if (preset->behavior == "PATROL_H" || preset->behavior == "PATROL_V" ||
                    preset->behavior == "SIN_WAVE" || preset->behavior == "HOVER_SHOOT" || preset->behavior == "HOVER") {
                    ImGui::DragFloat("移動幅 (Amplitude / Range)", &preset->moveAmplitude, 0.5f, 1.0f, 100.0f, "%.1f m");
                }

                ImGui::DragFloat("射撃間隔 (Shoot Interval)", &preset->shootInterval, 1.0f, 0.0f, 600.0f, "%.0f フレーム (0で無効)");
                ImGui::Checkbox("ホーミング弾を発射する", &preset->isHomingBullet);
                ImGui::DragFloat("弾速 (Bullet Speed)", &preset->bulletSpeed, 0.1f, 0.1f, 10.0f);
            }

            // 3. 当たり判定（コライダー）設定 ★ゲーム側で設定可能！
            if (ImGui::CollapsingHeader("当たり判定・コライダー (Collider)", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "★ゲーム側オーサリング: リアルタイムにコライダーを調整");

                int colType = (preset->colliderType == "BOX") ? 1 : 0;
                if (ImGui::RadioButton("球体 (SPHERE)", &colType, 0)) {
                    preset->colliderType = "SPHERE";
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("直方体 (BOX / OBB)", &colType, 1)) {
                    preset->colliderType = "BOX";
                }

                float center[3] = { preset->colliderCenter.x, preset->colliderCenter.y, preset->colliderCenter.z };
                if (ImGui::DragFloat3("中心オフセット (Center)", center, 0.05f, -50.0f, 50.0f)) {
                    preset->colliderCenter = { center[0], center[1], center[2] };
                }

                if (preset->colliderType == "SPHERE") {
                    ImGui::DragFloat("球半径 (Radius)", &preset->colliderRadius, 0.05f, 0.1f, 50.0f);
                } else {
                    float size[3] = { preset->colliderSize.x, preset->colliderSize.y, preset->colliderSize.z };
                    if (ImGui::DragFloat3("サイズ (Size)", size, 0.05f, 0.1f, 50.0f)) {
                        preset->colliderSize = { size[0], size[1], size[2] };
                    }
                }
            }

            // 4. 陣形（Formation）設定
            if (ImGui::CollapsingHeader("陣形・編隊設定 (Formation)", ImGuiTreeNodeFlags_DefaultOpen)) {
                const char* formations[] = { "NONE", "LINE_H", "LINE_V", "V_SHAPE", "BOX", "CIRCLE" };
                int currentFormIdx = 0;
                for (int i = 0; i < IM_ARRAYSIZE(formations); ++i) {
                    if (preset->formationType == formations[i]) {
                        currentFormIdx = i;
                        break;
                    }
                }
                if (ImGui::Combo("陣形タイプ", &currentFormIdx, formations, IM_ARRAYSIZE(formations))) {
                    preset->formationType = formations[currentFormIdx];
                }

                if (preset->formationType != "NONE") {
                    ImGui::SliderInt("機体数 (Count)", &preset->formationCount, 1, 10);
                    ImGui::DragFloat("機体間隔 (Spacing)", &preset->formationSpacing, 0.5f, 1.0f, 50.0f);
                }
            }
        }

        // 5. プレビュー表示オプション
        if (ImGui::CollapsingHeader("プレビュー表示オプション (View Options)")) {
            ImGui::Checkbox("プレイヤー機体を表示", &showPlayer_);
            if (showPlayer_) {
                float pPos[3] = { playerPosition_.x, playerPosition_.y, playerPosition_.z };
                if (ImGui::DragFloat3("プレイヤー位置", pPos, 0.5f, -100.0f, 100.0f)) {
                    playerPosition_ = { pPos[0], pPos[1], pPos[2] };
                }
            }
            ImGui::Checkbox("コライダー（当たり判定）を表示", &showCollider_);
            ImGui::Checkbox("陣形プレビューを表示", &showFormation_);
            ImGui::Checkbox("移動アニメーションを再生", &playPreviewAnim_);
            ImGui::Checkbox("グリッドを表示", &showGrid_);
            if (ImGui::Button("カメラを初期位置に戻す")) {
                cameraTarget_ = { 0.0f, 0.0f, 0.0f };
                cameraDistance_ = 18.0f;
                cameraYaw_ = 0.0f;
                cameraPitch_ = 0.35f;
            }
        }

        // 6. 保存・再読み込みボタン
        ImGui::Separator();
        if (ImGui::Button("JSON設定を保存 (Save All Presets)", ImVec2(240, 32))) {
            if (presetMgr->SavePresets()) {
                statusMessage_ = "敵プリセットを resources/json/enemy/enemy_presets.json に保存しました！";
                statusMessageTimer_ = 4.0f;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("設定を再読込 (Reload)", ImVec2(160, 32))) {
            if (presetMgr->LoadPresets()) {
                statusMessage_ = "設定を再読み込みしました";
                statusMessageTimer_ = 3.0f;
            }
        }

        if (!statusMessage_.empty()) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", statusMessage_.c_str());
        }
    }
    ImGui::End();
}

void EnemyStudio::DrawGridOverlay(const ImVec2& imagePos, const ImVec2& imageSize) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    Matrix4x4 vpMat = camera_.GetViewMatrix() * camera_.GetProjectionMatrix();

    float halfSize = gridSize_ * 0.5f;
    uint32_t gridColor = IM_COL32(80, 90, 110, 80);
    uint32_t axisXColor = IM_COL32(230, 60, 60, 160);
    uint32_t axisZColor = IM_COL32(60, 90, 230, 160);

    // Z軸に平行な線 (X方向に変化)
    for (float x = -halfSize; x <= halfSize; x += gridSpacing_) {
        uint32_t col = (std::abs(x) < 0.001f) ? axisZColor : gridColor;
        float thick = (std::abs(x) < 0.001f) ? 1.5f : 1.0f;
        Draw3DLine(drawList, { x, 0.0f, -halfSize }, { x, 0.0f, halfSize }, vpMat, imagePos, imageSize, col, thick);
    }

    // X軸に平行な線 (Z方向に変化)
    for (float z = -halfSize; z <= halfSize; z += gridSpacing_) {
        uint32_t col = (std::abs(z) < 0.001f) ? axisXColor : gridColor;
        float thick = (std::abs(z) < 0.001f) ? 1.5f : 1.0f;
        Draw3DLine(drawList, { -halfSize, 0.0f, z }, { halfSize, 0.0f, z }, vpMat, imagePos, imageSize, col, thick);
    }
}

void EnemyStudio::Draw3DLine(ImDrawList* drawList, const Vector3& start, const Vector3& end,
                             const Matrix4x4& vpMat, const ImVec2& minPos, const ImVec2& size,
                             uint32_t color, float thickness) {
    auto projectPoint = [&](const Vector3& p, ImVec2& outScreen) -> bool {
        Vector4 v = { p.x, p.y, p.z, 1.0f };
        Vector4 clip = {
            v.x * vpMat.m[0][0] + v.y * vpMat.m[1][0] + v.z * vpMat.m[2][0] + v.w * vpMat.m[3][0],
            v.x * vpMat.m[0][1] + v.y * vpMat.m[1][1] + v.z * vpMat.m[2][1] + v.w * vpMat.m[3][1],
            v.x * vpMat.m[0][2] + v.y * vpMat.m[1][2] + v.z * vpMat.m[2][2] + v.w * vpMat.m[3][2],
            v.x * vpMat.m[0][3] + v.y * vpMat.m[1][3] + v.z * vpMat.m[2][3] + v.w * vpMat.m[3][3],
        };

        if (clip.w <= 0.001f) return false;

        float ndcX = clip.x / clip.w;
        float ndcY = clip.y / clip.w;

        if (ndcX < -1.5f || ndcX > 1.5f || ndcY < -1.5f || ndcY > 1.5f) return false;

        outScreen.x = minPos.x + (ndcX * 0.5f + 0.5f) * size.x;
        outScreen.y = minPos.y + (-ndcY * 0.5f + 0.5f) * size.y;
        return true;
    };

    ImVec2 s, e;
    if (projectPoint(start, s) && projectPoint(end, e)) {
        drawList->AddLine(s, e, color, thickness);
    }
}

void EnemyStudio::DrawBossControlUI() {
    if (!previewBoss_) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "装甲列車ボスが初期化されていません");
        return;
    }

    const auto& cars = previewBoss_->GetCarriages();

    // 1. ファイル操作ヘッダー
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.8f, 1.0f), "装甲列車ボス・パーツ配置オーサリング");
    ImGui::Separator();

    if (ImGui::Button("設定を保存 (Save to boss_settings.json)", ImVec2(260, 30))) {
        if (previewBoss_->SaveSettingsJson()) {
            statusMessage_ = "設定を boss_settings.json に保存しました！";
            statusMessageTimer_ = 3.5f;
        } else {
            statusMessage_ = "保存に失敗しました。";
            statusMessageTimer_ = 3.5f;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("設定を再読み込み (Reload)", ImVec2(180, 30))) {
        if (previewBoss_->LoadSettingsJson()) {
            statusMessage_ = "設定を再読み込みしました！";
            statusMessageTimer_ = 3.5f;
        }
    }

    if (!statusMessage_.empty()) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "%s", statusMessage_.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();

    // 2. カメラ操作・視点プリセット
    if (ImGui::CollapsingHeader("カメラ・視点操作 (Camera Controls)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("車両フォーカス:");
        if (ImGui::Button("全体俯瞰")) {
            cameraTarget_ = { 0.0f, 6.0f, -50.0f };
            cameraDistance_ = 100.0f;
            cameraPitch_ = 0.35f;
            cameraYaw_ = 0.5f;
        }
        ImGui::SameLine();
        if (ImGui::Button("1両目 (機関車)")) {
            cameraTarget_ = { 0.0f, 8.0f, 0.0f };
            cameraDistance_ = 35.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("2両目 (砲塔車)")) {
            cameraTarget_ = { 0.0f, 6.0f, cars.size() > 1 ? cars[1].lengthOffset : -38.0f };
            cameraDistance_ = 30.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("3両目 (主砲車)")) {
            cameraTarget_ = { 0.0f, 6.0f, cars.size() > 2 ? cars[2].lengthOffset : -72.0f };
            cameraDistance_ = 32.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("4両目 (ミサイル)")) {
            cameraTarget_ = { 0.0f, 6.0f, cars.size() > 3 ? cars[3].lengthOffset : -105.0f };
            cameraDistance_ = 30.0f;
        }

        ImGui::Text("アングル切り替え (Blender風):");
        if (ImGui::Button("真上 (Top)")) {
            cameraPitch_ = 1.50f;
            cameraYaw_ = 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("右横 (Side R)")) {
            cameraPitch_ = 0.0f;
            cameraYaw_ = 1.5708f;
        }
        ImGui::SameLine();
        if (ImGui::Button("左横 (Side L)")) {
            cameraPitch_ = 0.0f;
            cameraYaw_ = -1.5708f;
        }
        ImGui::SameLine();
        if (ImGui::Button("正面 (Front)")) {
            cameraPitch_ = 0.0f;
            cameraYaw_ = 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("後面 (Back)")) {
            cameraPitch_ = 0.0f;
            cameraYaw_ = 3.14159f;
        }
        ImGui::SameLine();
        if (ImGui::Button("斜め (Iso)")) {
            cameraPitch_ = 0.35f;
            cameraYaw_ = 0.6f;
        }

        ImGui::TextDisabled("※操作: [右ドラッグ]回転 / [中ドラッグ or Shift+右ドラッグ]パン / [ホイール]ズーム / [W,A,S,D,Q,E]移動");
        ImGui::Checkbox("当たり判定ブロック（赤）を表示", &showCollider_);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // 3. ボス基本設定
    if (ImGui::CollapsingHeader("ボス基本・接近距離設定 (Boss Settings)", ImGuiTreeNodeFlags_DefaultOpen)) {
        float bScale = previewBoss_->GetScaleMultiplier();
        if (ImGui::SliderFloat("ボス全体スケール (Scale)", &bScale, 0.2f, 3.0f, "%.2fx")) {
            previewBoss_->SetScaleMultiplier(bScale);
        }

        bool isFollow = previewBoss_->IsFollowPlayer();
        if (ImGui::Checkbox("自機追従・並走チェイス有効化 (Follow Player)", &isFollow)) {
            previewBoss_->SetFollowPlayer(isFollow);
        }

        float leadDist = previewBoss_->GetDesiredLeadDistance();
        if (ImGui::SliderFloat("基準接近距離 (Lead Distance)", &leadDist, 100.0f, 400.0f, "%.1f m")) {
            previewBoss_->SetDesiredLeadDistance(leadDist);
        }
        ImGui::TextDisabled("※最後尾が自機に被らない推奨値: 180m〜240m (現在列車長: 約130m)");

        bool isRandLead = previewBoss_->IsRandomLeadDistanceEnabled();
        if (ImGui::Checkbox("ランダム接近チャンス演出有効 (Random Lead Distance)", &isRandLead)) {
            previewBoss_->SetRandomLeadDistanceEnabled(isRandLead);
        }

        ImGui::Separator();
        ImGui::Text("並走レーン・側面露出設定:");
        float laneOff = previewBoss_->GetLaneOffset();
        if (ImGui::SliderFloat("並走オフセット (Lane Offset)", &laneOff, -25.0f, 25.0f, "%.1f m")) {
            previewBoss_->SetLaneOffset(laneOff);
        }
        ImGui::TextDisabled("※左右にずらすことで、4両目だけでなく全車両の側面がズラリと見渡せます");

        bool isDynLane = previewBoss_->IsDynamicLaneChangeEnabled();
        if (ImGui::Checkbox("緩やかな車線変更を有効化 (Dynamic Lane Change)", &isDynLane)) {
            previewBoss_->SetDynamicLaneChangeEnabled(isDynLane);
        }

        ImGui::Separator();
        ImGui::Text("地面接地設定 (Ground Snap):");
        bool isSnap = previewBoss_->IsGroundSnapEnabled();
        if (ImGui::Checkbox("地面メッシュへの自動吸着 (Ground Snap)", &isSnap)) {
            previewBoss_->SetGroundSnapEnabled(isSnap);
        }
        float snapOff = previewBoss_->GetGroundSnapOffset();
        if (ImGui::DragFloat("接地高さ微調整 (Snap Offset)", &snapOff, 0.05f, -5.0f, 5.0f, "%.2f m")) {
            previewBoss_->SetGroundSnapOffset(snapOff);
        }
    }

    ImGui::Spacing();

    // 4. 車両ごとのパーツ調整
    previewBoss_->DrawPartsEditorImGui();
}
