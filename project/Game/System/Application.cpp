#define NOMINMAX
#include "Application.h"
#include <Windows.h>
#include <combaseapi.h>
#include "KHEngine/Core/Utility/Log/Logger.h"
#include "KHEngine/Core/Utility/Crash/CrashDump.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "Game/Scene/GameSceneFactory.h"
#include <memory>


void Application::Initialize()
{
    
    KHFramework::Initialize();

    
    EngineServices* services = EngineServices::GetInstance();
    services->SetObject3dCommon(object3dCommon_.get());
    services->SetDirectXCommon(dxCommon_.get());
    services->SetSrvManager(srvManager_);
    services->SetSpriteCommon(spriteCommon_.get());
    services->SetInput(input_.get());
    services->SetImGuiManager(imguiManager_.get());

    
    sceneFactory_ = std::make_unique<GameSceneFactory>();
    services->SetSceneFactory(sceneFactory_.get());

    
    sceneManager_ = std::make_unique<SceneManager>();
    sceneManager_->SetSceneFactory(sceneFactory_.get());
    services->SetSceneManager(sceneManager_.get());

    
    sceneManager_->ChangeScene("TITLE");
}


void Application::Finalize()
{
    EngineServices::GetInstance()->SetSceneManager(nullptr);

    if (sceneManager_)
    {
        sceneManager_.reset();
    }

    
    if (sceneFactory_)
    {
        sceneFactory_.reset();
    }

    
    KHFramework::Finalize();
}


void Application::Update()
{
    
    KHFramework::Update();

    
    if (sceneManager_)
    {
        sceneManager_->Update();
    }
}


void Application::Draw()
{
    
    if (sceneManager_)
    {
        sceneManager_->Draw();
    }
}


void Application::DrawUI()
{
    if (sceneManager_)
    {
        sceneManager_->DrawUI();
    }
}


void Application::DrawTransition()
{
    if (sceneManager_)
    {
        sceneManager_->DrawTransition();
    }
}
