#pragma once
#include "KHEngine/Graphics/PostProcess/IPostProcess.h"
#include "externals/imgui/imgui.h"

class OutlineEffect : public IPostProcess {
public:
    OutlineEffect(PostProcessData* data) : IPostProcess("Outline"), data_(data) {}

    void Initialize() override {
        data_->outlineThreshold = 0.5f;
    }

    void ApplyToData(PostProcessData& data) const override {
        data.enableOutline = 1;
    }

    void SaveToJson(nlohmann::json& effectJson) const override {
        effectJson["threshold"] = data_->outlineThreshold;
    }

    void LoadFromJson(const nlohmann::json& effectJson) override {
        if (effectJson.contains("threshold")) data_->outlineThreshold = effectJson["threshold"];
    }

    void DrawImGui() override {
#ifdef USE_IMGUI
        ImGui::SliderFloat("Threshold", &data_->outlineThreshold, 0.0f, 2.0f);
#endif
    }

private:
    PostProcessData* data_;
};
