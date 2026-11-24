#pragma once
#include "KHEngine/Graphics/3d/Particle/ParticleEmitter.h"
#include "KHEngine/Graphics/3d/Particle/ParticleRenderer.h"
#include "KHEngine/Graphics/3d/Particle/ParticleManager.h"
#include "KHEngine/Graphics/Resource/Descriptor/SrvManager.h"
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include <vector>
#include <memory>
#include <string>

/**
 * @brief 複数のパーティクル（Plane, Ring, Cylinder）を組み合わせて
 * 1つの複雑なエフェクトを作るためのクラス
 */
class ParticleEffect {
public:
    struct Node {
        char name[64] = "Node";
        ParticleRenderer renderer;
        ParticleEmitter emitter;
        ParticleForGPU* instancingData = nullptr;
        uint32_t numInstance = 0;
        
        int shapeType = 0; // 0: Quad(Plane), 1: Ring, 2: Cylinder, 3: HexRing(Honeycomb)
        
        // アニメーション設定
        float uvScrollSpeed = 0.0f;
        float uvScrollOffset = 0.0f;
        bool enableColorAnim = false;
        float colorAnimSpeed = 2.0f;
        float colorTimer = 0.0f;
        Vector4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        Vector4 startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        Vector4 endColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    ParticleEffect() = default;
    ~ParticleEffect() = default;
    ParticleEffect(ParticleEffect&&) noexcept = default;
    ParticleEffect& operator=(ParticleEffect&&) noexcept = default;
    ParticleEffect(const ParticleEffect&) = delete;
    ParticleEffect& operator=(const ParticleEffect&) = delete;

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update(float dt, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const Matrix4x4& billboardMatrix);
    void Draw();
    
    // ImGuiによるエディタ描画
    void DrawImGui();

    // セーブ・ロード
    void SaveToJson(const std::string& filename);
    void LoadFromJson(const std::string& filename);

    // ノード（エフェクトの層）を追加
    void AddNode(const std::string& name, int shapeType);
    
    // すべてのノードで放出（バースト）
    void Play();

    // エフェクト全体の発生位置を設定
    void SetPosition(const Vector3& pos);

    // エフェクト全体のベースカラーを設定
    void SetBaseColor(const Vector4& color);

    // 親行列設定（親子関係の構築）
    void SetParentMatrix(const Matrix4x4* parentMatrix);
    const Matrix4x4* GetParentMatrix() const { return parentMatrix_; }

    // 全ノードのパーティクルをクリア
    void ClearParticles();

    // ノードへのアクセス
    std::vector<std::unique_ptr<Node>>& GetNodes() { return nodes_; }
    void ClearNodes() { nodes_.clear(); }
    void SetupRendererForNode(Node* node);

private:
    std::vector<std::unique_ptr<Node>> nodes_;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    uint32_t maxInstances_ = 1000;
    const Matrix4x4* parentMatrix_ = nullptr;
};
