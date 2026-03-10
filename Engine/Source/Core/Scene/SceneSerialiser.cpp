#include "cepch.h"
#include "SceneSerialiser.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Scene/CameraModule.h"
#include "Core/Physics/PhysicsModule.h"
#include "Core/Physics/ConstraintModule.h"
#include "Core/Character/CharacterModule.h"
#include "Core/Scene/GamemodeModule.h"
#include "Platform/Win32/Application.h"

#include <fstream>
#include <ThirdParty/Flecs.h>

namespace SceneIO
{
	bool LoadSceneFromFile(std::string& filePath, flecs::world& world, flecs::entity sceneRoot)
	{
        try
        {
            std::ifstream inputFile(filePath);
            if (!inputFile.is_open())
            {
                EditorConsole::Inst()->AddConsoleError("Failed to open file for reading: " + filePath);
                return false;
            }

            sceneRoot.destruct();

            if (auto transformModule = world.try_get_mut<TransformModule>())
            {
                transformModule->ResetSpawnPointIDs();
            }

            if (auto constraintModule = world.try_get_mut<ConstraintModule>())
            {
                constraintModule->CleanupManagedConstraints();
            }

            std::string jsonContent((std::istreambuf_iterator<char>(inputFile)),
                std::istreambuf_iterator<char>());
            inputFile.close();

            const char* errorResult = world.from_json(jsonContent.c_str());

            auto newSceneRoot = world.lookup("Root");
            if (!newSceneRoot) return false;

            ResourceLibrary::ResetUsednames();

            auto editorModule = world.try_get_mut<EditorModule>();
            auto cameraModule = world.try_get_mut<CameraModule>();
            auto physicsModule = world.try_get_mut<PhysicsModule>();
            auto characterModule = world.try_get_mut<CharacterModule>();

            flecs::entity mainCamera;
            bool setMainCam = false;

            int count = 0;

            newSceneRoot.children([&](flecs::entity child)
                {
                    if (!setMainCam && child.has<CameraComponent>())
                    {
                        setMainCam = true;
                        child.add<MainCamera>();
                        mainCamera = child;
                    }
                    count++;
                });

            if (!mainCamera)
            {
                mainCamera = world.lookup("MainCamera");
            }

            cameraModule->SetMainCamera(mainCamera);
            editorModule->SetSceneRoot(newSceneRoot);

            physicsModule->ResolveUnboundEntities();
            characterModule->ResolveUnboundEntities();

            if (errorResult && *errorResult != '\0')
            {
                EditorConsole::Inst()->AddConsoleError("Failed to load scene from JSON. Error at: " + *errorResult);
                return false;
            }

            std::string logText = "Scene loaded from ";
            logText.append(filePath);
            EditorConsole::Inst()->AddConsoleMessage(logText.c_str());

            SDL_PushEvent(&EditorEvents::g_RegisterSceneEvent);
            //Cauda::Application::GetEditorLayer()->RegisterSceneEntityNames();
        }
        catch (const std::exception& e)
        {
            EditorConsole::Inst()->AddConsoleError("Load failed: " + *e.what());
            std::cerr << "Load failed: " << e.what() << std::endl;
            return false;
        }

		return true;
	}

	bool SaveSceneToFile(std::string& filePath, flecs::world& world)
	{
        try
        {
            std::ofstream outputFile(filePath);
            if (!outputFile.is_open())
            {
                EditorConsole::Inst()->AddConsoleError("Failed to open file for writing: " + filePath);
                return false;
            }

            auto sceneRoot = world.lookup("Root");
            if (!sceneRoot) return false;
            
            world.remove_all<SelectedTag>();

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

            SerialiseSceneGamemode(world, desc, jsonContent);

            jsonContent.append(", ");
            
            SerialiseEntityBranch(sceneRoot, desc, jsonContent);

            jsonContent.erase(jsonContent.find_last_of(","));
            jsonContent.append("]}");

            if (jsonContent.c_str())
            {
                outputFile << jsonContent;
                outputFile.close();

                EditorConsole::Inst()->AddConsoleMessage("Scene saved to " + filePath);
                return true;
            }
            else
            {
                EditorConsole::Inst()->AddConsoleError("Failed to serialize world to JSON");
                return false;
            }

        }
        catch (const std::exception& e)
        {
            EditorConsole::Inst()->AddConsoleError("Save failed: " + *e.what());
            return false;
        }
	}
    
    void SerialiseSceneGamemode(flecs::world& world, flecs::entity_to_json_desc_t& desc, std::string& content)
    {
        if (auto singleton = world.singleton<GamemodeSingleton>())
            content.append(singleton.to_json(&desc).c_str());
    }

    void SerialiseEntityBranch(flecs::entity entity, flecs::entity_to_json_desc_t& desc, std::string& content)
    {
        if (entity.has(flecs::IsA, flecs::Wildcard))
        {
            entity.remove(flecs::IsA, flecs::Wildcard);
        }

        if (entity.has<TransientEntity>())
        {
            return; 
        }

        content.append(entity.to_json(&desc).c_str());
        content.append(",");

        if (!entity.has(flecs::Prefab))
        {
            std::vector<flecs::entity> children;
            entity.children([&](flecs::entity child)
                {
                    children.push_back(child);
                });

            for (auto child : children)
            {
                SerialiseEntityBranch(child, desc, content);
            }
        }
        else
        {
            std::vector<flecs::entity> children;
            entity.children([&](flecs::entity child)
                {
                    children.push_back(child);
                });

            for (auto child : children)
            {
                child.add(flecs::Prefab);
                SerialiseEntityBranch(child, desc, content);
            }
        }
    }
}