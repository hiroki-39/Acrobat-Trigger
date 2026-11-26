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
