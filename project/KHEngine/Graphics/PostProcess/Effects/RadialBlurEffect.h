#pragma once
#include "KHEngine/Graphics/PostProcess/IPostProcess.h"
#include "externals/imgui/imgui.h"

class RadialBlurEffect : public IPostProcess {
public:
    RadialBlurEffect(PostProcessData* data) : IPostProcess("RadialBlur"), data_(data) {}

    void Initialize() override {
        data_->radialBlurCenterX = 0.5f;
        data_->radialBlurCenterY = 0.5f;
        data_->radialBlurIntensity = 0.02f;
    }

    void ApplyToData(PostProcessData& data) const override {
        data.enableRadialBlur = 1;
    }

    void SaveToJson(nlohmann::json& effectJson) const override {
        effectJson["centerX"] = data_->radialBlurCenterX;
        effectJson["centerY"] = data_->radialBlurCenterY;
        effectJson["intensity"] = data_->radialBlurIntensity;
    }

    void LoadFromJson(const nlohmann::json& effectJson) override {
        if (effectJson.contains("centerX")) data_->radialBlurCenterX = effectJson["centerX"];
        if (effectJson.contains("centerY")) data_->radialBlurCenterY = effectJson["centerY"];
        if (effectJson.contains("intensity")) data_->radialBlurIntensity = effectJson["intensity"];
    }

    void DrawImGui() override {
#ifdef USE_IMGUI
        ImGui::SliderFloat("Center X", &data_->radialBlurCenterX, 0.0f, 1.0f);
        ImGui::SliderFloat("Center Y", &data_->radialBlurCenterY, 0.0f, 1.0f);
        ImGui::SliderFloat("Intensity", &data_->radialBlurIntensity, 0.0f, 0.1f);
#endif
    }

private:
    PostProcessData* data_;
};
