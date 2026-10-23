#include "ParticleRenderer.h"
#include <cassert>
#include <d3d12.h>
#include <dxgidebug.h>
#include <vector>
#include <cstring>
#include <utility>

using Microsoft::WRL::ComPtr;

static D3D12_BLEND_DESC MakeBlendDescForMode(BlendMode mode)
{
	D3D12_BLEND_DESC desc{};
	desc.AlphaToCoverageEnable = FALSE;
	desc.IndependentBlendEnable = FALSE;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	switch (mode)
	{
	case BlendMode::Alpha: // Alpha
		desc.RenderTarget[0].BlendEnable = TRUE;
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	case BlendMode::Additive: // Additive
		desc.RenderTarget[0].BlendEnable = TRUE;
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	case BlendMode::Multiply: // Multiply
		desc.RenderTarget[0].BlendEnable = TRUE;
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_DEST_COLOR;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_DEST_ALPHA;
		desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	case BlendMode::PreMultiplied: // PreMultiplied
		desc.RenderTarget[0].BlendEnable = TRUE;
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	case BlendMode::None: // None
	default:
		desc.RenderTarget[0].BlendEnable = FALSE;
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		break;
	}
	return desc;
}

ParticleRenderer::~ParticleRenderer()
{
	Finalize();
}

void ParticleRenderer::Finalize()
{
	if (srvManager_ && instancingSrvIndex_ != UINT32_MAX)
	{
		srvManager_->Free(instancingSrvIndex_);
		instancingSrvIndex_ = UINT32_MAX;
	}
	instancingResource_.Reset();
	instancingData_ = nullptr;
	vertexResource_.Reset();
	vertexBufferView_ = {};
	vertexCount_ = 0;
	vertexStride_ = 0;
	materialResource_.Reset();
	materialData_ = nullptr;
	materialCBVAddress_ = 0;
	rootSignature_.Reset();
	psoForBlendMode_.clear();
	dxCommon_ = nullptr;
	srvManager_ = nullptr;
	maxInstances_ = 0;
}

ParticleRenderer::ParticleRenderer(ParticleRenderer&& other) noexcept
{
	*this = std::move(other);
}

ParticleRenderer& ParticleRenderer::operator=(ParticleRenderer&& other) noexcept
{
	if (this != &other)
	{
		Finalize();
		dxCommon_ = other.dxCommon_;
		srvManager_ = other.srvManager_;
		rootSignature_ = std::move(other.rootSignature_);
		psoForBlendMode_ = std::move(other.psoForBlendMode_);
		instancingSrvIndex_ = other.instancingSrvIndex_;
		instancingResource_ = std::move(other.instancingResource_);
		instancingData_ = other.instancingData_;
		vertexResource_ = std::move(other.vertexResource_);
		vertexBufferView_ = other.vertexBufferView_;
		vertexCount_ = other.vertexCount_;
		vertexStride_ = other.vertexStride_;
		materialResource_ = std::move(other.materialResource_);
		materialData_ = other.materialData_;
		materialCBVAddress_ = other.materialCBVAddress_;
		maxInstances_ = other.maxInstances_;

		other.dxCommon_ = nullptr;
		other.srvManager_ = nullptr;
		other.instancingSrvIndex_ = UINT32_MAX;
		other.instancingData_ = nullptr;
		other.materialData_ = nullptr;
		other.materialCBVAddress_ = 0;
		other.maxInstances_ = 0;
	}
	return *this;
}

void ParticleRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t maxInstances)
{
	Finalize();
	assert(dxCommon != nullptr);
	assert(srvManager != nullptr);
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	maxInstances_ = maxInstances;

	// Descriptor ranges
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1]{};
	descriptorRangeForInstancing[0].BaseShaderRegister = 0;
	descriptorRangeForInstancing[0].NumDescriptors = 1;
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeTexture[1]{};
	descriptorRangeTexture[0].BaseShaderRegister = 1;
	descriptorRangeTexture[0].NumDescriptors = 1;
	descriptorRangeTexture[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeTexture[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Root parameters
	D3D12_ROOT_PARAMETER rootPrametersForInstancing[4] = {};
	rootPrametersForInstancing[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootPrametersForInstancing[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootPrametersForInstancing[0].Descriptor.ShaderRegister = 0;

	rootPrametersForInstancing[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootPrametersForInstancing[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootPrametersForInstancing[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
	rootPrametersForInstancing[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

	rootPrametersForInstancing[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootPrametersForInstancing[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootPrametersForInstancing[2].DescriptorTable.pDescriptorRanges = descriptorRangeTexture;
	rootPrametersForInstancing[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeTexture);

	rootPrametersForInstancing[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootPrametersForInstancing[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootPrametersForInstancing[3].Descriptor.ShaderRegister = 1;

	D3D12_ROOT_SIGNATURE_DESC descripitionRootSignatureForInstancing{};
	descripitionRootSignatureForInstancing.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descripitionRootSignatureForInstancing.pParameters = rootPrametersForInstancing;
	descripitionRootSignatureForInstancing.NumParameters = _countof(rootPrametersForInstancing);

	// Static sampler
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1]{};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descripitionRootSignatureForInstancing.pStaticSamplers = staticSamplers;
	descripitionRootSignatureForInstancing.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズ & 作成
	ComPtr<ID3DBlob> signatureBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descripitionRootSignatureForInstancing,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		assert(false);
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	// Input layout (caller must match vertex structure used)
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// Rasterizer / DepthStencil
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// シェーダーコンパイル（main と同じパス）
	ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->compileshader(L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->compileshader(L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// 共通 PSO 設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC commonPsoDesc{};
	commonPsoDesc.pRootSignature = rootSignature_.Get();
	commonPsoDesc.InputLayout = inputLayoutDesc;
	commonPsoDesc.RasterizerState = rasterizerDesc;
	commonPsoDesc.DepthStencilState = depthStencilDesc;
	commonPsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	commonPsoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	commonPsoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	commonPsoDesc.NumRenderTargets = 1;
	commonPsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	commonPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	commonPsoDesc.SampleDesc.Count = 1;
	commonPsoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// PSO をブレンドモードごとに生成
	const int kBlendCount = static_cast<int>(BlendMode::Count);
	psoForBlendMode_.resize(kBlendCount);
	for (int i = 0; i < kBlendCount; ++i)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = commonPsoDesc;
		psoDesc.BlendState = MakeBlendDescForMode(static_cast<BlendMode>(i));

		ComPtr<ID3D12PipelineState> pso = nullptr;
		hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
		assert(SUCCEEDED(hr));
		psoForBlendMode_[i] = pso;
	}

	// Instancing 用バッファ作成
	instancingResource_ = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * maxInstances_);
	instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));
	for (uint32_t index = 0; index < maxInstances_; index++)
	{
		instancingData_[index].WVP = Matrix4x4::Identity();
		instancingData_[index].World = Matrix4x4::Identity();
		instancingData_[index].color = { 1.0f,1.0f,1.0f,1.0f };
	}

	// SRV を割り当てて作成
	instancingSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforStructuredBuffer(instancingSrvIndex_, instancingResource_.Get(), maxInstances_, sizeof(ParticleForGPU));
}

void ParticleRenderer::CreateVertexBuffer(const void* vertexData, uint32_t vertexCount, uint32_t vertexStride)
{
	if (!dxCommon_ || !vertexData || vertexCount == 0 || vertexStride == 0) return;
	vertexCount_ = vertexCount;
	vertexStride_ = vertexStride;

	const size_t bufferSize = static_cast<size_t>(vertexCount_) * vertexStride_;
	vertexResource_ = dxCommon_->CreateBufferResource(bufferSize);
	if (!vertexResource_) return;

	void* mapped = nullptr;
	HRESULT hr = vertexResource_->Map(0, nullptr, &mapped);
	if (SUCCEEDED(hr) && mapped)
	{
		std::memcpy(mapped, vertexData, bufferSize);
	}

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = static_cast<UINT>(bufferSize);
	vertexBufferView_.StrideInBytes = static_cast<UINT>(vertexStride);
}

void ParticleRenderer::CreateMaterialBuffer(size_t sizeInBytes, const void* initData)
{
	if (!dxCommon_ || !initData || sizeInBytes == 0) return;
	materialResource_ = dxCommon_->CreateBufferResource(sizeInBytes);
	if (!materialResource_) return;

	HRESULT hr = materialResource_->Map(0, nullptr, &materialData_);
	if (SUCCEEDED(hr) && materialData_)
	{
		std::memcpy(materialData_, initData, sizeInBytes);
	}
	// GPU 側アドレスを保存（SetGraphicsRootConstantBufferView に渡す）
	materialCBVAddress_ = materialResource_->GetGPUVirtualAddress();
}

void ParticleRenderer::Draw(uint32_t numInstances, uint32_t textureSrvIndex, BlendMode blendMode)
{
	if (numInstances == 0) return;
	auto commandList = dxCommon_->GetCommandList();

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(GetPipelineStateForBlend(blendMode).Get());

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 定数バッファ (マテリアル)
	commandList->SetGraphicsRootConstantBufferView(0, materialCBVAddress_);

	// インスタンシング用の SRV (Structured Buffer) をセット
	srvManager_->SetGraphicsRootDescriptorTable(1, instancingSrvIndex_);

	// テクスチャの SRV をセット
	srvManager_->SetGraphicsRootDescriptorTable(2, textureSrvIndex);

	// 描画コール (四角形なので 6 頂点 × インスタンス数)
	commandList->DrawInstanced(vertexCount_, numInstances, 0, 0);
}
