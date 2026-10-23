#include "PostProcess.h"
#include "KHEngine/Graphics/Resource/Descriptor/SrvManager.h"
#include "KHEngine/Graphics/PostProcess/Effects/GrayscaleEffect.h"
#include "KHEngine/Graphics/PostProcess/Effects/SepiaEffect.h"
#include "KHEngine/Graphics/PostProcess/Effects/VignettingEffect.h"
#include "KHEngine/Graphics/PostProcess/Effects/SmoothingEffect.h"
#include "KHEngine/Graphics/PostProcess/Effects/GaussianFilterEffect.h"
#include "KHEngine/Graphics/PostProcess/Effects/OutlineEffect.h"
#include "KHEngine/Graphics/PostProcess/Effects/RadialBlurEffect.h"
#include "KHEngine/Graphics/PostProcess/Effects/DissolveEffect.h"
#include "KHEngine/Graphics/PostProcess/Effects/RandomEffect.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "externals/imgui/imgui.h"
#include "externals/nlohmann/json.hpp"
#include <cassert>
#include <fstream>
#include <algorithm>

void PostProcess::Initialize(DirectXCommon* dxcommon)
{
	this->dxCommon_ = dxcommon;

	CreateRootSignature();
	CreateGraphicsPipeline();

	// ポストプロセス用のSRVをSrvManagerから安全に割り当て
	SrvManager* srvManager = SrvManager::GetInstance();
	srvIndex_ = srvManager->Allocate();
	resultSrvIndex_ = srvManager->Allocate();

	// RenderTextureのSRV作成
	srvManager->CreateSRVforTexture2D(srvIndex_, dxCommon_->GetRenderTextureResource().Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);
	// 書き込み先（結果）テクスチャのSRV作成
	srvManager->CreateSRVforTexture2D(resultSrvIndex_, dxCommon_->GetPostProcessTextureResource().Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);

	// 定数バッファの作成
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC cbDesc{};
	cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	cbDesc.Width = (sizeof(PostProcessData) + 0xff) & ~0xff; // 256バイトアライメント
	cbDesc.Height = 1;
	cbDesc.DepthOrArraySize = 1;
	cbDesc.MipLevels = 1;
	cbDesc.SampleDesc.Count = 1;
	cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&constBuffer_)
	);
	constBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constMap_));

	// エフェクトの登録
	effects_.push_back(std::make_unique<GrayscaleEffect>(&data_));
	effects_.push_back(std::make_unique<SepiaEffect>(&data_));
	effects_.push_back(std::make_unique<VignettingEffect>(&data_));
	effects_.push_back(std::make_unique<SmoothingEffect>(&data_));
	effects_.push_back(std::make_unique<GaussianFilterEffect>(&data_));
	effects_.push_back(std::make_unique<OutlineEffect>(&data_));
	effects_.push_back(std::make_unique<RadialBlurEffect>(&data_));
	effects_.push_back(std::make_unique<DissolveEffect>(&data_));
	effects_.push_back(std::make_unique<RandomEffect>(&data_));

	// ノイズテクスチャのロード (Dissolve用)
	TextureManager::GetInstance()->LoadTexture("resources/images/noise0.png");

	for (auto& effect : effects_) {
		effect->Initialize();
	}

	// 保存されている設定があればロード
	LoadFromJson();
}

void PostProcess::CreateRootSignature()
{
	HRESULT hr;

	D3D12_DESCRIPTOR_RANGE descriptorRange[2]{};
	descriptorRange[0].BaseShaderRegister = 0; // t0
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descriptorRange[1].BaseShaderRegister = 1; // t1
	descriptorRange[1].NumDescriptors = 1;
	descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	// t0 (メインテクスチャ)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange[0];
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	// b0 (定数バッファ: エフェクトのパラメータ)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0; // b0

	// t1 (マスクテクスチャ)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRange[1];
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1]{};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0; // s0
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));
}

void PostProcess::CreateGraphicsPipeline()
{
	HRESULT hr;

	// InputLayout (使わない)
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = nullptr;
	inputLayoutDesc.NumElements = 0;

	// BlendState
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerState
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaders
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->compileshader(L"resources/shaders/CopyImage.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->compileshader(L"resources/shaders/CopyImage.PS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// DepthStencilState (使わない)
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = false;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));
}

void PostProcess::Draw(bool drawToSwapchain)
{
	auto commandList = dxCommon_->GetCommandList();

	// 読み込み元(renderTexture)を RENDER_TARGET から PIXEL_SHADER_RESOURCE へ遷移
	D3D12_RESOURCE_BARRIER barriers[2]{};
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barriers[0].Transition.pResource = dxCommon_->GetRenderTextureResource().Get();
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	int barrierCount = 1;
	Microsoft::WRL::ComPtr<ID3D12Resource> targetResource;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;

	if (drawToSwapchain) {
		targetResource = dxCommon_->GetCurrentBackBufferResource();
		rtvHandle = dxCommon_->GetCurrentBackBufferRTVHandle();

		// スワップチェーン描画時: 画面全体（黒枠なし）にフルスクリーン拡大描画
		uint32_t bbWidth = dxCommon_->GetBackBufferWidth();
		uint32_t bbHeight = dxCommon_->GetBackBufferHeight();

		D3D12_VIEWPORT gameVp{};
		gameVp.Width = static_cast<float>(bbWidth);
		gameVp.Height = static_cast<float>(bbHeight);
		gameVp.TopLeftX = 0.0f;
		gameVp.TopLeftY = 0.0f;
		gameVp.MinDepth = 0.0f;
		gameVp.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &gameVp);

		D3D12_RECT gameScissor{ 0, 0, static_cast<LONG>(bbWidth), static_cast<LONG>(bbHeight) };
		commandList->RSSetScissorRects(1, &gameScissor);
	} else {
		targetResource = dxCommon_->GetPostProcessTextureResource();
		rtvHandle = dxCommon_->GetPostProcessRTVHandle();

		// オフスクリーン描画時: 1280x720 の全画面ビューポートを設定
		D3D12_VIEWPORT offscreenVp{};
		offscreenVp.Width = 1280.0f;
		offscreenVp.Height = 720.0f;
		offscreenVp.TopLeftX = 0.0f;
		offscreenVp.TopLeftY = 0.0f;
		offscreenVp.MinDepth = 0.0f;
		offscreenVp.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &offscreenVp);

		D3D12_RECT offscreenScissor{ 0, 0, 1280, 720 };
		commandList->RSSetScissorRects(1, &offscreenScissor);

		// 初回以外は PIXEL_SHADER_RESOURCE -> RENDER_TARGET へ遷移
		static bool isFirstDraw = true;
		if (!isFirstDraw) {
			barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barriers[1].Transition.pResource = targetResource.Get();
			barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; 
			barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrierCount = 2;
		}
		isFirstDraw = false;
	}

	commandList->ResourceBarrier(barrierCount, barriers);

	// 描画先のRTVを設定
	commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

	// クリア（スワップチェーンへの描画時はPreDrawSwapchainでクリア済みのため不要だが、念のため）
	if (!drawToSwapchain) {
		float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	}

	// 描画設定
	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->SetPipelineState(graphicsPipelineState.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ランダム(時間)の更新
	data_.randomTime += 1.0f / 60.0f;
	if (data_.randomTime > 1000.0f) data_.randomTime = 0.0f;

	// エフェクトの有効/無効フラグを定数バッファに反映
	data_.enableGrayscale = 0;
	data_.enableSepia = 0;
	data_.enableVignette = 0;
	data_.enableSmoothing = 0;
	data_.enableGaussian = 0;
	data_.enableOutline = 0;
	data_.enableRadialBlur = 0;
	data_.enableDissolve = 0;
	data_.enableRandom = 0;

	for (auto& effect : effects_) {
		if (effect->isActive_) {
			if (effect->name_ == "Grayscale") data_.enableGrayscale = 1;
			else if (effect->name_ == "Sepia") data_.enableSepia = 1;
			else if (effect->name_ == "Vignetting") data_.enableVignette = 1;
			else if (effect->name_ == "Smoothing") data_.enableSmoothing = 1;
			else if (effect->name_ == "GaussianFilter") data_.enableGaussian = 1;
			else if (effect->name_ == "Outline") data_.enableOutline = 1;
			else if (effect->name_ == "RadialBlur") data_.enableRadialBlur = 1;
			else if (effect->name_ == "Dissolve") data_.enableDissolve = 1;
			else if (effect->name_ == "Random(Glitch)") data_.enableRandom = 1;
		}
	}

	// CPUのデータをGPUへ転送
	if (constMap_) {
		*constMap_ = data_;
	}

	// SRVセット (t0)
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, srvIndex_);

	// CBVセット (b0)
	commandList->SetGraphicsRootConstantBufferView(1, constBuffer_->GetGPUVirtualAddress());

	// マスクテクスチャのSRVセット (t1)
	uint32_t maskSrvIndex = TextureManager::GetInstance()->GetSrvIndex("resources/images/noise0.png");
	if (maskSrvIndex != 0) {
		SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, maskSrvIndex);
	} else {
		// 見つからない場合はメインテクスチャをダミーとして設定
		SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(2, srvIndex_);
	}

	// 描画 (3頂点)
	commandList->DrawInstanced(3, 1, 0, 0);

}

void PostProcess::PostDraw(bool drawToSwapchain)
{
	auto commandList = dxCommon_->GetCommandList();
	D3D12_RESOURCE_BARRIER barriers[2]{};
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barriers[0].Transition.pResource = dxCommon_->GetRenderTextureResource().Get();
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	int barrierCount = 1;
	if (!drawToSwapchain) {
		Microsoft::WRL::ComPtr<ID3D12Resource> targetResource = dxCommon_->GetPostProcessTextureResource();
		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[1].Transition.pResource = targetResource.Get();
		barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrierCount = 2;
	}
	commandList->ResourceBarrier(barrierCount, barriers);
	if (!drawToSwapchain) {
		D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtvHandle = dxCommon_->GetCurrentBackBufferRTVHandle();
		// スワップチェーン描画にDSVは不要。1280x720のDSVをバインドすると1280x720外がクリップされるためnullptrにする
		commandList->OMSetRenderTargets(1, &backBufferRtvHandle, false, nullptr);

		// スワップチェーン全体のViewport・Scissorを設定
		uint32_t bbWidth = dxCommon_->GetBackBufferWidth();
		uint32_t bbHeight = dxCommon_->GetBackBufferHeight();
		D3D12_VIEWPORT scVp{};
		scVp.Width = static_cast<float>(bbWidth);
		scVp.Height = static_cast<float>(bbHeight);
		scVp.TopLeftX = 0.0f;
		scVp.TopLeftY = 0.0f;
		scVp.MinDepth = 0.0f;
		scVp.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &scVp);

		D3D12_RECT scScissor{ 0, 0, static_cast<LONG>(bbWidth), static_cast<LONG>(bbHeight) };
		commandList->RSSetScissorRects(1, &scScissor);
	}
}

void PostProcess::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("Post Process Editor");
	DrawImGuiContent();
	ImGui::End();
#endif
}

void PostProcess::DrawImGuiContent()
{
#ifdef USE_IMGUI
	for (auto& effect : effects_) {
		ImGui::PushID(effect->name_.c_str());
		ImGui::Checkbox(effect->name_.c_str(), &effect->isActive_);
		
		// ONの時だけ詳細なパラメータUIを表示
		if (effect->isActive_) {
			ImGui::Indent();
			effect->DrawImGui();
			ImGui::Unindent();
		}
		ImGui::PopID();
	}

	ImGui::Separator();
	if (ImGui::Button("設定を保存 (Save Settings)")) {
		SaveToJson();
	}
	ImGui::SameLine();
	if (ImGui::Button("設定を読込 (Load Settings)")) {
		LoadFromJson();
	}
#endif
}

void PostProcess::SaveToJson()
{
	nlohmann::json j;

	for (const auto& effect : effects_) {
		nlohmann::json effectJson;
		effectJson["isActive"] = effect->isActive_;
		
		if (effect->name_ == "Sepia") {
			effectJson["strength"] = data_.sepiaStrength;
		} else if (effect->name_ == "Vignetting") {
			effectJson["intensity"] = data_.vignetteIntensity;
			effectJson["power"] = data_.vignettePower;
		} else if (effect->name_ == "Smoothing") {
			effectJson["kernelSize"] = data_.smoothingKernelSize;
		} else if (effect->name_ == "GaussianFilter") {
			effectJson["sigma"] = data_.gaussianSigma;
		} else if (effect->name_ == "Outline") {
			effectJson["threshold"] = data_.outlineThreshold;
		} else if (effect->name_ == "RadialBlur") {
			effectJson["centerX"] = data_.radialBlurCenterX;
			effectJson["centerY"] = data_.radialBlurCenterY;
			effectJson["intensity"] = data_.radialBlurIntensity;
		} else if (effect->name_ == "Dissolve") {
			effectJson["threshold"] = data_.dissolveThreshold;
			effectJson["edgeWidth"] = data_.dissolveEdgeWidth;
			effectJson["edgeColor"] = { data_.dissolveEdgeColor[0], data_.dissolveEdgeColor[1], data_.dissolveEdgeColor[2] };
		} else if (effect->name_ == "Random(Glitch)") {
			effectJson["glitchStrength"] = data_.glitchStrength;
			effectJson["noiseStrength"] = data_.noiseStrength;
		}

		j[effect->name_] = effectJson;
	}

	std::ofstream file("resources/json/settings/postprocess.json");
	if (file.is_open()) {
		file << j.dump(4); // インデント付きで見やすく保存
	}
}

void PostProcess::LoadFromJson()
{
	std::ifstream file("resources/json/settings/postprocess.json");
	if (!file.is_open()) return;

	nlohmann::json j;
	file >> j;

	for (auto& effect : effects_) {
		if (j.contains(effect->name_)) {
			auto& effectJson = j[effect->name_];
			
			if (effectJson.contains("isActive")) effect->isActive_ = effectJson["isActive"];
			
			if (effect->name_ == "Sepia" && effectJson.contains("strength")) {
				data_.sepiaStrength = effectJson["strength"];
			} else if (effect->name_ == "Vignetting") {
				if (effectJson.contains("intensity")) data_.vignetteIntensity = effectJson["intensity"];
				if (effectJson.contains("power")) data_.vignettePower = effectJson["power"];
			} else if (effect->name_ == "Smoothing") {
				if (effectJson.contains("kernelSize")) data_.smoothingKernelSize = effectJson["kernelSize"];
			} else if (effect->name_ == "GaussianFilter") {
				if (effectJson.contains("sigma")) data_.gaussianSigma = effectJson["sigma"];
			} else if (effect->name_ == "Outline") {
				if (effectJson.contains("threshold")) data_.outlineThreshold = effectJson["threshold"];
			} else if (effect->name_ == "RadialBlur") {
				if (effectJson.contains("centerX")) data_.radialBlurCenterX = effectJson["centerX"];
				if (effectJson.contains("centerY")) data_.radialBlurCenterY = effectJson["centerY"];
				if (effectJson.contains("intensity")) data_.radialBlurIntensity = effectJson["intensity"];
			} else if (effect->name_ == "Dissolve") {
				if (effectJson.contains("threshold")) data_.dissolveThreshold = effectJson["threshold"];
				if (effectJson.contains("edgeWidth")) data_.dissolveEdgeWidth = effectJson["edgeWidth"];
				if (effectJson.contains("edgeColor")) {
					data_.dissolveEdgeColor[0] = effectJson["edgeColor"][0];
					data_.dissolveEdgeColor[1] = effectJson["edgeColor"][1];
					data_.dissolveEdgeColor[2] = effectJson["edgeColor"][2];
				}
			} else if (effect->name_ == "Random(Glitch)") {
				if (effectJson.contains("glitchStrength")) data_.glitchStrength = effectJson["glitchStrength"];
				if (effectJson.contains("noiseStrength")) data_.noiseStrength = effectJson["noiseStrength"];
			}
		}
	}
}