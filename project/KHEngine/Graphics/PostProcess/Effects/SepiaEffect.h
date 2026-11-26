#pragma once
#include "KHEngine/Graphics/PostProcess/IPostProcess.h"
#include "externals/imgui/imgui.h"

class SepiaEffect : public IPostProcess {
public:
    SepiaEffect(PostProcessData* data) : IPostProcess("Sepia"), data_(data) {}

    void Initialize() override {
        data_->sepiaStrength = 0.8f; // 初期値
    }

    void DrawImGui() override {
#ifdef USE_IMGUI
        ImGui::SliderFloat("Sepia Strength", &data_->sepiaStrength, 0.0f, 1.0f);
#endif
    }

private:
    PostProcessData* data_;
};
