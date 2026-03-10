#include "cepch.h"
#include "EngineDefaults.h"
#include "Core/Modules.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Scene/SceneSerialiser.h"
#include "Core/Components/GrabComponent.h"
#include <fstream>
#include <filesystem>

flecs::entity g_EngineSettingsEntity;

namespace fs = std::filesystem;

namespace SceneIO
{
    void InitialiseDefaultScene(flecs::world& world)
    {
        auto editorModule = world.try_get_mut<EditorModule>();
        auto rendererModule = world.try_get_mut<RendererModule>();
        auto cameraModule = world.try_get_mut<CameraModule>();
        auto lightingModule = world.try_get_mut<LightingModule>();
        auto physicsModule = world.try_get_mut<PhysicsModule>();
        auto characterModule = world.try_get_mut<CharacterModule>();
        auto ropeModule = world.try_get_mut<RopeModule>();

        auto sceneRoot = world.entity("Root")
            .set<TransformComponent>({ {0,0,0},{0,0,0},{1,1,1} })
            .add<SelectedTag>();

        auto camera = cameraModule->GetMainCamera();
        camera.child_of(sceneRoot);

        auto engine = g_EngineSettingsEntity.try_get_mut<EditorSettings>();
        if (engine && engine->loadScenePath != "")
        {
            LoadSceneFromFile(engine->loadScenePath, world, sceneRoot);

            std::string directory = g_ContentPath + "Scenes/";
            std::string fileName = engine->loadScenePath;
            fileName.erase(0, directory.length());

            editorModule->SetCurrentSceneName(fileName);

            return;
        }

        editorModule->SetSceneRoot(sceneRoot);

        camera.set<TransformComponent>({
        glm::vec3(30.f, 30.f, 30.f),
        glm::vec3(-35.f, 45.f, 0.f),
        glm::vec3(1.0f)
        });

        auto sun = lightingModule->CreateDirectionalLight(
            glm::vec3(-0.5f, -0.5f, 0.5f),
            glm::vec3(1.0f, 1.0f, 1.0f),
            1.0f,
            true,
            "SunLight"
        );

        auto ambientLight = lightingModule->CreateAmbientLight(
			glm::vec3(0.0f, 0.12f, 0.24f),
			0.5f,
			"AmbientLight"
		);

        auto sky = world.entity("SkySphere")
            .set<TransformComponent>
            ({
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.f, 0.f, 0.f),
                //glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                glm::vec3(150.0f, 150.0f, 150.0f)
            })
            .set<MeshComponent>
            ({
                "SM_InverseSphere",
                true
            })
            .set<MaterialComponent>
            ({
                "star_material",
            });
			
			

        for (int i = 0; i < MAX_GAMEPAD_SOCKETS; ++i)
        {
            flecs::entity character = editorModule->CreateEntityFromPrefab("Character", "PF_Character");
            flecs::entity rope;

            character.set<TransformComponent>({ glm::vec3(i * -2.0f, 5.0f, i * -2.0f) });

            character.children([&](flecs::entity child)
                {
                    if (child.has<RopeComponent>())
                    {
                        rope = child;
                    }
                });

            if (rope)
            {
                ropeModule->ConnectRope(rope, character, true);
            }
        }


        auto ground = world.entity(ResourceLibrary::GenerateUniqueName("Ground", "Root").c_str())
            .set<TransformComponent>({
            glm::vec3(0.0f, -16.0f, 0.0f),
            glm::vec3(0.f, 0.f, 0.f),
            //glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            glm::vec3(32.0f, 32.0f, 32.0f)
                });

        ground.set<ColliderComponent>({
            ShapeType::Box,
            glm::vec3(32.0f, 32.0f, 32.0f)
            });

        ground.set<PhysicsBodyComponent>({
            .motionType = MotionType::STATIC,
            .mass = 0.0f
            });

        ground.set<MeshComponent>({
            "box_mesh",
            true
            });

        ground.set<MaterialComponent>({
            "world_material",
            });

        ground.set<ShellTextureComponent>({
            .enabled = true,
            .shellCount = 6,
            .shellHeight = 0.01f,
            .tilingAmount = 0.05f,
            .thickness = 0.85f,
            .baseColour = glm::vec3(0.3f, 0.6f, 0.2f),
            .tipColour = glm::vec3(0.5f, 0.8f, 0.3f),
            .noiseTexture = "watercolour_texture",
            .maskTexture = "watercolour_texture"
            });

        auto pillar = world.entity(ResourceLibrary::GenerateUniqueName("Pillar", "Root").c_str())
            .set<TransformComponent>({
            glm::vec3(-5.0f, 4.0f, 7.0f),
            glm::vec3(0.f, 0.f, 0.f),
            glm::vec3(0.5f, 8.0f, 0.5f)
                });

        pillar.set<ColliderComponent>({
            ShapeType::Box,
            glm::vec3(0.5f, 8.0f, 0.5f)
            });

        pillar.set<PhysicsBodyComponent>({
            .motionType = MotionType::STATIC,
            .mass = 0.0f
            });

        pillar.set<MeshComponent>({
            "box_mesh",
            true
            });

        pillar.set<MaterialComponent>({
            "world_material",
            });

        pillar.set<ShellTextureComponent>({
            .enabled = true,
            .shellCount = 6,
            .shellHeight = 0.01f,
            .tilingAmount = 0.05f,
            .thickness = 0.85f,
            .baseColour = glm::vec3(0.3f, 0.6f, 0.2f),
            .tipColour = glm::vec3(0.5f, 0.8f, 0.3f),
            .noiseTexture = "watercolour_texture",
            .maskTexture = "watercolour_texture"
            });


        auto box1 = world.entity(ResourceLibrary::GenerateUniqueName("BigCrate", "Root").c_str())
            .set<TransformComponent>({ .position = glm::vec3(-2.0f, 5.0f, 0.0f), .scale = glm::vec3(1.5f) })
            .set<PhysicsBodyComponent>({ .motionType = MotionType::DYNAMIC, .collisionLayer = CollisionLayer::MOVING, .mass = 150.0f })
            .set<ColliderComponent>({ ShapeType::Box, glm::vec3(1.5f) })
            .set<MeshComponent>({ "wood_crate", true })
            .set<MaterialComponent>({ "atlas_material" });

        auto box2 = world.entity(ResourceLibrary::GenerateUniqueName("SmallCrate", "Root").c_str())
            .set<TransformComponent>({ .position = glm::vec3(2.0f, 5.0f, 0.0f), .scale = glm::vec3(1.0f) })
            .set<PhysicsBodyComponent>({ .motionType = MotionType::DYNAMIC, .collisionLayer = CollisionLayer::MOVING, .mass = 50.f})
            .set<ColliderComponent>({ .shapeSubType = ShapeType::Box, .dimensions = glm::vec3(1.0f)})
            .set<MeshComponent>({ "wood_crate", true })
            .set<MaterialComponent>({ "atlas_material" });

        physicsModule->ResolveUnboundEntities();
        characterModule->ResolveUnboundEntities();

        SDL_PushEvent(&EditorEvents::g_RegisterSceneEvent);

        return;
    }
}

void SetupEngineComponents(flecs::world& world)
{
    world.component<EditorSettings>()
        .member<bool>("vSync")
        .member<std::string>("loadScenePath")
        .member<bool>("orthographic");

    world.component<DebugDrawSettings>()
        .member<bool>("drawPhysicsDebug")
        .member<bool>("drawWireFrame");

    world.component<AssetBrowserSettings>()
        .member<float>("iconSize")
        .member<int>("iconSpacing")
        .member<bool>("stretchSpacing");

    world.component<GizmoSettings>()
        .member<float>("translationSnap")
        .member<float>("rotationSnap")
        .member<float>("scaleSnap")
        .member<bool>("translationSnapEnabled")
        .member<bool>("rotationSnapEnabled")
        .member<bool>("scaleSnapEnabled")
        .member<bool>("worldSpace");

    world.component<AudioMixerSettings>()
        .member<float>("masterVolume")
        .member<float>("bgmVolume")
        .member<float>("sfxVolume")
        .member<float>("ambientVolume");
}

void SaveEngineSettings()
{
    if (!fs::exists("Config/")) {
        fs::create_directory("Config/");
    }

    std::ofstream outputFile("Config/EngineSettings.json");

    if (!outputFile.is_open())
    {
        EditorConsole::Inst()->AddConsoleError("Failed to open file for writing: Config/EngineSettings.json");
        return;
    }

    flecs::entity_to_json_desc_t desc = ECS_ENTITY_TO_JSON_INIT;
    desc.serialize_values = true;
    desc.serialize_full_paths = true;
    desc.serialize_inherited = false;
    desc.serialize_builtin = false;
    desc.serialize_doc = false;
    desc.serialize_matches = false;
    desc.serialize_refs = false;
    desc.serialize_type_info = false;
    desc.serialize_entity_id = false;
    desc.serialize_alerts = false;

    std::string jsonContent = ("{\"results\":[");
    jsonContent.append(g_EngineSettingsEntity.to_json(&desc).c_str());
    jsonContent.append("]}");

    if (jsonContent.c_str())
    {
        outputFile << jsonContent;
        outputFile.close();

        EditorConsole::Inst()->AddConsoleMessage("Saved engine settings to Config/EngineSettings.json");
        return;
    }
    else
    {
        EditorConsole::Inst()->AddConsoleError("Failed to serialize engine settings to JSON");
        return;
    }

}

void LoadEngineSettings(flecs::world& world)
{
    ResourceLibrary::RegisterName("EngineSettings", "");

    std::ifstream inputFile("Config/EngineSettings.json");

    if (!inputFile.is_open())
    {
        EditorConsole::Inst()->AddConsoleError("Failed to open file for reading: Config/EngineSettings.json");

        g_EngineSettingsEntity = world.lookup("EngineSettings");
        if (!g_EngineSettingsEntity)
        {
            g_EngineSettingsEntity = world.entity("EngineSettings")
                .emplace<EditorSettings>()
                .emplace<GizmoSettings>()
                .emplace<AssetBrowserSettings>()
                .emplace<DebugDrawSettings>()
                .emplace<AudioMixerSettings>();
        }
        return;
    }

    std::string jsonContent((std::istreambuf_iterator<char>(inputFile)),
        std::istreambuf_iterator<char>());
    inputFile.close();

    const char* errorResult = world.from_json(jsonContent.c_str());

    g_EngineSettingsEntity = world.lookup("EngineSettings");
    if (!g_EngineSettingsEntity)
    {
        g_EngineSettingsEntity = world.entity("EngineSettings")
            .emplace<EditorSettings>()
            .emplace<GizmoSettings>()
            .emplace<AssetBrowserSettings>()
            .emplace<DebugDrawSettings>()
            .emplace<AudioMixerSettings>();
    }
}

void DrawEngineSettingsWindow(bool* p_open)
{
    if (p_open && !(*p_open)) return;

    if (!ImGui::Begin("Engine Settings", p_open, ImGuiWindowFlags_NoDocking)) { ImGui::End(); return; }

    EditorSettings* editor = g_EngineSettingsEntity.try_get_mut<EditorSettings>();
    GizmoSettings* gizmo = g_EngineSettingsEntity.try_get_mut<GizmoSettings>();
    AudioMixerSettings* audio = g_EngineSettingsEntity.try_get_mut<AudioMixerSettings>();
    AssetBrowserSettings* assets = g_EngineSettingsEntity.try_get_mut<AssetBrowserSettings>();
    DebugDrawSettings* debugDraw = g_EngineSettingsEntity.try_get_mut<DebugDrawSettings>();

    Cauda::Application& app = Cauda::Application::Get();

    if (ImGui::TreeNode("General Settings"))
    {
        if (editor)
        {
            static int selected_scene = 0;
            std::vector<const char*> scenes = ResourceLibrary::GetAllSceneHandles();
            scenes.emplace(scenes.begin(), "None");
            ImGui::Text("Default Scene:");
            ImGui::Separator();
            if (ImGui::Combo("##default_scene_combo", &selected_scene, scenes.data(), scenes.size()))
            {
                auto path = ResourceLibrary::GetScene(scenes[selected_scene]);
                editor->loadScenePath = path.string();
            }
            if (ImGui::Checkbox("vSync", &editor->vSync))
            {
                app.ToggleVsync();
            }
            ImGui::Checkbox("Use Orthographic for Editor Camera", &editor->orthographic);
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Gizmos"))
    {
        ImGui::Text("General settings here");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("AssetBrowser"))
    {
        ImGui::SliderFloat("Icon Size", &assets->iconSize, 32.0f, 128.0f);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            assets->iconSize = assets->iconSize;
            g_EngineSettingsEntity.modified<AssetBrowserSettings>();
        }
        ImGui::SliderInt("Icon Spacing", &assets->iconSpacing, 0, 32);
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            assets->iconSpacing = assets->iconSpacing;
            g_EngineSettingsEntity.modified<AssetBrowserSettings>();
        }
        if (ImGui::Checkbox("Stretch Spacing", &assets->stretchSpacing))
        {
            assets->stretchSpacing = assets->stretchSpacing;
            g_EngineSettingsEntity.modified<AssetBrowserSettings>();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Rendering"))
    {
        ImGui::Text("General settings here");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Audio"))
    {
        ImGui::Text("Audio Settings");
        ImGui::TreePop();
    }

    if (ImGui::Button("Save Settings"))
        SaveEngineSettings();

    ImGui::End();
}
