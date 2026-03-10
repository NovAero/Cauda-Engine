#include "cepch.h"
#include "EditorLayer.h"

#include "Platform/Win32/Application.h"
#include "Core/Input/InputSystem.h"
#include "Config/EngineDefaults.h"

#include "Core/Editor/EditorModule.h"
#include "Core/Editor/ResourceLibrary.h"
#include "Core/Layers/GameLayer.h"
#include "Core/Layers/RenderLayer.h"
#include "Core/Modules.h"
#include "Renderer/Texture.h"
#include "Renderer/FrameBuffer.h"
#include "Core/Scene/SceneSerialiser.h"
#include "Core/Utilities/ImGuiUtilities.h"

#include "ImGuizmo.h"

namespace Cauda
{
    EditorLayer::EditorLayer(flecs::world& w) : //Layer("Editor Layer"),
        m_world(w)
    {
        Cauda::Application& app = Cauda::Application::Get();

        m_editorModule = m_world.try_get_mut<EditorModule>();
        m_rendererModule = m_world.try_get_mut<RendererModule>();
        m_cameraModule = m_world.try_get_mut<CameraModule>();
        m_physicsModule = m_world.try_get_mut<PhysicsModule>();
        m_sensorModule = m_world.try_get_mut<SensorModule>();
        m_grimoriumModule = m_world.try_get_mut<GrimoriumModule>();
        m_constraintModule = m_world.try_get_mut<ConstraintModule>();
        m_audioModule = m_world.try_get_mut<AudioModule>();
        m_gamemodeModule = m_world.try_get_mut<GamemodeModule>();

        m_gameLayer = app.GetGameLayer();
        m_renderLayer = app.GetRenderLayer();
        m_console = new EditorConsole();
    }

    EditorLayer::~EditorLayer()
    {
        m_gameLayer = nullptr;
        delete m_console;
    }

    void EditorLayer::OnAttach()
    {
        Cauda::Application::SetEditorRunning(true);

        if (auto input = InputSystem::Inst())
        {
            input->AddKeyListener(this);
            input->AddMouseListener(this);
        }
    }

    void EditorLayer::OnDetach()
    {
        if (auto input = InputSystem::Inst())
        {
            input->RemoveKeyListener(this);
            input->RemoveMouseListener(this);
        }

        m_editorModule->SetPlayingInEditor(false);
        Cauda::Application::SetEditorRunning(false);
    }

    void EditorLayer::OnImGuiRender()
    {
        DrawGameViewport();
        DrawMenuBar();
        DrawSceneManagementPopups();
        EditorConsole::Inst()->OnImGuiRender(&m_showEditorConsole);

        ResourceLibrary::DrawAssetBrowser(&m_showAssetLibrary);

        DrawEngineSettingsWindow(&m_showEngineSettings);

        //ImGui::ShowDemoWindow();

        // Might be better in general to switch to 'DrawWindow' naming convention instead of 'OnImguiRender',
        // since in the future we might want a module to have two separate windows.
        m_editorModule->DrawHierarchy(&m_showHierarchy);
        m_editorModule->DrawInspector(&m_showInspector);
        m_rendererModule->DrawRenderStats(&m_showRenderStats);
        m_physicsModule->OnImGuiRender();
        m_audioModule->OnImGuiRender();
        m_gamemodeModule->DrawWindow(&m_showGamemodeInfo);
        //m_grimoriumModule->OnImGuiRender();
    }

    void EditorLayer::OnUserEvent(SDL_UserEvent& event)
    {
        switch (event.code)
        {
        case EVENT_PLAY_IN_EDITOR:
            TogglePlayInEditor();
        break;
        case EVENT_LOAD_SCENE:
        {
            if (event.data1 == nullptr) break;

            std::string sceneName = (const char*)event.data1;
            if (sceneName.find(".json") == std::string::npos)
                sceneName += ".json";

            m_editorModule->LoadScene(sceneName);
            m_editorModule->SetCurrentSceneName(sceneName);

            event.data1 = nullptr;
        } break;
        case EVENT_REGISTER_SCENE:
            RegisterScene();
            break;
        }
    }

    void EditorLayer::OnKeyPress(PKey key)
    {
        switch(key)
        {
        case PKey::Z:
        {
            if (InputSystem::IsKeyDown(PKey::LCtrl))
                HistoryStack::Undo();
        } break;
        case PKey::Y:
        {
            if (InputSystem::IsKeyDown(PKey::LCtrl))
                HistoryStack::Redo();
        } break;
        case PKey::Delete:
        {
            m_editorModule->DeleteEntity(m_editorModule->GetSelectedEntity());
        } break;
        case PKey::Space:
        {
            m_editorModule->CycleGizmoModes();
        } break;
        case PKey::Escape:
        {
            Cauda::Application::Close();
        } break;
        }
    }

    void EditorLayer::OnMouseScroll(float amount)
    {
        if (InputSystem::Inst()->IsKeyDown(PKey::LAlt))
        {
            auto mainCam = m_cameraModule->GetMainCamera();
            if (mainCam && mainCam.is_alive())
            {
                auto* camComp = mainCam.try_get_mut<CameraComponent>();
                auto* transform = mainCam.try_get_mut<TransformComponent>();
                if (camComp && transform)
                {
                    if (camComp->projectionMode == ProjectionMode::ORTHOGRAPHIC)
                    {
                        float zoomDelta = amount > 0 ? -camComp->zoomSpeed : camComp->zoomSpeed;
                        camComp->orthoSize = glm::clamp(camComp->orthoSize + zoomDelta, 0.1f, 70.0f);
                    }
                    else
                    {
                        glm::vec3 forward = m_cameraModule->GetForwardVector(mainCam);
                        float zoomDistance = amount > 0 ? camComp->zoomSpeed * 2.0f : -camComp->zoomSpeed * 2.0f;
                        transform->position += forward * zoomDistance;
                    }
                }
            }
        }
    }

    void EditorLayer::RegisterSceneEntityNames()
    {
        ResourceLibrary::ResetUsednames();

        ResourceLibrary::RegisterName("EngineSettings", "");
        auto query = m_world.query_builder().with(flecs::Prefab).build();
        query.each([](flecs::entity prefab)
        {
            ResourceLibrary::RegisterName(prefab.name().c_str(), "");
        });

        m_editorModule->m_sceneRoot.children([](flecs::entity entity)
        {
            ResourceLibrary::RegisterChildNames(entity);
        });
    }

    void EditorLayer::RegisterScene()
    {
        m_editorModule->m_sceneRoot = m_world.lookup("Root");
        RegisterSceneEntityNames();

        auto pinQuery = m_world.query<PinConstraintComponent>();
        pinQuery.each([&](flecs::entity entity, PinConstraintComponent&)
            {
                m_world.event<ReloadConstraints>()
                    .id<PinConstraintComponent>()
                    .entity(entity)
                    .emit();
            });

        auto distQuery = m_world.query<DistanceConstraintComponent>();
        distQuery.each([&](flecs::entity entity, DistanceConstraintComponent&)
            {
                m_world.event<ReloadConstraints>()
                    .id<DistanceConstraintComponent>()
                    .entity(entity)
                    .emit();
            });
    }

    void EditorLayer::TogglePlayInEditor()
    {
        // Toggle Play In Editor
        m_editorModule->m_PIE = !m_editorModule->m_PIE;

        bool beginPlay = m_editorModule->m_PIE;
        if (beginPlay) // Started playing in editor.
        {
            m_editorModule->SaveScene(m_pieSessionName);
            Application::GetGameLayer()->OnAttach();
        }
        else // Stopped playing in editor.
        {
            Application::GetGameLayer()->OnDetach();
            m_constraintModule->CleanupManagedConstraints();
            m_editorModule->LoadScene(m_pieSessionName);
        }

        // Disable camera movement when playing in editor and vice versa.
        if (auto camComp = m_cameraModule->GetMainCamera().try_get_mut<CameraComponent>())
            camComp->enableMovement = !beginPlay;
    }

    void EditorLayer::DrawMenuBar()
    {
        auto& app = Application::Get();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Scene"))
                {
                    //m_currentSceneName = "NewScene.json";
                    m_editorModule->SetCurrentSceneName("NewScene.json");
                }

                ImGui::Separator();

                if (ImGui::BeginMenu("Load Scene"))
                {
                    bool hasScenes = false;

                    for (const auto& asset : ResourceLibrary::GetAllAssets())
                    {
                        if (asset.type == "Scene")
                        {
                            hasScenes = true;
                            std::string sceneName = asset.handle;
                            if (sceneName.find(".json") == std::string::npos)
                                sceneName += ".json";

                            bool isCurrentScene = (sceneName == m_editorModule->GetCurrentSceneName());
                            if (ImGui::MenuItem(sceneName.c_str(), nullptr, isCurrentScene))
                            {
                                //m_currentSceneName = sceneName;
                                //m_editorModule->LoadScene(sceneName);
                                EditorEvents::g_LoadSceneEvent.user.data1 = (void*)asset.handle.c_str();
                                SDL_PushEvent(&EditorEvents::g_LoadSceneEvent);
                            }
                        }
                    }

                    if (!hasScenes)
                    {
                        ImGui::MenuItem("No scenes found", nullptr, false, false);
                    }

                    ImGui::EndMenu();
                }

                std::string_view currentSceneName = m_editorModule->GetCurrentSceneName();
                bool canSave = !currentSceneName.empty() &&
                    currentSceneName.find(".json") != std::string::npos &&
                    currentSceneName != ".json";

                if (!canSave) ImGui::BeginDisabled();
                if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
                {
                    m_showSaveConfirmation = true;
                }
                if (!canSave) ImGui::EndDisabled();

                if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
                {
                    m_saveAsSceneName = currentSceneName;
                    m_showSaveAsDialog = true;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Exit", "Alt+F4"))
                {
                    Application::Get().Close();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Engine Settings"))
                {
                    m_showEngineSettings = true;
                }

                if (ImGui::MenuItem("Undo", "Ctrl+Z"))
                {
                    HistoryStack::Undo();
                }

                if (ImGui::MenuItem("Redo", "Ctrl+Y"))
                {
                    HistoryStack::Redo();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Create Entity"))
                {
                    m_editorModule->CreateEntity("Entity");
                }

                if (ImGui::MenuItem("Delete Entity", "Delete"))
                {
                    auto selected = m_editorModule->GetSelectedEntity();
                    if (selected && selected.is_alive())
                    {
                        m_editorModule->DeleteEntity(selected);
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                if (ImGui::MenuItem("Reset Layout"))
                {

                }

                ImGui::Separator();

                if (ImGui::MenuItem("Toggle Physics Debug", nullptr, m_physicsModule->IsDebugDrawEnabled()))
                {
                    m_physicsModule->SetDebugDrawEnabled(!m_physicsModule->IsDebugDrawEnabled());
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Toggle V-Sync", nullptr))
                {
                    app.ToggleVsync();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Toggle Wireframe", nullptr))
                {
                    app.ToggleWireframe();
                }

                ImGui::EndMenu();
            }



            if (ImGui::BeginMenu("Window"))
            {
                // Show Hierarchy
                if (ImGui::MenuItem("Hierarchy"))
                    m_showHierarchy = true;

                // Show Inspector Window
                if (ImGui::MenuItem("Inspector"))
                    m_showInspector = true;

                // Show Inspector Window
                if (ImGui::MenuItem("Renderer Stats"))
                    m_showRenderStats = true;

                // I think the viewport should always be in editor.
                // We can add it later if we want
                //if (ImGui::MenuItem("Viewport"))
                //{
                //    // Show game viewport
                //}

                // Show asset library window
                if (ImGui::MenuItem("Library"))
                    m_showAssetLibrary = true;

                // Show Console Window
                if (ImGui::MenuItem("Console"))
                    m_showEditorConsole = true;

                // Show Gamemode Window
                if (ImGui::MenuItem("Gamemode"))
                    m_showGamemodeInfo = true;


                ImGui::EndMenu();
            }

            std::string_view currentSceneName = m_editorModule->GetCurrentSceneName();

            ImGui::Separator();
            ImGui::Text("Scene: %s", currentSceneName.data());

            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 150);
            ImGui::Text("FPS: %.1f (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);

            ImGui::EndMainMenuBar();
        }
    }

    void EditorLayer::DrawGameViewport()
    {
        ImGui::Begin("Viewport");

        auto camera = m_cameraModule->GetMainCamera();
        auto camComp = camera.try_get<CameraComponent>();

        float pixScale = camComp->pixelScale > 0.0f ? camComp->pixelScale : 1.0f;

        int textureWidth = camComp->resolution.x / pixScale;   
        int textureHeight = camComp->resolution.y / pixScale;  
        float aspectRatio = (float)textureWidth / (float)textureHeight;

        bool gameWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        bool gameWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        if (gameWindowFocused || gameWindowHovered)
        {
            ImGui::GetIO().WantCaptureMouse = false;
            ImGui::GetIO().WantCaptureKeyboard = false;
        }

        ImVec2 contentSize = ImGui::GetContentRegionAvail();

        ImVec2 renderSize;
        if (contentSize.x / aspectRatio <= contentSize.y)
        {
            renderSize.x = contentSize.x;
            renderSize.y = contentSize.x / aspectRatio;
        }
        else
        {
            renderSize.x = contentSize.y * aspectRatio;
            renderSize.y = contentSize.y;
        }

        ImVec2 pos = ImGui::GetCursorPos();
        pos.x += (contentSize.x - renderSize.x) * 0.5f;
        pos.y += (contentSize.y - renderSize.y) * 0.5f;
        ImGui::SetCursorPos(pos);

        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 renderPos = ImVec2(windowPos.x + pos.x, windowPos.y + pos.y);

        glm::vec2 displayOffset = m_cameraModule->GetDisplaySmoothingOffset(
            camera,
            glm::vec2(renderSize.x, renderSize.y),
            glm::vec2(textureWidth, textureHeight)
        );

        ImVec2 smoothPos = pos;
        smoothPos.x += displayOffset.x;
        smoothPos.y -= displayOffset.y;
        ImGui::SetCursorPos(smoothPos);

        glm::vec2 textureSize(textureWidth, textureHeight);
        m_editorModule->SetViewportInfo(
            glm::vec2(renderPos.x, renderPos.y),
            glm::vec2(renderSize.x, renderSize.y),
            textureSize,
            glm::vec2(windowPos.x, windowPos.y)
        );

        const Texture* viewportTexture = m_renderLayer->GetDebugTexture();

        if (viewportTexture != nullptr)
        {
            GLuint textureID = viewportTexture->GetHandle();

            if (m_inspectorMode)
            {
                ImGuiTexInspect::InspectorFlags flags = ImGuiTexInspect::InspectorFlags_FlipY;
                if (ImGuiTexInspect::BeginInspectorPanel("ViewportInspector",
                    (ImTextureID)(intptr_t)textureID,
                    ImVec2(textureWidth, textureHeight),
                    flags,
                    ImGuiTexInspect::SizeIncludingBorder{ renderSize }))
                {
                    ImGuiTexInspect::EndInspectorPanel();
                }
            }
            else
            {
                ImGui::Image((ImTextureID)(intptr_t)textureID, renderSize, ImVec2(0, 1), ImVec2(1, 0));

                if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver())
                {
                    auto entity = m_renderLayer->PickEntity(m_viewportPixelPos.x, m_viewportPixelPos.y);
                    if (entity && entity.is_alive())
                    {
                        m_editorModule->SetSelectedEntity(entity);
                    }
                }
            }
        }

        {
            m_editorModule->DrawGrid(renderPos, renderSize, m_cameraModule);

            ImVec2 viewManipulatePos = ImVec2(renderPos.x + renderSize.x - 100, renderPos.y);
            ImVec2 viewManipulateSize = ImVec2(100, 100);

            glm::mat4 viewMatrix = m_cameraModule->GetViewMatrixWithoutOffset(camera);
            float* cameraViewMatrix = glm::value_ptr(viewMatrix);

            ImDrawList* drawList = ImGui::GetBackgroundDrawList();
            ImGuizmo::SetDrawlist();

            bool allowCameraManipulation = !ImGuizmo::IsUsing();

            if (allowCameraManipulation)
            {
                static glm::vec3 cameraTarget = glm::vec3(0.0f);
                float camDistance = m_cameraModule->CalculateArmLengthToPlane(
                    camera,
                    cameraTarget,
                    glm::vec3(0.0f, 1.0f, 0.0f)
                );

                if (std::isinf(camDistance) || camDistance <= 0.0f)
                {
                    camDistance = 10.0f;
                }

                glm::mat4 originalViewMatrix = glm::make_mat4(cameraViewMatrix);

                ImGuizmo::ViewManipulate(
                    cameraViewMatrix,
                    camDistance,
                    viewManipulatePos,
                    viewManipulateSize,
                    IM_COL32(0x10, 0x10, 0x10, 0x0)
                );

                glm::mat4 newViewMatrix = glm::make_mat4(cameraViewMatrix);
                if (originalViewMatrix != newViewMatrix)
                {
                    m_cameraModule->SetFromViewMatrix(camera, newViewMatrix);
                }
            }

            DrawPlayControls(renderPos, renderSize);

            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 mouseRelative = ImVec2(mousePos.x - windowPos.x, mousePos.y - windowPos.y);
            ImVec2 mouseInImage = ImVec2(
                mouseRelative.x - renderPos.x + windowPos.x,
                mouseRelative.y - renderPos.y + windowPos.y
            );

            m_viewportMousePos = glm::vec2(mouseInImage.x, mouseInImage.y);

            float u = mouseInImage.x / renderSize.x;
            float v = mouseInImage.y / renderSize.y;

            int pixelX = (int)(u * textureWidth);
            int pixelY = (int)((1.0f - v) * textureHeight);

            m_viewportPixelPos = glm::ivec2(pixelX, pixelY);

            m_editorModule->DrawViewportGizmoControls(renderPos, renderSize);
            m_editorModule->HandleTransformGizmo(renderPos, renderSize, m_cameraModule);
        }

        ImGui::End();

        DrawInspectorControlsWindow();
    }


    void EditorLayer::DrawInspectorControlsWindow()
    {
        ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Buffer Visualiser", nullptr))
        {
           // ImGui::PushFont(Fonts::Custom);
            ImGui::Checkbox("Inspector Mode", &m_inspectorMode);
            ImGui::Separator();

            ImGui::Text("Buffer Selection:");

            const char* bufferNames[] = {
                "Final Output",
                "G-Buffer: Albedo",
                "G-Buffer: Normals",
                "G-Buffer: World Position",
                "G-Buffer: Material",
                "G-Buffer: Entity IDs",
                "G-Buffer: Depth",
                "G-Buffer: Stencil",
                "Backface: Position",
                "Lighting: Colour",
                "Lighting: Data",
                "Ambient Occlusion",
                "AO Blurred",
                "Shadow Map",
                "Post Process"
            };

            int currentBuffer = (int)m_renderLayer->GetDebugVisualisation();
            if (ImGui::Combo("Buffer", &currentBuffer, bufferNames, IM_ARRAYSIZE(bufferNames)))
            {
                m_renderLayer->SetDebugVisualisation((Cauda::BufferVisualisation)currentBuffer);
            }

            const char* channelNames[] = { "All", "Red", "Green", "Blue", "Alpha" };
            int currentChannel = m_renderLayer->m_bufferVisInfo.visualiseChannel + 1;
            if (ImGui::Combo("Channel", &currentChannel, channelNames, IM_ARRAYSIZE(channelNames)))
            {
                m_renderLayer->m_bufferVisInfo.visualiseChannel = currentChannel - 1;
            }

            ImGui::Separator();

            auto debugVis = m_renderLayer->GetDebugVisualisation();
            switch (debugVis)
            {
            case BufferVisualisation::GBuffer_Depth:
            case BufferVisualisation::Shadow:
            {
                float depthScale = m_renderLayer->m_bufferVisInfo.depthVisualisationScale;
                if (ImGui::SliderFloat("Depth Scale", &depthScale, 0.1f, 10.0f))
                {
                    m_renderLayer->m_bufferVisInfo.depthVisualisationScale = depthScale;
                }
            }
            break;

            case BufferVisualisation::GBuffer_Position:
            {
                float posScale = m_renderLayer->m_bufferVisInfo.positionVisualisationScale;
                if (ImGui::SliderFloat("Position Scale", &posScale, 0.01f, 1.0f))
                {
                    m_renderLayer->m_bufferVisInfo.positionVisualisationScale = posScale;
                }

                bool normalise = m_renderLayer->m_bufferVisInfo.normaliseVisualisation;
                if (ImGui::Checkbox("Normalise", &normalise))
                {
                    m_renderLayer->m_bufferVisInfo.normaliseVisualisation = normalise;
                }
            }
            break;

            case BufferVisualisation::GBuffer_Normal:
            {
                bool normalise = m_renderLayer->m_bufferVisInfo.normaliseVisualisation;
                if (ImGui::Checkbox("Normalise", &normalise))
                {
                    m_renderLayer->m_bufferVisInfo.normaliseVisualisation = normalise;
                }
            }
            break;

            case BufferVisualisation::GBuffer_EntityID:
            {
                bool colourise = m_renderLayer->m_bufferVisInfo.colouriseEntityIDs;
                if (ImGui::Checkbox("Colourise IDs", &colourise))
                {
                    m_renderLayer->m_bufferVisInfo.colouriseEntityIDs = colourise;
                }
            }
            break;

            case BufferVisualisation::Backface_Position:
            {
                float posScale = m_renderLayer->m_bufferVisInfo.positionVisualisationScale;
                if (ImGui::SliderFloat("Position Scale", &posScale, 0.01f, 1.0f))
                {
                    m_renderLayer->m_bufferVisInfo.positionVisualisationScale = posScale;
                }

                bool normalise = m_renderLayer->m_bufferVisInfo.normaliseVisualisation;
                if (ImGui::Checkbox("Normalise", &normalise))
                {
                    m_renderLayer->m_bufferVisInfo.normaliseVisualisation = normalise;
                }
            }
            break;
            }


            if (debugVis != BufferVisualisation::Final)
            {
                ImGui::Separator();
                ImVec2 size = m_renderLayer->GetBufferSize(debugVis);
                ImGui::Text("Resolution: %dx%d", (int)size.x, (int)size.y);
                const char* format = m_renderLayer->GetBufferFormat(debugVis);
                ImGui::Text("Format: %s", format);
            }
           // ImGui::PopFont();
        }
        ImGui::End();
    }

    void EditorLayer::DrawPlayControls(const ImVec2& renderPos, const ImVec2& renderSize)
    {
        using namespace Cauda;
        using namespace Colour;

        const float buttonHeight = 32.0f;
        const float overlayPadding = 10.0f;

        static Texture* uiAtlas = nullptr;
        if (!uiAtlas)
        {
            uiAtlas = ResourceLibrary::GetTexture("icon_atlas");
            if (!uiAtlas)
            {
                uiAtlas = ResourceLibrary::LoadTexture("icon_atlas", "content/textures/icon_atlas.png");
            }
        }

        const int atlasColumns = 10;
        const int atlasRows = 10;
        const int playIconIndex = 22;  
        const int stopIconIndex = 23; 
        const int pauseIconIndex = 24;
       
        ImVec2 playButtonSize = { 32.0f, 32.0f };
        playButtonSize.y = buttonHeight;

        float totalWidth = playButtonSize.x;

        ImVec2 overlayPos = ImVec2(
            renderPos.x + (renderSize.x - totalWidth) * 0.5f,
            renderPos.y + overlayPadding
        );

        ImVec2 localOverlayPos = ImVec2(
            overlayPos.x - ImGui::GetWindowPos().x,
            overlayPos.y - ImGui::GetWindowPos().y
        );

        ImGui::SetCursorPos(localOverlayPos);

        //ImDrawList* overlayDrawList = ImGui::GetWindowDrawList();
        //ImVec2 backgroundMin = ImVec2(overlayPos.x - 8.0f, overlayPos.y - 4.0f);
        //ImVec2 backgroundMax = ImVec2(overlayPos.x + totalWidth + 8.0f, overlayPos.y + buttonHeight + 4.0f);
        //overlayDrawList->AddRectFilled(backgroundMin, backgroundMax, IM_COL32(0, 0, 0, 128), 4.0f);

        /*ImGui::PushStyleColor(ImGuiCol_Button, Colours::HumbleGreen);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colours::Brighten(Colours::HumbleGreen));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Colours::HumbleRed);*/

        bool isPlaying = m_editorModule->m_PIE; 

        if (ImGui::Button("##PlayButton", playButtonSize))
        {
            SDL_PushEvent(&EditorEvents::g_PIEvent);
            //TogglePlayInEditor();
        }

        {
            ImVec2 buttonMin = ImGui::GetItemRectMin();
            ImVec2 buttonMax = ImGui::GetItemRectMax();
            ImDrawList* buttonDrawList = ImGui::GetWindowDrawList();

            if (uiAtlas && uiAtlas->GetHandle() != 0)
            {
                ImVec2 iconSize = ImVec2(16.0f, 16.0f);
                ImVec2 iconPos = ImVec2(buttonMin.x + 8.0f, buttonMin.y + (buttonHeight - iconSize.y) * 0.5f);

                int iconIndex = isPlaying ? stopIconIndex : playIconIndex;
                ImVec4 iconColour = isPlaying ? Colour::HumbleRed : Colour::HumbleGreen;

                Cauda::UI::DrawImageAtlasIndex(
                    (ImTextureID)(uintptr_t)uiAtlas->GetHandle(),
                    iconPos, iconSize,
                    iconIndex,
                    atlasColumns, atlasRows,
                    iconColour
                );

                /* const char* buttonText = isPlaying ? "Stop" : "Play";
                 ImVec2 textPos = ImVec2(iconPos.x + iconSize.x + 4.0f, buttonMin.y + (buttonHeight - ImGui::GetFontSize()) * 0.5f);
                 buttonDrawList->AddText(textPos, IM_COL32(255, 255, 255, 255), buttonText);*/
            }
            else
            {
                const char* buttonText = isPlaying ? "Stop" : "Play";
                ImVec2 textSize = ImGui::CalcTextSize(buttonText);
                ImVec2 textPos = ImVec2(
                    buttonMin.x + (playButtonSize.x - textSize.x) * 0.5f,
                    buttonMin.y + (buttonHeight - textSize.y) * 0.5f
                );
                buttonDrawList->AddText(textPos, IM_COL32(255, 255, 255, 255), buttonText);
            }
            // ImGui::PopStyleColor(3);
        }

        if (!Cauda::Application::IsGameRunning()) return;

        ImGui::SetCursorPos({ localOverlayPos.x + playButtonSize.x, localOverlayPos.y });

        if (ImGui::Button("##PauseButton", playButtonSize))
        {
            Cauda::Application::TogglePauseGame();
        }

        {
            ImVec2 buttonMin = ImGui::GetItemRectMin();
            ImVec2 buttonMax = ImGui::GetItemRectMax();
            ImDrawList* buttonDrawList = ImGui::GetWindowDrawList();

            if (uiAtlas && uiAtlas->GetHandle() != 0)
            {
                ImVec2 iconSize = ImVec2(16.0f, 16.0f);
                ImVec2 iconPos = ImVec2(buttonMin.x + 8.0f, buttonMin.y + (buttonHeight - iconSize.y) * 0.5f);

                int iconIndex = Cauda::Application::IsGamePaused() ? playIconIndex : pauseIconIndex;
                ImVec4 iconColour = Colour::HumbleSilver;

                Cauda::UI::DrawImageAtlasIndex(
                    (ImTextureID)(uintptr_t)uiAtlas->GetHandle(),
                    iconPos, iconSize,
                    iconIndex,
                    atlasColumns, atlasRows,
                    iconColour
                );
            }
            else
            {
                const char* buttonText = Cauda::Application::IsGamePaused() ? "Unpause" : "Pause";
                ImVec2 textSize = ImGui::CalcTextSize(buttonText);
                ImVec2 textPos = ImVec2(
                    buttonMin.x + (playButtonSize.x - textSize.x) * 0.5f,
                    buttonMin.y + (buttonHeight - textSize.y) * 0.5f
                );
                buttonDrawList->AddText(textPos, IM_COL32(255, 255, 255, 255), buttonText);
            }
        }
    }

    void EditorLayer::DrawSceneManagementPopups()
    {
        if (m_showSaveConfirmation)
        {
            ImGui::OpenPopup("Save Scene Confirmation");
            m_showSaveConfirmation = false;
        }

        std::string currentSceneName = m_editorModule->GetCurrentSceneName();

        if (ImGui::BeginPopupModal("Save Scene Confirmation", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::Text("Save current scene as:");
            ImGui::Text("'%s'", currentSceneName.c_str());
            ImGui::Separator();

            ImGui::Text("Are you sure you want to save?");
            ImGui::Text("This will overwrite any existing file with this name.");

            ImGui::Separator();

            float buttonWidth = 80.0f;
            float totalWidth = buttonWidth * 2 + ImGui::GetStyle().ItemSpacing.x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - totalWidth) * 0.5f);

            if (ImGui::Button("Save", ImVec2(buttonWidth, 0)))
            {
                m_editorModule->SaveScene(currentSceneName);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (m_showSaveAsDialog)
        {
            ImGui::OpenPopup("Save Scene As");
            m_showSaveAsDialog = false;
        }

        if (ImGui::BeginPopupModal("Save Scene As", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::Text("Save scene with new name:");
            ImGui::Separator();

            char nameBuffer[256];
            strcpy_s(nameBuffer, sizeof(nameBuffer), m_saveAsSceneName.c_str());

            ImGui::SetNextItemWidth(300.0f);
            if (ImGui::InputText("Scene Name", nameBuffer, sizeof(nameBuffer)))
            {
                m_saveAsSceneName = nameBuffer;
            }

            bool isValidName = !m_saveAsSceneName.empty() &&
                m_saveAsSceneName.find(".json") != std::string::npos &&
                m_saveAsSceneName != ".json" &&
                m_saveAsSceneName.find(".") != 0;

            if (!isValidName)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f),
                    "Scene name must end with .json and be valid");
            }

            bool fileExists = false;
            if (isValidName)
            {
                for (const auto& asset : ResourceLibrary::GetAllAssets())
                {
                    if (asset.type == "Scene")
                    {
                        std::string existingName = asset.handle;
                        if (existingName.find(".json") == std::string::npos)
                            existingName += ".json";

                        if (existingName == m_saveAsSceneName)
                        {
                            fileExists = true;
                            break;
                        }
                    }
                }

                if (fileExists)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.6f, 1.0f),
                        "Warning: File already exists and will be overwritten");
                }
            }

            ImGui::Separator();

            float buttonWidth = 80.0f;
            float totalWidth = buttonWidth * 2 + ImGui::GetStyle().ItemSpacing.x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - totalWidth) * 0.5f);

            if (!isValidName) ImGui::BeginDisabled();
            if (ImGui::Button("Save", ImVec2(buttonWidth, 0)))
            {
                m_editorModule->SaveScene(m_saveAsSceneName);
                //m_currentSceneName = m_saveAsSceneName;
                m_editorModule->SetCurrentSceneName(m_saveAsSceneName);
                ImGui::CloseCurrentPopup();
            }
            if (!isValidName) ImGui::EndDisabled();

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

}