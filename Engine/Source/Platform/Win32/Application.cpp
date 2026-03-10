#include "cepch.h"

#include "Application.h"
#include "Core/Input/InputSystem.h"
#include "Core/Character/CharacterController.h"
#include "Core/Modules.h"
#include "Core/Layers/GameLayer.h"
#include "Core/Layers/RenderLayer.h" 
#include "Core/Scene/SceneSerialiser.h"
#include "Config/EngineDefaults.h"
#include "Core/Components/Components.h"

#include "stb_image.h"

#define FLECS_IMPL 
#include "ThirdParty/Flecs.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"

#pragma warning (disable: 4244) //Implicit type cast warning

namespace Cauda {

    Application* Application::s_AppInstance;

    Application::Application(unsigned int width, unsigned int height, const char* title, SDL_WindowFlags flags)
    {
        s_AppInstance = this;
        Logger logger;

        m_world = std::make_unique<flecs::world>();
        m_world->import<STLModule>();

        m_resourceLibrary = std::make_unique<ResourceLibrary>();
        ResourceLibrary::SetInstance(m_resourceLibrary.get());
        ResourceLibrary::SetWorld(m_world.get());

        SetupEngineComponents(*m_world);
        LoadEngineSettings(*m_world);

        InitWindow(title, width, height, flags);

        // SetUpPipeline needs to be called before modules are imported.
        SetUpPipeline();
        ImportModules();
        Initialise();
    }

    Application::~Application()
    {
        Shutdown();
    }

    void Application::OnStart()
    {
        m_inputLayer->OnAttach();
        m_imGuiLayer->OnAttach();
        m_editorLayer->OnAttach();
        m_renderLayer->OnAttach();

        //m_inputSystem.enable();
        //m_imGuiSystem.enable();
        //m_renderSystem.enable();

        s_AppInstance->m_gameRunning = false;
        m_gameTickSource.stop();
    }


#ifdef _DISTRIB
    void Application::OnLoadComplete()
    {
        SwitchToGameMode();
    }
#endif


    void Application::OnExit()
    {
        SaveEngineSettings();
    }

    void Application::Run()
    {
        OnStart();

        /*if (auto audioModule = m_world->try_get_mut<AudioModule>())
        {
            if (auto asset = ResourceLibrary::GetAudioAsset("countrysideBGM"))
            {
                asset->settings.loop = true;
                audioModule->PlaySoundStream2D(*reinterpret_cast<SoundStream*>(asset), AudioChannel::AMBIENT);
                asset->settings.loop = false;
            }
        }*/

        SceneIO::InitialiseDefaultScene(*m_world);
        Logger::PrintLog("Initialised default scene");

        SDL_GL_SetSwapInterval(m_vsyncEnabled);

#ifdef _DISTRIB
        SDL_SetWindowPosition(m_window, 0, 0);

        OnLoadComplete();
        PauseGame(true);
#endif // _DISTRIB

        SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(primaryDisplay);

        float refreshRate = 60.f;
        if (mode && mode->refresh_rate > 0.0f)
        {
            refreshRate = mode->refresh_rate;
        }

        float fixedDelta = 1.f / refreshRate;
        if (refreshRate > 65.0f)
        {
            fixedDelta = 1.0f / (refreshRate / 2.0f);
        }

        //Logger::PrintDebugSeperator();
        Logger::PrintLog("Target refresh rate: %f | SDL_DisplayMode.refresh_rate: %f", refreshRate, mode ? mode->refresh_rate : 0.0f);
        Logger::PrintLog("Fixed delta: %f", fixedDelta);
       // Logger::PrintDebugSeperator();

        Uint64 lastTime = SDL_GetTicks();
        static float accum = 0.f;

        bool pollDuringFixed = false;
        bool hideMouse = true;

        m_isRunning = true;
        while (m_isRunning)
        {
            Uint64 currentTime = SDL_GetTicks();
            double deltaTime = SysTime::Delta(currentTime, lastTime);
            lastTime = currentTime;

            pollDuringFixed = false;

            PollEvents();

            hideMouse = UpdateInput(deltaTime);

            if (deltaTime > 1.0) deltaTime = 0.0;
            
            if (!m_isMinimised)
            {
                accum += deltaTime;

                while (accum >= fixedDelta)
                {
                    if (pollDuringFixed)
                    {
                        PollEvents();
                        UpdateInput(fixedDelta);
                    }

                    if (!m_isLoading)
                    {
                        m_imGuiLayer->BeginDraw();
                    }
                    m_world->progress(fixedDelta);

                    accum -= fixedDelta;
                    if (!m_isLoading)
                    {
                        m_renderLayer->RenderPipeline();
                    }
                    
                    if (!m_isLoading)
                    {
                        if (m_editorRunning) m_editorLayer->OnImGuiRender();
                        if (IsGameRunning())
                        {
                            m_gameLayer->DrawGameUI();
                        }

                        m_imGuiLayer->EndDraw();
                        m_imGuiLayer->RenderDrawData();
                        SDL_GL_SwapWindow(m_window);
                    }
                    pollDuringFixed = true;
                }
            }
        }

        OnExit();
    }

    void Application::ImportModules()
    {
        m_world->import<GlmModule>();
        m_world->import<TransformModule>();
        m_world->import<EditorModule>();
        m_world->import<AudioModule>();
        m_world->import<CameraModule>();
        m_world->import<PhysicsModule>();
        m_world->import<RendererModule>();
        m_world->import<AnimationModule>();
        m_world->import<CharacterModule>();
        m_world->import<AnimationController>();
        m_world->import<LightingModule>();
        m_world->import<RopeModule>();
        m_world->import<RagdollModule>();
        m_world->import<CaribbeanModule>();
        m_world->import<GrimoriumModule>();
        m_world->import<InteractorModule>();
        m_world->import<GamemodeModule>();
        m_world->component<TransientEntity>();
    }

    void Application::SetUpPipeline()
    {
        // Setup Tick Sources
        m_onGameStart = m_world->system("OnGameStart")
            .kind(0)
            .run([](flecs::iter&) { std::cout << "OnGameStart Called" << std::endl; });
        

        SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(primaryDisplay);

        float refreshRate = 60.f;
        if (mode && mode->refresh_rate > 0.0f)
        {
            refreshRate = mode->refresh_rate;
        }

        float fixedDelta = 1.f / refreshRate;
        if (refreshRate > 65.f)
        {
            fixedDelta = 1.0f / (refreshRate / 2.0f);
        }

         m_gameTickSource = m_world->timer()
            .interval(fixedDelta);
    }

    void Application::OnEvent(SDL_Event& event)
    {
        switch (event.type)
        {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            m_isRunning = false;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        {
            UpdateWindowSize();
            m_renderLayer->ResizeFrameBuffers();
        } break;
        case SDL_EVENT_USER:
        {
            OnUserEvent(event.user);
            if (m_editorRunning) m_editorLayer->OnUserEvent(event.user);
            if (IsGameRunning()) m_gameLayer->OnUserEvent(event.user);
        } break;
        }
    }

    void Application::OnUserEvent(SDL_UserEvent& event)
    {
        switch (event.code)
        {
        case EVENT_SWITCH_TO_EDITOR:
            SwitchToEditorMode();
            break;
        case EVENT_SWITCH_TO_GAME:
            SwitchToGameMode();
            break;
        case EVENT_POST_RELOAD: //Post reload relinking
        {
            Cauda::Application::Get().EndSceneLoadDuringPlay();
        }
        break;
        }
    }

    void Application::OnKeyPress(PKey key)
	{
		switch (key)
		{
        case PKey::F5:
        {
            // Queue switch for next tick.
            if (m_isGameMode)
                SDL_PushEvent(&AppEvents::g_SwitchToEditorModeEvent);
               // SwitchToGameMode();
            else
                SDL_PushEvent(&AppEvents::g_SwitchToGameModeEvent);
        } break;
        case PKey::F7:
        {
            std::cout << "Systems Found:\n";
            auto query = m_world->query_builder().with(flecs::System).build();
            query.each([](flecs::entity sys)
                {
                    std::cout << "\tSystem: " << sys.name() << std::endl;
                });

            std::cout << "Observers Found:\n";
            query = m_world->query_builder().with(flecs::Observer).build();
            query.each([](flecs::entity sys)
                {
                    std::cout << "\tObserver: " << sys.name() << " ID: " << sys.id() << std::endl;
                });
        } break;
        case PKey::P:
        {
            if (!IsGameRunning()) break;
            m_gameLayer->TogglePause();
        } break;
		}
	}

    void Application::SwitchToEditorMode(bool resizeWindow)
    {
        if (!m_isGameMode) return;

        std::cout << "Switching to editor mode" << std::endl;

        m_gameLayer->OnDetach();

        if (resizeWindow)
        {
            SDL_SetWindowFullscreen(m_window, 0);

            SDL_SetWindowSize(m_window, m_cachedWindowSize.x, m_cachedWindowSize.y);
            SDL_SetWindowPosition(m_window, m_cachedWindowPos.x, m_cachedWindowPos.y);
        }

        m_editorLayer->OnAttach();
        m_renderLayer->SetGameView(false);
        //m_renderLayer->ResizeFrameBuffers();

        auto editorModule = m_world->try_get_mut<EditorModule>();
        editorModule->LoadScene("pieSession.json");

        m_isGameMode = false;

        Logger::PrintLog("Switched to Editor Mode");
    }

    void Application::SwitchToGameMode(bool resizeWindow)
    {
        if (m_isGameMode) return;

        std::cout << "Switching to game mode" << std::endl;

        m_editorLayer->OnDetach();
        if (resizeWindow)
        {
            int x, y, w, h;
            SDL_GetWindowPosition(m_window, &x, &y);
            SDL_GetWindowSize(m_window, &w, &h);
            m_cachedWindowPos = { x, y };
            m_cachedWindowSize = { w, h };

            SDL_SetWindowFullscreen(m_window, SDL_WINDOW_BORDERLESS);
        }

        if (!IsGameRunning())
        {
            auto editorModule = m_world->try_get_mut<EditorModule>();
            editorModule->SaveScene("pieSession.json");
            m_gameLayer->OnAttach();
        }
        m_renderLayer->SetGameView(true);
        //m_renderLayer->ResizeFrameBuffers();

        m_isGameMode = true;

        Logger::PrintLog("Switched to Game Mode");
    }


    void Application::UpdateWindowSize()
    {
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        m_windowSize = { w, h };
    }


    glm::vec2 Application::GetGameViewportSize()
    {
        // if (!IsEditorRunning()) return m_windowSize;
        return glm::vec2(float(m_screenWidth), float(m_screenHeight));//m_editorLayer->GetViewportSize();
    }

    bool Application::HasGameViewportResized()
    {
        //return m_viewportSize != m_viewportSizePrev;
        return m_editorLayer->HasViewportResized();
    }

    void Application::ToggleVsync()
    {
        m_vsyncEnabled = !m_vsyncEnabled;
        SDL_GL_SetSwapInterval(m_vsyncEnabled);
    }

    void Application::ToggleWireframe()
    {
        m_wireframeEnabled = !m_wireframeEnabled;

        if (m_wireframeEnabled)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    void Application::RunGame()
    {
        s_AppInstance->m_onGameStart.run();
        s_AppInstance->m_gameRunning = true;
        s_AppInstance->m_gameTickSource.start();
    }

    void Application::StopGame()
    {
        s_AppInstance->m_gameRunning = false;
        s_AppInstance->m_gameTickSource.stop();
    }

    void Application::PauseGame(bool pause)
    {
        if (pause) s_AppInstance->m_gameTickSource.stop();
        else s_AppInstance->m_gameTickSource.start();
    }

    void Application::TogglePauseGame()
    {
        PauseGame(!IsGamePaused());
    }

    bool Application::IsGamePaused()
    {
        //flecs::entity tick_source = Cauda::Application::GetGameTickSource();
        flecs::entity tick_source = s_AppInstance->m_gameTickSource;
        if (!tick_source.has<flecs::Timer>()) return false;
        return !tick_source.try_get<flecs::Timer>()->active;
    }

    const CharacterController* Application::GetCharacterController(flecs::entity possessed)
    {
        return m_gameLayer->GetCharacterController(possessed);
    }


    void Application::PollEvents()
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            m_inputLayer->OnEvent(event);
            OnEvent(event);
            m_imGuiLayer->OnEvent(event);
        }
    }

    void Application::Initialise()
    {
        Logger::PrintLog("Started initialising Application");
        {
            int x, y;
            int channels;

            auto data = stbi_load("content/textures/cauda_cursor_32_alt2.png", &x, &y, &channels, STBI_default);

            if (data)
            {
                auto surface = SDL_CreateSurfaceFrom(x, y, SDL_PIXELFORMAT_RGBA8888, data, x * channels);

                m_gameCursor = SDL_CreateColorCursor(surface, 0, 0);

                stbi_image_free(data);
            }
        }

        AppEvents::InitialiseAppEvents();
        EditorEvents::InitialiseEditorEvents();
        GameEvents::InitialiseGameEvents();

		m_historyStack = new HistoryStack();
		m_input = new InputSystem();
		m_imGuiLayer = new ImGuiLayer();

        InputSystem::Inst()->AddKeyListener(this);
        InputSystem::Inst()->AddMouseListener(this);
        InputSystem::Inst()->SetUniversalListener(this);

        m_inputLayer = new InputLayer();
        m_renderLayer = new RenderLayer(*m_world);
        m_editorLayer = new EditorLayer(*m_world);
        m_gameLayer = new GameLayer(*m_world);

        Logger::PrintLog("Finished initialising Application");
    }

    void Application::InitWindow(std::string title, int width, int height, SDL_WindowFlags flags)
    {
        Logger::PrintLog("Starting SDL Window Setup");
        m_windowTitle = title;
        m_windowSize = glm::vec2{ width, height };
        m_flags = flags;

        //auto settings = g_EngineSettingsEntity.try_get_mut<EditorSettings>();

        SDL_InitSubSystem(SDL_INIT_VIDEO);

        //Set openGl attributes
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        //Set up window and function pointers
        m_window = SDL_CreateWindow(m_windowTitle.c_str(), m_windowSize.x, m_windowSize.y, m_flags);
        if (!m_window)
        {
            Logger::PrintDebugSeperator();
            Logger::PrintLog("ERROR: Window Creation Failed:");
            Logger::PrintLog(SDL_GetError());
            Logger::PrintDebugSeperator();
        }
        m_glContext = SDL_GL_CreateContext(m_window);
        if (!m_glContext)
        {
            Logger::PrintDebugSeperator();
            Logger::PrintLog("ERROR: OpenGL Context Creation failed:");
            Logger::PrintLog(SDL_GetError());
            Logger::PrintDebugSeperator();
        }
        SDL_GL_MakeCurrent(m_window, m_glContext);

        // m_vsyncEnabled = settings->vSync;
        SDL_GL_SetSwapInterval(m_vsyncEnabled);

        //set up function pointers
        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
        {
            Logger::PrintDebugSeperator();
            Logger::PrintLog("ERROR: Failed to initialise GLAD");
            Logger::PrintDebugSeperator();
            SDL_Quit();
        }

        glViewport(0, 0, m_windowSize.x, m_windowSize.y);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        glClearColor(.5f, 0.5f, 0.95f, 1.0f);

        SDL_DisplayID primaryMonitor = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(primaryMonitor);
        m_screenWidth = mode->w;
        m_screenHeight = mode->h;

        Logger::PrintLog("SDL Window setup succeeded");
    }

    void Application::Shutdown()
    {
        Logger::PrintLog("Application shutting down...");

        ResourceLibrary::SetInstance(nullptr);
		delete m_historyStack;
        if (m_gameCursor)
        {
            SDL_DestroyCursor(m_gameCursor);
        }
        SDL_DestroyWindow(m_window);
        SDL_Quit();
    }

    bool Application::UpdateInput(float deltaTime)
    {
        static bool mouseWasHidden = false;

        bool hideMouse = m_inputLayer->OnUpdate(deltaTime);

        if (mouseWasHidden && !hideMouse)
        {
            SDL_SetWindowRelativeMouseMode(m_window, false);
            mouseWasHidden = hideMouse;
        }
        else if (!mouseWasHidden && hideMouse)
        {
            SDL_SetWindowRelativeMouseMode(m_window, true);
            mouseWasHidden = hideMouse;
        }

        return hideMouse;
    }

    void Application::BeginSceneLoadDuringPlay(std::string sceneName)
    {
        if (!m_isGameMode) return;

        SetLoading(true);

        m_gameLayer->OnDetach();

        m_editorLayer->OnAttach();
        m_renderLayer->SetGameView(false);

        auto editorModule = m_world->try_get_mut<EditorModule>();
        editorModule->LoadScene(sceneName);

        m_isGameMode = false;

        Logger::PrintLog("Beginning scene load during play");

        SDL_PushEvent(&EditorEvents::g_PostReloadEvent);
    }

    void Application::EndSceneLoadDuringPlay()
    {
        if (m_isGameMode) return;

        m_editorLayer->OnDetach();

        if (!IsGameRunning())
        {
            auto editorModule = m_world->try_get_mut<EditorModule>();
            m_gameLayer->OnAttach();
        }
        m_renderLayer->SetGameView(true);

        m_isGameMode = true;

        Logger::PrintLog("Ended scene load during play");
        SetLoading(false);
    }

    void Application::SetWindowAttribute(SDL_GLAttr attribute, int value)
    {
        SDL_GL_SetAttribute(attribute, value);
    }

}