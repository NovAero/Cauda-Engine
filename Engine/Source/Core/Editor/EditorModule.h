#pragma once

#include "Core/Utilities/HistoryStack.h"
#include "Core/Math/TransformModule.h"

#include <ThirdParty/Flecs.h>
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include "imgui.h"
#include "imgui_internal.h"

struct ComponentMetadata 
{
    std::string name;
    std::string category;
    std::function<void(flecs::entity)> addToEntity;
    std::function<void(flecs::entity)> drawInspector;
    std::function<bool(flecs::entity)> hasComponent;
    std::function<bool(flecs::entity)> canRemove;
};

class FrameBuffer;
class CameraModule;
class ShaderProgram;
struct InputComponent;

namespace Cauda
{
    class EditorLayer;
    class ImGuiLayer;
}

struct NoGizmoTag {};

class EditorModule
{
    friend class Cauda::ImGuiLayer;
    friend class Cauda::EditorLayer;
private:

    enum class GizmoMode;
    enum class GizmoOperation;

public:
    EditorModule(flecs::world& world);
    ~EditorModule();

    void SetPlayingInEditor(bool PIE) { m_PIE = PIE; }
    bool IsPlayingInEditor() { return m_PIE; }
    bool IsSimulatingPhysics() { return m_simulatePhysics; }

    //void OnImGuiRender();
    void DrawHierarchy(bool* p_open = nullptr);
    void DrawInspector(bool* p_open = nullptr);

    bool SaveScene(const std::string& filePath);
    bool LoadScene(const std::string& filePath);
    void SetCurrentSceneName(const std::string& sceneName);
    const std::string& GetCurrentSceneName();

    void SetSceneRoot(flecs::entity root);
    void SetSelectedEntity(flecs::entity selected);

    flecs::entity GetSelectedEntity() const;
    void SelectEntity(flecs::entity entity);
    void CreateEntity(const std::string& name, flecs::entity parent = flecs::entity::null());
    void DeleteEntity(flecs::entity entity);
    void ReparentEntity(flecs::entity entity, flecs::entity newParent);

    flecs::entity DuplicateEntity(flecs::entity sourceEntity, flecs::entity parent);

    template<typename T>
    void RegisterComponent(const std::string& name,
        const std::string& category = "General",
        std::function<void(flecs::entity, T&)> customInspector = nullptr,
        std::function<bool(flecs::entity)> canRemove = nullptr,
        bool defaultOpen = false);

    void RegisterEntityColour(std::function<ImVec4(flecs::entity)> colorProvider);

    void DrawGrid(ImVec2 viewportPos, ImVec2 viewportSize, CameraModule* camera);

    GizmoMode GetGizmoMode() { return m_currentGizmoMode; }
    void CycleGizmoModes();

    glm::vec2 GetViewportPosition() const { return m_viewportPosition; }
    glm::vec2 GetViewportSize() const { return m_viewportSize; }
    glm::vec2 GetViewportTextureSize() const { return m_viewportTextureSize; }
    glm::vec2 GetViewportWindowPosition() const { return m_viewportWindowPosition; }

    void SetViewportInfo(const glm::vec2& position, const glm::vec2& size, const glm::vec2& textureSize, const glm::vec2& windowPos)
    {
        m_viewportPosition = position;
        m_viewportSize = size;
        m_viewportTextureSize = textureSize;
        m_viewportWindowPosition = windowPos;
    }

    void CreatePrefabFromEntity(flecs::entity target, std::string prefabName);
    flecs::entity CreateEntityFromPrefab(const std::string& desiredName, const std::string& prefabName);

private:

    void StripPrefabFromChildren(flecs::entity entity);

    enum class GizmoOperation
    {
        Translate = 0,
        Rotate = 1,
        Scale = 2
    };

    enum class GizmoMode
    {
        Local = 0,
        World = 1
    };

    flecs::world& m_world;
    flecs::entity m_sceneRoot;
    flecs::entity m_selectedEntity;


    std::string m_currentSceneName = "NewScene.json";
    char m_sceneName[MAX_NAME_STRING] = "Scene.json";
    bool m_disableSaveLoad = false;

    bool m_PIE = false;
    bool m_simulatePhysics = false;


    GizmoOperation m_currentGizmoOperation = GizmoOperation::Translate;
    GizmoMode m_currentGizmoMode = GizmoMode::World;
    bool m_gizmoEnabled = true;
    bool m_snapEnabled[3] = { true, true, true };
    float m_snapValues[3] = { 1.0f, 15.0f, 0.5f };

    bool m_gridEnabled = false;
    float m_gridSize = 10000.0f;
    glm::vec3 m_gridCenter = glm::vec3(0.0f);
    glm::vec3 m_gridNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    ImU32 m_gridColour = IM_COL32(200, 200, 200, 1);
    ImU32 m_gridAxisColour = IM_COL32(200, 200, 200, 10);

    glm::vec2 m_viewportPosition = glm::vec2(0.0f);
    glm::vec2 m_viewportSize = glm::vec2(0.0f);
    glm::vec2 m_viewportTextureSize = glm::vec2(0.0f);
    glm::vec2 m_viewportWindowPosition = glm::vec2(0.0f);

    bool m_isDuplicateManipulation = false;
    flecs::entity m_duplicatedEntity = flecs::entity::null();

    bool m_wasManipulating = false;
    bool m_manipulationDirty = false;
    TransformComponent m_cachedTransformForGizmo;

    std::vector<ComponentMetadata> m_registeredComponents;
    std::vector<std::function<ImVec4(flecs::entity)>> m_colourProviders;

    flecs::observer m_selectedEntityObserver;
    flecs::observer m_transformObserver;
    flecs::system m_orphanEntitySystem;

    ImGuiTreeNodeFlags m_baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_AllowItemOverlap;

    //void DrawHierarchy();
    //void DrawInspector();
    void DrawHierarchyAndInspector();
    void DrawViewportGizmoControls(ImVec2 viewportPos, ImVec2 viewportSize);

    void DrawEntityNode(flecs::entity entity, std::set<flecs::entity_t>& processedEntities, const ImGuiTextFilter& filter);
    void DrawAddComponentPopup();
    void DrawDefaultComponent(flecs::entity entity, flecs::id componentId);
    void ShowEntityProperties(flecs::entity entity);

    void HandleTransformGizmo(ImVec2 renderPos, ImVec2 renderSize, CameraModule* camera);

    ImVec4 GetEntityColour(flecs::entity entity);

    template<typename T>
    void DrawDefaultInspector(flecs::entity entity, T* component);

    template<typename T>
    void PushValueCommand(T* target, T* display);


    bool EntityOrChildMatchesFilter(flecs::entity entity, const ImGuiTextFilter& filter);
    bool CanReparentEntity(flecs::entity child, flecs::entity newParent);
};

#include "Core/Editor/EditorCommands.h"

template<typename T>
inline void EditorModule::RegisterComponent(const std::string& name,
    const std::string& category,
    std::function<void(flecs::entity, T&)> customInspector,
    std::function<bool(flecs::entity)> canRemove,
    bool defaultOpen)
{
    ComponentMetadata metadata;
    metadata.name = name;
    metadata.category = category;

    metadata.addToEntity = [this](flecs::entity entity)
        {
            Cauda::HistoryStack::Execute(new Cauda::AddComponentCommand<T>(entity));
        };

    metadata.hasComponent = [](flecs::entity entity)
        {
            return entity.has<T>();
        };

    if (canRemove)
    {
        metadata.canRemove = canRemove;
    }
    else
    {
        metadata.canRemove = [](flecs::entity entity) -> bool
            {
                return true;
            };
    }

    metadata.drawInspector = [this, name, customInspector, metadata, defaultOpen](flecs::entity entity)
        {
            if (!entity.has<T>()) return;

            ImGuiTreeNodeFlags flags = m_baseFlags;
            if (defaultOpen)
            {
                flags |= ImGuiTreeNodeFlags_DefaultOpen;
            }

            if (ImGui::CollapsingHeader(name.c_str(), flags))
            {
                T* component = entity.try_get_mut<T>();
                if (!component) return;

                ImGui::PushID(name.c_str());

                if (customInspector)
                {
                    customInspector(entity, *component);
                }
                else
                {
                    DrawDefaultInspector(entity, component);
                }

                ImGui::PopID();

                if (metadata.canRemove(entity))
                {
                    ImGui::Separator();
                    std::string removeLabel = "Remove " + name;
                    if (ImGui::Button(removeLabel.c_str()))
                    {
                        Cauda::HistoryStack::Execute(new Cauda::RemoveComponentCommand<T>(entity));
                    }
                }
            }
        };

    m_registeredComponents.push_back(metadata);
}

template<typename T>
inline void EditorModule::DrawDefaultInspector(flecs::entity entity, T* component)
{
    ImGui::Text("Component data (%zu bytes)", sizeof(T));
    ImGui::Text("No custom inspector provided");
}

template<typename T>
inline void EditorModule::PushValueCommand(T* target, T* display)
{
    Cauda::HistoryStack::Execute(new Cauda::EditorValueCommand<T>(target, display));
}
