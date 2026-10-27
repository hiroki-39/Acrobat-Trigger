#include "EffectStudio.h"
#include "externals/imgui/imgui.h"
#include "externals/nlohmann/json.hpp"
#include "KHEngine/Graphics/Resource/Descriptor/SrvManager.h"
#include "KHEngine/Graphics/3d/Particle/ParticleManager.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "KHEngine/Graphics/Billboard/Billboard.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <numbers>

EffectStudio* EffectStudio::GetInstance()
{
    static EffectStudio instance;
    return &instance;
}

void EffectStudio::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    CreateRenderTargets();

    // プレビュー用カメラ初期化
    camera_.SetFovY(45.0f * (std::numbers::pi_v<float> / 180.0f));
    camera_.SetAspectRatio(static_cast<float>(kWidth_) / static_cast<float>(kHeight_));
    camera_.SetNearClip(0.1f);
    camera_.SetFarClip(2000.0f);

    targetCameraTarget_ = cameraTarget_;
    targetCameraDistance_ = cameraDistance_;
    targetCameraYaw_ = cameraYaw_;
    targetCameraPitch_ = cameraPitch_;

    // パーティクル基本プリミティブアセットの登録確認
    auto particleMgr = ParticleManager::GetInstance();
    particleMgr->RegisterQuad("quad", "circle2.png");
    particleMgr->RegisterRing("ring", "resources/sprites/effect/gradationLine.png", 32, 0.5f, 1.0f);
    particleMgr->RegisterCylinder("Cylinder", "resources/sprites/effect/gradationLine.png");

    // プレビュー用エフェクト初期化 (CPU)
    previewEffect_.Initialize(dxCommon_, srvManager_);

    // GPU パーティクル初期化
    gpuParticleSystem_.Initialize(dxCommon_, srvManager_);

    // JSON ファイルリストを取得
    RefreshFileList();
    RefreshGPUFileList();

    // 初期プリセットの読み込み
    if (!gpuJsonFiles_.empty())
    {
        auto it = std::find(gpuJsonFiles_.begin(), gpuJsonFiles_.end(), "cyber_voxel_explosion.json");
        if (it != gpuJsonFiles_.end())
        {
            selectedGpuFileIndex_ = static_cast<int>(std::distance(gpuJsonFiles_.begin(), it));
            LoadGPUJson(*it);
        }
        else
        {
            selectedGpuFileIndex_ = 0;
            LoadGPUJson(gpuJsonFiles_[0]);
        }
    }

    if (!jsonFiles_.empty())
    {
        auto it = std::find(jsonFiles_.begin(), jsonFiles_.end(), "default_effect.json");
        if (it != jsonFiles_.end())
        {
            selectedFileIndex_ = static_cast<int>(std::distance(jsonFiles_.begin(), it));
            currentLoadedFile_ = *it;
            previewEffect_.LoadFromJson(currentLoadedFile_);
        }
        else
        {
            selectedFileIndex_ = 0;
            currentLoadedFile_ = jsonFiles_[0];
            previewEffect_.LoadFromJson(currentLoadedFile_);
        }
    }
}

void EffectStudio::FocusOrigin()
{
    targetCameraTarget_ = { 0.0f, 0.0f, 0.0f };
}

void EffectStudio::SetCameraPresetView(int preset)
{
    switch (preset)
    {
    case 0: // 斜め見下ろし (Isometric)
        targetCameraYaw_ = 0.785f;  // 45度
        targetCameraPitch_ = 0.45f; // 約26度
        break;
    case 1: // 正面 (Front)
        targetCameraYaw_ = 0.0f;
        targetCameraPitch_ = 0.0f;
        break;
    case 2: // 真上 (Top)
        targetCameraYaw_ = 0.0f;
        targetCameraPitch_ = 1.54f; // 約88度
        break;
    case 3: // 側面 (Side / Right)
        targetCameraYaw_ = 1.57f;   // 90度
        targetCameraPitch_ = 0.0f;
        break;
    }
}

void EffectStudio::CreateRenderTargets()
{
    auto device = dxCommon_->GetDevice();

    // 1. カラーバッファ (RenderTarget) の生成
    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resDesc.Width = kWidth_;
    resDesc.Height = kHeight_;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    resDesc.SampleDesc.Count = 1;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    clearValue.Color[0] = backgroundColor_.x;
    clearValue.Color[1] = backgroundColor_.y;
    clearValue.Color[2] = backgroundColor_.z;
    clearValue.Color[3] = backgroundColor_.w;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearValue,
        IID_PPV_ARGS(&renderTextureResource_));
    assert(SUCCEEDED(hr));
    currentResourceState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // 2. 深度ステンシルバッファの生成
    D3D12_RESOURCE_DESC depthDesc = resDesc;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthClear{};
    depthClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthClear.DepthStencil.Depth = 1.0f;
    depthClear.DepthStencil.Stencil = 0;

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClear,
        IID_PPV_ARGS(&depthStencilResource_));
    assert(SUCCEEDED(hr));

    // 3. RTV / DSV デスクリプタヒープの生成
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap_));
    assert(SUCCEEDED(hr));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1;
    hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvDescriptorHeap_));
    assert(SUCCEEDED(hr));

    rtvHandle_ = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    // 4. ビューの生成
    device->CreateRenderTargetView(renderTextureResource_.Get(), nullptr, rtvHandle_);
    device->CreateDepthStencilView(depthStencilResource_.Get(), nullptr, dsvHandle_);

    // 5. ImGui で描画するための SRV を SrvManager に確保して登録
    srvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVforTexture2D(srvIndex_, renderTextureResource_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);

    // 6. ビューポート & シザー設定
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = static_cast<float>(kWidth_);
    viewport_.Height = static_cast<float>(kHeight_);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = kWidth_;
    scissorRect_.bottom = kHeight_;
}

void EffectStudio::RefreshFileList()
{
    jsonFiles_.clear();
    std::filesystem::create_directories("resources/json/particles");

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator("resources/json/particles"))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                jsonFiles_.push_back(entry.path().filename().string());
            }
        }
        std::sort(jsonFiles_.begin(), jsonFiles_.end());
    }
    catch (const std::exception& e)
    {
        statusMessage_ = "ファイル走査エラー: " + std::string(e.what());
    }
}

void EffectStudio::RefreshGPUFileList()
{
    gpuJsonFiles_.clear();
    std::filesystem::create_directories("resources/effects/gpu");

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator("resources/effects/gpu"))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                gpuJsonFiles_.push_back(entry.path().filename().string());
            }
        }
        std::sort(gpuJsonFiles_.begin(), gpuJsonFiles_.end());
    }
    catch (const std::exception& e)
    {
        statusMessage_ = "GPUファイル走査エラー: " + std::string(e.what());
    }
}

void EffectStudio::SaveGPUJson(const std::string& filename)
{
    try
    {
        std::filesystem::create_directories("resources/effects/gpu");
        std::string fullPath = "resources/effects/gpu/" + filename;
        if (!fullPath.ends_with(".json")) fullPath += ".json";

        nlohmann::json j;
        j["name"] = filename;
        j["patternType"] = gpuEmitterData_.patternType;
        j["count"] = gpuEmitterData_.count;
        j["radius"] = gpuEmitterData_.radius;
        j["speed"] = gpuEmitterData_.speed;
        j["spread"] = gpuEmitterData_.spread;
        j["gravity"] = gpuEmitterData_.gravity;
        j["lifeTime"] = gpuEmitterData_.lifeTime;
        j["startScale"] = gpuEmitterData_.startScale;
        j["endScale"] = gpuEmitterData_.endScale;
        j["startColor"] = { gpuEmitterData_.startColor.x, gpuEmitterData_.startColor.y, gpuEmitterData_.startColor.z, gpuEmitterData_.startColor.w };
        j["endColor"] = { gpuEmitterData_.endColor.x, gpuEmitterData_.endColor.y, gpuEmitterData_.endColor.z, gpuEmitterData_.endColor.w };
        j["blendMode"] = static_cast<int>(gpuBlendMode_);

        std::ofstream ofs(fullPath);
        ofs << j.dump(4);
        statusMessage_ = "GPUエフェクト保存完了: " + filename;
        RefreshGPUFileList();
    }
    catch (const std::exception& e)
    {
        statusMessage_ = "保存エラー: " + std::string(e.what());
    }
}

void EffectStudio::LoadGPUJson(const std::string& filename)
{
    try
    {
        std::string fullPath = "resources/effects/gpu/" + filename;
        std::ifstream ifs(fullPath);
        if (!ifs.is_open()) return;

        nlohmann::json j;
        ifs >> j;

        gpuEmitterData_.patternType = j.value("patternType", 0);
        gpuEmitterData_.count = j.value("count", 256);
        gpuEmitterData_.radius = j.value("radius", 0.5f);
        gpuEmitterData_.speed = j.value("speed", 5.0f);
        gpuEmitterData_.spread = j.value("spread", 1.0f);
        gpuEmitterData_.gravity = j.value("gravity", 0.0f);
        gpuEmitterData_.lifeTime = j.value("lifeTime", 1.0f);
        gpuEmitterData_.startScale = j.value("startScale", 0.2f);
        gpuEmitterData_.endScale = j.value("endScale", 0.0f);

        if (j.contains("startColor") && j["startColor"].is_array() && j["startColor"].size() >= 4)
        {
            gpuEmitterData_.startColor = { j["startColor"][0], j["startColor"][1], j["startColor"][2], j["startColor"][3] };
        }
        if (j.contains("endColor") && j["endColor"].is_array() && j["endColor"].size() >= 4)
        {
            gpuEmitterData_.endColor = { j["endColor"][0], j["endColor"][1], j["endColor"][2], j["endColor"][3] };
        }
        gpuBlendMode_ = static_cast<GPUParticleBlendMode>(j.value("blendMode", 1));

        currentLoadedGpuFile_ = filename;
        statusMessage_ = "GPUエフェクト読込完了: " + filename;
    }
    catch (const std::exception& e)
    {
        statusMessage_ = "読込エラー: " + std::string(e.what());
    }
}

void EffectStudio::Update(float dt)
{
    float scaledDt = dt * playbackSpeed_;

    if (isGPUMode_)
    {
        // GPU パーティクルの更新
        gpuParticleSystem_.Update(scaledDt);

        // 自動射出ループ
        if (gpuAutoEmit_)
        {
            gpuEmitTimer_ += scaledDt;
            if (gpuEmitTimer_ >= gpuEmitInterval_)
            {
                gpuEmitTimer_ = 0.0f;
                gpuParticleSystem_.Emit(gpuEmitterData_);
            }
        }
    }
    else
    {
        // CPU パーティクルの自動ループ再生
        if (autoLoop_)
        {
            loopTimer_ += scaledDt;
            if (loopTimer_ >= loopInterval_)
            {
                loopTimer_ = 0.0f;
                previewEffect_.Play();
            }
        }
        Matrix4x4 bbMat = Billboard::CreateFromCamera(&camera_, true);
        previewEffect_.SetPosition({ 0.0f, 0.0f, 0.0f });
        previewEffect_.Update(scaledDt, camera_.GetViewMatrix(), camera_.GetProjectionMatrix(), bbMat);
    }

    // カメラの滑らかなイージング補間 (指数平滑化)
    float blend = 1.0f - std::exp(-22.0f * dt);
    cameraYaw_ += (targetCameraYaw_ - cameraYaw_) * blend;
    cameraPitch_ += (targetCameraPitch_ - cameraPitch_) * blend;
    cameraDistance_ += (targetCameraDistance_ - cameraDistance_) * blend;
    cameraTarget_.x += (targetCameraTarget_.x - cameraTarget_.x) * blend;
    cameraTarget_.y += (targetCameraTarget_.y - cameraTarget_.y) * blend;
    cameraTarget_.z += (targetCameraTarget_.z - cameraTarget_.z) * blend;

    // カメラ位置の更新 (注視点 + 球面座標)
    Vector3 camPos;
    camPos.x = cameraTarget_.x + cameraDistance_ * std::cos(cameraPitch_) * std::sin(cameraYaw_);
    camPos.y = cameraTarget_.y + cameraDistance_ * std::sin(cameraPitch_);
    camPos.z = cameraTarget_.z - cameraDistance_ * std::cos(cameraPitch_) * std::cos(cameraYaw_);

    camera_.SetTranslate(camPos);
    camera_.SetRotation({ cameraPitch_, -cameraYaw_, 0.0f }); // 常に原点(注視点)を真正面に捉える
    camera_.Update();
}

void EffectStudio::Render()
{
    if (!renderTextureResource_ || !dxCommon_ || !srvManager_) return;

    auto commandList = dxCommon_->GetCommandList();

    // 1. バリア遷移: 描画前 (Target へ遷移)
    if (currentResourceState_ != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = renderTextureResource_.Get();
        barrier.Transition.StateBefore = currentResourceState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        currentResourceState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    // 2. レンダーターゲット & 深度バッファをセット
    commandList->OMSetRenderTargets(1, &rtvHandle_, false, &dsvHandle_);

    // 3. 画面クリア
    float clearColor[4] = { backgroundColor_.x, backgroundColor_.y, backgroundColor_.z, backgroundColor_.w };
    commandList->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // 4. ビューポート & シザー矩形セット
    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);

    // 5. SRV ディスクリプタヒープをセット
    srvManager_->PreDraw();

    // 6. エフェクト描画
    if (isGPUMode_)
    {
        Matrix4x4 vpMat = camera_.GetViewProjectionMatrix();
        Matrix4x4 bbMat = Billboard::CreateFromCamera(&camera_, true);
        uint32_t texIndex = TextureManager::GetInstance()->GetDefaultTextureIndex();
        gpuParticleSystem_.Draw(vpMat, bbMat, texIndex, gpuBlendMode_);
    }
    else
    {
        previewEffect_.Draw();
    }

    // 7. バリア遷移: 描画後 (ImGui表示用に PIXEL_SHADER_RESOURCE へ遷移)
    if (currentResourceState_ != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    {
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

void EffectStudio::DrawViewportWindow()
{
#ifdef USE_IMGUI
    if (!showViewport_) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("エフェクト画面", &showViewport_))
    {
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        if (contentSize.x > 0.0f && contentSize.y > 0.0f)
        {
            float aspect = static_cast<float>(kWidth_) / static_cast<float>(kHeight_);
            float fitWidth = contentSize.x;
            float fitHeight = contentSize.x / aspect;

            if (fitHeight > contentSize.y)
            {
                fitHeight = contentSize.y;
                fitWidth = contentSize.y * aspect;
            }

            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            float offsetX = (contentSize.x - fitWidth) * 0.5f;
            float offsetY = (contentSize.y - fitHeight) * 0.5f;
            cursorPos.x += offsetX;
            cursorPos.y += offsetY;

            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + offsetX, ImGui::GetCursorPosY() + offsetY));

            // オフスクリーンテクスチャを描画
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = srvManager_->GetSRVGPUDescriptorHandle(srvIndex_);
            ImGui::Image((ImTextureID)gpuHandle.ptr, ImVec2(fitWidth, fitHeight));

            // ビューポート上でのマウスドラッグ操作によるカメラ回転・ズーム・パン
            HandleCameraInput(fitWidth, fitHeight);

            // グリッドと原点ギズモのオーバーレイ描画
            if (showGrid_)
            {
                DrawGridOverlay(cursorPos, ImVec2(fitWidth, fitHeight));
            }

            // --- ビューポート上部のクイック操作バー (視点切り替え & 操作ガイド) ---
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // ガイドパネル背景
            ImVec2 barPos = ImVec2(cursorPos.x + 10.0f, cursorPos.y + 10.0f);
            drawList->AddRectFilled(barPos, ImVec2(barPos.x + 530.0f, barPos.y + 30.0f),
                                    IM_COL32(16, 22, 34, 210), 6.0f);
            drawList->AddRect(barPos, ImVec2(barPos.x + 530.0f, barPos.y + 30.0f),
                              IM_COL32(50, 90, 150, 160), 6.0f);

            // ガイドテキスト (Blender仕様)
            drawList->AddText(ImVec2(barPos.x + 10.0f, barPos.y + 7.0f), IM_COL32(200, 225, 255, 230),
                              "[中ドラッグ] 視点回転  |  [Shift+中] 平行移動  |  [ホイール/Ctrl+中] ズーム  |  [テンキー/F] 視点");

            // 視点切り替えボタン群 (Blender風)
            ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 10.0f, cursorPos.y + 45.0f));
            if (ImGui::Button("原点フォーカス (Num . / F)")) FocusOrigin();
            ImGui::SameLine();
            if (ImGui::Button("正面 (Num 1)")) SetCameraPresetView(1);
            ImGui::SameLine();
            if (ImGui::Button("側面 (Num 3)")) SetCameraPresetView(3);
            ImGui::SameLine();
            if (ImGui::Button("真上 (Num 7)")) SetCameraPresetView(2);
            ImGui::SameLine();
            if (ImGui::Button("斜め (Iso)")) SetCameraPresetView(0);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
#endif
}

void EffectStudio::HandleCameraInput(float, float)
{
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    bool isHovered = ImGui::IsItemHovered();

    // -------------------------------------------------------------
    // Blender スタイル カメラ操作:
    // [中ドラッグ (MMB)]         : 視点回転 (オービット)
    // [Shift + 中ドラッグ]       : 平行移動 (パン)
    // [Ctrl + 中ドラッグ] / [ホイール] : スムーズズーム
    // [Alt + 左ドラッグ]         : 3ボタンマウス再現 (回転)
    // [Alt + Shift + 左ドラッグ] : 3ボタンマウス再現 (パン)
    // [Alt + Ctrl + 左ドラッグ]  : 3ボタンマウス再現 (ズーム)
    // [テンキー 1 / 3 / 7]       : 正面 / 側面 / 真上
    // [テンキー . / F]           : 原点フォーカス
    // -------------------------------------------------------------

    // 中クリック または Alt+左クリック (Emulate 3 Button Mouse)
    bool isMMBDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle) || (io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left));
    bool isMMBClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Middle) || (io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left));

    // 1. ズーム操作
    // (a) マウスホイール
    if (isHovered && io.MouseWheel != 0.0f)
    {
        float factor = std::pow(0.85f, io.MouseWheel);
        targetCameraDistance_ = std::clamp(targetCameraDistance_ * factor, 0.4f, 300.0f);
    }
    // (b) Ctrl + 中ドラッグ (Blender スムーズズーム)
    if (isMMBDown && io.KeyCtrl)
    {
        float zoomDelta = io.MouseDelta.y * 0.015f;
        targetCameraDistance_ *= (1.0f + zoomDelta);
        targetCameraDistance_ = std::clamp(targetCameraDistance_, 0.4f, 300.0f);
        return;
    }

    // 2. 平行移動 (パン) 操作: Shift + 中ドラッグ
    bool panStart = isHovered && isMMBClicked && io.KeyShift;
    if (panStart) isDraggingPan_ = true;
    if (!isMMBDown) isDraggingPan_ = false;

    if (isDraggingPan_ && io.KeyShift)
    {
        Matrix4x4 camWorld = camera_.GetWorldMatrix();
        Vector3 camRight = { camWorld.m[0][0], camWorld.m[0][1], camWorld.m[0][2] };
        Vector3 camUp = { camWorld.m[1][0], camWorld.m[1][1], camWorld.m[1][2] };

        float panSpeed = targetCameraDistance_ * 0.0018f;
        targetCameraTarget_.x -= (camRight.x * io.MouseDelta.x - camUp.x * io.MouseDelta.y) * panSpeed;
        targetCameraTarget_.y -= (camRight.y * io.MouseDelta.x + camUp.y * io.MouseDelta.y) * panSpeed;
        targetCameraTarget_.z -= (camRight.z * io.MouseDelta.x - camUp.z * io.MouseDelta.y) * panSpeed;
        return;
    }

    // 3. 視点回転 (オービット) 操作: 中ドラッグ (修飾キーなし)
    bool orbitStart = isHovered && isMMBClicked && !io.KeyShift && !io.KeyCtrl;
    if (orbitStart) isDraggingOrbit_ = true;
    if (!isMMBDown) isDraggingOrbit_ = false;

    if (isDraggingOrbit_ && !io.KeyShift && !io.KeyCtrl)
    {
        float rotSpeed = 0.007f;
        targetCameraYaw_ -= io.MouseDelta.x * rotSpeed;
        targetCameraPitch_ += io.MouseDelta.y * rotSpeed;

        // ピッチ角制限 (-88度 〜 88度)
        const float limit = 88.0f * (std::numbers::pi_v<float> / 180.0f);
        targetCameraPitch_ = std::clamp(targetCameraPitch_, -limit, limit);
    }

    // 4. Blender テンキーショートカット & 原点フォーカス
    if (isHovered || ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
    {
        // テンキー 1: 正面 (Front)
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad1) || (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_1)))
        {
            SetCameraPresetView(1);
        }
        // テンキー 3: 側面 (Right)
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad3) || (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_3)))
        {
            SetCameraPresetView(3);
        }
        // テンキー 7: 真上 (Top)
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad7) || (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_7)))
        {
            SetCameraPresetView(2);
        }
        // テンキー . (Period) または Fキー / Homeキー: 原点フォーカス (Focus Selected / View All)
        if (ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal) || ImGui::IsKeyPressed(ImGuiKey_F) || ImGui::IsKeyPressed(ImGuiKey_Home))
        {
            FocusOrigin();
        }
    }
#endif
}

void EffectStudio::DrawGridOverlay(const ImVec2& imagePos, const ImVec2& imageSize)
{
#ifdef USE_IMGUI
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    Matrix4x4 vpMat = camera_.GetViewProjectionMatrix();

    // X-Z 平面グリッド描画
    int lines = static_cast<int>(gridSize_ / gridSpacing_);
    float halfSize = gridSize_ * 0.5f;

    uint32_t subGridColor = IM_COL32(50, 70, 95, 60);     // 補助線 (薄いサイバーグリッド)
    uint32_t mainGridColor = IM_COL32(85, 120, 175, 125); // 主線 (10mごと)
    uint32_t axisColorX = IM_COL32(245, 65, 65, 230);     // 赤: X軸
    uint32_t axisColorZ = IM_COL32(65, 130, 255, 230);    // 青: Z軸
    uint32_t axisColorY = IM_COL32(65, 245, 95, 230);     // 緑: Y軸

    for (int i = -lines / 2; i <= lines / 2; ++i)
    {
        float pos = i * gridSpacing_;
        bool isMajor = (static_cast<int>(std::round(std::abs(pos))) % 10 == 0);

        // Z軸平行ライン
        Vector3 startZ = { pos, 0.0f, -halfSize };
        Vector3 endZ = { pos, 0.0f, halfSize };
        uint32_t colZ = (i == 0) ? axisColorZ : (isMajor ? mainGridColor : subGridColor);
        float thickZ = (i == 0) ? 2.5f : (isMajor ? 1.5f : 1.0f);
        Draw3DLine(drawList, startZ, endZ, vpMat, imagePos, imageSize, colZ, thickZ);

        // X軸平行ライン
        Vector3 startX = { -halfSize, 0.0f, pos };
        Vector3 endX = { halfSize, 0.0f, pos };
        uint32_t colX = (i == 0) ? axisColorX : (isMajor ? mainGridColor : subGridColor);
        float thickX = (i == 0) ? 2.5f : (isMajor ? 1.5f : 1.0f);
        Draw3DLine(drawList, startX, endX, vpMat, imagePos, imageSize, colX, thickX);
    }

    // Y軸原点ギズモ (高さ3.5m)
    if (showAxisGizmo_)
    {
        Draw3DLine(drawList, { 0.0f, 0.0f, 0.0f }, { 0.0f, 3.5f, 0.0f }, vpMat, imagePos, imageSize, axisColorY, 3.0f);
    }
#endif
}

void EffectStudio::Draw3DLine(ImDrawList* drawList, const Vector3& start, const Vector3& end,
                              const Matrix4x4& vpMat, const ImVec2& minPos, const ImVec2& size,
                              uint32_t color, float thickness)
{
#ifdef USE_IMGUI
    auto project = [&](const Vector3& p, ImVec2& out) -> bool {
        Vector4 clip = {
            p.x * vpMat.m[0][0] + p.y * vpMat.m[1][0] + p.z * vpMat.m[2][0] + vpMat.m[3][0],
            p.x * vpMat.m[0][1] + p.y * vpMat.m[1][1] + p.z * vpMat.m[2][1] + vpMat.m[3][1],
            p.x * vpMat.m[0][2] + p.y * vpMat.m[1][2] + p.z * vpMat.m[2][2] + vpMat.m[3][2],
            p.x * vpMat.m[0][3] + p.y * vpMat.m[1][3] + p.z * vpMat.m[2][3] + vpMat.m[3][3],
        };

        if (clip.w <= 0.001f) return false;

        float ndcX = clip.x / clip.w;
        float ndcY = clip.y / clip.w;

        out.x = minPos.x + (ndcX * 0.5f + 0.5f) * size.x;
        out.y = minPos.y + (-ndcY * 0.5f + 0.5f) * size.y;
        return true;
    };

    ImVec2 p0, p1;
    if (project(start, p0) && project(end, p1))
    {
        drawList->AddLine(p0, p1, color, thickness);
    }
#endif
}

void EffectStudio::DrawControlWindow()
{
#ifdef USE_IMGUI
    if (!showEditor_) return;

    if (ImGui::Begin("エフェクトエディター", &showEditor_))
    {
        if (ImGui::BeginTabBar("EffectStudioTabs"))
        {
            if (ImGui::BeginTabItem("GPUパーティクル (Cyber)"))
            {
                isGPUMode_ = true;
                DrawGPUControlTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("CPUパーティクル (従来)"))
            {
                isGPUMode_ = false;

                // 従来のCPUエフェクトUI
                ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "■ CPUエフェクト プリセット (JSON)");
                ImGui::Separator();

                std::vector<const char*> filePtrs;
                for (const auto& file : jsonFiles_) filePtrs.push_back(file.c_str());

                if (ImGui::Combo("ファイル選択", &selectedFileIndex_, filePtrs.data(), static_cast<int>(filePtrs.size())))
                {
                    if (selectedFileIndex_ >= 0 && selectedFileIndex_ < static_cast<int>(jsonFiles_.size()))
                    {
                        currentLoadedFile_ = jsonFiles_[selectedFileIndex_];
                        previewEffect_.LoadFromJson(currentLoadedFile_);
                        previewEffect_.Play();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("更新")) RefreshFileList();

                ImGui::Spacing();
                if (ImGui::Button("Play", ImVec2(80, 24))) previewEffect_.Play();
                ImGui::SameLine();
                ImGui::Checkbox("Auto Loop", &autoLoop_);
                if (autoLoop_) ImGui::SliderFloat("Loop Interval", &loopInterval_, 0.2f, 5.0f, "%.2f s");

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
#endif
}

void EffectStudio::DrawGPUControlTab()
{
#ifdef USE_IMGUI
    // 1. JSON プリセット管理
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.9f, 1.0f), "■ GPUパーティクル プリセット (JSON)");
    ImGui::Separator();

    std::vector<const char*> gpuFilePtrs;
    for (const auto& f : gpuJsonFiles_) gpuFilePtrs.push_back(f.c_str());

    if (ImGui::Combo("プリセット選択", &selectedGpuFileIndex_, gpuFilePtrs.data(), static_cast<int>(gpuFilePtrs.size())))
    {
        if (selectedGpuFileIndex_ >= 0 && selectedGpuFileIndex_ < static_cast<int>(gpuJsonFiles_.size()))
        {
            LoadGPUJson(gpuJsonFiles_[selectedGpuFileIndex_]);
            gpuParticleSystem_.Reset();
            gpuParticleSystem_.Emit(gpuEmitterData_);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("更新##GPU")) RefreshGPUFileList();

    ImGui::InputText("保存ファイル名", saveGpuFileName_, sizeof(saveGpuFileName_));
    ImGui::SameLine();
    if (ImGui::Button("JSON保存"))
    {
        SaveGPUJson(saveGpuFileName_);
    }

    if (!statusMessage_.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", statusMessage_.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();

    // 2. プレビュー再生 & カメラ制御
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.9f, 1.0f), "■ プレビュー制御 & カメラ");
    if (ImGui::Button("射出 (Emit Now)", ImVec2(120, 26)))
    {
        gpuParticleSystem_.Emit(gpuEmitterData_);
    }
    ImGui::SameLine();
    if (ImGui::Button("リセット (Clear)", ImVec2(100, 26)))
    {
        gpuParticleSystem_.Reset();
    }
    ImGui::SameLine();
    if (ImGui::Button("原点フォーカス (F)", ImVec2(120, 26)))
    {
        FocusOrigin();
    }

    ImGui::Checkbox("自動連続射出 (Loop)", &gpuAutoEmit_);
    if (gpuAutoEmit_)
    {
        ImGui::SliderFloat("射出間隔 (Interval)", &gpuEmitInterval_, 0.05f, 2.0f, "%.2f 秒");
    }

    ImGui::SliderFloat("グリッド範囲", &gridSize_, 20.0f, 200.0f, "%.0f m");

    ImGui::Spacing();
    ImGui::Separator();

    // 3. パターン & Emitter 幾何学パラメータ
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.9f, 1.0f), "■ エミッター設定");

    const char* patternNames[] = {
        "0: 球状散乱 (Explosion / Burst)",
        "1: ボクセル崩壊 (Cyber Voxel Break)",
        "2: 直線ストリーム (Data Stream / Warp)",
        "3: デジタルスパーク (Glitch Spark)"
    };
    int currentPattern = static_cast<int>(gpuEmitterData_.patternType);
    if (ImGui::Combo("射出パターン", &currentPattern, patternNames, IM_ARRAYSIZE(patternNames)))
    {
        gpuEmitterData_.patternType = static_cast<uint32_t>(currentPattern);
    }

    int countInt = static_cast<int>(gpuEmitterData_.count);
    if (ImGui::SliderInt("1回の射出粒子数", &countInt, 1, 1024))
    {
        gpuEmitterData_.count = static_cast<uint32_t>(countInt);
    }

    ImGui::SliderFloat("発生半径 (Radius)", &gpuEmitterData_.radius, 0.0f, 10.0f, "%.2f");
    ImGui::SliderFloat("射出速度 (Speed)", &gpuEmitterData_.speed, 0.0f, 50.0f, "%.1f");
    ImGui::SliderFloat("拡散度 (Spread)", &gpuEmitterData_.spread, 0.0f, 10.0f, "%.2f");
    ImGui::SliderFloat("重力加速度 (Gravity)", &gpuEmitterData_.gravity, -20.0f, 20.0f, "%.1f");
    ImGui::SliderFloat("寿命 (LifeTime)", &gpuEmitterData_.lifeTime, 0.1f, 5.0f, "%.2f 秒");

    ImGui::Spacing();
    ImGui::Separator();

    // 4. 外観・カラー・ブレンドモード
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.9f, 1.0f), "■ ビジュアル設定 (サイバー発光)");

    ImGui::SliderFloat("開始スケール", &gpuEmitterData_.startScale, 0.01f, 2.0f, "%.2f");
    ImGui::SliderFloat("終了スケール", &gpuEmitterData_.endScale, 0.0f, 2.0f, "%.2f");

    float startCol[4] = { gpuEmitterData_.startColor.x, gpuEmitterData_.startColor.y, gpuEmitterData_.startColor.z, gpuEmitterData_.startColor.w };
    if (ImGui::ColorEdit4("開始カラー (Start Color)", startCol))
    {
        gpuEmitterData_.startColor = { startCol[0], startCol[1], startCol[2], startCol[3] };
    }

    float endCol[4] = { gpuEmitterData_.endColor.x, gpuEmitterData_.endColor.y, gpuEmitterData_.endColor.z, gpuEmitterData_.endColor.w };
    if (ImGui::ColorEdit4("終了カラー (End Color)", endCol))
    {
        gpuEmitterData_.endColor = { endCol[0], endCol[1], endCol[2], endCol[3] };
    }

    const char* blendNames[] = { "Alpha (通常半透明)", "Add (加算合成 / ネオン発光推奨)" };
    int currentBlend = static_cast<int>(gpuBlendMode_);
    if (ImGui::Combo("ブレンドモード", &currentBlend, blendNames, IM_ARRAYSIZE(blendNames)))
    {
        gpuBlendMode_ = static_cast<GPUParticleBlendMode>(currentBlend);
    }
#endif
}