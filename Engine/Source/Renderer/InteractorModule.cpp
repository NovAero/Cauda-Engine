#include "cepch.h"
#include "InteractorModule.h"
#include "Core/Editor/EditorModule.h"
#include "Renderer/UniformBuffer.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

constexpr int INTERACTOR_UBO_BINDING = 5;

InteractorModule::InteractorModule(flecs::world& world)
    : m_world(world)
{
    m_editorModule = m_world.try_get_mut<EditorModule>();

    SetupComponents();
    SetupSystems();
    SetupObservers();
    SetupQueries();

    m_interactorUBO = new UniformBuffer();
    m_interactorUBO->Bind();
    m_interactorUBO->BufferData(sizeof(InteractorData) * MAX_INTERACTORS + sizeof(int) * 4);
    m_interactorUBO->BindBufferBase(INTERACTOR_UBO_BINDING);
    m_interactorUBO->Unbind();

    m_interactorData.reserve(MAX_INTERACTORS);

    if (m_editorModule)
    {
        RegisterWithEditor();
    }

    Logger::PrintLog("InteractorModule initialised successfully");
}

InteractorModule::~InteractorModule()
{
    delete m_interactorUBO;
}

void InteractorModule::SetupComponents()
{
    m_world.component<SphereInteractorComponent>("SphereInteractorComponent")
        .member<InteractorChannel>("channel")
        .member<float>("radius")
        .member<float>("intensity")
        .member<float>("smoothness")
        .member<bool>("enabled");

    m_world.component<BoxInteractorComponent>("BoxInteractorComponent")
        .member<InteractorChannel>("channel")
        .member<glm::vec3>("size")
        .member<float>("intensity")
        .member<float>("smoothness")
        .member<bool>("enabled");
}

void InteractorModule::SetupSystems()
{
    m_world.system<>("UpdateInteractorSystem")
        .kind(flecs::OnStore)
        .run([this](flecs::iter& it)
        {
            UpdateInteractorBuffer();
        });
}

void InteractorModule::SetupObservers()
{
}

void InteractorModule::SetupQueries()
{
    m_sphereInteractorQuery = m_world.query_builder<SphereInteractorComponent, TransformComponent>().build();
    m_boxInteractorQuery = m_world.query_builder<BoxInteractorComponent, TransformComponent>().build();
}

void InteractorModule::Update(float deltaTime)
{
   
}

void InteractorModule::OnImGuiRender()
{
    ImGui::Begin("Shader Interactions");

    ImGui::Checkbox("Enable Interactions", &m_enableInteraction);
    ImGui::SliderFloat("Global Smoothness", &m_globalSmoothness, 0.0f, 1.0f);

    ImGui::Separator();
    ImGui::Text("Active Interactors: %d / %d", m_activeInteractorCount, MAX_INTERACTORS);

    if (m_activeInteractorCount >= MAX_INTERACTORS)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
            "Warning: Maximum interactor limit reached!");
    }

    ImGui::Separator();

    if (ImGui::TreeNode("Sphere Interactors"))
    {
        auto spheres = GetActiveSphereInteractors();
        for (auto& entity : spheres)
        {
            if (ImGui::TreeNode(entity.name().c_str()))
            {
                auto* comp = entity.try_get_mut<SphereInteractorComponent>();
                auto* transform = entity.try_get_mut<TransformComponent>();

                if (comp && transform)
                {
                    ImVec4 channelCol = GetChannelColour(comp->channel);
                    ImGui::TextColored(channelCol, "Channel: %s", GetChannelName(comp->channel));
                    ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                        transform->position.x, transform->position.y, transform->position.z);
                    ImGui::SliderFloat("Radius", &comp->radius, 0.1f, 50.0f);
                    ImGui::SliderFloat("Intensity", &comp->intensity, 0.0f, 10.0f);
                    ImGui::SliderFloat("Smoothness", &comp->smoothness, 0.0f, 1.0f);
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Box Interactors"))
    {
        auto boxes = GetActiveBoxInteractors();
        for (auto& entity : boxes)
        {
            if (ImGui::TreeNode(entity.name().c_str()))
            {
                auto* comp = entity.try_get_mut<BoxInteractorComponent>();
                auto* transform = entity.try_get_mut<TransformComponent>();

                if (comp && transform)
                {
                    ImVec4 channelCol = GetChannelColour(comp->channel);
                    ImGui::TextColored(channelCol, "Channel: %s", GetChannelName(comp->channel));
                    ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                        transform->position.x, transform->position.y, transform->position.z);
                    ImGui::DragFloat3("Size", glm::value_ptr(comp->size), 0.1f, 0.1f, 50.0f);
                    ImGui::SliderFloat("Intensity", &comp->intensity, 0.0f, 10.0f);
                    ImGui::SliderFloat("Smoothness", &comp->smoothness, 0.0f, 1.0f);
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    ImGui::End();
}

void InteractorModule::RegisterWithEditor()
{
    if (m_editorModule)
    {
        m_editorModule->RegisterComponent<SphereInteractorComponent>(
            "Sphere Interactor",
            "Shader Interaction",
            [this](flecs::entity entity, SphereInteractorComponent& component)
            {
                DrawSphereInteractorInspector(entity, component);
            }
        );

        m_editorModule->RegisterComponent<BoxInteractorComponent>(
            "Box Interactor",
            "Shader Interaction",
            [this](flecs::entity entity, BoxInteractorComponent& component)
            {
                DrawBoxInteractorInspector(entity, component);
            }
        );

        m_editorModule->RegisterEntityColour([this](flecs::entity entity) -> ImVec4
            {
                return GetInteractorEntityColour(entity);
            });
    }
}

void InteractorModule::DrawSphereInteractorInspector(flecs::entity entity, SphereInteractorComponent& component)
{
    if (ImGui::BeginTable("##sphere_interactor_props", 2, ImGuiTableFlags_Resizable))
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
        ImGui::TextUnformatted("Channel");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        int channelValue = (int)component.channel;
        if (ImGui::Combo("##Channel", &channelValue,
            "Occlusion\0Magic Effects\0Water Ripples\0Heat Zones\0Cold Zones\0Detection\0Deformation\0Vegetation\0"))
        {
            component.channel = (InteractorChannel)channelValue;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Radius");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Radius", &component.radius, 0.1f, 500.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Intensity");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Intensity", &component.intensity, 0.0f, 10.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Smoothness");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Smoothness", &component.smoothness, 0.0f, 1.0f);

        ImGui::EndTable();
    }

    if (entity.has<TransformComponent>())
    {
        ImGui::Separator();
        auto* transform = entity.try_get<TransformComponent>();
        ImGui::Text("Position: (%.2f, %.2f, %.2f)",
            transform->position.x, transform->position.y, transform->position.z);
    }
}

void InteractorModule::DrawBoxInteractorInspector(flecs::entity entity, BoxInteractorComponent& component)
{
    if (ImGui::BeginTable("##box_interactor_props", 2, ImGuiTableFlags_Resizable))
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
        ImGui::TextUnformatted("Channel");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        int channelValue = (int)component.channel;
        if (ImGui::Combo("##Channel", &channelValue,
            "Occlusion\0Magic Effects\0Water Ripples\0Heat Zones\0Cold Zones\0Detection\0Deformation\0Vegetation\0"))
        {
            component.channel = (InteractorChannel)channelValue;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Size");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::DragFloat3("##Size", glm::value_ptr(component.size), 0.1f, 0.1f, 50.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Intensity");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Intensity", &component.intensity, 0.0f, 10.0f);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Smoothness");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Smoothness", &component.smoothness, 0.0f, 1.0f);

        ImGui::EndTable();
    }

    if (entity.has<TransformComponent>())
    {
        ImGui::Separator();
        auto* transform = entity.try_get<TransformComponent>();
        ImGui::Text("Position: (%.2f, %.2f, %.2f)",
            transform->position.x, transform->position.y, transform->position.z);
        ImGui::Text("Rotation: (%.2f, %.2f, %.2f)",
            transform->cachedEuler.x, transform->cachedEuler.y, transform->cachedEuler.z);
    }
}

const char* InteractorModule::GetChannelName(InteractorChannel channel)
{
    switch (channel)
    {
    case InteractorChannel::Occlusion: return "Occlusion";
    case InteractorChannel::MagicEffects: return "Magic Effects";
    case InteractorChannel::WaterRipples: return "Water Ripples";
    case InteractorChannel::HeatZones: return "Heat Zones";
    case InteractorChannel::ColdZones: return "Cold Zones";
    case InteractorChannel::Detection: return "Detection";
    case InteractorChannel::Deformation: return "Deformation";
    case InteractorChannel::Vegetation: return "Vegetation";
    default: return "Custom";
    }
}

ImVec4 InteractorModule::GetChannelColour(InteractorChannel channel)
{
    switch (channel)
    {
    case InteractorChannel::Occlusion: return ImVec4(0.6f, 0.4f, 0.2f, 1.0f);
    case InteractorChannel::MagicEffects: return ImVec4(0.8f, 0.2f, 1.0f, 1.0f);
    case InteractorChannel::WaterRipples: return ImVec4(0.2f, 0.5f, 1.0f, 1.0f);
    case InteractorChannel::HeatZones: return ImVec4(1.0f, 0.3f, 0.0f, 1.0f);
    case InteractorChannel::ColdZones: return ImVec4(0.5f, 0.8f, 1.0f, 1.0f);
    case InteractorChannel::Detection: return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    case InteractorChannel::Deformation: return ImVec4(0.5f, 0.5f, 0.3f, 1.0f);
    case InteractorChannel::Vegetation: return ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
    default: return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    }
}

ImVec4 InteractorModule::GetInteractorEntityColour(flecs::entity entity)
{
    if (entity.has<SphereInteractorComponent>())
    {
        const auto* interactor = entity.try_get<SphereInteractorComponent>();
        if (interactor->enabled)
        {
            return GetChannelColour(interactor->channel);
        }
        else
        {
            return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        }
    }
    else if (entity.has<BoxInteractorComponent>())
    {
        const auto* interactor = entity.try_get<BoxInteractorComponent>();
        if (interactor->enabled)
        {
            return GetChannelColour(interactor->channel);
        }
        else
        {
            return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        }
    }

    return ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
}

flecs::entity InteractorModule::CreateSphereInteractor(
    const glm::vec3& position,
    float radius,
    InteractorChannel channel,
    float intensity,
    std::string name)
{
    auto entity = m_world.entity(name.c_str());

    entity.set<TransformComponent>({
        .position = position,
        .cachedEuler = glm::vec3(0.0f),
        .scale = glm::vec3(1.0f)
        });

    entity.set<SphereInteractorComponent>({
        .channel = channel,
        .radius = radius,
        .intensity = intensity,
        .smoothness = m_globalSmoothness,
        .enabled = true
        });

    return entity;
}

flecs::entity InteractorModule::CreateBoxInteractor(
    const glm::vec3& position,
    const glm::vec3& size,
    const glm::vec3& rotation,
    InteractorChannel channel,
    float intensity,
    std::string name)
{
    auto entity = m_world.entity(name.c_str());

    entity.set<TransformComponent>({
        .position = position,
        .cachedEuler = rotation,
        .scale = glm::vec3(1.0f)
        });

    entity.set<BoxInteractorComponent>({
        .channel = channel,
        .size = size,
        .intensity = intensity,
        .smoothness = m_globalSmoothness,
        .enabled = true
        });

    return entity;
}

std::vector<flecs::entity> InteractorModule::GetActiveSphereInteractors()
{
    std::vector<flecs::entity> interactors;

    m_sphereInteractorQuery.each([&interactors](flecs::entity e,
        SphereInteractorComponent& comp, TransformComponent& transform)
        {
            if (comp.enabled)
            {
                interactors.push_back(e);
            }
        });

    return interactors;
}

std::vector<flecs::entity> InteractorModule::GetActiveBoxInteractors()
{
    std::vector<flecs::entity> interactors;

    m_boxInteractorQuery.each([&interactors](flecs::entity e,
        BoxInteractorComponent& comp, TransformComponent& transform)
        {
            if (comp.enabled)
            {
                interactors.push_back(e);
            }
        });

    return interactors;
}

std::vector<flecs::entity> InteractorModule::GetAllActiveInteractors()
{
    std::vector<flecs::entity> all;
    auto spheres = GetActiveSphereInteractors();
    auto boxes = GetActiveBoxInteractors();

    all.insert(all.end(), spheres.begin(), spheres.end());
    all.insert(all.end(), boxes.begin(), boxes.end());

    return all;
}

std::vector<flecs::entity> InteractorModule::GetInteractorsByChannel(InteractorChannel channel)
{
    std::vector<flecs::entity> interactors;

    m_sphereInteractorQuery.each([&](flecs::entity e, SphereInteractorComponent& comp, TransformComponent& transform)
        {
            if (comp.enabled && comp.channel == channel)
            {
                interactors.push_back(e);
            }
        });

    m_boxInteractorQuery.each([&](flecs::entity e, BoxInteractorComponent& comp, TransformComponent& transform)
        {
            if (comp.enabled && comp.channel == channel)
            {
                interactors.push_back(e);
            }
        });

    return interactors;
}

void InteractorModule::CollectInteractorData()
{
    m_interactorData.clear();
    m_activeInteractorCount = 0;

    m_sphereInteractorQuery.each([this](flecs::entity e,
        SphereInteractorComponent& comp, TransformComponent& transform)
        {
            if (!comp.enabled || m_activeInteractorCount >= MAX_INTERACTORS) return;

            InteractorData data{};
            data.positionRadius = glm::vec4(transform.position, comp.radius);
            data.sizeSmoothness = glm::vec4(comp.radius, comp.radius, comp.radius, comp.smoothness);
            data.rotationMatrix = glm::mat4(1.0f);
            data.intensity = comp.intensity;
            data.channel = static_cast<uint8_t>(comp.channel);
            data.shapeType = 0; // Sphere
            data.enabled = comp.enabled ? 1 : 0;

            m_interactorData.push_back(data);
            m_activeInteractorCount++;
        });

    m_boxInteractorQuery.each([this](flecs::entity e,
        BoxInteractorComponent& comp, TransformComponent& transform)
        {
            if (!comp.enabled || m_activeInteractorCount >= MAX_INTERACTORS) return;

            InteractorData data{};
            data.positionRadius = glm::vec4(transform.position, 0.0f);
            data.sizeSmoothness = glm::vec4(comp.size, comp.smoothness);
            data.rotationMatrix = glm::mat4_cast(transform.rotation);
            data.intensity = comp.intensity;
            data.channel = static_cast<uint8_t>(comp.channel);
            data.shapeType = 1; // Box
            data.enabled = comp.enabled ? 1 : 0;

            m_interactorData.push_back(data);
            m_activeInteractorCount++;
        });
}

void InteractorModule::UpdateInteractorBuffer()
{
    CollectInteractorData();

    if (!m_enableInteraction)
    {
        m_interactorData.clear();
        m_activeInteractorCount = 0;
    }

    const size_t arraySize = sizeof(InteractorData) * MAX_INTERACTORS;
    const size_t settingsSize = sizeof(int) * 4;
    const size_t totalBufferSize = arraySize + settingsSize;

    std::vector<uint8_t> bufferData(totalBufferSize, 0);

    if (!m_interactorData.empty())
    {
        memcpy(bufferData.data(), m_interactorData.data(),
            sizeof(InteractorData) * m_interactorData.size());
    }

    int settings[4] = {
        m_activeInteractorCount,
        m_enableInteraction ? 1 : 0,
        0, 0
    };

    memcpy(bufferData.data() + arraySize, settings, settingsSize);

    m_interactorUBO->Bind();
    m_interactorUBO->BufferSubData(0, totalBufferSize, bufferData.data());
    m_interactorUBO->Unbind();
}