#pragma once
#include "KHEngine/Graphics/PostProcess/IPostProcess.h"
#include "externals/imgui/imgui.h"

class GrayscaleEffect : public IPostProcess {
public:
    GrayscaleEffect(PostProcessData* data) : IPostProcess("Grayscale"), data_(data) {}

    void Initialize() override {
        // 初期化処理があればここに
    }

    void ApplyToData(PostProcessData& data) const override {
        data.enableGrayscale = 1;
    }

    void DrawImGui() override {
#ifdef USE_IMGUI
        // グレースケールは特に調整パラメータがないのでテキストのみ
        ImGui::Text("Grayscale is Active.");
#endif
    }

private:
    PostProcessData* data_;
};
