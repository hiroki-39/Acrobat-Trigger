#pragma once
#include <cstdint>
#include <memory>

#include "KHEngine/Core/OS/WinApp.h"
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Input/Input.h"
#include "KHEngine/Debug/Imgui/ImGuiManager.h"
#include "KHEngine/Graphics/2d/SpriteCommon.h"
#include "KHEngine/Graphics/3d/Object/Object3dCommon.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "KHEngine/Sound/Core/SoundManager.h"

#include "KHEngine/Scene/AbstractSceneFactory.h"
#include "KHEngine/Graphics/PostProcess/PostProcess.h"
#include "KHEngine/Debug/Editor/EditorSystem.h"

class KHFramework
{
public:
    virtual ~KHFramework() = default;

    
    
    
    void Run();

protected:

    
    
    
    virtual void Initialize() {}

    
    
    
    virtual void Update() {}

    
    
    
    virtual void Draw() = 0;

    
    
    
    virtual void DrawUI() {}
    virtual void DrawTransition() {}

    
    
    
    virtual void Finalize() {}

protected:

    std::unique_ptr<WinApp> winApp_ = nullptr;

    std::unique_ptr<DirectXCommon> dxCommon_ = nullptr;
    
    std::unique_ptr<Input> input_ = nullptr;
    
    std::unique_ptr<ImGuiManager> imguiManager_ = nullptr;

    std::unique_ptr<SpriteCommon> spriteCommon_ = nullptr;
    
    std::unique_ptr<Object3dCommon> object3dCommon_ = nullptr;
    
    SrvManager* srvManager_ = nullptr;

    std::unique_ptr<PostProcess> postProcess_ = nullptr;

    
    float kDeltaTime_ = 1.0f / 60.0f;

    
    bool endRequest_ = false;

private:

    
    void FrameworkInitialize();
    void FrameworkUpdate(float deltaTime);
    void FrameworkDrawBegin();
    void FrameworkDrawEnd();
    void FrameworkFinalize();

    

    
    void InitializeEngineSubsystems();
    void FinalizeEngineSubsystems();

    
    void BeginFrameCommon();
    void EndFrameCommon();
};
