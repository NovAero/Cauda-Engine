#pragma once
#include "Core/Layers/EditorLayer.h"
#include "Core/Layers/ImGuiLayer.h"
#include "Core/Layers/GameLayer.h"
#include "Core/Layers/RenderLayer.h"
#include <Platform/Win32/Application.h>
#include "ThirdParty/Flecs.h"

class CaudaApp : public Cauda::Application
{
public:
    CaudaApp(unsigned int width, unsigned int height, const char* title, SDL_WindowFlags flags = SDL_WINDOW_OPENGL) :
        Application(width, height, title, flags)
    {
        //m_gameLayer = new Cauda::GameLayer(*m_world);
        //m_editorLayer = new Cauda::EditorLayer(*m_world);
    }
    ~CaudaApp() {}

    virtual void OnStart() override;
#ifdef _DISTRIB
    virtual void OnLoadComplete() override;
#endif

    virtual void OnKeyPress(PKey key) override;
    virtual void OnEvent(SDL_Event& event) override; // Temp maybe

private:
    void LoadGameScripts(); 

    //void SwitchToEditorMode();
    //void SwitchToGameMode();
};