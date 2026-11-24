#include "TerrainCollisionDebugger.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "externals/imgui/imgui.h"
#include <cmath>
#include <algorithm>

TerrainCollisionDebugger* TerrainCollisionDebugger::GetInstance() {
    static TerrainCollisionDebugger instance;
    return &instance;
}

void TerrainCollisionDebugger::Initialize(DirectXCommon* dxCommon, Object3dCommon* object3dCommon) {
    dxCommon_ = dxCommon;
    object3dCommon_ = object3dCommon;

    // 頂点バッファの作成
    vertexBuffer_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * kMaxDebugVertices);
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices_));

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * kMaxDebugVertices);
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    // 行列バッファの作成
    wvpBuffer_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
    wvpBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedWvp_));
    mappedWvp_->World = Matrix4x4::Identity();
    mappedWvp_->WVP = Matrix4x4::Identity();
    mappedWvp_->WorldInverseTranspose = Matrix4x4::Identity();

    // マテリアルバッファの作成
    materialBuffer_ = dxCommon_->CreateBufferResource(sizeof(MaterialData));
    materialBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedMaterial_));
    mappedMaterial_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    mappedMaterial_->enableLighting = 0;
    mappedMaterial_->selectLightings = 0;
    mappedMaterial_->uvTransform = Matrix4x4::Identity();

    // 接触点マーカー（球）
    ModelManager::GetInstance()->LoadModel("collider_sphere.obj");
    hitMarkerSphere_ = std::make_unique<Object3d>();
    hitMarkerSphere_->Initialize(object3dCommon_);
    hitMarkerSphere_->SetModel("collider_sphere.obj");
    hitMarkerSphere_->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f }); // 鮮やかな赤色
    hitMarkerSphere_->SetEnableLighting(false);

    // 押し戻し法線ベクトル（矢印棒）
    ModelManager::GetInstance()->LoadModel("collider_cube.obj");
    normalVectorCylinder_ = std::make_unique<Object3d>();
    normalVectorCylinder_->Initialize(object3dCommon_);
    normalVectorCylinder_->SetModel("collider_cube.obj");
    normalVectorCylinder_->SetColor({ 0.1f, 1.0f, 1.0f, 1.0f }); // シアン
    normalVectorCylinder_->SetEnableLighting(false);
}

void TerrainCollisionDebugger::BeginFrame() {
    totalStageTriangles_ = 0;
    totalTestedTriangles_ = 0;
    totalHitTriangles_ = 0;
    isHitThisFrame_ = false;
    testedTriangles_.clear();
    hitTriangles_.clear();

    if (hitHoldTimer_ > 0.0f) {
        hitHoldTimer_ -= 0.016f;
    }
}

void TerrainCollisionDebugger::RecordCollisionCheck(
    int modelTotalTriangles,
    const std::vector<Triangle>& testedTriangles,
    const std::vector<Triangle>& hitTriangles,
    const CollisionResult* result
) {
    totalStageTriangles_ += modelTotalTriangles;
    totalTestedTriangles_ += static_cast<int>(testedTriangles.size());
    totalHitTriangles_ += static_cast<int>(hitTriangles.size());

    // 描画用のリストに追加（最大件数制限）
    if (showTestedTriangles_) {
        for (const auto& tri : testedTriangles) {
            if (testedTriangles_.size() < kMaxDebugTriangles) {
                testedTriangles_.push_back(tri);
            }
        }
    }

    if (showHitTriangles_) {
        for (const auto& tri : hitTriangles) {
            if (hitTriangles_.size() < kMaxDebugTriangles) {
                hitTriangles_.push_back(tri);
            }
        }
    }

    if (result && result->isHit) {
        isHitThisFrame_ = true;
        lastHitResult_ = *result;
        hitHoldTimer_ = 0.5f; // ヒット時は半秒間マーカーを保持して視認しやすくする
    }
}

void TerrainCollisionDebugger::EndFrame() {
    // 必要に応じた集計後処理
}

void TerrainCollisionDebugger::Draw(Camera* camera) {
    if (!isDebugEnabled_ || !camera || !dxCommon_ || !object3dCommon_) return;

    auto commandList = dxCommon_->GetCommandList();
    const Matrix4x4& vp = camera->GetViewMatrix() * camera->GetProjectionMatrix();

    // 1. 三角形描画用の行列を更新（ワールド座標系をそのまま使用）
    mappedWvp_->World = Matrix4x4::Identity();
    mappedWvp_->WVP = vp;
    mappedWvp_->WorldInverseTranspose = Matrix4x4::Identity();

    // ワイヤーフレーム描画設定
    object3dCommon_->SetWireframeDrawSetting();

    // 定数バッファをセット（WVP: root 1, Material: root 0）
    commandList->SetGraphicsRootConstantBufferView(1, wvpBuffer_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(0, materialBuffer_->GetGPUVirtualAddress());
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // ① SAT判定対象ポリゴンを「黄色」で描画
    if (showTestedTriangles_ && !testedTriangles_.empty()) {
        mappedMaterial_->color = { 1.0f, 0.85f, 0.0f, 1.0f }; // 鮮やかな黄色

        size_t triCount = (std::min)(testedTriangles_.size(), kMaxDebugTriangles);
        for (size_t i = 0; i < triCount; ++i) {
            const auto& tri = testedTriangles_[i];
            mappedVertices_[i * 3 + 0] = { { tri.p0.x, tri.p0.y, tri.p0.z, 1.0f }, { 0, 0 }, tri.normal };
            mappedVertices_[i * 3 + 1] = { { tri.p1.x, tri.p1.y, tri.p1.z, 1.0f }, { 0, 0 }, tri.normal };
            mappedVertices_[i * 3 + 2] = { { tri.p2.x, tri.p2.y, tri.p2.z, 1.0f }, { 0, 0 }, tri.normal };
        }
        commandList->DrawInstanced(static_cast<UINT>(triCount * 3), 1, 0, 0);
    }

    // ② 激突したポリゴンを「赤色」で描画
    if (showHitTriangles_ && !hitTriangles_.empty()) {
        mappedMaterial_->color = { 1.0f, 0.15f, 0.15f, 1.0f }; // 鮮やかな赤色

        size_t triCount = (std::min)(hitTriangles_.size(), kMaxDebugTriangles);
        for (size_t i = 0; i < triCount; ++i) {
            const auto& tri = hitTriangles_[i];
            mappedVertices_[i * 3 + 0] = { { tri.p0.x, tri.p0.y, tri.p0.z, 1.0f }, { 0, 0 }, tri.normal };
            mappedVertices_[i * 3 + 1] = { { tri.p1.x, tri.p1.y, tri.p1.z, 1.0f }, { 0, 0 }, tri.normal };
            mappedVertices_[i * 3 + 2] = { { tri.p2.x, tri.p2.y, tri.p2.z, 1.0f }, { 0, 0 }, tri.normal };
        }
        commandList->DrawInstanced(static_cast<UINT>(triCount * 3), 1, 0, 0);
    }

    // 通常描画設定に戻す
    object3dCommon_->SetCommonDrawSetting();

    // ③ 接触点マーカー ＆ 押し戻し法線ベクトル（矢印）の描画
    if (showHitPointAndNormal_ && (isHitThisFrame_ || hitHoldTimer_ > 0.0f)) {
        const Vector3& hp = lastHitResult_.hitPoint;
        const Vector3& hn = lastHitResult_.normal;

        // 接触点マーカー（小さな赤い球）
        if (hitMarkerSphere_) {
            hitMarkerSphere_->SetTranslate(hp);
            hitMarkerSphere_->SetScale({ 0.45f, 0.45f, 0.45f });
            hitMarkerSphere_->Update();
            hitMarkerSphere_->Draw();
        }

        // 押し戻し法線ベクトル（法線方向に伸びるシアン色のバー）
        if (normalVectorCylinder_) {
            float arrowLen = (std::max)(lastHitResult_.penetrationDepth * 4.0f, 2.0f);
            Vector3 centerPos = {
                hp.x + hn.x * (arrowLen * 0.5f),
                hp.y + hn.y * (arrowLen * 0.5f),
                hp.z + hn.z * (arrowLen * 0.5f)
            };

            // 法線向きの回転計算
            float pitch = -std::asinf(std::clamp(hn.y, -1.0f, 1.0f));
            float yaw = std::atan2f(hn.x, hn.z);

            normalVectorCylinder_->SetTranslate(centerPos);
            normalVectorCylinder_->SetRotation({ pitch, yaw, 0.0f });
            normalVectorCylinder_->SetScale({ 0.1f, 0.1f, arrowLen });
            normalVectorCylinder_->Update();
            normalVectorCylinder_->Draw();
        }
    }
}

void TerrainCollisionDebugger::DrawImGui() {
    if (!showImGuiMonitor_) return;
    if (!ImGui::GetCurrentContext() || ImGui::GetFrameCount() <= 0) return;

    ImGui::SetNextWindowPos(ImVec2(10, 80), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340, 290), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("地形コリジョン解析モニター [F6]", &showImGuiMonitor_)) {
        ImGui::Checkbox("デバッグ可視化を有効化", &isDebugEnabled_);
        ImGui::Separator();

        // 判定ステータス
        if (isHitThisFrame_ || hitHoldTimer_ > 0.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "● 判定ステータス: [ HIT (地形激突中) ]");
        } else {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "● 判定ステータス: [ SAFE (非接触) ]");
        }

        // カリング統計
        float cullingRate = 0.0f;
        if (totalStageTriangles_ > 0) {
            cullingRate = (1.0f - static_cast<float>(totalTestedTriangles_) / static_cast<float>(totalStageTriangles_)) * 100.0f;
        }

        ImGui::Spacing();
        ImGui::Text("■ 多段階カリング統計 (Broad/Mid-Phase)");
        ImGui::Text("  ステージ総ポリゴン : %d 枚", totalStageTriangles_);
        ImGui::Text("  SAT精密判定対象    : %d 枚", totalTestedTriangles_);
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "  カリング削減率     : %.2f %%", cullingRate);

        ImGui::Spacing();
        ImGui::Text("■ 衝突応答データ (Collision Response)");
        ImGui::Text("  激突ポリゴン数   : %d 枚", totalHitTriangles_);
        ImGui::Text("  めり込み深さ     : %.3f m", lastHitResult_.penetrationDepth);
        ImGui::Text("  押し戻し法線     : (%.2f, %.2f, %.2f)",
            lastHitResult_.normal.x, lastHitResult_.normal.y, lastHitResult_.normal.z);
        ImGui::Text("  接触点 (HitPoint): (%.1f, %.1f, %.1f)",
            lastHitResult_.hitPoint.x, lastHitResult_.hitPoint.y, lastHitResult_.hitPoint.z);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Checkbox("判定中ポリゴンを黄色表示", &showTestedTriangles_);
        ImGui::Checkbox("激突ポリゴンを赤色表示", &showHitTriangles_);
        ImGui::Checkbox("接触点マーカー・法線を表示", &showHitPointAndNormal_);
    }
    ImGui::End();
}
