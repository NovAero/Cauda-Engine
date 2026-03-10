#include "cepch.h"
#include "TransformModule.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Physics/PhysicsModule.h"

TransformModule::TransformModule(flecs::world& world) : m_world(world)
{
	SetupComponents();
	SetupSystems();
	SetupObservers();
    SetupQueries();

    m_world.import<EditorModule>();
    m_editorModule = m_world.try_get_mut<EditorModule>();

    RegisterWithEditor();

    EditorConsole::Inst()->AddConsoleInfo("TransformModule initialised successfully", true);
}

void TransformModule::ResetSpawnPointIDs()
{
    m_nextSpawnPointID = 0;
}

void TransformModule::SetupComponents()
{
    m_world.component<TransformComponent>("TransformComponent")
        .member<glm::vec3>("position")
        .member<glm::vec3>("rotation")
        .member<glm::vec3>("scale");

    m_world.component<SelectedTag>()
        .add(flecs::Exclusive);

    m_world.component<SpawnPoint>()
        .member<int>("spawnPointID");

    m_world.component<SpawnLobby>();
}

void TransformModule::SetupSystems()
{

}

void TransformModule::SetupObservers()
{
    m_transformSetObserver = m_world.observer<TransformComponent>()
        .event(flecs::OnSet)
        .each([this](flecs::entity entity, TransformComponent& transform)
        {
            //std::string log = "Set transform for entity: " + std::string(entity.name());
           // Logger::PrintLog(log.c_str());

            // Checks if current rotation quaternion is different from the one produced by the cached euler angles.
            glm::quat eulerQuat = Math::EulerToQuat(transform.cachedEuler);
            if (Math::IsEqual(transform.rotation, eulerQuat))
                return;

            //std::cout << "Unmodified: " << CEMath::ToString(transform.rotation) << std::endl;
            //std::cout << "Modified: " << CEMath::ToString(eulerQuat) << std::endl;

            // Update rotation quaternion to match cached euler.
            transform.rotation = eulerQuat;
        });

    m_world.observer<SpawnPoint>()
        .event(flecs::OnSet)
        .each([&](flecs::entity spawner, SpawnPoint& component)
            {
                component.spawnPointID = m_nextSpawnPointID;
                std::cout << "Spawn ID " << m_nextSpawnPointID << '\n';
                m_nextSpawnPointID++;
            });

    m_world.observer<SpawnPoint>()
        .event(flecs::OnRemove)
        .each([&](flecs::entity spawner, SpawnPoint& component)
            {
                m_nextSpawnPointID = 0;
                auto query = m_world.query<SpawnPoint>();
                query.each([&](flecs::entity entity, SpawnPoint& component)
                    {
                        component.spawnPointID = m_nextSpawnPointID;
                        m_nextSpawnPointID++;
                    });
            });

    m_world.observer<TransformComponent>()
        .event<OnPhysicsUpdateTransform>()
        .each([this](flecs::entity entity, TransformComponent& transform)
            {
                // Update cached euler to match rotation quaternion.
                glm::vec3 euler = Math::GetEulerAngles(transform.rotation);

                transform.cachedEuler = euler;
            });

    m_selectEntityObserver = m_world.observer<SelectedTag>("OnSelectEntity")
        .event(flecs::OnSet)
        .with(flecs::Prefab).optional()
        .each([this](flecs::entity entity, SelectedTag)
        {
            if (!entity.is_alive()) return;
            m_editorModule->SetSelectedEntity(entity);

            if (entity.has<TransformComponent>())
            {
                const auto* transform = entity.try_get<TransformComponent>();
                m_displayedTransform = *transform;
            }
        });
}

void TransformModule::SetupQueries()
{

}



void TransformModule::DrawTransformComponent(flecs::entity entity, TransformComponent& transform)
{
    auto physicsModule = m_world.try_get_mut<PhysicsModule>();
    static bool startedEditing = false;
    static bool isEditing = false;
    static bool finishedEditing = false;

    if (!isEditing)
    {
        m_displayedTransform = transform;
    }

    if (ImGui::BeginTable("##transform_props", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("World Position");

        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::BeginDisabled();
        glm::vec3 worldPos = Math::GetWorldPosition(entity);
        ImGui::DragFloat3("##WorldPosition", glm::value_ptr(worldPos));
        ImGui::EndDisabled();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Local Position");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);

        if (ImGui::DragFloat3("##LocalPosition", glm::value_ptr(m_displayedTransform.position), 0.1f))
        {
            if (!startedEditing && !isEditing)
            {
                startedEditing = true;
            }
        }
        else if (ImGui::IsItemDeactivated() && isEditing)
        {
            isEditing = false;
            finishedEditing = true;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Rotation");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);

        if (ImGui::DragFloat3("##Rotation", glm::value_ptr(m_displayedTransform.cachedEuler), 1.0f))
        {
            if (!startedEditing && !isEditing)
            {
                startedEditing = true;
            }

            m_displayedTransform.rotation = Math::EulerToQuat(m_displayedTransform.cachedEuler);
        }
        else if (ImGui::IsItemDeactivated() && isEditing)
        {
            isEditing = false;
            finishedEditing = true;
            m_displayedTransform.cachedEuler = Math::ClampEulerAngles(m_displayedTransform.cachedEuler);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Scale");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);

        if (ImGui::DragFloat3("##Scale", glm::value_ptr(m_displayedTransform.scale), 0.1f, 0.01f, 100.0f))
        {
            if (!startedEditing && !isEditing)
            {
                startedEditing = true;
            }
        }
        else if (ImGui::IsItemDeactivated() && isEditing)
        {
            isEditing = false;
            finishedEditing = true;
        }

        if (startedEditing)
        {
            startedEditing = false;
            isEditing = true;
            m_cachedTransform = transform;
        }
        if (isEditing)
        {
            transform = m_displayedTransform;
        }
        if (finishedEditing)
        {
            finishedEditing = false;
            transform = m_cachedTransform;
            Cauda::HistoryStack::Execute(new Cauda::GizmoModifyCommand(entity, &transform, &m_displayedTransform));
        }

        ImGui::EndTable();
    }

    if (ImGui::Button("Reset Position"))
    {
        m_displayedTransform.position = glm::vec3(0.f);
        Cauda::HistoryStack::Execute(new Cauda::GizmoModifyCommand(entity, &transform, &m_displayedTransform));
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Rotation"))
    {
        m_displayedTransform.cachedEuler = glm::vec3(0.f);
        m_displayedTransform.rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
        Cauda::HistoryStack::Execute(new Cauda::GizmoModifyCommand(entity, &transform, &m_displayedTransform));
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Scale"))
    {
        m_displayedTransform.scale = glm::vec3(1.0f);
        Cauda::HistoryStack::Execute(new Cauda::GizmoModifyCommand(entity, &transform, &m_displayedTransform));
    }
}

void TransformModule::RegisterWithEditor()
{
	m_editorModule->RegisterComponent<TransformComponent>("Transform", "Core",
		[this](flecs::entity entity, TransformComponent& component)
		{
			DrawTransformComponent(entity, component);
		},
		[](flecs::entity entity) -> bool { return false; },
        true

	);
    m_editorModule->RegisterComponent<SpawnPoint>("SpawnPoint", "Tags",
        [](flecs::entity entity, SpawnPoint& component)
        {
            ImGui::Text("ID: %i", component.spawnPointID);
        }, [](flecs::entity entity) -> bool { return true; });

    m_editorModule->RegisterComponent<SpawnLobby>("SpawnLobby", "Tags", nullptr, [](flecs::entity entity) -> bool { return true; });
}