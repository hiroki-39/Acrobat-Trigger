#pragma once
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Graphics/3d/Object/Object3dCommon.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"
#include "KHEngine/Math/CollisionMath.h"
#include <vector>
#include <memory>

class TerrainCollisionDebugger {
public:
    static TerrainCollisionDebugger* GetInstance();

    void Initialize(DirectXCommon* dxCommon, Object3dCommon* object3dCommon);
    
    // 毎フレームの判定前に呼び出し
    void BeginFrame();

    // 地形オブジェクトごとの判定結果を記録
    void RecordCollisionCheck(
        int modelTotalTriangles,
        const std::vector<Triangle>& testedTriangles,
        const std::vector<Triangle>& hitTriangles,
        const CollisionResult* result
    );

    // 毎フレームの判定終了後に呼び出し
    void EndFrame();

    // 3Dデバッグ描画（黄色/赤色の三角形、接触点マーカー、押し戻し法線ベクトル）
    void Draw(Camera* camera);

    // ImGuiリアルタイムモニター表示
    void DrawImGui();

    // ゲッター・セッター
    bool IsHit() const { return isHitThisFrame_; }
    bool IsDebugEnabled() const { return isDebugEnabled_; }
    void SetDebugEnabled(bool enable) { isDebugEnabled_ = enable; }
    void ToggleDebugEnabled() { isDebugEnabled_ = !isDebugEnabled_; }

    bool IsShowImGuiMonitor() const { return showImGuiMonitor_; }
    void SetShowImGuiMonitor(bool show) { showImGuiMonitor_ = show; }
    void ToggleImGuiMonitor() { showImGuiMonitor_ = !showImGuiMonitor_; }

    const CollisionResult& GetLastHitResult() const { return lastHitResult_; }

private:
    TerrainCollisionDebugger() = default;
    ~TerrainCollisionDebugger() = default;
    TerrainCollisionDebugger(const TerrainCollisionDebugger&) = delete;
    TerrainCollisionDebugger& operator=(const TerrainCollisionDebugger&) = delete;

    DirectXCommon* dxCommon_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;

    // 頂点構造体
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    // TransformationMatrix定数バッファ
    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Matrix4x4 WorldInverseTranspose;
    };

    // Material定数バッファ
    struct MaterialData {
        Vector4 color;
        int32_t enableLighting;
        float padding0[3];
        Matrix4x4 uvTransform;
        int32_t selectLightings;
        float shininess;
        float environmentCoefficient;
        float fresnelF0;
        Vector3 specularColor;
    };

    static const size_t kMaxDebugTriangles = 3000;
    static const size_t kMaxDebugVertices = kMaxDebugTriangles * 3;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    VertexData* mappedVertices_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> wvpBuffer_;
    TransformationMatrix* mappedWvp_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialBuffer_;
    MaterialData* mappedMaterial_ = nullptr;

    // 接触点マーカーおよび法線ベクトル用3Dオブジェクト
    std::unique_ptr<Object3d> hitMarkerSphere_;
    std::unique_ptr<Object3d> normalVectorCylinder_;

    // フレーム毎の集計データ
    int totalStageTriangles_ = 0;
    int totalTestedTriangles_ = 0;
    int totalHitTriangles_ = 0;
    bool isHitThisFrame_ = false;
    CollisionResult lastHitResult_{};

    std::vector<Triangle> testedTriangles_;
    std::vector<Triangle> hitTriangles_;

    // 表示トグルフラグ
    bool isDebugEnabled_ = true;             // デバッグ可視化全体ON/OFF
    bool showTestedTriangles_ = true;        // 検査対象ポリゴン（黄色）
    bool showHitTriangles_ = true;           // 激突ポリゴン（赤色）
    bool showHitPointAndNormal_ = true;      // 接触点マーカー＆法線矢印
    bool showImGuiMonitor_ = true;           // ImGuiパネル表示（F6で表示切替）
    float hitHoldTimer_ = 0.0f;              // ヒット時の余韻表示タイマー
};
