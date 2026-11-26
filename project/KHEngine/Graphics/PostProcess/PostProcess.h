#pragma once
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Graphics/PostProcess/IPostProcess.h"
#include <vector>
#include <memory>

class PostProcess
{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DirectXCommon* dxcommon);

	/// <summary>
	/// 描画
	/// </summary>
	void Draw(bool drawToSwapchain = false);
	void PostDraw(bool drawToSwapchain = false);

	/// <summary>
	/// エディタ(ImGui)での描画
	/// </summary>
	void DrawImGui();
	void DrawImGuiContent();

	/// <summary>
	/// 設定の保存と読み込み（今回は枠組みだけ）
	/// </summary>
	void SaveToJson();
	void LoadFromJson();

	/// <summary>
	/// SRVインデックスの取得
	/// </summary>
	uint32_t GetSrvIndex() const { return srvIndex_; }

	/// <summary>
	/// 結果出力用SRVインデックスの取得
	/// </summary>
	uint32_t GetResultSrvIndex() const { return resultSrvIndex_; }

	/// <summary>
	/// 指定した名前のエフェクトのON/OFFを切り替える
	/// </summary>
	void SetEffectActive(const std::string& name, bool isActive) {
		for (auto& effect : effects_) {
			if (effect->GetName() == name) {
				effect->SetActive(isActive);
				break;
			}
		}
	}

	/// <summary>
	/// エフェクトのデータを取得（パラメータ変更用）
	/// </summary>
	PostProcessData* GetData() { return &data_; }

private:
	/// <summary>
	/// ルートシグネチャの作成
	/// </summary>
	void CreateRootSignature();

	/// <summary>
	/// グラフィックスパイプライン生成
	/// </summary>
	void CreateGraphicsPipeline();

private:
	DirectXCommon* dxCommon_;

	//ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;

	//グラフィックスパイプラインステート
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState;

	//SRVインデックス
	uint32_t srvIndex_;
	uint32_t resultSrvIndex_;

	// エフェクトリスト (マネージャーとしての機能)
	std::vector<std::unique_ptr<IPostProcess>> effects_;

	// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;
	PostProcessData* constMap_ = nullptr;
	PostProcessData data_{}; // CPU側データ
};