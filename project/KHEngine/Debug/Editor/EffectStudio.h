#pragma once
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Graphics/Resource/Descriptor/SrvManager.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"
#include "KHEngine/Graphics/3d/Particle/ParticleEffect.h"
#include "KHEngine/Graphics/3d/GPUParticle/GPUParticleSystem.h"
#include <memory>
#include <string>
#include <vector>

/**
 * @brief ゲーム画面から独立したエフェクト制作専用のプレビュースタジオ
 * CPUパーティクルおよびGPUパーティクルのリアルタイム編集・JSON保存をサポートします。
 */
class EffectStudio
{
public:
    static EffectStudio* GetInstance();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update(float dt);
    void Render();

    // ImGui ウィンドウ描画
    void DrawViewportWindow(); // エフェクト画面（ビューポート）
    void DrawControlWindow();  // エフェクト設定・ファイル管理ウィンドウ

    // プレビュー対象のエフェクトを取得
    ParticleEffect* GetPreviewEffect() { return &previewEffect_; }
    GPUParticleSystem* GetGPUParticleSystem() { return &gpuParticleSystem_; }

    // ウィンドウ表示フラグ
    bool& GetShowViewport() { return showViewport_; }
    bool& GetShowEditor() { return showEditor_; }

    // ファイルスキャン
    void RefreshFileList();
    void RefreshGPUFileList();

    // GPUパーティクルJSON保存・読込
    void SaveGPUJson(const std::string& filename);
    void LoadGPUJson(const std::string& filename);

    // カメラプリセット視点
    void SetCameraPresetView(int preset); // 0: 斜め(Iso), 1: 正面(Front), 2: 真上(Top), 3: 側面(Side)
    void FocusOrigin();

private:
    EffectStudio() = default;
    ~EffectStudio() = default;
    EffectStudio(const EffectStudio&) = delete;
    EffectStudio& operator=(const EffectStudio&) = delete;

    void CreateRenderTargets();
    void HandleCameraInput(float contentWidth, float contentHeight);
    void DrawGPUControlTab();

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // オフスクリーン描画用リソース (1280 x 720)
    const uint32_t kWidth_ = 1280;
    const uint32_t kHeight_ = 720;

    Microsoft::WRL::ComPtr<ID3D12Resource> renderTextureResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
    uint32_t srvIndex_ = 0;
    D3D12_RESOURCE_STATES currentResourceState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};

    // プレビュー用カメラ
    Camera camera_;
    Vector3 cameraTarget_ = { 0.0f, 0.0f, 0.0f };
    float cameraDistance_ = 6.0f;
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 0.35f;

    // スムーズカメラ補間用の目標値
    Vector3 targetCameraTarget_ = { 0.0f, 0.0f, 0.0f };
    float targetCameraDistance_ = 6.0f;
    float targetCameraYaw_ = 0.0f;
    float targetCameraPitch_ = 0.35f;

    // ドラッグ継続用フラグ
    bool isDraggingOrbit_ = false;
    bool isDraggingPan_ = false;

    // プレビュー用エフェクト (CPU)
    ParticleEffect previewEffect_;

    // プレビュー用エフェクト (GPU)
    GPUParticleSystem gpuParticleSystem_;
    GPUEmitterData gpuEmitterData_;
    GPUParticleBlendMode gpuBlendMode_ = GPUParticleBlendMode::Add;
    bool isGPUMode_ = true; // デフォルトでGPUパーティクルモード
    bool gpuAutoEmit_ = true;
    float gpuEmitInterval_ = 0.5f;
    float gpuEmitTimer_ = 0.0f;

    // 再生コントロール
    bool autoLoop_ = true;
    float loopInterval_ = 1.5f;
    float loopTimer_ = 0.0f;
    float playbackSpeed_ = 1.0f;

    // 表示設定
    Vector4 backgroundColor_ = { 0.08f, 0.08f, 0.12f, 1.0f };
    bool showGrid_ = true;
    float gridSize_ = 80.0f;
    float gridSpacing_ = 2.0f;
    bool showAxisGizmo_ = true;
    bool showViewport_ = true;
    bool showEditor_ = true;

    // CPU JSON ファイル管理
    std::vector<std::string> jsonFiles_;
    int selectedFileIndex_ = -1;
    char saveAsFileName_[64] = "new_effect.json";
    std::string currentLoadedFile_ = "default_effect.json";

    // GPU JSON ファイル管理
    std::vector<std::string> gpuJsonFiles_;
    int selectedGpuFileIndex_ = -1;
    char saveGpuFileName_[64] = "cyber_explosion.json";
    std::string currentLoadedGpuFile_ = "cyber_voxel_explosion.json";

    std::string statusMessage_ = "";

    // グリッド描画ヘルパー
    void DrawGridOverlay(const struct ImVec2& imagePos, const struct ImVec2& imageSize);
    void Draw3DLine(struct ImDrawList* drawList, const Vector3& start, const Vector3& end,
                    const Matrix4x4& vpMat, const struct ImVec2& minPos, const struct ImVec2& size,
                    uint32_t color, float thickness = 1.0f);
};