#pragma once
#include "Renderer/Graphics.h"
#include "Core/Layers/LayerStack.h"
#include "Core/Layers/InputLayer.h"
#include "Core/Layers/ImGuiLayer.h"
#include "Core/Layers/GameLayer.h"
#include "Core/Layers/EditorLayer.h"
#include "Core/Input/Input.h"
#include "Core/Utilities/HistoryStack.h"
#include "Core/Editor/ResourceLibrary.h"
#include <memory>
#include "imgui.h"
#include "ThirdParty/Flecs.h"


// Aight here's the rough plan for the flecs-based pipeline.
// Gonna add more to it later.
//  OnLoad
//      CaptureRawInputs
//  PostLoad
//      ProcessRawInputs
//      ProcessImGui
//      EditorUpdate
//      EditorCameraUpdate
//  PreUpdate
//      ResolvePhysicsBodyEntities
//          ResolveCharacterBodyEntites
//              ResolveRopeConnectedEntities
//  OnUpdate
//      GameUpdate
//          GrimoireUpdate
//          PhysicsUpdate
//          CharacterUpdate
//  OnValidate
//      CollisionCheck
//  PostUpdate
//      CollisionResponse
//  PreStore
//      PrepareImGui
//      PrepareOpenGL
//  OnStore
//      RenderImGui
//      RenderOpenGL
//


class FrameBuffer;
class Camera;
class CharacterController;



namespace Cauda 
{
    class RenderLayer;
    class Application : public IKeyListener, IMouseListener, IGamepadListener
    {
    public:
        Application(unsigned int width, unsigned int height, const char* = "Unnamed App", SDL_WindowFlags flags = SDL_WINDOW_OPENGL);
        virtual ~Application();

        virtual void OnStart();
#ifdef _DISTRIB
        virtual void OnLoadComplete();
#endif
        virtual void OnExit();

        void Run();

        virtual void ImportModules();
        virtual void SetUpPipeline();

        void Initialise();
        //void RenderFrame();
        void PollEvents();
        void Shutdown();

        void BeginSceneLoadDuringPlay(std::string sceneName);
        void EndSceneLoadDuringPlay();

        //Setters
        void UpdateWindowSize();

        void SetLoading(bool loading) { m_isLoading = loading; }

    protected:
        virtual void OnEvent(SDL_Event& event);
        virtual void OnUserEvent(SDL_UserEvent& event);

		//Input listening
        virtual void OnKeyPress(PKey key) override;
		virtual void OnKeyRelease(PKey key) override {}

        virtual void OnMouseClick(MouseButton button, int clicks) override {}
        virtual void OnMouseRelease(MouseButton mouseButton) override {}
        virtual void OnMouseScroll(float amount) override {}

        virtual void OnButtonDown(Uint8 button) override {}
        virtual void OnButtonUp(Uint8 button) override {}

        virtual void OnLeftAxisInput(float xValue, float yValue) override {}
        virtual void OnRightAxisInput(float xValue, float yValue) override {}

        virtual void OnTriggerDown(bool left) override {}
        virtual void OnTriggerUp(bool left) override {}


    public:
        //Getters
        glm::vec2 GetGameViewportSize();
        bool HasGameViewportResized();
        glm::vec2& GetWindowSize() { return m_windowSize; }
        glm::vec2 GetScreenSize() const { return glm::vec2{ m_screenWidth, m_screenHeight }; }
        glm::vec2& GetWindowPos() { return m_windowPos; }
        bool& GetForceResizeImGui() { return m_forceResizeImGui; }
        bool& GetWindowMaximised() { return m_windowMaximised; }
        SDL_Window* GetSDLWindow() { return m_window; }
        SDL_GLContext GetGLContext() { return m_glContext; }
        std::string GetWindowTitle() { return m_windowTitle; }
        bool IsRumbleEnabled() { return m_rumbleEnabled; }
        void SetRumbleEnabled(bool enabled) { m_rumbleEnabled = enabled; }
        static RenderLayer* GetRenderLayer() { return s_AppInstance->m_renderLayer; }
        static GameLayer* GetGameLayer() { return s_AppInstance->m_gameLayer; }
        static EditorLayer* GetEditorLayer() { return s_AppInstance->m_editorLayer; }
        void ToggleVsync();
        bool GetVsync() { return m_vsyncEnabled; }
        void ToggleWireframe();

        static void Close() { s_AppInstance->m_isRunning = false; }
        static Application& Get() { return *s_AppInstance; }
        static flecs::world& GetWorld() { return *s_AppInstance->m_world; }

        bool m_editorRunning = false;
        static void SetEditorRunning(bool running) { s_AppInstance->m_editorRunning = running; }
        static bool IsEditorRunning() { return s_AppInstance->m_editorRunning; }

        //bool m_gameRunning = false;

        static void RunGame();
        static void StopGame();
        static void PauseGame(bool pause);
        static void TogglePauseGame();
        static bool IsGameRunning() { return s_AppInstance->m_gameRunning; }
        static bool IsGamePaused();

        // Custom Flecs Phases and Tick Source Getters
        static flecs::entity GetOnGameStart() { return s_AppInstance->m_onGameStart; }
        static flecs::entity GetGameTickSource() { return s_AppInstance->m_gameTickSource; }
        //static flecs::entity GetGamePhase() { return s_AppInstance->m_gameUpdatePhase; }
        //static flecs::entity ResolvePhysicsPhase() { return s_AppInstance->m_resolvePhysicsPhase; }
       // static flecs::entity GetPhysicsPhase() { return s_AppInstance->m_physicsUpdatePhase; }

        const CharacterController* GetCharacterController(flecs::entity possessed);

    protected:
        // Objects
        std::unique_ptr<flecs::world>       m_world;
        std::unique_ptr<ResourceLibrary>    m_resourceLibrary;
        SDL_Window*                         m_window;
        SDL_GLContext                       m_glContext;
        InputSystem*                        m_input;
		HistoryStack*					    m_historyStack;

        bool m_gameRunning = false;
        bool m_isLoading = false;

        // Custom Flecs Phases
       // flecs::entity m_gameUpdatePhase;
        //flecs::entity m_resolvePhysicsPhase;
        //flecs::entity m_physicsUpdatePhase;

        // Tick Sources
        flecs::system m_onGameStart; // This one only ticks once when the game begins play.
        flecs::timer m_gameTickSource;

		// Layers
        flecs::system m_inputSystem;
        flecs::system m_imGuiSystem;
        flecs::system m_renderSystem;

        SDL_Cursor*                         m_gameCursor = nullptr;

        InputLayer*                         m_inputLayer;
        EditorLayer*                        m_editorLayer;
        GameLayer*                          m_gameLayer;
        ImGuiLayer*                         m_imGuiLayer;
        RenderLayer*                        m_renderLayer;
        
        //Variables
		std::string						m_windowTitle;
		SDL_WindowFlags					m_flags;
        //glm::vec2                       m_viewportSize;
        //glm::vec2                       m_viewportSizePrev;
        glm::vec2                       m_cachedWindowSize;
        glm::vec2                       m_cachedWindowPos;
        glm::vec2                       m_windowSize;
        glm::vec2                       m_windowPos;
        int                             m_windowedWidth = 0;
        int                             m_windowedHeight = 0;

        //bool                            m_pauseUpdates = false;
        bool                            m_forceResizeImGui = false;
        bool                            m_isRunning = false;
        bool                            m_windowMaximised;
        bool                            m_isMinimised = false;
        bool                            m_isGameMode = false;
        bool                            m_vsyncEnabled = true;
        bool                            m_wireframeEnabled = false;
        unsigned int                    m_screenWidth, m_screenHeight;
        bool                            m_rumbleEnabled = true;

        //Functions
        bool UpdateInput(float deltaTime);

        void SetWindowAttribute(SDL_GLAttr attribute, int value);
        void InitWindow(std::string title, int width, int height, SDL_WindowFlags flags = SDL_WINDOW_OPENGL);

        void SwitchToEditorMode(bool resizeWindow = true);
        void SwitchToGameMode(bool resizeWindow = true);
    private:
        static Application* s_AppInstance;
    };

    Application* CreateApplication();
}
