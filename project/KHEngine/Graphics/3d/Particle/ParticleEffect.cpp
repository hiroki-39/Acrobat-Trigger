#include "ParticleEffect.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "externals/imgui/imgui.h"
#include "externals/nlohmann/json.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>

using json = nlohmann::json;

void ParticleEffect::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    // フォルダ作成
    std::filesystem::create_directories("resources/json/particles");
    
    // 初期ロードは各シーンで必要なjsonを明示的に呼び出すためここでは行わない
}

void ParticleEffect::AddNode(const std::string& name, int shapeType)
{
    auto node = std::make_unique<Node>();
    strncpy_s(node->name, name.c_str(), sizeof(node->name) - 1);
    node->shapeType = shapeType;
    
    if (shapeType == 0) node->emitter.SetParameter(ParticleEmitter::CreateExplosionPreset());
    else if (shapeType == 1) node->emitter.SetParameter(ParticleEmitter::CreateRingPreset());
    else if (shapeType == 2) node->emitter.SetParameter(ParticleEmitter::CreateCylinderPreset());

    node->emitter.SetTextureName("gradationLine.png");
    node->emitter.SetBlendMode(BlendMode::Alpha);
    
    SetupRendererForNode(node.get());
    node->emitter.SetParentMatrix(parentMatrix_);
    
    nodes_.push_back(std::move(node));
}

void ParticleEffect::SetupRendererForNode(Node* node)
{
    std::string assetName = "quad";
    if (node->shapeType == 1) assetName = "ring";
    else if (node->shapeType == 2) assetName = "Cylinder";

    ParticleManager::GetInstance()->SetupRendererFromAsset(node->renderer, assetName, dxCommon_, srvManager_, maxInstances_);
    node->instancingData = node->renderer.GetInstancingData();
}

void ParticleEffect::Play()
{
    for (auto& node : nodes_)
    {
        node->emitter.Emit(node->emitter.GetParameter().count);
    }
}

void ParticleEffect::SetPosition(const Vector3& pos)
{
    for (auto& node : nodes_)
    {
        node->emitter.SetPosition(pos);
    }
}

void ParticleEffect::SetBaseColor(const Vector4& color)
{
    for (auto& node : nodes_)
    {
        node->baseColor = color;
    }
}

void ParticleEffect::SetParentMatrix(const Matrix4x4* parentMatrix)
{
    parentMatrix_ = parentMatrix;
    for (auto& node : nodes_)
    {
        node->emitter.SetParentMatrix(parentMatrix_);
    }
}

void ParticleEffect::ClearParticles()
{
    for (auto& node : nodes_)
    {
        node->emitter.Clear();
    }
}

void ParticleEffect::Update(float dt, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const Matrix4x4& billboardMatrix)
{
    for (auto& node : nodes_)
    {
        node->emitter.Update(dt);

        // UV Scroll & Color Anim
        node->uvScrollOffset += node->uvScrollSpeed * dt;
        if (node->uvScrollOffset > 1.0f) node->uvScrollOffset -= 1.0f;
        if (node->uvScrollOffset < -1.0f) node->uvScrollOffset += 1.0f;

        Vector4 currentColor = node->baseColor;
        if (node->enableColorAnim)
        {
            node->colorTimer += node->colorAnimSpeed * dt;
            float t = (std::sin(node->colorTimer) + 1.0f) * 0.5f;
            currentColor = {
                std::lerp(node->startColor.x, node->endColor.x, t),
                std::lerp(node->startColor.y, node->endColor.y, t),
                std::lerp(node->startColor.z, node->endColor.z, t),
                std::lerp(node->startColor.w, node->endColor.w, t)
            };
        }

        Matrix4x4 uvTransform = Matrix4x4::Translation({ node->uvScrollOffset, 0.0f, 0.0f });
        ParticleManager::GetInstance()->UpdateMaterial(node->renderer, currentColor, uvTransform);

        node->numInstance = node->emitter.FillInstancingBuffer(node->instancingData, maxInstances_, viewMatrix, projectionMatrix, billboardMatrix);
    }
}

void ParticleEffect::Draw()
{
    for (auto& node : nodes_)
    {
        if (node->numInstance == 0) continue;
        uint32_t texIndex = TextureManager::GetInstance()->GetSrvIndex(node->emitter.GetTextureName());
        BlendMode blendMode = node->emitter.GetBlendMode();
        node->renderer.Draw(node->numInstance, texIndex, blendMode);
    }
}

void ParticleEffect::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("Effect Editor");

    static char filename[64] = "default_effect.json";
    ImGui::InputText("Filename", filename, sizeof(filename));
    
    if (ImGui::Button("Save JSON")) {
        SaveToJson(filename);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load JSON")) {
        LoadFromJson(filename);
    }

    ImGui::Separator();

    if (ImGui::Button("Play Effect (Burst)"))
    {
        Play();
    }
    
    ImGui::Separator();
    ImGui::Text("Add New Node");
    static int newShapeType = 0;
    const char* shapes[] = { "Quad (Plane)", "Ring", "Cylinder" };
    ImGui::Combo("Shape", &newShapeType, shapes, 3);
    if (ImGui::Button("Add Node"))
    {
        AddNode("New Node", newShapeType);
    }

    ImGui::Separator();
    
    for (size_t i = 0; i < nodes_.size(); ++i)
    {
        auto& node = nodes_[i];
        ImGui::PushID(static_cast<int>(i));
        
        if (ImGui::CollapsingHeader(node->name, ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::InputText("Node Name", node->name, sizeof(node->name));
            
            if (ImGui::Combo("Shape Type", &node->shapeType, shapes, 3))
            {
                SetupRendererForNode(node.get());
            }

            auto param = node->emitter.GetParameter();
            bool changed = false;

            if (ImGui::TreeNode("Emitter Settings")) {
                int count = static_cast<int>(param.count);
                if (ImGui::DragInt("Emit Count", &count, 1, 0, 100)) { param.count = count; changed = true; }
                if (ImGui::DragFloat("Frequency", &param.frequency, 0.01f, 0.0f, 2.0f)) { changed = true; }
                
                float life[2] = { param.minLifeTime, param.maxLifeTime };
                if (ImGui::DragFloat2("LifeTime (Min/Max)", life, 0.05f, 0.0f, 10.0f)) {
                    param.minLifeTime = life[0]; param.maxLifeTime = life[1]; changed = true;
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Transform (Min/Max)")) {
                if (ImGui::DragFloat3("Min Velocity", &param.minVelocity.x, 0.05f)) changed = true;
                if (ImGui::DragFloat3("Max Velocity", &param.maxVelocity.x, 0.05f)) changed = true;
                if (ImGui::DragFloat3("Min Scale", &param.minScale.x, 0.05f)) changed = true;
                if (ImGui::DragFloat3("Max Scale", &param.maxScale.x, 0.05f)) changed = true;
                if (ImGui::DragFloat3("Min Rotation", &param.minRotation.x, 0.05f)) changed = true;
                if (ImGui::DragFloat3("Max Rotation", &param.maxRotation.x, 0.05f)) changed = true;
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Color Settings")) {
                if (ImGui::ColorEdit4("Min Color", &param.minColor.x)) changed = true;
                if (ImGui::ColorEdit4("Max Color", &param.maxColor.x)) changed = true;
                
                if (ImGui::Checkbox("Color Over Lifetime", &param.isColorOverLifetime)) changed = true;
                if (param.isColorOverLifetime) {
                    if (ImGui::ColorEdit4("End Color", &param.endColor.x)) changed = true;
                }

                if (ImGui::Checkbox("Scale Over Lifetime", &param.isScaleOverLifetime)) changed = true;
                if (param.isScaleOverLifetime) {
                    if (ImGui::DragFloat3("End Scale", &param.endScale.x, 0.05f)) changed = true;
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Physics & Options")) {
                if (ImGui::Checkbox("Use Gravity", &param.useGravity)) changed = true;
                if (param.useGravity) {
                    if (ImGui::DragFloat("Gravity Force", &param.gravity, 0.1f)) changed = true;
                }
                if (ImGui::DragFloat("Air Drag", &param.drag, 0.01f, 0.0f, 1.0f)) changed = true;
                
                bool billboard = node->emitter.GetParameter().count; // ダミー取得
                // 実際は Emitter 側にあるので getter/setter を通す
                // 簡易的に直接変更
                ImGui::TreePop();
            }

            if (changed) {
                node->emitter.SetParameter(param);
            }

            ImGui::Separator();
            ImGui::Text("Material & Blend");
            
            char texName[64];
            strncpy_s(texName, node->emitter.GetTextureName().c_str(), sizeof(texName)-1);
            if (ImGui::InputText("Texture", texName, sizeof(texName))) {
                node->emitter.SetTextureName(texName);
            }

            int mode = static_cast<int>(node->emitter.GetBlendMode());
            const char* blendNames[] = { "None", "Alpha", "Additive", "Multiply", "PreMultiplied" };
            if (ImGui::Combo("Blend Mode", &mode, blendNames, 5)) {
                node->emitter.SetBlendMode(static_cast<BlendMode>(mode));
            }

            ImGui::DragFloat("UV Scroll Speed", &node->uvScrollSpeed, 0.05f, -5.0f, 5.0f);

            if (ImGui::Button("Delete Node"))
            {
                nodes_.erase(nodes_.begin() + i);
                ImGui::PopID();
                break;
            }
        }
        ImGui::PopID();
    }

    ImGui::End();
#endif
}

void ParticleEffect::SaveToJson(const std::string& filename) {
    json root = json::array();

    for (const auto& node : nodes_) {
        json j;
        j["name"] = node->name;
        j["shapeType"] = node->shapeType;
        j["uvScrollSpeed"] = node->uvScrollSpeed;
        j["enableColorAnim"] = node->enableColorAnim;
        j["colorAnimSpeed"] = node->colorAnimSpeed;
        
        j["startColor"] = { node->startColor.x, node->startColor.y, node->startColor.z, node->startColor.w };
        j["endColor"] = { node->endColor.x, node->endColor.y, node->endColor.z, node->endColor.w };

        auto p = node->emitter.GetParameter();
        json ep;
        ep["count"] = p.count;
        ep["frequency"] = p.frequency;
        ep["minLifeTime"] = p.minLifeTime;
        ep["maxLifeTime"] = p.maxLifeTime;
        ep["minScale"] = { p.minScale.x, p.minScale.y, p.minScale.z };
        ep["maxScale"] = { p.maxScale.x, p.maxScale.y, p.maxScale.z };
        ep["minVelocity"] = { p.minVelocity.x, p.minVelocity.y, p.minVelocity.z };
        ep["maxVelocity"] = { p.maxVelocity.x, p.maxVelocity.y, p.maxVelocity.z };
        ep["minRotation"] = { p.minRotation.x, p.minRotation.y, p.minRotation.z };
        ep["maxRotation"] = { p.maxRotation.x, p.maxRotation.y, p.maxRotation.z };
        ep["minColor"] = { p.minColor.x, p.minColor.y, p.minColor.z, p.minColor.w };
        ep["maxColor"] = { p.maxColor.x, p.maxColor.y, p.maxColor.z, p.maxColor.w };
        ep["isColorOverLifetime"] = p.isColorOverLifetime;
        ep["endColor"] = { p.endColor.x, p.endColor.y, p.endColor.z, p.endColor.w };
        ep["isScaleOverLifetime"] = p.isScaleOverLifetime;
        ep["endScale"] = { p.endScale.x, p.endScale.y, p.endScale.z };
        ep["useGravity"] = p.useGravity;
        ep["gravity"] = p.gravity;
        ep["drag"] = p.drag;

        j["emitterParam"] = ep;
        j["textureName"] = node->emitter.GetTextureName();
        j["blendMode"] = static_cast<int>(node->emitter.GetBlendMode());

        root.push_back(j);
    }

    std::ofstream file("resources/json/particles/" + filename);
    if (file.is_open()) {
        file << root.dump(4);
    }
}

void ParticleEffect::LoadFromJson(const std::string& filename) {
    std::ifstream file("resources/json/particles/" + filename);
    if (!file.is_open()) return;

    json root;
    file >> root;
    
    nodes_.clear();

    for (auto& j : root) {
        auto node = std::make_unique<Node>();
        std::string name = j["name"];
        strncpy_s(node->name, name.c_str(), sizeof(node->name) - 1);
        node->shapeType = j["shapeType"];
        node->uvScrollSpeed = j.value("uvScrollSpeed", 0.0f);
        node->enableColorAnim = j.value("enableColorAnim", false);
        node->colorAnimSpeed = j.value("colorAnimSpeed", 2.0f);
        
        if (j.contains("startColor")) {
            node->startColor = { j["startColor"][0], j["startColor"][1], j["startColor"][2], j["startColor"][3] };
        }
        if (j.contains("endColor")) {
            node->endColor = { j["endColor"][0], j["endColor"][1], j["endColor"][2], j["endColor"][3] };
        }

        ParticleEmitterParameter p;
        auto ep = j["emitterParam"];
        p.count = ep["count"];
        p.frequency = ep["frequency"];
        p.minLifeTime = ep["minLifeTime"];
        p.maxLifeTime = ep["maxLifeTime"];
        p.minScale = { ep["minScale"][0], ep["minScale"][1], ep["minScale"][2] };
        p.maxScale = { ep["maxScale"][0], ep["maxScale"][1], ep["maxScale"][2] };
        p.minVelocity = { ep["minVelocity"][0], ep["minVelocity"][1], ep["minVelocity"][2] };
        p.maxVelocity = { ep["maxVelocity"][0], ep["maxVelocity"][1], ep["maxVelocity"][2] };
        p.minRotation = { ep["minRotation"][0], ep["minRotation"][1], ep["minRotation"][2] };
        p.maxRotation = { ep["maxRotation"][0], ep["maxRotation"][1], ep["maxRotation"][2] };
        p.minColor = { ep["minColor"][0], ep["minColor"][1], ep["minColor"][2], ep["minColor"][3] };
        p.maxColor = { ep["maxColor"][0], ep["maxColor"][1], ep["maxColor"][2], ep["maxColor"][3] };
        p.isColorOverLifetime = ep["isColorOverLifetime"];
        p.endColor = { ep["endColor"][0], ep["endColor"][1], ep["endColor"][2], ep["endColor"][3] };
        p.isScaleOverLifetime = ep["isScaleOverLifetime"];
        p.endScale = { ep["endScale"][0], ep["endScale"][1], ep["endScale"][2] };
        p.useGravity = ep["useGravity"];
        p.gravity = ep["gravity"];
        p.drag = ep["drag"];

        node->emitter.SetParameter(p);
        node->emitter.SetTextureName(j["textureName"]);
        node->emitter.SetBlendMode(static_cast<BlendMode>(j["blendMode"]));

        SetupRendererForNode(node.get());
        node->emitter.SetParentMatrix(parentMatrix_);
        nodes_.push_back(std::move(node));
    }
}
