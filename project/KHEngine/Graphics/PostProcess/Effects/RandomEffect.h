#pragma once
#include "KHEngine/Graphics/PostProcess/IPostProcess.h"
#include "externals/imgui/imgui.h"

class RandomEffect : public IPostProcess {
public:
    RandomEffect(PostProcessData* data) : IPostProcess("Random(Glitch)"), data_(data) {}

    void Initialize() override {
        data_->glitchStrength = 0.05f;
        data_->noiseStrength = 0.1f;
    }

    void ApplyToData(PostProcessData& data) const override {
        data.enableRandom = 1;
    }

    void SaveToJson(nlohmann::json& effectJson) const override {
        effectJson["glitchStrength"] = data_->glitchStrength;
        effectJson["noiseStrength"] = data_->noiseStrength;
    }

    void LoadFromJson(const nlohmann::json& effectJson) override {
        if (effectJson.contains("glitchStrength")) data_->glitchStrength = effectJson["glitchStrength"];
        if (effectJson.contains("noiseStrength")) data_->noiseStrength = effectJson["noiseStrength"];
    }

    void DrawImGui() override {
#ifdef USE_IMGUI
        ImGui::SliderFloat("Glitch Strength", &data_->glitchStrength, 0.0f, 0.2f);
        ImGui::SliderFloat("Noise Strength (Sandstorm)", &data_->noiseStrength, 0.0f, 1.0f);
#endif
    }

private:
    PostProcessData* data_;
};
