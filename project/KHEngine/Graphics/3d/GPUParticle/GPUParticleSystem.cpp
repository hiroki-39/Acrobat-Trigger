#include "GPUParticleSystem.h"
#include <cassert>
#include <random>

namespace {
    // 256バイトアライメント計算
    size_t Align256(size_t size) {
        return (size + 255) & ~255;
    }

    // DEFAULTヒープバッファ生成ヘルパー (UAVフラグ付き)
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateUAVBuffer(ID3D12Device* device, size_t sizeInBytes) {
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = sizeInBytes;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        Microsoft::WRL::ComPtr<ID3D12Resource> res;
        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&res)
        );
        assert(SUCCEEDED(hr));
        return res;
    }
}

GPUParticleSystem::~GPUParticleSystem() {
    if (emitterConstantBuffer_) emitterConstantBuffer_->Unmap(0, nullptr);
    if (perFrameConstantBuffer_) perFrameConstantBuffer_->Unmap(0, nullptr);
    if (perViewConstantBuffer_) perViewConstantBuffer_->Unmap(0, nullptr);

    if (srvManager_) {
        srvManager_->Free(particleUAVIndex_);
        srvManager_->Free(freeListIndexUAVIndex_);
        srvManager_->Free(freeListUAVIndex_);
        srvManager_->Free(particleSRVIndex_);
    }
}

void GPUParticleSystem::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    assert(dxCommon && srvManager);
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    CreateBuffers();
    CreateComputePipelines();
    CreateGraphicsPipelines();

    // 初回初期化CSディスパッチ
    DispatchInitialize();
}

void GPUParticleSystem::CreateBuffers() {
    ID3D12Device* device = dxCommon_->GetDevice();

    // 1. DEFAULTヒープバッファの生成 (UAV)
    particleBuffer_ = CreateUAVBuffer(device, sizeof(GPUParticleData) * kMaxParticles);
    freeListIndexBuffer_ = CreateUAVBuffer(device, sizeof(int32_t));
    freeListBuffer_ = CreateUAVBuffer(device, sizeof(uint32_t) * kMaxParticles);

    // 2. ディスクリプタの確保とUAV/SRV生成
    particleUAVIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(particleUAVIndex_, particleBuffer_.Get(), kMaxParticles, sizeof(GPUParticleData));

    freeListIndexUAVIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(freeListIndexUAVIndex_, freeListIndexBuffer_.Get(), 1, sizeof(int32_t));

    freeListUAVIndex_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(freeListUAVIndex_, freeListBuffer_.Get(), kMaxParticles, sizeof(uint32_t));

    // 描画用SRVの生成
    particleSRVIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(particleSRVIndex_, particleBuffer_.Get(), kMaxParticles, sizeof(GPUParticleData));

    // 3. 定数バッファの生成 (UPLOADヒープ)
    emitterConstantBuffer_ = dxCommon_->CreateBufferResource(Align256(sizeof(GPUEmitterData)));
    emitterConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedEmitter_));
    *mappedEmitter_ = GPUEmitterData{};

    perFrameConstantBuffer_ = dxCommon_->CreateBufferResource(Align256(sizeof(PerFrameCS)));
    perFrameConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPerFrame_));
    *mappedPerFrame_ = PerFrameCS{};

    perViewConstantBuffer_ = dxCommon_->CreateBufferResource(Align256(sizeof(PerViewGPUParticle)));
    perViewConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPerView_));
    *mappedPerView_ = PerViewGPUParticle{};
}

void GPUParticleSystem::CreateComputePipelines() {
    ID3D12Device* device = dxCommon_->GetDevice();

    // 1. Compute RootSignature の作成
    // Param 0: Emitter CBV (b0)
    // Param 1: PerFrame CBV (b1)
    // Param 2: UAV gParticles (u0)
    // Param 3: UAV gFreeListIndex (u1)
    // Param 4: UAV gFreeList (u2)
    D3D12_ROOT_PARAMETER rootParams[5]{};

    // Param 0: CBV b0
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace = 0;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Param 1: CBV b1
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[1].Descriptor.ShaderRegister = 1;
    rootParams[1].Descriptor.RegisterSpace = 0;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Param 2: UAV Table u0 (gParticles)
    D3D12_DESCRIPTOR_RANGE uavRange0{};
    uavRange0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange0.NumDescriptors = 1;
    uavRange0.BaseShaderRegister = 0;
    uavRange0.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[2].DescriptorTable.pDescriptorRanges = &uavRange0;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Param 3: UAV Table u1 (gFreeListIndex)
    D3D12_DESCRIPTOR_RANGE uavRange1{};
    uavRange1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange1.NumDescriptors = 1;
    uavRange1.BaseShaderRegister = 1;
    uavRange1.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[3].DescriptorTable.pDescriptorRanges = &uavRange1;
    rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Param 4: UAV Table u2 (gFreeList)
    D3D12_DESCRIPTOR_RANGE uavRange2{};
    uavRange2.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange2.NumDescriptors = 1;
    uavRange2.BaseShaderRegister = 2;
    uavRange2.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[4].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[4].DescriptorTable.pDescriptorRanges = &uavRange2;
    rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters = _countof(rootParams);
    rsDesc.pParameters = rootParams;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    assert(SUCCEEDED(hr));
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature_));
    assert(SUCCEEDED(hr));

    // 2. 各ComputeShaderのコンパイルとPSO作成
    auto compileAndCreatePSO = [&](const std::wstring& path) -> Microsoft::WRL::ComPtr<ID3D12PipelineState> {
        auto blob = dxCommon_->compileshader(path, L"cs_6_0");
        assert(blob);

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = computeRootSignature_.Get();
        psoDesc.CS = { blob->GetBufferPointer(), blob->GetBufferSize() };

        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
        HRESULT psoHr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso));
        assert(SUCCEEDED(psoHr));
        return pso;
    };

    initComputePSO_ = compileAndCreatePSO(L"resources/shaders/GPUParticle/GPUParticleInitialize.CS.hlsl");
    emitComputePSO_ = compileAndCreatePSO(L"resources/shaders/GPUParticle/GPUParticleEmit.CS.hlsl");
    updateComputePSO_ = compileAndCreatePSO(L"resources/shaders/GPUParticle/GPUParticleUpdate.CS.hlsl");
}

void GPUParticleSystem::CreateGraphicsPipelines() {
    ID3D12Device* device = dxCommon_->GetDevice();

    // 1. Graphics RootSignature
    // Param 0: PerView CBV (b0, VS)
    // Param 1: StructuredBuffer<Particle> SRV Table (t0, VS)
    // Param 2: Texture2D SRV Table (t1, PS)
    D3D12_ROOT_PARAMETER rootParams[3]{};

    // Param 0: CBV b0
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace = 0;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // Param 1: SRV Table t0
    D3D12_DESCRIPTOR_RANGE srvRangeParticle{};
    srvRangeParticle.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRangeParticle.NumDescriptors = 1;
    srvRangeParticle.BaseShaderRegister = 0;
    srvRangeParticle.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &srvRangeParticle;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // Param 2: SRV Table t1 (Texture)
    D3D12_DESCRIPTOR_RANGE srvRangeTexture{};
    srvRangeTexture.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRangeTexture.NumDescriptors = 1;
    srvRangeTexture.BaseShaderRegister = 1;
    srvRangeTexture.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[2].DescriptorTable.pDescriptorRanges = &srvRangeTexture;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 静的サンプラー (s0, PS)
    D3D12_STATIC_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.ShaderRegister = 0;
    samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters = _countof(rootParams);
    rsDesc.pParameters = rootParams;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers = &samplerDesc;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    assert(SUCCEEDED(hr));
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&graphicsRootSignature_));
    assert(SUCCEEDED(hr));

    // 2. VS / PS コンパイル
    auto vsBlob = dxCommon_->compileshader(L"resources/shaders/GPUParticle/GPUParticle.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->compileshader(L"resources/shaders/GPUParticle/GPUParticle.PS.hlsl", L"ps_6_0");
    assert(vsBlob && psBlob);

    // 3. Graphics PSO 設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = graphicsRootSignature_.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    // 頂点バッファなし (SV_VertexIDで生成) のため入力レイアウトは空
    psoDesc.InputLayout = { nullptr, 0 };

    // ラスタライザ: カリングなし
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    // 深度設定: 深度テストON、深度書き込みOFF (パーティクル標準)
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // ブレンドステートの作成 (Add / Alpha)
    auto createPSOForBlend = [&](GPUParticleBlendMode mode) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = psoDesc;
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        if (mode == GPUParticleBlendMode::Add) {
            // 加算合成
            desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
            desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
            desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        } else {
            // 半透明
            desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
            desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        }

        hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&graphicsPSOs_[static_cast<size_t>(mode)]));
        assert(SUCCEEDED(hr));
    };

    createPSOForBlend(GPUParticleBlendMode::Alpha);
    createPSOForBlend(GPUParticleBlendMode::Add);

    // 4. Cube逕ｨ Graphics PSO 險ｭ螳・(36鬆らせ 遶区婿菴薙う繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ)
    auto cubeVsBlob = dxCommon_->compileshader(L"resources/shaders/GPUParticle/GPUParticleCube.VS.hlsl", L"vs_6_0");
    auto cubePsBlob = dxCommon_->compileshader(L"resources/shaders/GPUParticle/GPUParticleCube.PS.hlsl", L"ps_6_0");
    assert(cubeVsBlob && cubePsBlob);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC cubePsoDesc = psoDesc;
    cubePsoDesc.VS = { cubeVsBlob->GetBufferPointer(), cubeVsBlob->GetBufferSize() };
    cubePsoDesc.PS = { cubePsBlob->GetBufferPointer(), cubePsBlob->GetBufferSize() };

    auto createCubePSOForBlend = [&](GPUParticleBlendMode mode) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = cubePsoDesc;
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        if (mode == GPUParticleBlendMode::Add) {
            desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
            desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
            desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        } else {
            desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
            desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        }

        hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&cubeGraphicsPSOs_[static_cast<size_t>(mode)]));
        assert(SUCCEEDED(hr));
    };

    createCubePSOForBlend(GPUParticleBlendMode::Alpha);
    createCubePSOForBlend(GPUParticleBlendMode::Add);
}

void GPUParticleSystem::DispatchInitialize() {
    auto commandList = dxCommon_->GetCommandList();

    // SrvManagerのヒープをセット
    srvManager_->PreDraw();

    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(initComputePSO_.Get());

    // 定数バッファとUAVテーブルのセット
    commandList->SetComputeRootConstantBufferView(0, emitterConstantBuffer_->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(1, perFrameConstantBuffer_->GetGPUVirtualAddress());
    srvManager_->SetComputeRootDescriptorTable(2, particleUAVIndex_);
    srvManager_->SetComputeRootDescriptorTable(3, freeListIndexUAVIndex_);
    srvManager_->SetComputeRootDescriptorTable(4, freeListUAVIndex_);

    // Dispatch: 10240 / 1024 = 10 グループ
    commandList->Dispatch((kMaxParticles + 1023) / 1024, 1, 1);

    // UAVバリア
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = particleBuffer_.Get();
    commandList->ResourceBarrier(1, &barrier);
}

void GPUParticleSystem::Reset() {
    DispatchInitialize();
}

void GPUParticleSystem::Update(float deltaTime) {
    totalTime_ += deltaTime;
    frameCounter_++;

    mappedPerFrame_->deltaTime = deltaTime;
    mappedPerFrame_->time = totalTime_;
    mappedPerFrame_->randomSeed = frameCounter_;

    auto commandList = dxCommon_->GetCommandList();
    srvManager_->PreDraw();

    // 1. Update CS のディスパッチ
    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(updateComputePSO_.Get());

    commandList->SetComputeRootConstantBufferView(0, emitterConstantBuffer_->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(1, perFrameConstantBuffer_->GetGPUVirtualAddress());
    srvManager_->SetComputeRootDescriptorTable(2, particleUAVIndex_);
    srvManager_->SetComputeRootDescriptorTable(3, freeListIndexUAVIndex_);
    srvManager_->SetComputeRootDescriptorTable(4, freeListUAVIndex_);

    commandList->Dispatch((kMaxParticles + 1023) / 1024, 1, 1);

    // UAVバリア (更新完了保証)
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = particleBuffer_.Get();
    commandList->ResourceBarrier(1, &barrier);
}

void GPUParticleSystem::Emit(const GPUEmitterData& emitter) {
    if (emitter.count == 0) return;

    *mappedEmitter_ = emitter;
    mappedEmitter_->emit = 1;

    auto commandList = dxCommon_->GetCommandList();
    srvManager_->PreDraw();

    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(emitComputePSO_.Get());

    commandList->SetComputeRootConstantBufferView(0, emitterConstantBuffer_->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(1, perFrameConstantBuffer_->GetGPUVirtualAddress());
    srvManager_->SetComputeRootDescriptorTable(2, particleUAVIndex_);
    srvManager_->SetComputeRootDescriptorTable(3, freeListIndexUAVIndex_);
    srvManager_->SetComputeRootDescriptorTable(4, freeListUAVIndex_);

    // Dispatch (スレッド数256で割る)
    uint32_t groupCount = (emitter.count + 255) / 256;
    commandList->Dispatch(groupCount, 1, 1);

    // UAVバリア
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = particleBuffer_.Get();
    commandList->ResourceBarrier(1, &barrier);
}

void GPUParticleSystem::Draw(const Matrix4x4& viewProjectionMatrix, const Matrix4x4& billboardMatrix, uint32_t textureSrvIndex, GPUParticleBlendMode blendMode) {
    mappedPerView_->viewProjection = viewProjectionMatrix;
    mappedPerView_->billboardMatrix = billboardMatrix;

    auto commandList = dxCommon_->GetCommandList();
    srvManager_->PreDraw();

    commandList->SetGraphicsRootSignature(graphicsRootSignature_.Get());
    commandList->SetPipelineState(graphicsPSOs_[static_cast<size_t>(blendMode)].Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->SetGraphicsRootConstantBufferView(0, perViewConstantBuffer_->GetGPUVirtualAddress());
    srvManager_->SetGraphicsRootDescriptorTable(1, particleSRVIndex_);
    srvManager_->SetGraphicsRootDescriptorTable(2, textureSrvIndex);

    // 6頂点 (クアッド) × kMaxParticles インスタンス
    commandList->DrawInstanced(6, kMaxParticles, 0, 0);
}


void GPUParticleSystem::DrawCube(const Matrix4x4& viewProjectionMatrix, uint32_t textureSrvIndex, GPUParticleBlendMode blendMode) {
    mappedPerView_->viewProjection = viewProjectionMatrix;
    mappedPerView_->billboardMatrix = Matrix4x4::Identity();

    auto commandList = dxCommon_->GetCommandList();
    srvManager_->PreDraw();

    commandList->SetGraphicsRootSignature(graphicsRootSignature_.Get());
    commandList->SetPipelineState(cubeGraphicsPSOs_[static_cast<size_t>(blendMode)].Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->SetGraphicsRootConstantBufferView(0, perViewConstantBuffer_->GetGPUVirtualAddress());
    srvManager_->SetGraphicsRootDescriptorTable(1, particleSRVIndex_);
    srvManager_->SetGraphicsRootDescriptorTable(2, textureSrvIndex);

    // 36頂点 (立方体) × kMaxParticles インスタンス
    commandList->DrawInstanced(36, kMaxParticles, 0, 0);
}


void GPUParticleSystem::DrawCustomCubes(const Matrix4x4& viewProjectionMatrix, uint32_t particleSrvIndex, uint32_t particleCount, uint32_t textureSrvIndex, GPUParticleBlendMode blendMode) {
    if (particleCount == 0) return;

    mappedPerView_->viewProjection = viewProjectionMatrix;
    mappedPerView_->billboardMatrix = Matrix4x4::Identity();

    auto commandList = dxCommon_->GetCommandList();
    srvManager_->PreDraw();

    commandList->SetGraphicsRootSignature(graphicsRootSignature_.Get());
    commandList->SetPipelineState(cubeGraphicsPSOs_[static_cast<size_t>(blendMode)].Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->SetGraphicsRootConstantBufferView(0, perViewConstantBuffer_->GetGPUVirtualAddress());
    srvManager_->SetGraphicsRootDescriptorTable(1, particleSrvIndex);
    srvManager_->SetGraphicsRootDescriptorTable(2, textureSrvIndex);

    // 36頂点 (立方体) × particleCount インスタンス
    commandList->DrawInstanced(36, particleCount, 0, 0);
}
