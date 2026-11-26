#pragma once
#include "KHEngine/Graphics/PostProcess/IPostProcess.h"
#include "externals/imgui/imgui.h"

class DissolveEffect : public IPostProcess {
public:
    DissolveEffect(PostProcessData* data) : IPostProcess("Dissolve"), data_(data) {}

    void Initialize() override {
        data_->dissolveThreshold = 0.5f;
        data_->dissolveEdgeWidth = 0.05f;
        data_->dissolveEdgeColor[0] = 0.0f;
        data_->dissolveEdgeColor[1] = 1.0f;
        data_->dissolveEdgeColor[2] = 0.0f;
    }

    void ApplyToData(PostProcessData& data) const override {
        data.enableDissolve = 1;
    }

    void SaveToJson(nlohmann::json& effectJson) const override {
        effectJson["threshold"] = data_->dissolveThreshold;
        effectJson["edgeWidth"] = data_->dissolveEdgeWidth;
        effectJson["edgeColor"] = { data_->dissolveEdgeColor[0], data_->dissolveEdgeColor[1], data_->dissolveEdgeColor[2] };
    }

    void LoadFromJson(const nlohmann::json& effectJson) override {
        if (effectJson.contains("threshold")) data_->dissolveThreshold = effectJson["threshold"];
        if (effectJson.contains("edgeWidth")) data_->dissolveEdgeWidth = effectJson["edgeWidth"];
        if (effectJson.contains("edgeColor")) {
            data_->dissolveEdgeColor[0] = effectJson["edgeColor"][0];
            data_->dissolveEdgeColor[1] = effectJson["edgeColor"][1];
            data_->dissolveEdgeColor[2] = effectJson["edgeColor"][2];
        }
    }

    void DrawImGui() override {
#ifdef USE_IMGUI
        ImGui::SliderFloat("Threshold", &data_->dissolveThreshold, 0.0f, 1.0f);
        ImGui::SliderFloat("Edge Width", &data_->dissolveEdgeWidth, 0.0f, 0.2f);
        ImGui::ColorEdit3("Edge Color", data_->dissolveEdgeColor);
#endif
    }

private:
    PostProcessData* data_;
};
