#pragma once
#include "KHEngine/Core/OS/WinApp.h"
#include "KHEngine/Core/Graphics/DirectXCommon.h"
#include "KHEngine/Input/Input.h"
#include "KHEngine/Debug/Imgui/ImGuiManager.h"
#include "KHEngine/Core/Framework/KHFramework.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Scene/SceneManager.h"
#include "KHEngine/Scene/AbstractSceneFactory.h"
#include "Game/Scene/GameSceneFactory.h"
#include <memory>

class Application : public KHFramework
{

public:

    
    
    
    void Initialize() override;

    
    
    
    void Finalize() override;

    
    
    
    void Update() override;

    
    
    
    void Draw() override;
    void DrawUI() override;
    void DrawTransition() override;

private:
    
    std::unique_ptr<SceneManager> sceneManager_ = nullptr;

    
    std::unique_ptr<AbstractSceneFactory> sceneFactory_ = nullptr;
};
