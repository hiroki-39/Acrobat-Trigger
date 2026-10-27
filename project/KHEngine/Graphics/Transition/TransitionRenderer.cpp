#include "TransitionRenderer.h"
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Graphics/Resource/Descriptor/SrvManager.h"
#ifdef ENABLE_EDITOR
#include "KHEngine/Debug/Editor/EditorSystem.h"
#include "KHEngine/Core/Services/EngineServices.h"
#endif
#include <cassert>

void TransitionRenderer::Initialize(DirectXCommon* dxCommon)
{
	assert(dxCommon);
	dxCommon_ = dxCommon;

	// 定数バッファ作成 (256バイトアライメント)
	uint32_t size = (sizeof(ConstBufferData) + 0xff) & ~0xff;
	constBuffer_ = dxCommon_->CreateBufferResource(size);
	constBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constMap_));

	CreateRootSignature();
	CreateGraphicsPipeline();
}

void TransitionRenderer::CreateRootSignature()
{
	HRESULT hr;

	// ディスクリプタレンジ (t0: ルールテクスチャ)
	D3D12_DESCRIPTOR_RANGE descriptorRange[1]{};
	descriptorRange[0].BaseShaderRegister = 0; // t0
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータ
	D3D12_ROOT_PARAMETER rootParameters[2]{};

	// 0: SRV テーブル (t0)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	// 1: CBV (b0: TransitionBuffer)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0; // b0

	// 静的サンプラー (Linear, Clamp)
	D3D12_STATIC_SAMPLER_DESC staticSampler{};
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
	staticSampler.ShaderRegister = 0; // s0
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSigDesc.pParameters = rootParameters;
	rootSigDesc.NumParameters = _countof(rootParameters);
	rootSigDesc.pStaticSamplers = &staticSampler;
	rootSigDesc.NumStaticSamplers = 1;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

	hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void TransitionRenderer::CreateGraphicsPipeline()
{
	HRESULT hr;

	// 入力レイアウト (SV_VertexIDを使用するため不要)
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = nullptr;
	inputLayoutDesc.NumElements = 0;

	// ブレンドステート (標準アルファブレンド)
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN | D3D12_COLOR_WRITE_ENABLE_BLUE;

	// ラスタライザステート (両面描画)
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// シェーダーのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = dxCommon_->compileshader(L"resources/shaders/Transition.VS.hlsl", L"vs_6_0");
	assert(vsBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> psBlob = dxCommon_->compileshader(L"resources/shaders/Transition.PS.hlsl", L"ps_6_0");
	assert(psBlob != nullptr);

	// 深度ステンシルステート (無効)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = FALSE;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}

void TransitionRenderer::Draw(uint32_t ruleTextureIndex, float progress, bool isOpening,
	TransitionType type,
	const Vector4& fadeColor, float edgeSoftness, const Vector4& edgeColor,
	const Vector2& originCoord, float hexTileScale, float borderWidth, float glitchIntensity)
{
	if (!constMap_) return;

	// 定数バッファの更新
	constMap_->fadeColor = fadeColor;
	constMap_->progress = progress;
	constMap_->edgeSoftness = edgeSoftness;
	constMap_->isOpening = isOpening ? 1 : 0;
	constMap_->transitionType = static_cast<int32_t>(type);
	constMap_->edgeColor = edgeColor;
	constMap_->originCoord = originCoord;
	constMap_->hexTileScale = hexTileScale;
	constMap_->borderWidth = borderWidth;
	constMap_->glitchIntensity = glitchIntensity;
	constMap_->padding0 = 0.0f;
	constMap_->padding1 = 0.0f;
	constMap_->padding2 = 0.0f;

	auto commandList = dxCommon_->GetCommandList();

	uint32_t bbWidth = dxCommon_->GetBackBufferWidth();
	uint32_t bbHeight = dxCommon_->GetBackBufferHeight();

	float vpX = 0.0f;
	float vpY = 0.0f;
	float vpW = static_cast<float>(bbWidth);
	float vpH = static_cast<float>(bbHeight);

#ifdef ENABLE_EDITOR
	auto services = EngineServices::GetInstance();
	if (services && services->GetEditorMode())
	{
		auto editor = EditorSystem::GetInstance();
		if (editor)
		{
			ImVec2 vPos = editor->GetViewportPos();
			ImVec2 vSize = editor->GetViewportSize();
			if (vSize.x > 10.0f && vSize.y > 10.0f)
			{
				vpX = vPos.x;
				vpY = vPos.y;
				vpW = vSize.x;
				vpH = vSize.y;
			}
		}
	}
#endif

	D3D12_VIEWPORT vp{ vpX, vpY, vpW, vpH, 0.0f, 1.0f };
	D3D12_RECT scissor{
		static_cast<LONG>(vpX),
		static_cast<LONG>(vpY),
		static_cast<LONG>(vpX + vpW),
		static_cast<LONG>(vpY + vpH)
	};
	commandList->RSSetViewports(1, &vp);
	commandList->RSSetScissorRects(1, &scissor);

	// パイプライン設定
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// SRV (ルールテクスチャ)
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, ruleTextureIndex);

	// CBV (定数バッファ)
	commandList->SetGraphicsRootConstantBufferView(1, constBuffer_->GetGPUVirtualAddress());

	// フルスクリーントライアングル描画 (頂点バッファ不要、3頂点)
	commandList->DrawInstanced(3, 1, 0, 0);
}
