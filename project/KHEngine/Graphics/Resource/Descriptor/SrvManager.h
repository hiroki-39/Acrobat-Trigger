#pragma once
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include <vector>
#include <cstdint>

class DirectXCommon;

class SrvManager
{
public:// メンバ関数

	static SrvManager* GetInstance();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DirectXCommon* dxCommon);

	void Finalize();

	/// <summary>
	/// 確保用
	/// </summary>
	uint32_t Allocate();

	/// <summary>
	/// 解放
	/// </summary>
	void Free(uint32_t index);

	/// <summary>
	/// SRVの指定番号のCPUデスクリプタハンドルを取得
	/// </summary>
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);

	/// <summary>
	/// SRVの指定番号のGPUデスクリプタハンドルを取得
	/// </summary>
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

	/// <summary>
	/// SRV作成(Texture2D用)
	/// </summary>
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);

	/// <summary>
	/// SRV作成(TextureCube用)
	/// </summary>
	void CreateSRVforTextureCube(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);

	/// <summary>
	/// SRV作成(StructuredBuffer用)
	/// </summary>
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT NumElements, UINT StructureByteStride);

	/// <summary>
	/// UAV作成(StructuredBuffer用)
	/// </summary>
	void CreateUAVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	/// <summary>
	/// 
	/// </summary>
	void PreDraw();

	/// <summary>
	/// コマンドリストにSRVをセット (Graphics用)
	/// </summary>
	void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex);

	/// <summary>
	/// コマンドリストにSRV/UAVをセット (Compute用)
	/// </summary>
	void SetComputeRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex);

	/// <summary>
	/// 確保可能かチェック
	/// </summary>
	bool CanAllocate();

	/// <summary>
	/// CPUデスクリプタハンドルからSRVインデックスを取得
	/// </summary>
	uint32_t GetIndexFromCPUDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle);

public:

	// 最大SRV数 (最大テクスチャ枚数)
	static const uint32_t kMaxSRVCount;

private:

	// シングルトンインスタンス（Finalizeで delete して nullptr に戻せるようにする）
	static SrvManager* s_instance;

	DirectXCommon* directXCommon = nullptr;

	// SRV用のDescriptorSize
	uint32_t descriptorSize;

	// SRVのヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	// 次に使用するSRVインデックス
	uint32_t useIndex = 0;

	// 解放されたインデックスを再利用するためのスタック
	std::vector<uint32_t> freeList;
};