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

    void DrawImGui() override {
#ifdef USE_IMGUI
        ImGui::SliderFloat("Intensity", &data_->vignetteIntensity, 0.0f, 5.0f);
        ImGui::SliderFloat("Power", &data_->vignettePower, 0.1f, 5.0f);
#endif
    }

private:
    PostProcessData* data_;
};
