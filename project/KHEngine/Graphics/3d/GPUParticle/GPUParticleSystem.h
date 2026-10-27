#pragma once
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Graphics/Resource/Descriptor/SrvManager.h"
#include "KHEngine/Math/Vector3.h"
#include "KHEngine/Math/Vector4.h"
#include "KHEngine/Math/Matrix4x4.h"
#include <wrl/client.h>
#include <cstdint>
#include <string>

// ブレンドモード
enum class GPUParticleBlendMode {
    Alpha,    // 半透明
    Add,      // 加算合成 (ネオンサイバー発光推奨)
    Count
};

// パーティクル構造体 (64バイト)
struct GPUParticleData {
    Vector3 translate;
    float pad0 = 0.0f;
    Vector3 scale;
    float lifeTime = 0.0f;
    Vector3 velocity;
    float currentTime = 0.0f;
    Vector4 color;
};

// エミッターパラメータ (CPU -> GPU)
struct GPUEmitterData {
    Vector3 translate = { 0.0f, 0.0f, 0.0f };
    float radius = 0.5f;

    Vector3 baseVelocity = { 0.0f, 0.0f, 0.0f };
    float speed = 5.0f;

    Vector4 startColor = { 0.0f, 1.0f, 1.0f, 1.0f }; // シアン
    Vector4 endColor = { 1.0f, 0.0f, 1.0f, 0.0f };   // マゼンタ

    float startScale = 0.2f;
    float endScale = 0.0f;
    float lifeTime = 1.0f;
    float spread = 1.0f;

    float gravity = 0.0f;
    uint32_t count = 32;       // 1回あたりの射出数
    uint32_t emit = 0;         // 1: 射出, 0: 待機
    uint32_t patternType = 0;  // 0: 球状散乱, 1: ボクセル崩壊, 2: 直線ストリーム
};

// フレーム定数バッファ
struct PerFrameCS {
    float deltaTime = 0.016f;
    float time = 0.0f;
    uint32_t randomSeed = 0;
    float pad = 0.0f;
};

// 描画用ビュー定数バッファ
struct PerViewGPUParticle {
    Matrix4x4 viewProjection;
    Matrix4x4 billboardMatrix;
};

/**
 * @brief GPUパーティクル制御クラス
 * ComputeShader (Initialize, Emit, Update) と Instancing描画パイプラインを統合管理します。
 */
class GPUParticleSystem {
public:
    static const uint32_t kMaxParticles = 10240;

    GPUParticleSystem() = default;
    ~GPUParticleSystem();

    /// <summary>
    /// 初期化（バッファ確保、PSO構築、初期化CS実行）
    /// </summary>
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    /// <summary>
    /// 毎フレームの更新（Update CS のディスパッチ）
    /// </summary>
    void Update(float deltaTime);

    /// <summary>
    /// パーティクルの射出要求（Emit CS のディスパッチ）
    /// </summary>
    void Emit(const GPUEmitterData& emitter);

    /// <summary>
    /// 描画（Vertex-less Instancing 描画）
    /// </summary>
    void Draw(const Matrix4x4& viewProjectionMatrix, const Matrix4x4& billboardMatrix, uint32_t textureSrvIndex, GPUParticleBlendMode blendMode = GPUParticleBlendMode::Add);

    /// <summary>
    /// 3Dボクセルキューブとして描画（Vertex-less Instancing 36頂点）
    /// </summary>
    void DrawCube(const Matrix4x4& viewProjectionMatrix, uint32_t textureSrvIndex, GPUParticleBlendMode blendMode = GPUParticleBlendMode::Alpha);

    /// <summary>
    /// 外部のパーティクルバッファを使用して3Dボクセルキューブを描画（DrawCall 1回）
    /// </summary>
    void DrawCustomCubes(const Matrix4x4& viewProjectionMatrix, uint32_t particleSrvIndex, uint32_t particleCount, uint32_t textureSrvIndex, GPUParticleBlendMode blendMode = GPUParticleBlendMode::Alpha);



    /// <summary>
    /// パーティクルとFreeListを初期状態にリセット
    /// </summary>
    void Reset();

    /// <summary>
    /// パーティクルSRVインデックス取得
    /// </summary>
    uint32_t GetParticleSRVIndex() const { return particleSRVIndex_; }

private:
    void CreateBuffers();
    void CreateComputePipelines();
    void CreateGraphicsPipelines();
    void DispatchInitialize();

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // GPU バッファ (DEFAULT Heap)
    Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListBuffer_;

    // 定数バッファ (UPLOAD Heap)
    Microsoft::WRL::ComPtr<ID3D12Resource> emitterConstantBuffer_;
    GPUEmitterData* mappedEmitter_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> perFrameConstantBuffer_;
    PerFrameCS* mappedPerFrame_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> perViewConstantBuffer_;
    PerViewGPUParticle* mappedPerView_ = nullptr;

    // デスクリプタインデックス (SrvManagerで管理)
    uint32_t particleUAVIndex_ = 0;
    uint32_t freeListIndexUAVIndex_ = 0;
    uint32_t freeListUAVIndex_ = 0;
    uint32_t particleSRVIndex_ = 0;

    // Compute パイプライン
    Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> initComputePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> emitComputePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> updateComputePSO_;

    // Graphics パイプライン
    Microsoft::WRL::ComPtr<ID3D12RootSignature> graphicsRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPSOs_[static_cast<size_t>(GPUParticleBlendMode::Count)];
    Microsoft::WRL::ComPtr<ID3D12PipelineState> cubeGraphicsPSOs_[static_cast<size_t>(GPUParticleBlendMode::Count)];

    float totalTime_ = 0.0f;
    uint32_t frameCounter_ = 0;
};
