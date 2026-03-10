#include "cepch.h"
#include "LightingModule.h"
#include "Core/Editor/EditorModule.h"
#include "CameraModule.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

LightingModule::LightingModule(flecs::world& world)
    : m_world(world)
{
    m_editorModule = m_world.try_get_mut<EditorModule>();
    m_cameraModule = m_world.try_get_mut<CameraModule>();

    SetupComponents();
    SetupSystems();
    SetupObservers();
    SetupQueries();

    if (m_editorModule)
    {
        RegisterWithEditor();
    }

    Logger::PrintLog("LightingModule initialised successfully");
}

void LightingModule::SetupQueries()
{
    m_directionalLightQuery = m_world.query_builder<DirectionalLightComponent, TransformComponent>().build();
    m_mainLightQuery = m_world.query_builder<DirectionalLightComponent, TransformComponent>()
        .with<MainLight>()
        .build();
    m_ambientLightQuery = m_world.query_builder<AmbientLightComponent>().build();
    m_pointLightQuery = m_world.query_builder<PointLightComponent, TransformComponent>().build();
    m_spotLightQuery = m_world.query_builder<SpotLightComponent, TransformComponent>().build();
}

void LightingModule::SetupComponents()
{
    m_world.component<DirectionalLightComponent>("DirectionalLightComponent")
        .member<glm::vec3>("colour")
        .member<float>("intensity")
        .member<bool>("enabled")
        .member<bool>("castShadows")
        .member<float>("shadowBias");

    m_world.component<AmbientLightComponent>("AmbientLightComponent")
        .member<glm::vec3>("colour")
        .member<float>("intensity")
        .member<bool>("enabled");

    m_world.component<PointLightComponent>("PointLightComponent")
        .member<glm::vec3>("colour")
        .member<float>("intensity")
        .member<bool>("enabled")
        .member<float>("range")
        .member<float>("attenuation")
        .member<bool>("castShadows")
        .member<bool>("thicknessAware")
        .member<int>("shadowSteps")
        .member<float>("shadowBias");

    m_world.component<SpotLightComponent>("SpotLightComponent")
        .member<glm::vec3>("colour")
        .member<float>("intensity")
        .member<bool>("enabled")
        .member<float>("range")
        .member<float>("attenuation")
        .member<float>("innerConeAngle")
        .member<float>("outerConeAngle")
        .member<bool>("castShadows")
        .member<bool>("thicknessAware")
        .member<int>("shadowSteps")
        .member<float>("shadowBias");

    m_world.component<MainLight>("MainLight")
        .add(flecs::Exclusive);
}


void LightingModule::SetupSystems()
{
}

void LightingModule::SetupObservers()
{
}

void LightingModule::Update(float deltaTime)
{
}

void LightingModule::OnImGuiRender()
{
    ImGui::Begin("Lighting");

    glm::vec3 combinedAmbient = GetCombinedAmbientLight();
    ImGui::Text("Combined Ambient Light: (%.3f, %.3f, %.3f)", combinedAmbient.r, combinedAmbient.g, combinedAmbient.b);

    ImGui::Separator();
    ImGui::Spacing();

    auto mainLight = GetMainLight();
    if (mainLight)
    {
        ImGui::Text("Main Light: %s", mainLight.name().c_str());
        auto* lightComp = mainLight.try_get_mut<DirectionalLightComponent>();
        if (lightComp)
        {
            DrawDirectionalLightInspector(mainLight, *lightComp);
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "No main light assigned");
    }

    ImGui::End();
}


void LightingModule::RegisterWithEditor()
{
    if (m_editorModule)
    {
        m_editorModule->RegisterComponent<DirectionalLightComponent>(
            "Directional Light",
            "Lighting",
            [this](flecs::entity entity, DirectionalLightComponent& component)
            {
                DrawDirectionalLightInspector(entity, component);
            }
        );

        m_editorModule->RegisterComponent<AmbientLightComponent>(
            "Ambient Light",
            "Lighting",
            [this](flecs::entity entity, AmbientLightComponent& component)
            {
                DrawAmbientLightInspector(entity, component);
            }
        );

        m_editorModule->RegisterComponent<PointLightComponent>(
            "Point Light",
            "Lighting",
            [this](flecs::entity entity, PointLightComponent& component)
            {
                DrawPointLightInspector(entity, component);
            }
        );

        m_editorModule->RegisterComponent<SpotLightComponent>(
            "Spot Light",
            "Lighting",
            [this](flecs::entity entity, SpotLightComponent& component)
            {
                DrawSpotLightInspector(entity, component);
            }
        );

        m_editorModule->RegisterEntityColour([this](flecs::entity entity) -> ImVec4
        {
            return GetLightEntityColour(entity);
        });
    }
}

void LightingModule::DrawDirectionalLightInspector(flecs::entity entity, DirectionalLightComponent& component)
{
    if (ImGui::BeginTable("##directional_light_props", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Enabled");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##Enabled", &component.enabled);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Colour");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::ColorEdit3("##Colour", glm::value_ptr(component.colour));

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Intensity");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Intensity", &component.intensity, 0.0f, 50.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Cast Shadows");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##CastShadows", &component.castShadows);

        if (component.castShadows)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Shadow Bias");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat("##ShadowBias", &component.shadowBias, -0.01f, 0.01f, "%.4f");
        }

        ImGui::EndTable();
    }

    if (entity.has<TransformComponent>())
    {
        ImGui::Separator();
        glm::vec3 direction = GetDirectionFromTransform(entity);
        ImGui::Text("Direction: (%.2f, %.2f, %.2f)", direction.x, direction.y, direction.z);
    }
}

void LightingModule::DrawAmbientLightInspector(flecs::entity entity, AmbientLightComponent& component)
{
    if (ImGui::BeginTable("##ambient_light_props", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Enabled");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##Enabled", &component.enabled);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Colour");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::ColorEdit3("##Colour", glm::value_ptr(component.colour));

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Intensity");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Intensity", &component.intensity, 0.0f, 50.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Final Contribution");
        ImGui::TableNextColumn();
        if (component.enabled)
        {
            glm::vec3 finalColour = component.colour * component.intensity;
            ImGui::Text("(%.3f, %.3f, %.3f)", finalColour.r, finalColour.g, finalColour.b);
        }
        else
        {
            ImGui::Text("(disabled)");
        }

        ImGui::EndTable();
    }
}

void LightingModule::DrawPointLightInspector(flecs::entity entity, PointLightComponent& component)
{
    if (ImGui::BeginTable("##point_light_props", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Enabled");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##Enabled", &component.enabled);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Colour");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::ColorEdit3("##Colour", glm::value_ptr(component.colour));

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Intensity");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Intensity", &component.intensity, 0.0f, 50.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Range");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Range", &component.range, 0.1f, 50.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Attenuation");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Attenuation", &component.attenuation, -1.0f, 1.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Cast Shadows");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##CastShadows", &component.castShadows);

        if (component.castShadows)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Thickness Aware");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##ThicknessAware", &component.thicknessAware);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Shadow Steps");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderInt("##ShadowSteps", &component.shadowSteps, 8, 64);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Shadow Bias");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat("##ShadowBias", &component.shadowBias, 0.0001f, 0.00f, "%.4f");
        }

        ImGui::EndTable();
    }

    if (entity.has<TransformComponent>())
    {
        ImGui::Separator();
        auto* transform = entity.try_get<TransformComponent>();
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", transform->position.x, transform->position.y, transform->position.z);
    }
}

void LightingModule::DrawSpotLightInspector(flecs::entity entity, SpotLightComponent& component)
{
    if (ImGui::BeginTable("##spot_light_props", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Enabled");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##Enabled", &component.enabled);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Colour");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::ColorEdit3("##Colour", glm::value_ptr(component.colour));

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Intensity");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Intensity", &component.intensity, 0.0f, 50.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Range");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Range", &component.range, 0.1f, 50.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Attenuation");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Attenuation", &component.attenuation, 0.1f, 5.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Inner Cone Angle");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderFloat("##InnerCone", &component.innerConeAngle, 1.0f, 89.0f))
        {
            if (component.innerConeAngle >= component.outerConeAngle)
            {
                component.outerConeAngle = component.innerConeAngle + 1.0f;
            }
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Outer Cone Angle");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderFloat("##OuterCone", &component.outerConeAngle, 2.0f, 90.0f))
        {
            if (component.outerConeAngle <= component.innerConeAngle)
            {
                component.innerConeAngle = component.outerConeAngle - 1.0f;
            }
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Cast Shadows");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##CastShadows", &component.castShadows);

        if (component.castShadows)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Thickness Aware");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##ThicknessAware", &component.thicknessAware);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Shadow Steps");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderInt("##ShadowSteps", &component.shadowSteps, 8, 64);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Shadow Bias");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat("##ShadowBias", &component.shadowBias, 0.0001f, 0.01f, "%.4f");
        }

        ImGui::EndTable();
    }

    if (entity.has<TransformComponent>())
    {
        ImGui::Separator();
        auto* transform = entity.try_get<TransformComponent>();
        glm::vec3 direction = GetDirectionFromTransform(entity);
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", transform->position.x, transform->position.y, transform->position.z);
        ImGui::Text("Direction: (%.2f, %.2f, %.2f)", direction.x, direction.y, direction.z);
    }
}

ImVec4 LightingModule::GetLightEntityColour(flecs::entity entity)
{
    if (entity.has<DirectionalLightComponent>())
    {
        const auto* light = entity.try_get<DirectionalLightComponent>();
        bool isMainLight = entity.has<MainLight>();

        if (light->enabled)
        {
            if (isMainLight)
            {
                return ImVec4(0.85f, 0.65f, 0.25f, 1.0f);
            }
            else
            {
                return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); 
            }
        }
        else
        {
            return ImVec4(0.8f, 0.8f, 0.8f, 1.0f); 
        }
    }
    else if (entity.has<AmbientLightComponent>())
    {
        const auto* light = entity.try_get<AmbientLightComponent>();
        if (light->enabled)
        {
            return ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
        }
        else
        {
            return ImVec4(0.8f, 0.8f, 0.8f, 1.0f); 
        }
    }
    else if (entity.has<PointLightComponent>())
    {
        const auto* light = entity.try_get<PointLightComponent>();
        if (light->enabled)
        {
            return ImVec4(1.0f, 1.0f, 0.6f, 1.0f);
        }
        else
        {
            return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        }
    }
    else if (entity.has<SpotLightComponent>())
    {
        const auto* light = entity.try_get<SpotLightComponent>();
        if (light->enabled)
        {
            return ImVec4(1.0f, 0.6f, 1.0f, 1.0f); 
        }
        else
        {
            return ImVec4(0.8f, 0.8f, 0.8f, 1.0f); 
        }
    }

    return ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
}

flecs::entity LightingModule::GetMainLight()
{
    flecs::entity mainLight;

    m_mainLightQuery.each([&mainLight](flecs::entity e, DirectionalLightComponent& light, TransformComponent& transform)
    {
        mainLight = e;
        return false;
    });

    return mainLight;
}
void LightingModule::SetMainLight(flecs::entity lightEntity)
{
    if (!lightEntity || !lightEntity.has<DirectionalLightComponent>())
        return;

    lightEntity.add<MainLight>();
}

glm::vec3 LightingModule::GetCombinedAmbientLight()
{
    glm::vec3 combinedAmbient = glm::vec3(0.0f);

    m_ambientLightQuery.each([&combinedAmbient](flecs::entity e, AmbientLightComponent& light)
    {
        if (light.enabled)
        {
            combinedAmbient += light.colour * light.intensity;
        }
    });

    return combinedAmbient;
}

std::vector<flecs::entity> LightingModule::GetActivePointLights()
{
    std::vector<flecs::entity> lights;

    m_pointLightQuery.each([&lights](flecs::entity e, PointLightComponent& light, TransformComponent& transform)
    {
        if (light.enabled)
        {
            lights.push_back(e);
        }
    });

    return lights;
}

std::vector<flecs::entity> LightingModule::GetActiveSpotLights()
{
    std::vector<flecs::entity> lights;

    m_spotLightQuery.each([&lights](flecs::entity e, SpotLightComponent& light, TransformComponent& transform)
    {
        if (light.enabled)
        {
            lights.push_back(e);
        }
    });

    return lights;
}

glm::vec3 LightingModule::GetMainLightDirection()
{
    auto mainLight = GetMainLight();
    if (mainLight && mainLight.has<TransformComponent>())
    {
        return GetDirectionFromTransform(mainLight);
    }
    return glm::vec3(0.01f, -1.0f, 0.01f);
}

DirectionalLightComponent* LightingModule::GetMainLightComponent()
{
    auto mainLight = GetMainLight();
    if (mainLight)
    {
        return mainLight.try_get_mut<DirectionalLightComponent>();
    }
    return nullptr;
}

flecs::entity LightingModule::CreateDirectionalLight(const glm::vec3& direction, const glm::vec3& colour, float intensity, bool setAsMain, std::string name)
{
    auto entity = m_world.entity(name.c_str());

    entity.set<TransformComponent>({
        .position = glm::vec3(0.0f),
        .cachedEuler = glm::vec3(0),
        //.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        .scale = glm::vec3(1.0f) });

    entity.set<DirectionalLightComponent>({
        .colour = colour,
        .intensity = intensity,
        .enabled = true,
        .castShadows = true,
        .shadowBias = -0.0001f
        });

    SetDirectionFromVector(entity, direction);

    if (setAsMain)
    {
        SetMainLight(entity);
    }

    return entity;
}

flecs::entity LightingModule::CreateAmbientLight(const glm::vec3& colour, float intensity, std::string name)
{
    auto entity = m_world.entity(name.c_str());

    entity.set<TransformComponent>({
        .position = glm::vec3(0.0f),
        .cachedEuler = glm::vec3(0.0f, 0.0f, 0.0f),
        .scale = glm::vec3(1.0f) });

    entity.set<AmbientLightComponent>({
        .colour = colour,
        .intensity = intensity,
        .enabled = true
        });

    return entity;
}

flecs::entity LightingModule::CreatePointLight(const glm::vec3& position, const glm::vec3& colour, float intensity, float range, std::string name)
{
    auto entity = m_world.entity(name.c_str());

    entity.set<TransformComponent>({
        .position = position,
        .cachedEuler = glm::vec3(0.0f, 0.0f, 0.0f),
        .scale = glm::vec3(1.0f)
        });

    entity.set<PointLightComponent>({
        .colour = colour,
        .intensity = intensity,
        .enabled = true,
        .range = range,
        .attenuation = 0.01f,
        .castShadows = false,
        .thicknessAware = false,
        .shadowSteps = 64,
        .shadowBias = 0.001f
        });

    return entity;
}

flecs::entity LightingModule::CreateSpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& colour, float intensity, float range, float innerAngle, float outerAngle, std::string name)
{
    auto entity = m_world.entity(name.c_str());

    entity.set<TransformComponent>({
        .position = position,
        .cachedEuler = glm::vec3(0.0f, 0.0f, 0.0f),
        .scale = glm::vec3(1.0f)
        });

    entity.set<SpotLightComponent>({
        .colour = colour,
        .intensity = intensity,
        .enabled = true,
        .range = range,
        .attenuation = 1.0f,
        .innerConeAngle = innerAngle,
        .outerConeAngle = outerAngle,
        .castShadows = false,
        .thicknessAware = false,
        .shadowSteps = 16,
        .shadowBias = 0.001f
        });

    SetDirectionFromVector(entity, direction);

    return entity;
}

glm::vec3 LightingModule::GetDirectionFromTransform(flecs::entity entity)
{
    glm::quat worldRotation = Math::GetWorldRotation(entity);
    glm::vec3 forward = worldRotation * glm::vec3(0.0f, 0.0f, -1.0f);
    return glm::normalize(forward);
}

glm::mat4 LightingModule::CalculateLightSpaceMatrix()
{
    auto mainLight = GetMainLight();
    auto* transform = mainLight.try_get_mut<TransformComponent>();
    if (!transform)
        return glm::mat4(1.0f);
    auto* light = mainLight.try_get_mut<DirectionalLightComponent>();
    if (!light)
        return glm::mat4(1.0f);
    auto camera = m_cameraModule->GetMainCamera();
    if (!camera)
        return glm::mat4(1.0f);

    float lightDist = 1000.0f;

    float lightShadowSize = camera.try_get<CameraComponent>()->orthoSize * 4;
    glm::mat4 lightProjection = glm::ortho(-lightShadowSize, lightShadowSize, -lightShadowSize, lightShadowSize, 0.1f, lightDist * 2.0f);

    glm::vec3 cameraPos = camera.try_get<TransformComponent>()->position;
    glm::vec3 cameraForward = m_cameraModule->GetForwardVector(camera);
    glm::vec3 targetPos = cameraPos + (cameraForward * m_cameraModule->CalculateArmLengthToPlane(camera, glm::vec3(0), glm::vec3(0, 1, 0)));

    glm::vec3 lightDirection = GetDirectionFromTransform(mainLight);

    glm::vec3 lightPos = targetPos - (lightDirection * lightDist);

    glm::mat4 lightView = glm::lookAt(lightPos, targetPos, glm::vec3(0.0f, 1.0f, 0.0f));

    return lightProjection * lightView;
}

void LightingModule::SetDirectionFromVector(flecs::entity entity, const glm::vec3& direction)
{
    auto* transform = entity.try_get_mut<TransformComponent>();
    if (!transform) return;

    glm::vec3 normalizedDirection = glm::normalize(direction);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    if (abs(glm::dot(normalizedDirection, up)) > 0.999f) {
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::vec3 right = glm::normalize(glm::cross(up, normalizedDirection));
    glm::vec3 realUp = glm::cross(normalizedDirection, right);

    glm::mat3 rotationMatrix;
    rotationMatrix[0] = right;
    rotationMatrix[1] = realUp;
    rotationMatrix[2] = normalizedDirection;

    transform->rotation = glm::quat_cast(rotationMatrix);
    transform->cachedEuler = Math::GetEulerAngles(transform->rotation);
}