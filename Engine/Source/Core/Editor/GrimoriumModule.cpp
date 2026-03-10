#include "cepch.h"
#include "GrimoriumModule.h"
#include "Platform/Win32/Application.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Utilities/STLModule.h"
#include "Core/Utilities/ImGuiUtilities.h"
#include "Core/Utilities/Timer.h"
#include "Core/Utilities/Curve.h"

#include <iostream>
#include <unordered_map>

GrimoriumModule::GrimoriumModule(flecs::world& world) : m_world(world)
{
    SetupComponents();
    SetupObservers();
    RegisterWithEditor();

    Timer::Initialise(world);
}

void GrimoriumModule::SetupComponents()
{
    m_world.component<OnEnableGrimoire>();
    m_world.component<OnDisableGrimoire>();
    m_world.component<OnStartGrimoire>();
    m_world.component<OnLoadGrimoire>();
    m_world.component<OnPostLoadGrimoire>();
    m_world.component<OnPreUpdateGrimoire>();
    m_world.component<OnUpdateGrimoire>();
    m_world.component<OnValidateGrimoire>();
    m_world.component<OnPostUpdateGrimoire>();
    m_world.component<OnPreStoreGrimoire>();
    m_world.component<OnStoreGrimoire>();

    m_world.component<GrimoireComponent>()
        .member<bool>("enabled");
}

void GrimoriumModule::SetupObservers()
{
    m_world.observer<GrimoireComponent>()
        .event(flecs::OnRemove)
        .each([this](flecs::entity entity, GrimoireComponent& component)
        {
            for (const auto& reg : s_scriptRegistrations)
            {
                if (reg.has(entity))
                {
                    reg.remove(entity);
                }
            }
        });
}

const std::vector<ScriptRegistration>& GrimoriumModule::GetRegisteredScripts()
{
    return s_scriptRegistrations;
}

void GrimoriumModule::OnImGuiRender()
{
    if (ImGui::CollapsingHeader("Grimoire System"))
    {
        ImGui::Text("Active Scripts: %zu", s_scriptRegistrations.size());
        ImGui::Text("Loading State: %s", s_isLoading ? "Loading" : "Ready");

        if (ImGui::TreeNode("Registered Script Types"))
        {
            for (const auto& registration : s_scriptRegistrations)
            {
                ImGui::BulletText("%s", registration.name.c_str());
            }
            ImGui::TreePop();
        }

        if (ImGui::Button("Force Loading State ON"))
        {
            SetLoading(true);
        }

        ImGui::SameLine();
        if (ImGui::Button("Force Loading State OFF"))
        {
            SetLoading(false);
        }
    }
}

void GrimoriumModule::CallGrimoireDebugDraw()
{
    if (IsLoading() || !Cauda::Application::IsGameRunning()) return;

    const auto& scripts = GetRegisteredScripts();

    m_world.each([&](flecs::entity entity, GrimoireComponent& grimoire)
    {
        if (!entity.is_valid() || !grimoire.enabled) return;

        for (const auto& script : scripts)
        {
            if (script.has(entity))
            {
                if (Grimoire* grimoirePtr = script.get_grimoire(entity))
                {
                    grimoirePtr->OnDrawDebug(entity);
                }
            }
        }
    });
}

void GrimoriumModule::RegisterScript(ScriptRegistration&& registration)
{
    s_scriptRegistrations.push_back(std::move(registration));
}

void GrimoriumModule::RegisterWithEditor()
{
    auto editorModule = m_world.try_get_mut<EditorModule>();
    if (!editorModule) return;

    editorModule->RegisterComponent<GrimoireComponent>("Grimoire Scripts", "Behaviour",
        [this](flecs::entity entity, GrimoireComponent& mod)
        {
            DrawScriptInspector(entity);
        },
        [](flecs::entity entity) -> bool
        {
            return true;
        }
    );
}

void GrimoriumModule::DrawScriptInspector(flecs::entity entity)
{
    ImGui::PushID(entity.raw_id());

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Scripts");
    ImGui::SameLine();

    float buttonWidth = 100.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - buttonWidth);

    if (ImGui::Button("+ Add Script", ImVec2(buttonWidth, 0)))
    {
        ImGui::OpenPopup("AddScriptPopup");
    }

    if (ImGui::BeginPopup("AddScriptPopup"))
    {
        ImGui::Text("Available Scripts");
        ImGui::Separator();

        for (auto& reg : s_scriptRegistrations)
        {
            if (!reg.has(entity))
            {
                if (ImGui::Selectable(reg.name.c_str()))
                {
                    reg.add(entity);
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();
    ImGui::Spacing();

    int script_to_remove = -1;
    for (int i = 0; i < s_scriptRegistrations.size(); ++i)
    {
        auto& reg = s_scriptRegistrations[i];

        if (reg.has(entity))
        {
            ImGui::PushID(i);

            ImVec4 statusColour = Cauda::Colour::HumbleGreen;
            ImGui::PushStyleColor(ImGuiCol_Text, statusColour);

            bool expanded = ImGui::CollapsingHeader(reg.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::PopStyleColor();

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Remove Script"))
                {
                    script_to_remove = i;
                }
                ImGui::EndPopup();
            }

            if (expanded)
            {
                ImGui::Indent();

                if (reg.draw_inspector)
                {
                    reg.draw_inspector(entity);
                }
                else
                {
                    ImGui::TextDisabled("No custom inspector defined");
                }

                ImGui::Unindent();
            }

            ImGui::PopID();
            ImGui::Spacing();
        }
    }

    if (script_to_remove != -1)
    {
        s_scriptRegistrations[script_to_remove].remove(entity);
    }

    bool hasAnyScripts = false;
    for (const auto& reg : s_scriptRegistrations)
    {
        if (reg.has(entity))
        {
            hasAnyScripts = true;
            break;
        }
    }

    if (!hasAnyScripts)
    {
        ImGui::TextDisabled("No scripts attached.");
    }

    ImGui::PopID();
}