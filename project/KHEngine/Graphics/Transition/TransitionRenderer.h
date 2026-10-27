#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>
#include "KHEngine/Math/Vector2.h"
#include "KHEngine/Math/Vector4.h"

class DirectXCommon;

// 画面遷移演出の種類
enum class TransitionType
{
	SquareGrid = 0, // 四角形サイバーグリッド（すっきりフラットタイル）
	Hexagon = 1,    // ヘキサゴン・トランジション（六角形波紋伝播＆ネオン発光）
	CRT_Off = 2     // CRT TV 電源オフ演出（上下圧縮 -> 左右圧縮 -> 点消失）
};

class TransitionRenderer
{
public:
	// 定数バッファ用構造体 (16バイトアライメント)
	struct ConstBufferData
	{
		Vector4 fadeColor;      // トランジション色 (16 bytes)
		float progress;         // 進行度 0.0 - 1.0 (4 bytes)
		float edgeSoftness;     // 境界ぼかし幅 (4 bytes)
		int32_t isOpening;      // 0 = 閉じる(イン), 1 = 開く(アウト) (4 bytes)
		int32_t transitionType; // 0 = SquareGrid, 1 = Hexagon (4 bytes)
		Vector4 edgeColor;      // 境界ハイライト色 / ネオン発光色 (16 bytes)
		Vector2 originCoord;    // 波紋の起点 (0.5, 0.0) (8 bytes)
		float hexTileScale;     // 六角形タイルの密度 (36.0) (4 bytes)
		float borderWidth;      // 六角形輪郭線の太さ (0.08) (4 bytes)
		float glitchIntensity;  // ピーク時の明滅・乱数幅 (0.25) (4 bytes)
		float padding0;         // パディング (4 bytes)
		float padding1;         // パディング (4 bytes)
		float padding2;         // パディング (4 bytes)
	};

	TransitionRenderer() = default;
	~TransitionRenderer() = default;

	void Initialize(DirectXCommon* dxCommon);
	void Draw(uint32_t ruleTextureIndex, float progress, bool isOpening,
		TransitionType type = TransitionType::SquareGrid,
		const Vector4& fadeColor = { 0.010f, 0.040f, 0.075f, 1.0f },
		float edgeSoftness = 0.06f,
		const Vector4& edgeColor = { 0.15f, 1.0f, 0.88f, 1.0f },
		const Vector2& originCoord = { 0.5f, 0.5f },
		float hexTileScale = 14.0f,
		float borderWidth = 0.08f,
		float glitchIntensity = 0.25f);

private:
	void CreateRootSignature();
	void CreateGraphicsPipeline();

private:
	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;
	ConstBufferData* constMap_ = nullptr;
};
