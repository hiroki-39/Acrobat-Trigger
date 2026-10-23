#pragma once
#include<Windows.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<wrl.h>
#include <array>
#include <dxcapi.h>
#include <cassert>
#include <string>
#include "KHEngine/Core/Utility/Log/Logger.h"
#include "KHEngine/Core/Utility/String/StringUtility.h"
#include "externals/DirectXTex/d3dx12.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "KHEngine/Core/Utility/Timer/Timer.h"
#include "KHEngine/Core/OS/WinApp.h"
#include "KHEngine/Graphics/Resource/Descriptor/SrvManager.h"
#include "KHEngine/Math/Vector4.h"

class SrvManager;

class DirectXCommon
{
public:

	// テクスチャアップロードバッチ用構造体
	struct UploadBatch
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> dst;
		Microsoft::WRL::ComPtr<ID3D12Resource> interm;
		std::vector<D3D12_SUBRESOURCE_DATA> sub;
	};

public://メンバ関数

	/// <summary>
	/// 初期化処理
	/// </summary>
	void Initialize(WinApp* winApp);

	/// <summary>
	/// デバイスの初期化
	/// </summary>
	void InitDevice();

	/// <summary>
	/// コマンド関連の初期化
	/// </summary>
	void InitCommand();

	/// <summary>
	/// スワップチェーンの生成
	/// </summary>
	void CreateSwapChain();

	/// <summary>
	/// 各種デスクリプタヒープの生成
	/// </summary>
	void CreateDescriptorHeaps();

	/// <summary>
	/// レンダーターゲットビューの初期化
	/// </summary>
	void CreateRTV();

	/// <summary>
	/// 深度ステンシルビューの初期化
	/// </summary>
	void CreateDSV();

	/// <summary>
	//フェンスの生成
	/// </summary>
	void CreateFence();

	/// <summary>
	/// ビューポート矩形の初期化
	/// </summary>
	void SetViewportRect();

	/// <summary>
	/// シザー矩形の初期化
	/// </summary>
	void SetScissorRect();

	/// DXCコンパイラの生成
	void CreateDXCCompiler();

	/// <summary>
	/// デスクリプタヒープを生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heaptype, UINT numDescriptors, bool shaderVisible);



	/// <summary>
	/// RenderTextureリソースの生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);

	/// <summary>
	/// 深度ステンシルテクスチャリソースを生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreatDepthStencilTextureResource(int32_t width, int32_t height);

	/// <summary>
	/// 描画前処理(RenderTextureへの描画用)
	/// </summary>
	void PreDraw();

	/// <summary>
	/// スワップチェーン用の描画前処理
	/// </summary>
	void PreDrawSwapchain();

	/// <summary>
	/// 描画後処理
	/// </summary>
	void PostDraw();

	/// <summary>
	/// シェーダーのコンパイル
	/// </summary>
	Microsoft::WRL::ComPtr<IDxcBlob> compileshader(const std::wstring& filePath, const wchar_t* profile);

	/// <summary>
	/// バッファリソースの生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInbytes);

	/// <summary>
	/// テクスチャリソースの生成
	/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metdata);

	/// <summary>
	/// テクスチャデータの転送
	/// </summary>
	[[nodiscard]]
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages);

	/// <summary>
	/// テクスチャアップロードバッチ開始
	/// </summary>
	void BeginTextureUploadBatch();

	/// <summary>
	/// テクスチャアップロードバッチ完了
	/// </summary>
	void AddTextureUpload(
		Microsoft::WRL::ComPtr<ID3D12Resource> texture,
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediate,
		const std::vector<D3D12_SUBRESOURCE_DATA>& subresources
	);

	/// <summary>
	/// テクスチャアップロードバッチ実行
	/// </summary>
	void ExecuteTextureUploadBatch();

	/// <summary>
	/// スワップチェーンのリサイズ処理 (ウィンドウサイズ変更・フルスクリーン対応)
	/// </summary>
	void ResizeSwapChain(uint32_t width, uint32_t height);

	void RegisterSrvManager(class SrvManager* srv);

	// --- Getter ---
	ID3D12Device* GetDevice() { return device.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() { return commandList.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(); }
	ID3D12CommandQueue* GetCommandQueue() { return commandQueue.Get(); }
	WinApp* GetWinApp() const { return winApp; }
	size_t GetSwapChainResourceNum() const { return swapChainResources.size(); }
	Microsoft::WRL::ComPtr<ID3D12Resource> GetRenderTextureResource() const { return renderTextureResource_; }

	// スワップチェーンのバックバッファ解像度
	uint32_t GetBackBufferWidth() const { return backBufferWidth_; }
	uint32_t GetBackBufferHeight() const { return backBufferHeight_; }

	// PostProcess用のGetter
	Microsoft::WRL::ComPtr<ID3D12Resource> GetPostProcessTextureResource() const { return postProcessTextureResource_; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetPostProcessRTVHandle() const { return postProcessRTVHandle_; }

	// スワップチェーンのバックバッファ取得用
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferRTVHandle() const {
		return rtvHandles[swapChain->GetCurrentBackBufferIndex()];
	}
	Microsoft::WRL::ComPtr<ID3D12Resource> GetCurrentBackBufferResource() const {
		return swapChainResources[swapChain->GetCurrentBackBufferIndex()];
	}


private://メンバ変数

	WinApp* winApp = nullptr;

	SrvManager* srvManager = nullptr;

	// FPS制御タイマー
	Timer timer;

	// DirectX12のデバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device;

	// DXGIファクトリ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;

	// コマンドアロケータ
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;

	// コマンドリスト
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;

	// コマンドキュー
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;

	// スワップチェーン
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

	// スワップチェーンの解像度
	uint32_t backBufferWidth_ = WinApp::kClientWidth;
	uint32_t backBufferHeight_ = WinApp::kClientHeight;

	// RTV用のDescriptorSize
	uint32_t desriptorSizeRTV;

	// DSV用のDescriptorSize
	uint32_t desriptorSizeDSV;

	// RTVのヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;

	// DSVのヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;

	//スワップチェーンリソース
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources;

	// RTVを2つ作るので、ディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

	// RTVの生成
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};

	// RTVハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;

	// RenderTexture リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> renderTextureResource_;
	// RenderTexture 用の RTV ハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE renderTextureRTVHandle_;

	// PostProcess用 RenderTexture リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> postProcessTextureResource_;
	// PostProcess用 RenderTexture RTV ハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE postProcessRTVHandle_;

	// 深度バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource;

	// フェンス
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;

	//フェンス用イベントハンドル
	HANDLE fenceEvent;

	// ビューポート
	D3D12_VIEWPORT viewport{ };

	// シザー矩形
	D3D12_RECT scissorRect{ };

	// dxc 関連を ComPtr に変更（明示的な Release を避けるため）
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler;

	// includeHandler
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;

	// TransitionBarrier
	D3D12_RESOURCE_BARRIER barrier{};

	// フェンス値
	UINT64 fenceValue;

	// テクスチャアップロードキュー
	std::vector<UploadBatch> textureUploadQueue_;
	// アップロード用コマンド関連
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> uploadAllocator_;
	// アップロード用コマンドリスト
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> uploadCommandList_;
};