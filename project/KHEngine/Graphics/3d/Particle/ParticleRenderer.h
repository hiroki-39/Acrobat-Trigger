#pragma once
#include <wrl.h>
#include <vector>
#include <cstdint>
#include <d3d12.h>
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Graphics/Resource/Descriptor/SrvManager.h"
#include "Particle.h"

class ParticleRenderer
{
public:
	ParticleRenderer() = default;
	~ParticleRenderer();

	// コピー禁止（二重解放防止）
	ParticleRenderer(const ParticleRenderer&) = delete;
	ParticleRenderer& operator=(const ParticleRenderer&) = delete;

	// ムーブ対応
	ParticleRenderer(ParticleRenderer&& other) noexcept;
	ParticleRenderer& operator=(ParticleRenderer&& other) noexcept;

	// 明示的解放
	void Finalize();

	// 初期化：DX と SRV 管理、インスタンス数を指定
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t maxInstances);

	// モデル頂点を渡して頂点バッファを作成（main から呼ぶ）
	void CreateVertexBuffer(const void* vertexData, uint32_t vertexCount, uint32_t vertexStride);

	// マテリアル用 CBV を内部で作成して初期化（main から呼ぶ）
	void CreateMaterialBuffer(size_t sizeInBytes, const void* initData);
	
	// マテリアルバッファの生ポインタ取得
	void* GetMaterialData() { return materialData_; }

	// インスタンスバッファの生ポインタ取得（書き込み用）
	ParticleForGPU* GetInstancingData() { return instancingData_; }

	// SRV のインデックス
	uint32_t GetInstancingSrvIndex() const { return instancingSrvIndex_; }

	// 描画：numInstances は FillInstancingBuffer で書き込んだ個数
	void Draw(uint32_t numInstances, uint32_t textureSrvIndex, BlendMode blendMode);

	// RootSignature と PSO 取得（必要なら外部参照用）
	Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() const { return rootSignature_; }
	Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineStateForBlend(BlendMode blendMode) const
	{
		int blendIndex = static_cast<int>(blendMode);
		if (blendIndex < 0 || blendIndex >= static_cast<int>(psoForBlendMode_.size())) return nullptr;
		return psoForBlendMode_[blendIndex];
	}

private:
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> psoForBlendMode_;
	uint32_t instancingSrvIndex_ = UINT32_MAX;

	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
	ParticleForGPU* instancingData_ = nullptr;

	// 頂点バッファ (パーティクル用四角形)
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	uint32_t vertexCount_ = 0;
	uint32_t vertexStride_ = 0;

	// マテリアル用 CBV 内部管理
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	void* materialData_ = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS materialCBVAddress_ = 0;

	uint32_t maxInstances_ = 0;
};
