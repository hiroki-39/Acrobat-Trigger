#include "DirectXCommon.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")

using namespace Microsoft::WRL;

void DirectXCommon::Initialize(WinApp* winApp)
{
	//FPS固定の初期化
	timer.InitalizeFixFPS();

	//NULLチェック
	assert(winApp);

	//メンバ変数に記憶
	this->winApp = winApp;

	//デバイスの生成
	InitDevice();

	//コマンド関連の初期化
	InitCommand();

	//スワップチェーンの生成
	CreateSwapChain();

	//各種デスクリプタヒープの生成
	CreateDescriptorHeaps();

	//レンダーターゲットビューの生成
	CreateRTV();

	//深度ステンシルビューの生成
	CreateDSV();

	//フェンスの生成
	CreateFence();

	//ビューポートの設定
	SetViewportRect();

	//シザー矩形の設定
	SetScissorRect();

	//DXCコンパイラの生成
	CreateDXCCompiler();

}

void DirectXCommon::InitDevice()
{
	/*--- DirectXの初期化 ---*/

#ifdef _DEBUG

	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		//デバックレイヤーの有効化
		debugController->EnableDebugLayer();
		//GPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}

#endif 

	/*HRESULTはWindows系のエラーコードであり
	 関数が成功したかSUCCEEDEDマクロで判定できる*/
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	/*初期化の根本的な部分でエラーが出た場合ｈプログラムが間違っているか、
	どうにも出来ない場合が多いのでassertにしておく*/
	assert(SUCCEEDED(hr));

	//使用するアダプタ用の変数。
	ComPtr <IDXGIAdapter4> useAdapter = nullptr;

	//良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i)
	{
		//アダプタの情報を取得
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));

		//ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
		{
			//採用したアダプタの情報をログに出力
			/*Log(logStream, ConvertString(std::format(
				L"Use Adapater: {}\n", adapterDesc.Description)));*/
			break;
		}

		//ソフトウェアアダプタは見なかったことにする
		useAdapter = nullptr;
	}

	//アダプタが見つからなかった場合
	assert(useAdapter != nullptr);

	/*--- D3D12Deviceの生成 ---*/

	//機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
	};

	const char* featureLevelStrings[] =
	{
		"12.2",
		"12.1",
		"12.0",
	};

	//機能レベルを順に試す
	for (int i = 0; i < _countof(featureLevels); ++i)
	{
		//採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(
			useAdapter.Get(),               //アダプタ
			featureLevels[i],               //機能レベル
			IID_PPV_ARGS(&device));         //デバイスのポインタ

		//指定した機能レベルでデバイスが生成できたか確認
		if (SUCCEEDED(hr))
		{
			//機能レベルが採用されたので、ループを抜ける
			/*Log(logStream, std::format("FeatureLevel: {} \n", featureLevelStrings[i]));*/
			break;
		}
	}

	//デバイスが生成できなかった場合
	assert(device != nullptr);
	//初期化完了のメッセージを出力
	/*Log(logStream, "Complete create D3D12Device!!!!\n");*/


#ifdef _DEBUG

	ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		//ヤバイエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		//エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		//警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] =
		{
			//デバッグレイヤーの警告
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
		};

		//抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] =
		{
			//警告
			D3D12_MESSAGE_SEVERITY_INFO
		};

		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;

		//指定したメッセージの表示を抑制
		infoQueue->AddStorageFilterEntries(&filter);
	}

#endif 
}

void DirectXCommon::InitCommand()
{
	HRESULT hr;

	//コマンドアロケータの生成
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&commandAllocator));

	//コマンドリストの生成
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));

	//コマンドリストの生成に失敗した場合起動できない
	assert(SUCCEEDED(hr));

	//コマンドキューの生成
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc,
		IID_PPV_ARGS(&commandQueue));

	//コマンドキューの生成に失敗した場合起動できない
	assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateSwapChain()
{
	HRESULT hr;

	backBufferWidth_ = winApp->GetClientWidth();
	backBufferHeight_ = winApp->GetClientHeight();
	if (backBufferWidth_ == 0) backBufferWidth_ = WinApp::kClientWidth;
	if (backBufferHeight_ == 0) backBufferHeight_ = WinApp::kClientHeight;

	//ウィンドウの幅
	swapChainDesc.Width = backBufferWidth_;
	//ウィンドウの高さ
	swapChainDesc.Height = backBufferHeight_;
	//色の形式
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//マルチサンプルしない
	swapChainDesc.SampleDesc.Count = 1;
	//描画をターゲットとして利用
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	//ダブルバッファ
	swapChainDesc.BufferCount = 2;
	//モニタにうつしたら、中身を破壊
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	//コマンドキュー、ウィンドウハンドル、設定を渡して生成
	hr = dxgiFactory->CreateSwapChainForHwnd(
		commandQueue.Get(),     //コマンドキュー
		winApp->GetHwnd(),      //ウィンドウハンドル
		&swapChainDesc,         //スワップチェーンの設定
		nullptr,                //モニタの設定
		nullptr,                //コンシューマーの設定
		reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()) //スワップチェーンのポインタ
	);

	//スワップチェーンの生成に失敗した場合起動できない
	assert(SUCCEEDED(hr));

	// DXGIの自動Alt+Enterを抑止（アプリ側でフルスクリーン制御するため）
	dxgiFactory->MakeWindowAssociation(winApp->GetHwnd(), DXGI_MWA_NO_ALT_ENTER);
}

void DirectXCommon::CreateDescriptorHeaps()
{
	//DescriptorSizeを取得
	desriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	desriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	//RTV用のヒープでディスクリプタの数は4(SwapChainx2 + RenderTexturex1 + PostProcessTexturex1)。
	rtvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 4, false);

	//DSV用のヒープでディスクリプタの数は1。RTVはShader内で触るものではないので、ShaderVisibleはfalse
	dsvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

}

void DirectXCommon::CreateRTV()
{
	HRESULT hr;

	//SwapChainからResourceを引っ張ってくる
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	assert(SUCCEEDED(hr));

	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

	// 出力をスワップチェーンのフォーマットに合わせる
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	//2Dテクスチャとして書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	// ディスクリプタの先頭ハンドルを取得してローカルでインクリメントする（元の実装は誤って配列要素を直接増やしていた）
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	const UINT incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	for (uint32_t i = 0; i < 2; i++)
	{
		// 現在のハンドルを配列に保存してから作成する
		rtvHandles[i] = handle;

		// リソースごとにRTVを作成
		device->CreateRenderTargetView(swapChainResources[i].Get(), &rtvDesc, rtvHandles[i]);

		// ローカルハンドルを次のディスクリプタに進める
		handle.ptr = handle.ptr + incrementSize;
	}

	// RenderTextureの生成
	const Vector4 kRenderTargetClearValue{ 0.1f, 0.25f, 0.5f, 1.0f }; // 空の色に近い青系
	renderTextureResource_ = CreateRenderTextureResource(WinApp::kClientWidth, WinApp::kClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, kRenderTargetClearValue);

	// RenderTexture用のRTV作成
	renderTextureRTVHandle_ = handle;
	device->CreateRenderTargetView(renderTextureResource_.Get(), &rtvDesc, renderTextureRTVHandle_);

	handle.ptr = handle.ptr + incrementSize;

	// PostProcess用Textureの生成とRTV作成
	postProcessTextureResource_ = CreateRenderTextureResource(WinApp::kClientWidth, WinApp::kClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, kRenderTargetClearValue);
	postProcessRTVHandle_ = handle;
	device->CreateRenderTargetView(postProcessTextureResource_.Get(), &rtvDesc, postProcessRTVHandle_);
}

void DirectXCommon::CreateDSV()
{
	// 正しくは DSV ヒープを作成（RTV ヒープを上書きしない）
	dsvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// 深度リソースを作成
	depthStencilResource = CreatDepthStencilTextureResource(winApp->kClientWidth, winApp->kClientHeight);

	// DSV 設定
	D3D12_DEPTH_STENCIL_VIEW_DESC devDesc{};
	devDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	devDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	device->CreateDepthStencilView(depthStencilResource.Get(), &devDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void DirectXCommon::CreateFence()
{
	HRESULT hr;

	//初期値0でFanceを作る
	fence = nullptr;
	fenceValue = 0;


	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&fence));

	//フェンスの生成に失敗した場合起動できない
	assert(SUCCEEDED(hr));

	//Fanceのイベントを作成
	fenceEvent = CreateEvent(nullptr, false, false, nullptr);
	//イベントの生成に失敗した場合起動できない
	assert(fenceEvent != nullptr);
}

void DirectXCommon::SetViewportRect()
{
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = winApp->kClientWidth;
	viewport.Height = winApp->kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

void DirectXCommon::SetScissorRect()
{
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = winApp->kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = winApp->kClientHeight;
}

void DirectXCommon::CreateDXCCompiler()
{
	HRESULT hr;

	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	//現時点でincludeはしないが、includeに対応する為の設定を行う
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heaptype, UINT numDescriptors, bool shaderVisible)
{

	//ディスクリプタヒープの生成
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};

	//ディスクリプタヒープの数
	descriptorHeapDesc.Type = heaptype;
	//ダブルバッファロ用の2つ
	descriptorHeapDesc.NumDescriptors = numDescriptors;

	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE :
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc,
		IID_PPV_ARGS(&descriptorHeap));

	//ディスクリプタヒープの生成に失敗した場合起動できない
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateRenderTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor)
{
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE clearValue;
	clearValue.Format = format;
	clearValue.Color[0] = clearColor.x;
	clearValue.Color[1] = clearColor.y;
	clearValue.Color[2] = clearColor.z;
	clearValue.Color[3] = clearColor.w;

	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		&clearValue,
		IID_PPV_ARGS(&resource));

	assert(SUCCEEDED(hr));
	return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreatDepthStencilTextureResource(int32_t width, int32_t height)
{
	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	//Textureの幅
	resourceDesc.Width = width;
	//Textureの高さ
	resourceDesc.Height = height;
	//mipmapの数
	resourceDesc.MipLevels = 1;
	//奥行き or　配列Textureの配列数
	resourceDesc.DepthOrArraySize = 1;
	//TextureのFormat
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	//サンプリングカウント。1固定。
	resourceDesc.SampleDesc.Count = 1;
	//Textrueの次元数。普段使っているのは2次元
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	//通知
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	//2.利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	//細かい設定
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	//深度のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	//3.Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resourece = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,//Heapの設定
		D3D12_HEAP_FLAG_NONE, //Heapの特殊な設定
		&resourceDesc, //Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE,//深度値を」書き込むため状態にする
		&depthClearValue,//Clearの最適値
		IID_PPV_ARGS(&resourece)//生成したResourceのポインタ
	);

	assert(SUCCEEDED(hr));

	return resourece;
}

void DirectXCommon::PreDraw()
{
	// 描画先のRTVとDSVを設定
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(1, &renderTextureRTVHandle_, false, &dsvHandle);

	// 指定した色で画面全体をクリアする(RenderTextureの生成時と同じ色)
	float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
	commandList->ClearRenderTargetView(renderTextureRTVHandle_, clearColor, 0, nullptr);

	// 指定した深度で画面全体をクリア
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// 描画用のDesciptorHeapを設定
	if (srvManager)
	{
		srvManager->PreDraw();
	}

	//Viewportを設定
	commandList->RSSetViewports(1, &viewport);

	//Scirssorを設定
	commandList->RSSetScissorRects(1, &scissorRect);
}

void DirectXCommon::PreDrawSwapchain()
{
	//これから書き込むバックバッファのインデックスを取得
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	//バリアの種類(今回はTransition)
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	//TransitionBarrierを張る
	commandList->ResourceBarrier(1, &barrier);

	//描画先のRTVを設定 (スワップチェーン描画にDSVは不要なのでnullptr)
	commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);

	//指定した色で画面全体をクリアする (黒クリアでレターボックスの余白を黒く保つ)
	float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

	//スワップチェーン全体のViewportを設定
	D3D12_VIEWPORT scViewport{};
	scViewport.Width = static_cast<float>(backBufferWidth_);
	scViewport.Height = static_cast<float>(backBufferHeight_);
	scViewport.TopLeftX = 0;
	scViewport.TopLeftY = 0;
	scViewport.MinDepth = 0.0f;
	scViewport.MaxDepth = 1.0f;
	commandList->RSSetViewports(1, &scViewport);

	//スワップチェーン全体のScissorを設定
	D3D12_RECT scScissor{};
	scScissor.left = 0;
	scScissor.top = 0;
	scScissor.right = static_cast<LONG>(backBufferWidth_);
	scScissor.bottom = static_cast<LONG>(backBufferHeight_);
	commandList->RSSetScissorRects(1, &scScissor);
}

void DirectXCommon::PostDraw()
{
	HRESULT hr;

	// バックバッファの番号を取得
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	//状態を遷移(RenderTargetからPresentにする)
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	//TransitionBarrierを張る
	commandList->ResourceBarrier(1, &barrier);

	//コマンドリストの内容を確定させ、全てのコマンドを積んでからcloseする
	hr = commandList->Close();
	//コマンドリストの確定に失敗した場合起動できない
	assert(SUCCEEDED(hr));

	//GPUにコマンドリストを実行させる
	Microsoft::WRL::ComPtr<ID3D12CommandList> commandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(1, commandLists->GetAddressOf());

	//GPUとOSに画面交換を行うように通知
	swapChain->Present(1, 0);

	//Fanceの値を更新
	fenceValue++;

	//GPUがここまでたどり着いた時、Fanseの値を指定した値に代入するようにsignalを送る
	commandQueue->Signal(fence.Get(), fenceValue);

	//Fanceの値が指定したSignal値たどり着いているか確認する
	//GetCompletedValueの初期値はFance作成時に渡した初期値
	if (fence->GetCompletedValue() < fenceValue)
	{
		//指定したSignal値までGPUがたどり着いていない場合、たどり着くまで待つように、イベントを設定する
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		//イベントが発火するまで待つ
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	//FPS固定
	timer.UpdateFixFPS();

	//次のフレーム用のコマンドリストを準備
	hr = commandAllocator->Reset();
	//コマンドアロケータのリセットに失敗した場合起動できない
	assert(SUCCEEDED(hr));
	hr = commandList->Reset(commandAllocator.Get(), nullptr);
	//コマンドリストのリセットに失敗した場合起動できない
	assert(SUCCEEDED(hr));
}

Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::compileshader(const std::wstring& filePath, const wchar_t* profile)
{
	/* 1.hlslファイルを読む */

	//これからシェーダーのコンパイルする旨をログを出す
	//Log(os, ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n",
	//	filePath, profile)));

	//hlslファイルを読む
	ComPtr<IDxcBlobEncoding> shaderSource;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, shaderSource.GetAddressOf());

	//読めなかったら止める
	assert(SUCCEEDED(hr));

	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();

	//UFT8の文字コードであることを通知
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	/* 2.Compileする */
	LPCWSTR arguments[] = {
		filePath.c_str(), //コンパイル対象のhlslファイル名
		L"-E",L"main",    //エントリーポイントの指定。基本的にmain以外にはしない
		L"-T",profile,    //ShaderProfileの設定
		L"-Zi",L"-Qembed_debug", //デバック用の情報を埋め込む
		L"-Od",  //最適化を外す
		L"-Zpr", //メモリーレイアウトは行優先
	};

	//実際にShaderをコンパイルする
	ComPtr<IDxcResult> shaderResult;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer, // 読み込んだファイル
		arguments,           // コンパイルオプション
		_countof(arguments), // コンパイルオプションの数
		includeHandler.Get(),// ComPtrから生ポインタへ変換
		IID_PPV_ARGS(&shaderResult)
	);

	//コンパイルエラーではなくdxcが起動できないなどの致命的な状況
	assert(SUCCEEDED(hr));

	/* 3.警告・エラーが出ていないか確認する */
	ComPtr<IDxcBlobUtf8> shaderError;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0)
	{
		/*Log(os, shaderError->GetStringPointer());*/
		
		//警告・エラーダメ絶対
		assert(false);
	}

	/* 4.Compile結果を受け取って返す */
	//コンパイル結果から実行用バイナリ部分を取得
	ComPtr<IDxcBlob> shaderBlob;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));

	//成功したらログを出力
	/*Log(os, ConvertString(std::format(L"Compile Succeded, path:{}, profile:{}\n", filePath, profile)));*/

	// ComPtr を返す（自動で解放されるため明示的 Release は不要）
	return shaderBlob;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInbytes)
{

	//頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};

	//UploadHeapを使う
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	//頂点リソースの設定
	D3D12_RESOURCE_DESC resourceDesc{};

	//バッファリソース。テクスチャの場合はまた別の設定をする
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInbytes;

	//バッファの場合はこれらを1にする決まり
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;

	//バッファの場合はこれにする決まり
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;


	//実際に頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> bufferResource = nullptr;

	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&bufferResource)
	);

	assert(SUCCEEDED(hr));

	return bufferResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metdata)
{
	//1.metadataを元にResureceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	//Textureの幅
	resourceDesc.Width = UINT(metdata.width);
	//Textureの高さ
	resourceDesc.Height = UINT(metdata.height);
	//mipmapの数
	resourceDesc.MipLevels = UINT16(metdata.mipLevels);
	//奥行き or　配列Textureの配列数
	resourceDesc.DepthOrArraySize = UINT16(metdata.arraySize);
	//TextureのFormat
	resourceDesc.Format = metdata.format;
	//サンプリングカウント。1固定。
	resourceDesc.SampleDesc.Count = 1;
	//Textrueの次元数。普段使っているのは2次元
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metdata.dimension);

	//2.利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	//細かい設定
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	//3.Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resourece = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,//Heapの設定
		D3D12_HEAP_FLAG_NONE, //Heapの特殊な設定
		&resourceDesc, //Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST,//データ転送される設定
		nullptr,//Clearの最適値
		IID_PPV_ARGS(&resourece)//生成したResourceのポインタ
	);
	assert(SUCCEEDED(hr));

	return resourece;
}

[[nodiscard]]
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(
		device.Get(),
		mipImages.GetImages(),
		mipImages.GetImageCount(),
		mipImages.GetMetadata(),
		subresources
	);

	uint64_t intermediatesize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediataeResource = CreateBufferResource(intermediatesize);

	UpdateSubresources(
		commandList.Get(),
		texture.Get(),
		intermediataeResource.Get(),
		0,
		0,
		UINT(subresources.size()),
		subresources.data()
	);

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;

	commandList->ResourceBarrier(1, &barrier);

	return intermediataeResource;
}

void DirectXCommon::BeginTextureUploadBatch()
{
	HRESULT hr;

	// Note: Use DIRECT command list type here so ResourceBarrier calls are allowed.
	hr = device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&uploadAllocator_)
	);
	assert(SUCCEEDED(hr));

	hr = device->CreateCommandList(
		0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		uploadAllocator_.Get(), nullptr,
		IID_PPV_ARGS(&uploadCommandList_)
	);
	assert(SUCCEEDED(hr));

	// Ensure the command list starts open and ready for recording (CreateCommandList usually returns it open)
	textureUploadQueue_.clear();
}

void DirectXCommon::AddTextureUpload(Microsoft::WRL::ComPtr<ID3D12Resource> texture, Microsoft::WRL::ComPtr<ID3D12Resource> intermediate, const std::vector<D3D12_SUBRESOURCE_DATA>& subresources)
{
	textureUploadQueue_.push_back({ texture, intermediate, subresources });
}

void DirectXCommon::ExecuteTextureUploadBatch()
{
	// record copies & barriers into uploadCommandList_
	for (auto& job : textureUploadQueue_)
	{
		UpdateSubresources(
			uploadCommandList_.Get(),
			job.dst.Get(),
			job.interm.Get(),
			0, 0,
			(UINT)job.sub.size(),
			job.sub.data()
		);

		// バリア
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = job.dst.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		
		uploadCommandList_->ResourceBarrier(1, &barrier);

	}

	// close and execute
	uploadCommandList_->Close();

	ID3D12CommandList* lists[] = { uploadCommandList_.Get() };
	commandQueue->ExecuteCommandLists(1, lists);

	// GPU 完了をフェンスで待つ（PostDraw と同様）
	fenceValue++;
	commandQueue->Signal(fence.Get(), fenceValue);

	if (fence->GetCompletedValue() < fenceValue)
	{
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	// GPU 完了後にコマンドアロケータ／コマンドリストをリセットして解放
	uploadAllocator_.Reset();
	uploadCommandList_.Reset();

	// キューをクリア（中間リソースは TextureManager 側で明示的に解放する）
	textureUploadQueue_.clear();
}

void DirectXCommon::RegisterSrvManager(SrvManager* srv)
{
	srvManager = srv;
}

void DirectXCommon::ResizeSwapChain(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0) return;
	if (width == backBufferWidth_ && height == backBufferHeight_) return;

	Logger::Log("DirectXCommon::ResizeSwapChain: " + std::to_string(backBufferWidth_) + "x" + std::to_string(backBufferHeight_) + " -> " + std::to_string(width) + "x" + std::to_string(height));

	// GPUの完了を待機
	fenceValue++;
	commandQueue->Signal(fence.Get(), fenceValue);
	if (fence->GetCompletedValue() < fenceValue)
	{
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	// バックバッファの参照を解放（リサイズ前に必須）
	for (size_t i = 0; i < swapChainResources.size(); ++i)
	{
		swapChainResources[i].Reset();
	}

	// スワップチェーンのバッファをリサイズ
	HRESULT hr = swapChain->ResizeBuffers(
		static_cast<UINT>(swapChainResources.size()),
		width,
		height,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		0
	);
	assert(SUCCEEDED(hr));

	// バックバッファの再取得とRTVの再作成
	for (uint32_t i = 0; i < 2; i++)
	{
		hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainResources[i]));
		assert(SUCCEEDED(hr));

		device->CreateRenderTargetView(swapChainResources[i].Get(), &rtvDesc, rtvHandles[i]);
	}

	backBufferWidth_ = width;
	backBufferHeight_ = height;
}