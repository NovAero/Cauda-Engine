#include "CaudaApp.h"
#include <Platform/Win32/EntryPoint.h>
#include "Core/Editor/EditorModule.h"
#include "Grimorium.h"

namespace Cauda
{
    Application* CreateApplication()
    {
#ifdef _DISTRIB
        return new CaudaApp(720, 360, "Tumult v1.0.8", SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
#else
        return new CaudaApp(1280, 720, "Cauda Engine v1.7", SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
#endif // 
    }
}

void CaudaApp::OnStart()
{
    Application::OnStart();

    LoadGameScripts();
    ResourceLibrary::LoadAllContent();

    GetRenderLayer()->SetGameView(false);
}

void CaudaApp::OnKeyPress(PKey key)
{

#ifndef _DISTRIB

    Application::OnKeyPress(key);
#else

#endif // !_DISTRIB
}

void CaudaApp::OnEvent(SDL_Event& event)
{
    Application::OnEvent(event);

    switch (event.type)
    {

    }
}

#ifdef _DISTRIB
void CaudaApp::OnLoadComplete()
{
    Application::OnLoadComplete();

    SwitchToGameMode();

}
#endif

void CaudaApp::LoadGameScripts()
{
    Grimorium::InitialiseGrimoires(*m_world);
}