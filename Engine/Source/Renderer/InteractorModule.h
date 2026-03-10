#pragma once

#include <glm/glm.hpp>
#include <imgui.h>
#include <ThirdParty/flecs.h>
#include "Core/Components/Components.h"
#include <memory>
#include <string>
#include <vector>

class EditorModule;
class UniformBuffer;

constexpr int MAX_INTERACTORS = 50;

enum class InteractorChannel : uint32_t
{
    Occlusion = 0,
    MagicEffects = 1,
    WaterRipples = 2,
    HeatZones = 3,
    ColdZones = 4,
    Detection = 5,
    Deformation = 6,
    Vegetation = 7
};

struct SphereInteractorComponent
{
    InteractorChannel channel = InteractorChannel::Occlusion;
    float radius = 5.0f;
    float intensity = 1.0f;
    float smoothness = 0.5f;
    bool enabled = true;
};

struct BoxInteractorComponent
{
    InteractorChannel channel = InteractorChannel::Occlusion;
    glm::vec3 size = glm::vec3(5.0f);
    float intensity = 1.0f;
    float smoothness = 0.5f;
    bool enabled = true;
};

struct InteractorData
{
    glm::vec4 positionRadius;       // xyz = position, w = radius 
    glm::vec4 sizeSmoothness;       // xyz = size, w = smoothness
    glm::mat4 rotationMatrix;       
    float intensity;
    uint32_t channel;               
    uint32_t shapeType;              
    uint32_t enabled;                
   // uint32_t _padding;
};

class InteractorModule
{
public:
    InteractorModule(flecs::world& world);
    ~InteractorModule();

    InteractorModule(const InteractorModule&) = delete;
    InteractorModule& operator=(const InteractorModule&) = delete;
    InteractorModule(InteractorModule&&) = default;
    InteractorModule& operator=(InteractorModule&&) = default;

    void SetupComponents();
    void SetupSystems();
    void SetupObservers();
    void SetupQueries();

    void Update(float deltaTime);
    void OnImGuiRender();

    flecs::entity CreateSphereInteractor(
        const glm::vec3& position,
        float radius = 5.0f,
        InteractorChannel channel = InteractorChannel::Occlusion,
        float intensity = 1.0f,
        std::string name = "sphere_interactor"
    );

    flecs::entity CreateBoxInteractor(
        const glm::vec3& position,
        const glm::vec3& size = glm::vec3(5.0f),
        const glm::vec3& rotation = glm::vec3(0.0f),
        InteractorChannel channel = InteractorChannel::Occlusion,
        float intensity = 1.0f,
        std::string name = "box_interactor"
    );

    std::vector<flecs::entity> GetActiveSphereInteractors();
    std::vector<flecs::entity> GetActiveBoxInteractors();
    std::vector<flecs::entity> GetAllActiveInteractors();
    std::vector<flecs::entity> GetInteractorsByChannel(InteractorChannel channel);

    const std::vector<InteractorData>& GetInteractorData() const { return m_interactorData; }
    int GetActiveInteractorCount() const { return m_activeInteractorCount; }

    void UpdateInteractorBuffer();

    void RegisterWithEditor();

private:
    flecs::world& m_world;
    EditorModule* m_editorModule = nullptr;

    flecs::query<SphereInteractorComponent, TransformComponent> m_sphereInteractorQuery;
    flecs::query<BoxInteractorComponent, TransformComponent> m_boxInteractorQuery;

    std::vector<InteractorData> m_interactorData;
    int m_activeInteractorCount = 0;

    UniformBuffer* m_interactorUBO = nullptr;

    float m_globalSmoothness = 0.5f;
    bool m_enableInteraction = true;

    void DrawSphereInteractorInspector(flecs::entity entity, SphereInteractorComponent& component);
    void DrawBoxInteractorInspector(flecs::entity entity, BoxInteractorComponent& component);

    ImVec4 GetInteractorEntityColour(flecs::entity entity);
    void CollectInteractorData();
    const char* GetChannelName(InteractorChannel channel);
    ImVec4 GetChannelColour(InteractorChannel channel);
};