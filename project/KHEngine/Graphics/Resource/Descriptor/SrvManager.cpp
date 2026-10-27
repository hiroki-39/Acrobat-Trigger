#include "SrvManager.h"
#include <algorithm>
#include <cassert>

const uint32_t SrvManager::kMaxSRVCount = 512;

// シングルトン取得（Meyers singleton）
SrvManager* SrvManager::GetInstance()
{
	static SrvManager instance;
	return &instance;
}

void SrvManager::Finalize()
{
	// 内部リソースの解放／状態クリア
	if (descriptorHeap)
	{
		descriptorHeap.Reset();
	}
	directXCommon = nullptr;
	descriptorSize = 0;
	useIndex = 0;
	freeList.clear();
}

void SrvManager::Initialize(DirectXCommon* dxCommon)
{
	directXCommon = dxCommon;

	// デスクリプタヒープの生成
	descriptorHeap = directXCommon->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);

	// デスクリプタ1個分のサイズを取得取得して記録
	descriptorSize = directXCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

uint32_t SrvManager::Allocate()
{
	// まず解放済みリストをチェックして再利用
	if (!freeList.empty())
	{
		uint32_t idx = freeList.back();
		freeList.pop_back();
		return idx;
	}

	// 上限チェックしてassertをかける
	assert(useIndex < kMaxSRVCount);

	// 使用する番号を退避
	uint32_t index = useIndex;

	// 次回のために番号を1進める
	useIndex++;

	return index;
}

void SrvManager::Free(uint32_t index)
{
	// 範囲外は無視
	if (index >= kMaxSRVCount) return;

	// 二重解放は無視
	if (std::find(freeList.begin(), freeList.end(), index) != freeList.end()) return;

	// 解放リストに追加（後入れ先出しで再利用）
	freeList.push_back(index);
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetSRVCPUDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetSRVGPUDescriptorHandle(uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

uint32_t SrvManager::GetIndexFromCPUDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	D3D12_CPU_DESCRIPTOR_HANDLE start = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	size_t offset = static_cast<size_t>(handle.ptr - start.ptr);
	return static_cast<uint32_t>(offset / descriptorSize);
}

void SrvManager::CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels)
{
	// SRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = static_cast<UINT>(MipLevels);
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// 設定をもとにSRVの生成
	directXCommon->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetSRVCPUDescriptorHandle(srvIndex)
	);
}

void SrvManager::CreateSRVforTextureCube(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels)
{
	// SRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE; // TEXTURECUBEに変更
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = UINT_MAX;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	// 設定をもとにSRVの生成
	directXCommon->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetSRVCPUDescriptorHandle(srvIndex)
	);
}

void SrvManager::CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT NumElements, UINT StructureByteStride)
{
	// SRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = NumElements;
	srvDesc.Buffer.StructureByteStride = StructureByteStride;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	// 設定をもとにSRVの生成
	directXCommon->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetSRVCPUDescriptorHandle(srvIndex));
}

void SrvManager::PreDraw()
{
	// デスクリプタヒープの設定
	ID3D12DescriptorHeap* ppHeaps[] = { descriptorHeap.Get() };
	directXCommon->GetCommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
}

void SrvManager::SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex)
{
	directXCommon->GetCommandList()->SetGraphicsRootDescriptorTable(rootParameterIndex, GetSRVGPUDescriptorHandle(srvIndex));
}

bool SrvManager::CanAllocate()
{
	//上限に足していればtrueを返す、そうでなければfalseを返す
	return useIndex < kMaxSRVCount || !freeList.empty();
}


void SrvManager::CreateUAVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = numElements;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	uavDesc.Buffer.StructureByteStride = structureByteStride;

	directXCommon->GetDevice()->CreateUnorderedAccessView(
		pResource,
		nullptr,
		&uavDesc,
		GetSRVCPUDescriptorHandle(srvIndex)
	);
}

void SrvManager::SetComputeRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex)
{
	directXCommon->GetCommandList()->SetComputeRootDescriptorTable(rootParameterIndex, GetSRVGPUDescriptorHandle(srvIndex));
}