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

    void DrawImGui() override {
#ifdef USE_IMGUI
        ImGui::SliderFloat("Glitch Strength", &data_->glitchStrength, 0.0f, 0.2f);
        ImGui::SliderFloat("Noise Strength (Sandstorm)", &data_->noiseStrength, 0.0f, 1.0f);
#endif
    }

private:
    PostProcessData* data_;
};
