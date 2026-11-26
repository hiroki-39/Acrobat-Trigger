#pragma once
#include "KHEngine/Graphics/PostProcess/IPostProcess.h"
#include "externals/imgui/imgui.h"

class VignettingEffect : public IPostProcess {
public:
    VignettingEffect(PostProcessData* data) : IPostProcess("Vignetting"), data_(data) {}

    void Initialize() override {
        data_->vignetteIntensity = 1.0f;
        data_->vignettePower = 1.5f;
    }

    void ApplyToData(PostProcessData& data) const override {
        data.enableVignette = 1;
    }

    void SaveToJson(nlohmann::json& effectJson) const override {
        effectJson["intensity"] = data_->vignetteIntensity;
        effectJson["power"] = data_->vignettePower;
    }

    void LoadFromJson(const nlohmann::json& effectJson) override {
        if (effectJson.contains("intensity")) data_->vignetteIntensity = effectJson["intensity"];
        if (effectJson.contains("power")) data_->vignettePower = effectJson["power"];
    }

    void DrawImGui() override {
#ifdef USE_IMGUI
        ImGui::SliderFloat("Intensity", &data_->vignetteIntensity, 0.0f, 5.0f);
        ImGui::SliderFloat("Power", &data_->vignettePower, 0.1f, 5.0f);
#endif
    }

private:
    PostProcessData* data_;
};
