#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Core/Resource/ResourceLocator.h"
#include "KHEngine/Core/Utility/String/StringUtility.h"
#include "KHEngine/Core/Utility/Log/Logger.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <DirectXTex.h> 

TextureManager* TextureManager::GetInstance()
{
	static TextureManager instance;
	return &instance;
}

void TextureManager::Finalize()
{
	// 内部リソースの解放（ComPtr とコンテナの自動管理に任せる）
	textureDatas.clear();
	textureIndexToFilePath.clear();
	dxCommon_ = nullptr;
	srvManager = nullptr;
}

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	dxCommon_ = dxCommon;

	// メンバ変数として記録する
	this->srvManager = srvManager;

	// 事前確保（ヒープの最大数に合わせて）
	textureDatas.reserve(srvManager->kMaxSRVCount);
	textureIndexToFilePath.reserve(srvManager->kMaxSRVCount);

	// 常に専用の1x1白テクスチャを生成して登録する（uvChecker を既定にしない）
	{
		TextureData tex{};

		// メタデータ設定
		DirectX::TexMetadata meta{};
		meta.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		meta.width = 1;
		meta.height = 1;
		meta.arraySize = 1;
		meta.mipLevels = 1;
		meta.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;

		tex.metadata = meta;

		// リソース作成
		tex.resource = dxCommon_->CreateTextureResource(tex.metadata);

		// 生ピクセル (RGBA 255,255,255,255)
		tex.rawData.resize(4);
		tex.rawData[0] = 0xFF;
		tex.rawData[1] = 0xFF;
		tex.rawData[2] = 0xFF;
		tex.rawData[3] = 0xFF;

		// SRV インデックス割当
		tex.srvIndex = srvManager->Allocate();

		// マッピングを追加（srvIndex と配列インデックスを一致させる）
		if (textureIndexToFilePath.size() <= tex.srvIndex) {
			textureIndexToFilePath.resize(tex.srvIndex + 1);
		}
		// 内部キーとして固定の論理名を使う
		const std::string key = "__default_white__";
		textureIndexToFilePath[tex.srvIndex] = key;

		tex.srvHandleCPU = srvManager->GetSRVCPUDescriptorHandle(tex.srvIndex);
		tex.srvHandleGPU = srvManager->GetSRVGPUDescriptorHandle(tex.srvIndex);

		// SRVの作成
		srvManager->CreateSRVforTexture2D(
			tex.srvIndex,
			tex.resource.Get(),
			tex.metadata.format,
			static_cast<UINT>(tex.metadata.mipLevels)
		);

		// サブリソース情報をセット（生ピクセルへのポインタを指定）
		D3D12_SUBRESOURCE_DATA sub{};
		sub.pData = tex.rawData.data();
		sub.RowPitch = 4;   // 4 bytes per row (1 pixel * 4 bytes)
		sub.SlicePitch = 4;

		tex.subresources.push_back(sub);

		// intermediateサイズは正しいサブリソース数で計算する
		uint64_t intermediateSize = GetRequiredIntermediateSize(
			tex.resource.Get(),
			0,
			(UINT)tex.subresources.size()
		);

		// 転送用に生成した中間リソースをテクスチャデータ構造体に格納
		tex.intermediateResource = dxCommon_->CreateBufferResource(intermediateSize);

		// textureDatas に格納（キーは内部キー）
		textureDatas.emplace(key, std::move(tex));

		// Add upload
		auto& added = textureDatas.at(key);
		dxCommon_->BeginTextureUploadBatch();
		dxCommon_->AddTextureUpload(added.resource, added.intermediateResource, added.subresources);
		dxCommon_->ExecuteTextureUploadBatch();

		// デフォルトインデックスを保持（必ずこの生成白テクスチャを既定にする）
		defaultTextureIndex_ = textureDatas.at(key).srvIndex;
	}
}

void TextureManager::LoadTexture(const std::string& filePath)
{
	// 論理名またはパスを実パスに解決
	std::string resolved = ResourceLocator::Resolve(filePath, ResourceLocator::AssetType::Texture);

	// 存在しない場合はそのまま試す（古いコード互換）
	if (resolved.empty())
	{
		resolved = filePath;
	}

	// すでに読み込まれているかチェック（キーは実パス）
	if (textureDatas.find(resolved) != textureDatas.end())
	{
		return;
	}

	// テクスチャ枚数上限チェック
	assert(textureDatas.size() + 1 < SrvManager::kMaxSRVCount);

	// ファイルパスをwstringに変換
	std::wstring wFilePath = StringUtility::ConvertString(resolved);

	// テクスチャデータを読み込むための変数
	DirectX::ScratchImage image{};
	DirectX::TexMetadata metadata{};
	HRESULT hr;

	// ファイル拡張子をチェックして、DDSとそれ以外で処理を分岐
	if (std::filesystem::path(wFilePath).extension() == L".dds")
	{
		// DDSファイルから読み込み
		hr = DirectX::LoadFromDDSFile(wFilePath.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, image);
	}
	else
	{
		// WIC対応ファイルから読み込み
		hr = DirectX::LoadFromWICFile(wFilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, &metadata, image);
	}
	if (FAILED(hr))
	{
		Logger::Log("Failed to load texture: " + resolved + " hr: " + std::to_string(hr));
		assert(SUCCEEDED(hr) && "Failed to load texture file!");
		return;
	}

	// ミップマップの作成
	DirectX::ScratchImage mipImages{};
	// 圧縮フォーマットかどうかで分岐
	if (DirectX::IsCompressed(metadata.format))
	{
		// 圧縮フォーマットの場合は、DDSに含まれるミップマップをそのまま使う
		mipImages = std::move(image);
	}
	else
	{
		// 非圧縮フォーマットの場合はミップマップを生成する
		hr = DirectX::GenerateMipMaps(
			image.GetImages(), image.GetImageCount(), metadata,
			DirectX::TEX_FILTER_SRGB, 0, mipImages);
		if (FAILED(hr))
		{
			// ミップマップ生成に失敗した場合は元の画像をそのまま使う
			mipImages = std::move(image);
		}
	}

	// テクスチャデータを追加（key=resolved）
	textureDatas.emplace(resolved, TextureData{});

	// 追加したテクスチャデータの参照を取得
	TextureData& textureData = textureDatas[resolved];

	// ファイルから読み取った情報を格納
	const auto& finalMetadata = mipImages.GetMetadata();
	textureData.metadata = finalMetadata;
	textureData.resource = dxCommon_->CreateTextureResource(finalMetadata);

	if (!textureData.resource)
	{
		textureDatas.erase(resolved);
		return;
	}

	// テクスチャデータの要素数番号をSRVのインデックスにする（生インデックス）
	textureData.srvIndex = srvManager->Allocate();

	// インデックス -> ファイルパス マッピングを追加（srvIndex と配列インデックスを一致させる）
	if (textureIndexToFilePath.size() <= textureData.srvIndex) {
		textureIndexToFilePath.resize(textureData.srvIndex + 1);
	}
	textureIndexToFilePath[textureData.srvIndex] = resolved;

	textureData.srvHandleCPU = srvManager->GetSRVCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager->GetSRVGPUDescriptorHandle(textureData.srvIndex);

	// SRVの作成（キューブマップか2Dテクスチャかで分岐）
	if (finalMetadata.IsCubemap())
	{
		srvManager->CreateSRVforTextureCube(
			textureData.srvIndex,
			textureData.resource.Get(),
			finalMetadata.format,
			static_cast<UINT>(finalMetadata.mipLevels)
		);
	}
	else
	{
		srvManager->CreateSRVforTexture2D(
			textureData.srvIndex,
			textureData.resource.Get(),
			finalMetadata.format,
			static_cast<UINT>(finalMetadata.mipLevels)
		);
	}

	// ローカルで作成したサブリソース情報（ここをtextureDataへ保存する）
	std::vector<D3D12_SUBRESOURCE_DATA> subresources = CreateSubresources(mipImages);

	// intermediateサイズは正しいサブリソース数で計算する
	uint64_t intermediateSize = GetRequiredIntermediateSize(
		textureData.resource.Get(),
		0,
		(UINT)subresources.size()
	);

	// 転送用に生成した中間リソースをテクスチャデータ構造体に格納
	textureData.intermediateResource = dxCommon_->CreateBufferResource(intermediateSize);

	// サブリソース情報を保存しておく（後でバッチ転送で使う）
	textureData.subresources = std::move(subresources);

	// image/mipImages の所有権を textureData.image に移動（明示的 Release は行わない）
	textureData.image = std::move(mipImages);

	dxCommon_->AddTextureUpload(textureData.resource, textureData.intermediateResource, textureData.subresources);
}


void TextureManager::ClearIntermediateResources()
{
	for (auto& [filePath, textureData] : textureDatas)
	{
		// 中間リソースを解放
		textureData.intermediateResource.Reset();
		// ピクセルデータを解放
		textureData.image = DirectX::ScratchImage();
		// rawData は保持（SRV生成後は不要だが安全に保持しておく）
	}
}

void TextureManager::ExecuteUploadCommands()
{
	dxCommon_->ExecuteTextureUploadBatch();
}


uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
	// 解決して実パスキーで検索
	std::string resolved = ResourceLocator::Resolve(filePath, ResourceLocator::AssetType::Texture);
	if (resolved.empty()) resolved = filePath;

	// 読み込み済みテクスチャを検索
	if (textureDatas.contains(resolved))
	{
		TextureData& textureData = textureDatas[resolved];
		return textureData.srvIndex;
	}

	// テクスチャ条件をチェック
	assert(srvManager->CanAllocate());

	// 見つからなかったらデフォルトを返す（null 相当より安全）
	return defaultTextureIndex_;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath)
{
	std::string resolved = ResourceLocator::Resolve(filePath, ResourceLocator::AssetType::Texture);
	if (resolved.empty()) resolved = filePath;

	assert(textureDatas.contains(resolved));
	TextureData& textureData = textureDatas[resolved];

	return textureData.srvHandleGPU;
}

std::vector<D3D12_SUBRESOURCE_DATA> TextureManager::CreateSubresources(const DirectX::ScratchImage& mipImages)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;


	const DirectX::Image* images = mipImages.GetImages();


	size_t numImages = mipImages.GetImageCount();

	subresources.resize(numImages);

	for (size_t i = 0; i < numImages; ++i)
	{
		const DirectX::Image& img = images[i];

		D3D12_SUBRESOURCE_DATA& sub = subresources[i];
		sub.pData = img.pixels;
		sub.RowPitch = img.rowPitch;
		sub.SlicePitch = img.slicePitch;
	}

	return subresources;
}

const DirectX::TexMetadata& TextureManager::GetMetaData(uint32_t TextureIndex)
{
	// 生インデックス（0始まり）でチェック
	assert(TextureIndex < textureIndexToFilePath.size());

	// インデックス -> ファイルパス経由で参照
	const std::string& filePath = textureIndexToFilePath[TextureIndex];
	auto it = textureDatas.find(filePath);
	assert(it != textureDatas.end());

	// メタデータを返す
	return it->second.metadata;
}

uint32_t TextureManager::GetSrvIndex(const std::string& filePath)
{
	std::string resolved = ResourceLocator::Resolve(filePath, ResourceLocator::AssetType::Texture);
	if (resolved.empty()) resolved = filePath;

	if (textureDatas.contains(resolved))
	{
		TextureData& textureData = textureDatas[resolved];
		return textureData.srvIndex;
	}

	assert(srvManager->CanAllocate());

	return defaultTextureIndex_;
}