#pragma once
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Graphics/Resource/Descriptor/SrvManager.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Graphics/3d/Object/Object3dCommon.h"
#include "Game/Actor/Enemy/EnemyPresetManager.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include <memory>
#include <string>
#include <vector>

/**
 * @brief ゲーム画面から独立した敵オーサリング専用スタジオ
 */
class EnemyStudio {
public:
    enum class EditorMode {
        NORMAL_ENEMY, // 通常敵エディタ
        BOSS_TRAIN    // 装甲列車ボスエディタ
    };

    static EnemyStudio* GetInstance();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, Object3dCommon* object3dCommon);
    void Update(float dt);
    void Render();

    // ImGui ウィンドウ描画
    void DrawViewportWindow(); // エネミー画面（3Dビューポート）
    void DrawControlWindow();  // エネミーエディター（設定・ファイル管理）

    // 表示フラグ
    bool& GetShowViewport() { return showViewport_; }
    bool& GetShowEditor() { return showEditor_; }
    EditorMode GetEditorMode() const { return editorMode_; }
    void SetEditorMode(EditorMode mode) { editorMode_ = mode; }

private:
    EnemyStudio() = default;
    ~EnemyStudio() = default;
    EnemyStudio(const EnemyStudio&) = delete;
    EnemyStudio& operator=(const EnemyStudio&) = delete;

    void CreateRenderTargets();
    void HandleCameraInput(float contentWidth, float contentHeight);
    void RefreshPreviewObjects();

    // グリッド描画ヘルパー
    void DrawGridOverlay(const struct ImVec2& imagePos, const struct ImVec2& imageSize);
    void Draw3DLine(struct ImDrawList* drawList, const Vector3& start, const Vector3& end,
                    const Matrix4x4& vpMat, const struct ImVec2& minPos, const struct ImVec2& size,
                    uint32_t color, float thickness = 1.0f);

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;

    // オフスクリーン描画用リソース (1280 x 720)
    const uint32_t kWidth_ = 1280;
    const uint32_t kHeight_ = 720;

    Microsoft::WRL::ComPtr<ID3D12Resource> renderTextureResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
    uint32_t srvIndex_ = 0;
    D3D12_RESOURCE_STATES currentResourceState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};

    // プレビュー用カメラ
    Camera camera_;
    Vector3 cameraTarget_ = { 0.0f, 0.0f, 0.0f };
    float cameraDistance_ = 18.0f;
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 0.35f;

    // プレビュー用3Dオブジェクト
    std::unique_ptr<Object3d> playerObj_;
    std::vector<std::unique_ptr<Object3d>> enemyObjs_;
    std::unique_ptr<Object3d> colliderSphereObj_;
    std::unique_ptr<Object3d> colliderBoxObj_;

    // 表示設定
    Vector4 backgroundColor_ = { 0.10f, 0.12f, 0.16f, 1.0f };
    bool showPlayer_ = true;
    Vector3 playerPosition_ = { 0.0f, 0.0f, -15.0f }; // 敵の手前に配置
    bool showCollider_ = false; // 当たり判定ブロック（デフォルト非表示）
    bool showFormation_ = true;
    bool showGrid_ = true;
    float gridSize_ = 30.0f;
    float gridSpacing_ = 2.0f;
    bool showAxisGizmo_ = true;
    bool showViewport_ = true;
    bool showEditor_ = true;
    bool playPreviewAnim_ = true; // 行動パターンのプレビューアニメーション再生
    float previewAnimTimer_ = 0.0f;

    // 編集ステータス
    EditorMode editorMode_ = EditorMode::NORMAL_ENEMY;
    std::string currentPresetKey_ = "RUSHER";
    char newPresetKeyInput_[64] = "NEW_ENEMY";
    std::string statusMessage_ = "";
    float statusMessageTimer_ = 0.0f;

    // 装甲列車ボスプレビュー
    std::unique_ptr<ArmoredTrainBoss> previewBoss_;
    int selectedCarriageIndex_ = 1; // 選択中車両 (0〜3)
    int selectedTurretIndex_ = -1;
    int selectedMissilePodIndex_ = -1;
    void DrawBossControlUI();

    // 利用可能なモデル一覧
    std::vector<std::string> availableModels_;
    void RefreshAvailableModels();
};
