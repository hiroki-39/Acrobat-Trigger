#include "EditorSystem.h"
#include "EffectStudio.h"
#include "EnemyStudio.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#include "KHEngine/Graphics/Resource/Descriptor/SrvManager.h"
#include "KHEngine/Graphics/3d/Particle/ParticleManager.h"
#include "KHEngine/UI/UITextManager.h"
#include "KHEngine/Core/Services/EngineServices.h"

EditorSystem* EditorSystem::GetInstance() {
    static EditorSystem instance;
    return &instance;
}

void EditorSystem::Initialize(DirectXCommon* dxCommon) {
    dxCommon_ = dxCommon;
    UITextManager::GetInstance()->Initialize();
}

void EditorSystem::Draw(uint32_t gameSceneSrvIndex) {
#ifdef USE_IMGUI
    // 1. メインのDockSpaceを作成
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("DockSpace Demo", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");

        // DockSpaceを配置
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        static bool isFirstFrame = true;
        // 明示的なリセット要求、またはiniにドッキング情報が存在しない初回起動時のみ初期構築
        if (resetLayout_ || (isFirstFrame && ImGui::DockBuilderGetNode(dockspace_id) == nullptr)) {
            resetLayout_ = false;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            ImGuiID dock_main_id = dockspace_id;
            // 左側に「メニュー」領域 (20%)
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
            // 右側に「インスペクター」領域 (28%)
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.28f, nullptr, &dock_main_id);
            // 下部に「タイムライン」領域 (26%)
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.26f, nullptr, &dock_main_id);
            // 残った中央上が「ゲーム画面」

            // 各ウィンドウを対応するドックに割り当てる
            ImGui::DockBuilderDockWindow("メニュー", dock_id_left);
            ImGui::DockBuilderDockWindow("インスペクター", dock_id_right);
            ImGui::DockBuilderDockWindow("エフェクトエディター", dock_id_right);
            ImGui::DockBuilderDockWindow("エネミーエディター", dock_id_right);
            ImGui::DockBuilderDockWindow("UIテキストエディタ", dock_id_right);
            ImGui::DockBuilderDockWindow("タイムライン", dock_id_bottom);
            ImGui::DockBuilderDockWindow("ゲーム画面", dock_main_id);
            ImGui::DockBuilderDockWindow("エフェクト画面", dock_main_id);
            ImGui::DockBuilderDockWindow("エネミー画面", dock_main_id);

            // 構築完了
            ImGui::DockBuilderFinish(dockspace_id);
        }
        isFirstFrame = false;
    }

    DrawMenuBar();

    ImGui::End(); // DockSpace Demo

    // 各ウィンドウの描画
    DrawViewport(gameSceneSrvIndex);

    // -------------------------------------------------------------------------
    // ドッキングタブ順序の永続固定保証:
    // シーン遷移（タイトル⇔ゲームプレイ等）の際、シーン破棄により「インスペクター」が
    // 1フレームでも呼ばれなくなると、再生成時にタブ末尾（一番右）へ再配置されてしまう。
    // ここでエフェクトエディター等より前に必ずインスペクターを先行登録しておくことで、
    // シーンを何往復またいでも「インスペクター」が常に一番左（先頭タブ）に永続固定される。
    // （実際のUIコンテンツは各シーンの DrawUI() 内で追記される）
    // -------------------------------------------------------------------------
    if (ImGui::Begin("インスペクター")) {
    }
    ImGui::End();

    if (ImGui::Begin("メニュー")) {
    }
    ImGui::End();

    if (ImGui::Begin("タイムライン")) {
    }
    ImGui::End();
    
    // エフェクトスタジオのウィンドウ描画
    EffectStudio::GetInstance()->DrawViewportWindow();
    EffectStudio::GetInstance()->DrawControlWindow();

    // エネミースタジオのウィンドウ描画
    EnemyStudio::GetInstance()->DrawViewportWindow();
    EnemyStudio::GetInstance()->DrawControlWindow();

    if (showParticleEditor_) {
        DrawParticleEditor();
    }

    if (showUITextEditor_) {
        if (ImGui::Begin("UIテキストエディタ", &showUITextEditor_)) {
            UITextManager::GetInstance()->DrawImGuiEditor();
        }
        ImGui::End();
    }
#endif
}

void EditorSystem::DrawMenuBar() {
#ifdef USE_IMGUI
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("ウィンドウ")) {
            if (ImGui::MenuItem("現在のレイアウトを保存 (Save Layout)")) {
                ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
            }
            if (ImGui::MenuItem("デフォルトレイアウトに復元")) {
                resetLayout_ = true;
            }
            ImGui::Separator();
            ImGui::MenuItem("UIテキストエディタ", nullptr, &showUITextEditor_);
            ImGui::MenuItem("エフェクト画面 (ビューポート)", nullptr, &EffectStudio::GetInstance()->GetShowViewport());
            ImGui::MenuItem("エフェクトエディター", nullptr, &EffectStudio::GetInstance()->GetShowEditor());
            ImGui::Separator();
            ImGui::MenuItem("エネミー画面 (ビューポート)", nullptr, &EnemyStudio::GetInstance()->GetShowViewport());
            ImGui::MenuItem("エネミーエディター", nullptr, &EnemyStudio::GetInstance()->GetShowEditor());
            ImGui::EndMenu();
        }

        // --- 全シーン共通 ゲーム再生制御 (Play / Pause) ---
        auto services = EngineServices::GetInstance();
        bool isPlaying = services->IsGamePlaying();

        // メニューバー中央付近に配置
        float menuBarWidth = ImGui::GetWindowWidth();
        float buttonAreaWidth = 260.0f;
        float spacing = (menuBarWidth - buttonAreaWidth) * 0.5f - 80.0f;
        if (spacing > 10.0f) {
            ImGui::SameLine(spacing);
        } else {
            ImGui::SameLine();
        }

        if (isPlaying) {
            // 再生中: 一時停止ボタンを表示
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.55f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.65f, 0.25f, 1.0f));
            if (ImGui::Button("一時停止 (Pause)", ImVec2(140, 0))) {
                services->SetGamePlaying(false);
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "● プレイ中 (PLAYING)");
        } else {
            // 停止中: 再生ボタンを表示
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.68f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.82f, 0.38f, 1.0f));
            if (ImGui::Button("ゲーム再生 (Play)", ImVec2(140, 0))) {
                services->SetGamePlaying(true);
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "■ 停止中 (BGM/SE待機)");
        }

        ImGui::EndMenuBar();
    }
#endif
}

namespace {
    void DrawPlayModeBadgeInViewport(ImDrawList* drawList, ImVec2 pos)
    {
        auto services = EngineServices::GetInstance();
        if (!drawList || !services) return;

        bool isDemo = services->IsDemoMode();
        const char* badgeText = isDemo ? "DEMO PLAY MODE | Press [1] to Play" : "PLAY MODE (MANUAL) | Press [1] for Demo";

        ImVec2 textSize = ImGui::CalcTextSize(badgeText);
        float padX = 10.0f;
        float padY = 4.0f;
        float dotRadius = 3.5f;
        float dotSpacing = 10.0f;

        float badgeWidth = textSize.x + padX * 2.0f + dotSpacing;
        float badgeHeight = textSize.y + padY * 2.0f;
        float badgeX = pos.x;
        float badgeY = pos.y;

        static float pulseTimer = 0.0f;
        float dt = services->GetDeltaTime();
        pulseTimer += dt * 3.5f;
        float pulse = (sinf(pulseTimer) * 0.5f + 0.5f);

        if (isDemo)
        {
            // Demo play mode badge (cyber blue neon)
            drawList->AddRectFilled(ImVec2(badgeX - 1.0f, badgeY - 1.0f), ImVec2(badgeX + badgeWidth + 1.0f, badgeY + badgeHeight + 1.0f), IM_COL32(0, 180, 255, static_cast<int>(30 + pulse * 45)), 6.0f);
            drawList->AddRectFilled(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeWidth, badgeY + badgeHeight), IM_COL32(6, 16, 28, 235), 6.0f);
            drawList->AddRect(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeWidth, badgeY + badgeHeight), IM_COL32(0, 220, 255, static_cast<int>(200 + pulse * 55)), 6.0f, 0, 1.5f);

            float dotCenterX = badgeX + padX + dotRadius;
            float dotCenterY = badgeY + badgeHeight * 0.5f;
            drawList->AddCircleFilled(ImVec2(dotCenterX, dotCenterY), dotRadius, IM_COL32(0, 240, 255, static_cast<int>(180 + pulse * 75)));
            drawList->AddCircle(ImVec2(dotCenterX, dotCenterY), dotRadius + 1.2f, IM_COL32(0, 200, 255, static_cast<int>(100 + pulse * 100)), 0, 1.0f);

            ImVec2 textPos(dotCenterX + dotRadius + 6.0f, badgeY + padY);
            drawList->AddText(textPos, IM_COL32(0, 240, 255, 255), badgeText);
        }
        else
        {
            // Manual play mode badge (emerald green)
            drawList->AddRectFilled(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeWidth, badgeY + badgeHeight), IM_COL32(6, 22, 14, 210), 6.0f);
            drawList->AddRect(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeWidth, badgeY + badgeHeight), IM_COL32(50, 220, 130, 200), 6.0f, 0, 1.2f);

            float dotCenterX = badgeX + padX + dotRadius;
            float dotCenterY = badgeY + badgeHeight * 0.5f;
            drawList->AddCircleFilled(ImVec2(dotCenterX, dotCenterY), dotRadius, IM_COL32(60, 240, 140, 240));

            ImVec2 textPos(dotCenterX + dotRadius + 6.0f, badgeY + padY);
            drawList->AddText(textPos, IM_COL32(80, 255, 160, 240), badgeText);
        }
    }
}

void EditorSystem::DrawViewport(uint32_t srvIndex) {
#ifdef USE_IMGUI
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("ゲーム画面");
    {
        // ウィンドウのサイズに合わせて画像を表示
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        
        // SrvManagerからGPUハンドルを取得
        SrvManager* srvManager = SrvManager::GetInstance();
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = srvManager->GetSRVGPUDescriptorHandle(srvIndex);

        // 画像を表示 (UVを上下反転させる必要がある場合があります)
        ImVec2 imagePos = ImGui::GetCursorScreenPos();
        viewportPos_ = imagePos;
        viewportSize_ = contentSize;
        ImGui::Image((ImTextureID)srvHandle.ptr, contentSize);

        // ゲーム画面上にUIテキストを描画 (1280x720 基準の座標・スケーリング)
                UITextManager::GetInstance()->Draw(ImGui::GetWindowDrawList(), imagePos, contentSize);

        // ゲーム画面ビューポート内にプレイモードバッジを描画
        DrawPlayModeBadgeInViewport(ImGui::GetWindowDrawList(), ImVec2(imagePos.x + 14.0f, imagePos.y + 14.0f));
    }
    ImGui::End();
    ImGui::PopStyleVar();
#endif
}

void EditorSystem::DrawParticleEditor() {
#ifdef USE_IMGUI
    ImGui::Begin("パーティクルエディタ");
    
    ParticleManager* manager = ParticleManager::GetInstance();
    auto& emitters = manager->GetEmitters();

    static int selectedEmitter = -1;
    
    // エミッターリスト
    ImGui::BeginChild("EmitterList", ImVec2(150, 0), true);
    for (int i = 0; i < (int)emitters.size(); ++i) {
        std::string label = "エミッター " + std::to_string(i);
        if (ImGui::Selectable(label.c_str(), selectedEmitter == i)) {
            selectedEmitter = i;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // パラメータ編集
    ImGui::BeginChild("Parameters");
    if (selectedEmitter >= 0 && selectedEmitter < (int)emitters.size()) {
        auto& emitter = emitters[selectedEmitter];
        auto param = emitter->GetParameter();

        bool changed = false;
        if (ImGui::CollapsingHeader("基本設定", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::DragInt("発生数", (int*)&param.count, 1, 1, 100);
            changed |= ImGui::DragFloat("発生頻度", &param.frequency, 0.01f, 0.0f, 10.0f);
        }

        if (ImGui::CollapsingHeader("寿命 (LifeTime)", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::DragFloat("最小寿命", &param.minLifeTime, 0.1f, 0.0f, 10.0f);
            changed |= ImGui::DragFloat("最大寿命", &param.maxLifeTime, 0.1f, 0.0f, 10.0f);
        }

        if (ImGui::CollapsingHeader("スケール", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::DragFloat3("最小スケール", &param.minScale.x, 0.1f);
            changed |= ImGui::DragFloat3("最大スケール", &param.maxScale.x, 0.1f);
        }

        if (ImGui::CollapsingHeader("速度 (Velocity)", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::DragFloat3("最小速度", &param.minVelocity.x, 0.1f);
            changed |= ImGui::DragFloat3("最大速度", &param.maxVelocity.x, 0.1f);
        }

        if (ImGui::CollapsingHeader("色 (Color)", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::ColorEdit4("開始色 (最小)", &param.minColor.x);
            changed |= ImGui::ColorEdit4("開始色 (最大)", &param.maxColor.x);
        }

        if (ImGui::CollapsingHeader("時間変化 (Over Lifetime)", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::Checkbox("時間で色を変化させる", &param.isColorOverLifetime);
            if (param.isColorOverLifetime) {
                changed |= ImGui::ColorEdit4("終了時の色", &param.endColor.x);
            }
            
            changed |= ImGui::Checkbox("時間でスケールを変化させる", &param.isScaleOverLifetime);
            if (param.isScaleOverLifetime) {
                changed |= ImGui::DragFloat3("終了時のスケール", &param.endScale.x, 0.1f);
            }
        }

        if (ImGui::CollapsingHeader("物理挙動 (Physics)", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::Checkbox("重力を有効化", &param.useGravity);
            if (param.useGravity) {
                changed |= ImGui::DragFloat("重力の強さ (Y方向)", &param.gravity, 0.1f);
            }
            changed |= ImGui::DragFloat("空気抵抗 (Drag)", &param.drag, 0.01f, 0.0f, 10.0f);
        }

        if (changed) {
            emitter->SetParameter(param);
        }
    } else {
        ImGui::Text("編集するエミッターを選択してください");
    }
    ImGui::EndChild();

    ImGui::End();
#endif
}

void EditorSystem::DrawPerformance() {
#ifdef USE_IMGUI
    ImGui::Begin("パフォーマンス");

    ImGuiIO& io = ImGui::GetIO();
    float fps = io.Framerate;
    float frameTime = 1000.0f / (fps > 0.0f ? fps : 1.0f);

    ImGui::Text("フレームレート (FPS): %.1f", fps);
    ImGui::Text("処理時間 (ms/frame): %.3f", frameTime);

    ImGui::Separator();

    // 簡易的なFPSの推移グラフ
    static float fps_values[90] = {};
    static int values_offset = 0;
    
    // 値を更新
    fps_values[values_offset] = fps;
    values_offset = (values_offset + 1) % IM_ARRAYSIZE(fps_values);

    ImGui::PlotLines("FPS 履歴", fps_values, IM_ARRAYSIZE(fps_values), values_offset, nullptr, 0.0f, 120.0f, ImVec2(0, 80));

    // もし将来的にGPUやCPUの正確な処理時間を計測するTimerクラスなどがあれば、
    // ここに ImGui::Text("GPU Draw: %.3f ms", gpuTime); のように追加できます。

    ImGui::End();
#endif
}

