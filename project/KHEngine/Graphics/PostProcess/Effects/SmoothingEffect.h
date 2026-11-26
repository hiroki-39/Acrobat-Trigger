#pragma once
#include "KHEngine/Graphics/PostProcess/IPostProcess.h"
#include "externals/imgui/imgui.h"

class SmoothingEffect : public IPostProcess {
public:
    SmoothingEffect(PostProcessData* data) : IPostProcess("Smoothing"), data_(data) {}

    void Initialize() override {
        data_->smoothingKernelSize = 2.0f;
    }

    void ApplyToData(PostProcessData& data) const override {
        data.enableSmoothing = 1;
    }

    void SaveToJson(nlohmann::json& effectJson) const override {
        effectJson["kernelSize"] = data_->smoothingKernelSize;
    }

    void LoadFromJson(const nlohmann::json& effectJson) override {
        if (effectJson.contains("kernelSize")) data_->smoothingKernelSize = effectJson["kernelSize"];
    }

    void DrawImGui() override {
#ifdef USE_IMGUI
        ImGui::SliderFloat("Kernel Size", &data_->smoothingKernelSize, 1.0f, 10.0f);
#endif
    }

private:
    PostProcessData* data_;
};
