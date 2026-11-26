#pragma once
#include "KHEngine/Graphics/PostProcess/IPostProcess.h"
#include "externals/imgui/imgui.h"

class SepiaEffect : public IPostProcess {
public:
    SepiaEffect(PostProcessData* data) : IPostProcess("Sepia"), data_(data) {}

    void Initialize() override {
        data_->sepiaStrength = 0.8f; // 初期値
    }

    void ApplyToData(PostProcessData& data) const override {
        data.enableSepia = 1;
    }

    void SaveToJson(nlohmann::json& effectJson) const override {
        effectJson["strength"] = data_->sepiaStrength;
    }

    void LoadFromJson(const nlohmann::json& effectJson) override {
        if (effectJson.contains("strength")) data_->sepiaStrength = effectJson["strength"];
    }

    void DrawImGui() override {
#ifdef USE_IMGUI
        ImGui::SliderFloat("Sepia Strength", &data_->sepiaStrength, 0.0f, 1.0f);
#endif
    }

private:
    PostProcessData* data_;
};
