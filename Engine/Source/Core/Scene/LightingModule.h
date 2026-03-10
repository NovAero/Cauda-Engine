#pragma once

#include <glm/glm.hpp>
#include <imgui.h>
#include <ThirdParty/flecs.h>
#include "Core/Components/Components.h"
#include <memory>
#include <string>

class EditorModule;
class CameraModule;

struct DirectionalLightComponent
{
    glm::vec3 colour = glm::vec3(1.0f);
    float intensity = 1.0f;
    bool enabled = true;
    bool castShadows = true;
    float shadowBias = -0.0001f;
};

struct AmbientLightComponent 
{
    glm::vec3 colour = glm::vec3(1.0f);
    float intensity = 1.0f;
    bool enabled = true;
};

struct PointLightComponent
{
    glm::vec3 colour = glm::vec3(1.0f);
    float intensity = 5.0f;
    bool enabled = true;
    float range = 10.0f;
    float attenuation = 1.0f;

    bool castShadows = true;
    bool thicknessAware = true;
    int shadowSteps = 32;
    float shadowBias = 0.001f;
};

struct SpotLightComponent
{
    glm::vec3 colour = glm::vec3(1.0f);
    float intensity = 5.0f;
    bool enabled = true;
    float range = 15.0f;
    float attenuation = 1.0f;
    float innerConeAngle = 25.0f;
    float outerConeAngle = 30.0f;

    bool castShadows = true;
    bool thicknessAware = true;
    int shadowSteps = 32;
    float shadowBias = 0.001f;
};

struct MainLight {};

class LightingModule
{
public:
    LightingModule(flecs::world& world);
    ~LightingModule() = default;

    LightingModule(const LightingModule&) = delete;
    LightingModule& operator=(const LightingModule&) = delete;
    LightingModule(LightingModule&&) = default;
    LightingModule& operator=(LightingModule&&) = default;

    void SetupComponents();
    void SetupSystems();
    void SetupObservers();
    void SetupQueries();

    void Update(float deltaTime);
    void OnImGuiRender();


    flecs::entity CreateDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity = 1.0f, bool setAsMain = true, std::string name = "directional_light");
    flecs::entity CreateAmbientLight(const glm::vec3& color, float intensity = 1.0f, std::string name = "ambient_light");
    flecs::entity CreatePointLight(const glm::vec3& position, const glm::vec3& color, float intensity = 1.0f, float range = 10.0f, std::string name = "point_light");
    flecs::entity CreateSpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity = 1.0f, float range = 10.0f, float innerAngle = 30.0f, float outerAngle = 45.0f, std::string name = "spot_light");

    void SetMainLight(flecs::entity lightEntity);

    flecs::entity GetMainLight();
    DirectionalLightComponent* GetMainLightComponent();
    glm::vec3 GetMainLightDirection();

    glm::vec3 GetCombinedAmbientLight();

    std::vector<flecs::entity> GetActivePointLights();
    std::vector<flecs::entity> GetActiveSpotLights();

 

    glm::vec3 GetDirectionFromTransform(flecs::entity entity);

    void RegisterWithEditor();

    glm::mat4 CalculateLightSpaceMatrix();

private:
    flecs::world& m_world;
    EditorModule* m_editorModule = nullptr;
    CameraModule* m_cameraModule = nullptr;

    flecs::query<DirectionalLightComponent, TransformComponent> m_directionalLightQuery;
    flecs::query<DirectionalLightComponent, TransformComponent> m_mainLightQuery;
    flecs::query<AmbientLightComponent> m_ambientLightQuery;
    flecs::query<PointLightComponent, TransformComponent> m_pointLightQuery;
    flecs::query<SpotLightComponent, TransformComponent> m_spotLightQuery;

    void SetDirectionFromVector(flecs::entity entity, const glm::vec3& direction);

    void DrawDirectionalLightInspector(flecs::entity entity, DirectionalLightComponent& component);
    void DrawAmbientLightInspector(flecs::entity entity, AmbientLightComponent& component);
    void DrawPointLightInspector(flecs::entity entity, PointLightComponent& component);
    void DrawSpotLightInspector(flecs::entity entity, SpotLightComponent& component);

    ImVec4 GetLightEntityColour(flecs::entity entity);
};