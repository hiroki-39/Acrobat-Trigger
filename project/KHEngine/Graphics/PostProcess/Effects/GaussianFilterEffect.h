#pragma once
#include "KHEngine/Graphics/PostProcess/IPostProcess.h"
#include "externals/imgui/imgui.h"

class GaussianFilterEffect : public IPostProcess {
public:
    GaussianFilterEffect(PostProcessData* data) : IPostProcess("GaussianFilter"), data_(data) {}

    void Initialize() override {
        data_->gaussianSigma = 2.0f;
    }

    void DrawImGui() override {
#ifdef USE_IMGUI
        ImGui::SliderFloat("Sigma (Blur Strength)", &data_->gaussianSigma, 0.1f, 10.0f);
#endif
    }

private:
    PostProcessData* data_;
};
