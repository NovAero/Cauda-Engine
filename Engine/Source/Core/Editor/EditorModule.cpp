#include "cepch.h"

#include "EditorModule.h"
#include "Renderer/ShaderProgram.h"
#include "Renderer/FrameBuffer.h"
#include "Core/Scene/CameraModule.h"
#include "Core/Character/CharacterController.h"
#include "Core/Math/Gizmos.h"
#include "Core/Scene/SceneSerialiser.h"
#include "Core/Editor/GrimoriumModule.h"
#include "Core/Utilities/ImGuiUtilities.h"

#include "imgui.h"
#include "ImGuizmo.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

bool s_scrollToSelection = false;

EditorModule::EditorModule(flecs::world& world)
    : m_world(world),
    m_selectedEntity(flecs::entity::null())
{
    m_transformObserver = m_world.observer<TransformComponent>("AutoParent")
        .event(flecs::OnSet)
        .without(flecs::Prefab)
        .each([this](flecs::entity entity, TransformComponent& transform)
        {
            if (!m_sceneRoot) return;
            if (entity.id() == m_sceneRoot.id()) return;
            if (m_isDuplicateManipulation && entity.id() == m_duplicatedEntity.id()) return;

            if (!entity.has(flecs::ChildOf, flecs::Wildcard))
            {
                entity.add(flecs::ChildOf, m_sceneRoot);
            }
        });

    m_orphanEntitySystem = m_world.system<TransformComponent>()
        .kind(flecs::OnUpdate)
        .interval(5.0f)
        .without(flecs::ChildOf, flecs::Wildcard)
        .each([this](flecs::entity entity, TransformComponent& transform)
        {
            if (!m_sceneRoot) return;
            if (entity.id() == m_sceneRoot.id()) return;
            if (ImGuizmo::IsUsing()) return;

            entity.add(flecs::ChildOf, m_sceneRoot);
        });

    auto gizmoSettings = g_EngineSettingsEntity.try_get_mut<GizmoSettings>();
    if (gizmoSettings)
    {
        m_snapEnabled[0] = gizmoSettings->translationSnapEnabled;
        m_snapEnabled[1] = gizmoSettings->rotationSnapEnabled;
        m_snapEnabled[2] = gizmoSettings->scaleSnapEnabled;

        m_snapValues[0] = gizmoSettings->translationSnap;
        m_snapValues[1] = gizmoSettings->rotationSnap;
        m_snapValues[2] = gizmoSettings->scaleSnap;

        m_currentGizmoMode = (GizmoMode)gizmoSettings->worldSpace;
    }

    m_world.component<NoGizmoTag>();
}

EditorModule::~EditorModule()
{
    if (m_transformObserver && m_transformObserver.is_alive())
        m_transformObserver.destruct();
    if (m_orphanEntitySystem && m_orphanEntitySystem.is_alive())
        m_orphanEntitySystem.destruct();
}

//void EditorModule::OnImGuiRender()
//{
//    DrawHierarchy();
//    DrawInspector();
//}

void EditorModule::DrawHierarchy(bool* p_open)
{
    if (p_open && !(*p_open)) return;

    const ImVec4 cubeGreen = ImVec4(0.60f, 0.85f, 0.70f, 1.00f);

    if (!ImGui::Begin("Hierachy", p_open)) { ImGui::End(); return; }

    if (ImGui::BeginChild("##hierarchy", ImVec2(0, 0), ImGuiChildFlags_Borders))
    {
        static Texture* uiAtlas = nullptr;
        if (!uiAtlas)
        {
            uiAtlas = ResourceLibrary::GetTexture("icon_atlas");
            if (!uiAtlas)
            {
                uiAtlas = ResourceLibrary::LoadTexture("icon_atlas", "content/textures/icon_atlas.png");
            }
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImGuiIO& io = ImGui::GetIO();

        ImVec4 buttonNormal = ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
        ImVec4 buttonHovered = ImVec4(0.3f, 0.3f, 0.3f, 0.9f);
        ImVec4 buttonActive = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
        ImVec4 buttonDisabled = ImVec4(0.15f, 0.15f, 0.15f, 0.6f);

        const float buttonSize = 24.0f;
        const float buttonSpacing = 4.0f;
        const int atlasColumns = 10;
        const int atlasRows = 10;

        const int plusIconIndex = 0;
        const int minusIconIndex = 1;

        ImVec2 startPos = ImGui::GetCursorScreenPos();

        ImVec2 createButtonPos = startPos;
        ImVec2 createButtonMax = ImVec2(createButtonPos.x + buttonSize, createButtonPos.y + buttonSize);

        bool isCreateHovered = io.MousePos.x >= createButtonPos.x && io.MousePos.x <= createButtonMax.x &&
            io.MousePos.y >= createButtonPos.y && io.MousePos.y <= createButtonMax.y;

        ImVec4 createButtonColour = isCreateHovered ? buttonHovered : buttonNormal;

        drawList->AddRectFilled(createButtonPos, createButtonMax,
            IM_COL32(createButtonColour.x * 255, createButtonColour.y * 255, createButtonColour.z * 255, createButtonColour.w * 255),
            4.0f);

        drawList->AddRect(createButtonPos, createButtonMax,
            IM_COL32(80, 80, 80, 180), 4.0f, 0, 1.0f);

        if (uiAtlas && uiAtlas->GetHandle() != 0)
        {
            ImVec2 iconSize = ImVec2(16.0f, 16.0f);
            ImVec2 iconPos = ImVec2(
                createButtonPos.x + (buttonSize - iconSize.x) * 0.5f,
                createButtonPos.y + (buttonSize - iconSize.y) * 0.5f
            );

            Cauda::UI::DrawImageAtlasIndex(
                (ImTextureID)(uintptr_t)uiAtlas->GetHandle(),
                iconPos, iconSize,
                plusIconIndex,
                atlasColumns, atlasRows,
                Cauda::Colour::HumbleGreen
            );
        }
        else
        {
            ImVec2 textSize = ImGui::CalcTextSize("+");
            ImVec2 textPos = ImVec2(
                createButtonPos.x + (buttonSize - textSize.x) * 0.5f,
                createButtonPos.y + (buttonSize - textSize.y) * 0.5f
            );
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), "+");
        }

        if (isCreateHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            CreateEntity("Entity", m_sceneRoot);
        }

        if (isCreateHovered)
        {
            ImGui::SetTooltip("Create Entity");
        }

        ImVec2 deleteButtonPos = ImVec2(createButtonMax.x + buttonSpacing, createButtonPos.y);
        ImVec2 deleteButtonMax = ImVec2(deleteButtonPos.x + buttonSize, deleteButtonPos.y + buttonSize);

        bool canDelete = false;// m_selectedEntity&& m_selectedEntity.is_alive() && m_selectedEntity.id() != m_sceneRoot.id();
        bool isDeleteHovered = io.MousePos.x >= deleteButtonPos.x && io.MousePos.x <= deleteButtonMax.x &&
            io.MousePos.y >= deleteButtonPos.y && io.MousePos.y <= deleteButtonMax.y;

        ImVec4 deleteButtonColour;
        if (!canDelete)
            deleteButtonColour = buttonDisabled;
        else if (isDeleteHovered)
            deleteButtonColour = buttonHovered;
        else
            deleteButtonColour = buttonNormal;

        drawList->AddRectFilled(deleteButtonPos, deleteButtonMax,
            IM_COL32(deleteButtonColour.x * 255, deleteButtonColour.y * 255, deleteButtonColour.z * 255, deleteButtonColour.w * 255),
            4.0f);

        drawList->AddRect(deleteButtonPos, deleteButtonMax,
            IM_COL32(80, 80, 80, 180), 4.0f, 0, 1.0f);

        if (uiAtlas && uiAtlas->GetHandle() != 0)
        {
            ImVec2 iconSize = ImVec2(16.0f, 16.0f);
            ImVec2 iconPos = ImVec2(
                deleteButtonPos.x + (buttonSize - iconSize.x) * 0.5f,
                deleteButtonPos.y + (buttonSize - iconSize.y) * 0.5f
            );

            ImVec4 iconTint = canDelete ? Cauda::Colour::HumbleRed : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

            Cauda::UI::DrawImageAtlasIndex(
                (ImTextureID)(uintptr_t)uiAtlas->GetHandle(),
                iconPos, iconSize,
                minusIconIndex,
                atlasColumns, atlasRows,
                iconTint
            );
        }
        else
        {
            const char* deleteText = "-";
            ImVec2 textSize = ImGui::CalcTextSize(deleteText);
            ImVec2 textPos = ImVec2(
                deleteButtonPos.x + (buttonSize - textSize.x) * 0.5f,
                deleteButtonPos.y + (buttonSize - textSize.y) * 0.5f
            );
            ImU32 textColour = canDelete ? IM_COL32(255, 255, 255, 255) : IM_COL32(128, 128, 128, 255);
            drawList->AddText(textPos, textColour, deleteText);
        }

        if (canDelete && isDeleteHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            DeleteEntity(m_selectedEntity);
            m_selectedEntity = flecs::entity::null();
        }

        if (isDeleteHovered)
        {
            if (canDelete)
                ImGui::SetTooltip("Delete Entity");
            else
                ImGui::SetTooltip("Select an entity to delete");
        }

        float filterSpacing = 4.0f;
        ImVec2 filterPos = ImVec2(deleteButtonMax.x + filterSpacing, startPos.y);
        ImGui::SetCursorScreenPos(filterPos);

        float availableWidth = ImGui::GetContentRegionAvail().x - filterSpacing;
        ImGui::SetNextItemWidth(availableWidth);
        static ImGuiTextFilter filter;
        if (ImGui::InputTextWithHint("##Filter", "Filter...", filter.InputBuf, IM_ARRAYSIZE(filter.InputBuf),
            ImGuiInputTextFlags_EscapeClearsAll))
        {
            filter.Build();
        }

        ImGui::Separator();

        if (ImGui::BeginTable("##EntityTree", 1, ImGuiTableFlags_RowBg))
        {
            std::set<flecs::entity_t> processedEntities;
            processedEntities.insert(m_sceneRoot.id());

            std::vector<flecs::entity> rootChildren;
            m_sceneRoot.children([&rootChildren](flecs::entity child) {
                rootChildren.push_back(child);
                return true;
                });

            ImGuiTreeNodeFlags rootFlags = m_baseFlags | ImGuiTreeNodeFlags_DefaultOpen;
            if (rootChildren.empty())
            {
                rootFlags |= ImGuiTreeNodeFlags_Leaf;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, cubeGreen);
            bool rootOpen = ImGui::TreeNodeEx((void*)(uintptr_t)m_sceneRoot.id(), rootFlags, "Root");
            ImGui::PopStyleColor();

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_NODE"))
                {
                    flecs::entity_t draggedEntityId = *(const flecs::entity_t*)payload->Data;
                    flecs::entity draggedEntity(m_world, draggedEntityId);

                    if (draggedEntity.is_alive() && CanReparentEntity(draggedEntity, m_sceneRoot))
                    {
                        ReparentEntity(draggedEntity, m_sceneRoot);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsItemClicked())
            {
                SelectEntity(m_sceneRoot);
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Create Entity"))
                {
                    CreateEntity("Entity", m_sceneRoot);
                }
                ImGui::EndPopup();
            }

            if (rootOpen)
            {
                std::sort(rootChildren.begin(), rootChildren.end(), [](const flecs::entity& a, const flecs::entity& b) {
                    std::string aName = a.name().c_str();
                    std::string bName = b.name().c_str();
                    return aName < bName; 
                    });

                for (auto& child : rootChildren)
                {
                    if (!filter.IsActive() || EntityOrChildMatchesFilter(child, filter))
                    {
                        if (processedEntities.find(child.id()) == processedEntities.end())
                        {
                            processedEntities.insert(child.id());
                            DrawEntityNode(child, processedEntities, filter);
                        }
                    }
                }

                ImGui::TreePop();
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}


void EditorModule::DrawInspector(bool* p_open)
{
    if (p_open && !(*p_open)) return;

    const ImVec4 cubeGreen = ImVec4(0.60f, 0.85f, 0.70f, 1.00f);

    if (!ImGui::Begin("Inspector", p_open)) { ImGui::End(); return; }

    if (ImGui::BeginChild("##inspector", ImVec2(0, 0), ImGuiChildFlags_Borders))
    {
        if (m_selectedEntity && m_selectedEntity.is_alive())
        {
            ShowEntityProperties(m_selectedEntity);
        }
        else
        {
            ImGui::TextDisabled("No entity selected");
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

void EditorModule::DrawHierarchyAndInspector()
{
    const ImVec4 cubeGreen = ImVec4(0.60f, 0.85f, 0.70f, 1.00f);

    ImGui::Begin("Hierarchy");

    static float split_size = 0.4f;
    float region_width = ImGui::GetContentRegionAvail().x;
    float split_width = region_width * split_size;

    if (ImGui::BeginChild("##hierarchy", ImVec2(split_width, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
    {
        if (ImGui::Button("Create Entity"))
        {
            CreateEntity("Entity", m_sceneRoot);
        }
        ImGui::SameLine();

        bool canDelete = m_selectedEntity && m_selectedEntity.is_alive() && m_selectedEntity.id() != m_sceneRoot.id();
        if (!canDelete) ImGui::BeginDisabled();
        if (ImGui::Button("Delete") && canDelete)
        {
            DeleteEntity(m_selectedEntity);
            m_selectedEntity = flecs::entity::null();
        }
        if (!canDelete) ImGui::EndDisabled();

        ImGui::Separator();

        ImGui::InputText("Scene Name", m_sceneName, (size_t)MAX_NAME_STRING * sizeof(char));

        std::string sceneName = m_sceneName;
        m_disableSaveLoad = sceneName.find(".json") == std::string::npos || sceneName.find(".") == 0 || sceneName == "";
        
        if (m_disableSaveLoad) ImGui::BeginDisabled();
        if (ImGui::Button("Save Scene"))
        {
            SaveScene(m_sceneName);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Scene"))
        {
            LoadScene(m_sceneName);
        }
        if (m_disableSaveLoad) ImGui::EndDisabled();


        static ImGuiTextFilter filter;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputTextWithHint("##Filter", "Filter...", filter.InputBuf, IM_ARRAYSIZE(filter.InputBuf),
            ImGuiInputTextFlags_EscapeClearsAll))
        {
            filter.Build();
        }

        ImGui::Separator();

        if (ImGui::BeginTable("##EntityTree", 1, ImGuiTableFlags_RowBg))
        {
            std::set<flecs::entity_t> processedEntities;
            processedEntities.insert(m_sceneRoot.id());

            std::vector<flecs::entity> rootChildren;
            m_sceneRoot.children([&rootChildren](flecs::entity child) {
                rootChildren.push_back(child);
                return true;
                });

            ImGuiTreeNodeFlags rootFlags = m_baseFlags | ImGuiTreeNodeFlags_DefaultOpen;
            if (rootChildren.empty())
            {
                rootFlags |= ImGuiTreeNodeFlags_Leaf;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, cubeGreen);
            bool rootOpen = ImGui::TreeNodeEx((void*)(uintptr_t)m_sceneRoot.id(), rootFlags, "Root");
            ImGui::PopStyleColor();

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_NODE"))
                {
                    flecs::entity_t draggedEntityId = *(const flecs::entity_t*)payload->Data;
                    flecs::entity draggedEntity(m_world, draggedEntityId);

                    if (draggedEntity.is_alive() && CanReparentEntity(draggedEntity, m_sceneRoot))
                    {
                        ReparentEntity(draggedEntity, m_sceneRoot);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsItemClicked())
            {
                SelectEntity(m_sceneRoot);
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Create Entity"))
                {
                    CreateEntity("Entity", m_sceneRoot);
                }
                ImGui::EndPopup();
            }

            if (rootOpen)
            {
                std::sort(rootChildren.begin(), rootChildren.end(), [](const flecs::entity& a, const flecs::entity& b) {
                    std::string aName = a.name().c_str();
                    std::string bName = b.name().c_str();
                    return aName < bName; //Alphabetical sort
                    });

                for (auto& child : rootChildren)
                {
                    const char* childName = child.name();
                    if ((childName && filter.PassFilter(childName)) || !filter.IsActive())
                    {
                        if (processedEntities.find(child.id()) == processedEntities.end())
                        {
                            processedEntities.insert(child.id());
                            DrawEntityNode(child, processedEntities, filter);
                        }
                    }
                }

                ImGui::TreePop();
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    if (ImGui::BeginChild("##inspector", ImVec2(0, 0), ImGuiChildFlags_Borders))
    {
        if (m_selectedEntity && m_selectedEntity.is_alive())
        {
            ShowEntityProperties(m_selectedEntity);
        }
        else
        {
            ImGui::TextDisabled("No entity selected");
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

void EditorModule::DrawViewportGizmoControls(ImVec2 viewportPos, ImVec2 viewportSize)
{
    if (!m_gizmoEnabled) return;

    const float overlayPadding = 10.0f;
    ImVec2 controlsPos = ImVec2(viewportPos.x + overlayPadding, viewportPos.y + overlayPadding);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();

    static Texture* uiAtlas = nullptr;
    if (!uiAtlas)
    {
        uiAtlas = ResourceLibrary::GetTexture("icon_atlas");
        if (!uiAtlas)
        {
            uiAtlas = ResourceLibrary::LoadTexture("icon_atlas", "content/textures/icon_atlas.png");
        }
    }

    ImVec4 buttonNormal = ImVec4(0.2f, 0.2f, 0.2f, 0.8f);
    ImVec4 buttonHovered = ImVec4(0.3f, 0.3f, 0.3f, 0.9f);
    ImVec4 buttonActive = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    ImVec4 buttonSnapped = ImVec4(0.60f, 0.85f, 0.70f, 1.00f);

    const float buttonSize = 32.0f;
    const float buttonSpacing = 4.0f;

    const int atlasColumns = 10;
    const int atlasRows = 10;

    const int iconIndices[] = { 10, 12, 11 };  
    const int gridIconIndex = 62;
    const int localIconIndex = 8;
    const int worldIconIndex = 9;

    const char* operationTooltips[] = {
        "Translate - Right click for snap settings",
        "Rotate - Right click for snap settings",
        "Scale - Right click for snap settings"
    };

    /*float totalGizmoWidth = 3 * (buttonSize + buttonSpacing) - buttonSpacing + 10 + (buttonSize + 20) + 10 + buttonSize;
    ImVec2 backgroundMin = ImVec2(controlsPos.x - 8.0f, controlsPos.y - 4.0f);
    ImVec2 backgroundMax = ImVec2(controlsPos.x + totalGizmoWidth + 8.0f, controlsPos.y + buttonSize + 4.0f);
    drawList->AddRectFilled(backgroundMin, backgroundMax, IM_COL32(0, 0, 0, 128), 4.0f);*/

    for (int i = 0; i < 3; i++)
    {
        ImVec2 buttonPos = ImVec2(
            controlsPos.x + i * (buttonSize + buttonSpacing),
            controlsPos.y
        );
        ImVec2 buttonMax = ImVec2(buttonPos.x + buttonSize, buttonPos.y + buttonSize);

        bool isHovered = io.MousePos.x >= buttonPos.x && io.MousePos.x <= buttonMax.x &&
            io.MousePos.y >= buttonPos.y && io.MousePos.y <= buttonMax.y;

        bool isSelected = (static_cast<int>(m_currentGizmoOperation) == i);
        bool isSnapped = m_snapEnabled[i];

        ImVec4 buttonColour;
        if (isSelected && isSnapped) buttonColour = buttonActive;
        else if (isSelected) buttonColour = buttonActive;
        else if (isHovered) buttonColour = buttonHovered;
        else buttonColour = buttonNormal;

        drawList->AddRectFilled(buttonPos, buttonMax,
            IM_COL32(buttonColour.x * 255, buttonColour.y * 255, buttonColour.z * 255, buttonColour.w * 255),
            4.0f);

        if (isSelected)
        {
            drawList->AddRect(buttonPos, buttonMax,
                IM_COL32(128, 128, 255, 255), 4.0f, 0, 2.0f);
        }
        else
        {
            drawList->AddRect(buttonPos, buttonMax,
                IM_COL32(80, 80, 80, 180), 4.0f, 0, 1.0f);
        }

        if (uiAtlas && uiAtlas->GetHandle() != 0)
        {
            ImVec2 iconSize = ImVec2(16.0f, 16.0f);
            ImVec2 iconPos = ImVec2(
                buttonPos.x + (buttonSize - iconSize.x) * 0.5f,  
                buttonPos.y + (buttonSize - iconSize.y) * 0.5f   
            );

            ImVec4 iconTint = isSelected ? ImVec4(1.2f, 1.2f, 1.2f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

            Cauda::UI::DrawImageAtlasIndex(
                (ImTextureID)(uintptr_t)uiAtlas->GetHandle(),
                iconPos, iconSize,
                iconIndices[i],
                atlasColumns, atlasRows,
                iconTint
            );
        }
        else
        {
            const char* fallbackIcons[] = { "T", "R", "S" };
            ImVec2 textSize = ImGui::CalcTextSize(fallbackIcons[i]);
            ImVec2 textPos = ImVec2(
                buttonPos.x + (buttonSize - textSize.x) * 0.5f,
                buttonPos.y + (buttonSize - textSize.y) * 0.5f
            );
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), fallbackIcons[i]);
        }

        if (isSnapped)
        {
            ImVec2 dotPos = ImVec2(buttonMax.x - 4, buttonPos.y + 4);
            drawList->AddCircleFilled(dotPos, 2.0f, IM_COL32(255, 128, 128, 255));
        }

        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_currentGizmoOperation = static_cast<GizmoOperation>(i);
        }

        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup(("SnapSettings" + std::to_string(i)).c_str());
        }

        if (isHovered)
        {
            ImGui::SetTooltip("%s", operationTooltips[i]);
        }

        if (ImGui::BeginPopup(("SnapSettings" + std::to_string(i)).c_str()))
        {
            const char* popupTitles[] = { "Translate Snap", "Rotate Snap", "Scale Snap" };
            const char* valueLabels[] = { "Grid Size", "Angle", "Factor" };
            const char* valueFormats[] = { "%.1f units", "%.0f degrees", "%.2f" };
            const float valueSteps[] = { 0.1f, 1.0f, 0.01f };
            const float valueMin[] = { 0.1f, 1.0f, 0.01f };
            const float valueMax[] = { 10.0f, 90.0f, 1.0f };

            ImGui::Text("%s", popupTitles[i]);
            ImGui::Separator();

            ImGui::Checkbox("Enable Snap", &m_snapEnabled[i]);

            if (m_snapEnabled[i])
            {
                ImGui::DragFloat(valueLabels[i], &m_snapValues[i], valueSteps[i], valueMin[i], valueMax[i], valueFormats[i]);
            }

            ImGui::Separator();
            ImGui::TextDisabled("Tip: Hold X for temporary snap toggle");

            ImGui::EndPopup();
        }
    }

    ImVec2 modeButtonPos = ImVec2(
        controlsPos.x + 3 * (buttonSize + buttonSpacing) + 10,
        controlsPos.y
    );
    ImVec2 modeButtonMax = ImVec2(modeButtonPos.x + buttonSize, modeButtonPos.y + buttonSize);

    bool isModeHovered = io.MousePos.x >= modeButtonPos.x && io.MousePos.x <= modeButtonMax.x &&
        io.MousePos.y >= modeButtonPos.y && io.MousePos.y <= modeButtonMax.y;

    ImVec4 modeButtonColour = isModeHovered ? buttonHovered : buttonNormal;

    drawList->AddRectFilled(modeButtonPos, modeButtonMax,
        IM_COL32(modeButtonColour.x * 255, modeButtonColour.y * 255, modeButtonColour.z * 255, modeButtonColour.w * 255),
        4.0f);

    drawList->AddRect(modeButtonPos, modeButtonMax,
        IM_COL32(80, 80, 80, 180), 4.0f, 0, 1.0f);

    if (uiAtlas && uiAtlas->GetHandle() != 0)
    {
        ImVec2 iconSize = ImVec2(16.0f, 16.0f);
        ImVec2 iconPos = ImVec2(
            modeButtonPos.x + (buttonSize - iconSize.x) * 0.5f, 
            modeButtonPos.y + (buttonSize - iconSize.y) * 0.5f          
        );

        int modeIconIndex = (m_currentGizmoMode == GizmoMode::Local) ? localIconIndex : worldIconIndex;

        Cauda::UI::DrawImageAtlasIndex(
            (ImTextureID)(uintptr_t)uiAtlas->GetHandle(),
            iconPos, iconSize,
            modeIconIndex,
            atlasColumns, atlasRows,
            ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
        );
    }
    else
    {
        const char* modeText = (m_currentGizmoMode == GizmoMode::Local) ? "Local" : "World";
        ImVec2 modeTextSize = ImGui::CalcTextSize(modeText);
        ImVec2 modeTextPos = ImVec2(
            modeButtonPos.x + (modeButtonMax.x - modeButtonPos.x - modeTextSize.x) * 0.5f,
            modeButtonPos.y + (buttonSize - modeTextSize.y) * 0.5f
        );
        drawList->AddText(modeTextPos, IM_COL32(255, 255, 255, 255), modeText);
    }

    if (isModeHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_currentGizmoMode = (m_currentGizmoMode == GizmoMode::Local) ? GizmoMode::World : GizmoMode::Local;
    }

    if (isModeHovered)
    {
        ImGui::SetTooltip("Toggle Local/World Space");
    }

    ImVec2 gridButtonPos = ImVec2(
        modeButtonMax.x + 10,
        controlsPos.y
    );
    ImVec2 gridButtonMax = ImVec2(gridButtonPos.x + buttonSize, gridButtonPos.y + buttonSize);

    bool isGridHovered = io.MousePos.x >= gridButtonPos.x && io.MousePos.x <= gridButtonMax.x &&
        io.MousePos.y >= gridButtonPos.y && io.MousePos.y <= gridButtonMax.y;

    ImVec4 gridButtonColour;
    if (m_gridEnabled) gridButtonColour = isGridHovered ? buttonActive : buttonSnapped;
    else gridButtonColour = isGridHovered ? buttonHovered : buttonNormal;

    drawList->AddRectFilled(gridButtonPos, gridButtonMax,
        IM_COL32(gridButtonColour.x * 255, gridButtonColour.y * 255, gridButtonColour.z * 255, gridButtonColour.w * 255),
        4.0f);

    if (m_gridEnabled)
    {
        drawList->AddRect(gridButtonPos, gridButtonMax,
            IM_COL32(128, 255, 128, 255), 4.0f, 0, 2.0f);
    }
    else
    {
        drawList->AddRect(gridButtonPos, gridButtonMax,
            IM_COL32(80, 80, 80, 180), 4.0f, 0, 1.0f);
    }

    if (uiAtlas && uiAtlas->GetHandle() != 0)
    {
        ImVec2 iconSize = ImVec2(16.0f, 16.0f);
        ImVec2 iconPos = ImVec2(
            gridButtonPos.x + (buttonSize - iconSize.x) * 0.5f,  
            gridButtonPos.y + (buttonSize - iconSize.y) * 0.5f   
        );

        ImVec4 iconTint = m_gridEnabled ? ImVec4(1.2f, 1.2f, 1.2f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        Cauda::UI::DrawImageAtlasIndex(
            (ImTextureID)(uintptr_t)uiAtlas->GetHandle(),
            iconPos, iconSize,
            gridIconIndex,
            atlasColumns, atlasRows,
            iconTint
        );
    }
    else
    {
        ImVec2 textSize = ImGui::CalcTextSize("G");
        ImVec2 textPos = ImVec2(
            gridButtonPos.x + (buttonSize - textSize.x) * 0.5f,
            gridButtonPos.y + (buttonSize - textSize.y) * 0.5f
        );
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), "G");
    }

    if (isGridHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_gridEnabled = !m_gridEnabled;
    }

    if (isGridHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup("GridSettings");
    }

    if (isGridHovered)
    {
        ImGui::SetTooltip("Toggle Grid - Right click for settings");
    }

    if (ImGui::BeginPopup("GridSettings"))
    {
        ImGui::Text("Grid Settings");
        ImGui::Separator();

        ImGui::Checkbox("Show Grid", &m_gridEnabled);
        ImGui::DragFloat("Grid Size", &m_gridSize, 0.1f, 0.1f, 100.0f, "%.1f units");

        ImGui::Text("Grid Centre");
        ImGui::DragFloat3("##GridCentre", glm::value_ptr(m_gridCenter), 0.1f);

        ImGui::Text("Grid Normal");
        if (ImGui::DragFloat3("##GridNormal", glm::value_ptr(m_gridNormal), 0.01f, -1.0f, 1.0f))
        {
            float length = glm::length(m_gridNormal);
            if (length > 0.001f) {
                m_gridNormal = m_gridNormal / length;
            }
        }

        float normalLength = glm::length(m_gridNormal);
        if (normalLength < 0.99f || normalLength > 1.01f) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "(!normalised)");
        }

        if (ImGui::Button("Normalise"))
        {
            if (glm::length(m_gridNormal) > 0.001f) {
                m_gridNormal = glm::normalize(m_gridNormal);
            }
            else {
                m_gridNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }

        ImGui::Separator();
        ImGui::Text("Plane Presets:");

        if (ImGui::Button("XZ Plane (Y-up)"))
        {
            m_gridNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            m_gridCenter = glm::vec3(0.0f, 0.0f, 0.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("XY Plane (Z-up)"))
        {
            m_gridNormal = glm::vec3(0.0f, 0.0f, 1.0f);
            m_gridCenter = glm::vec3(0.0f, 0.0f, 0.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("YZ Plane (X-up)"))
        {
            m_gridNormal = glm::vec3(1.0f, 0.0f, 0.0f);
            m_gridCenter = glm::vec3(0.0f, 0.0f, 0.0f);
        }

        if (ImGui::Button("Reset to Origin"))
        {
            m_gridCenter = glm::vec3(0.0f, 0.0f, 0.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Centre on Selected"))
        {
            flecs::entity selected = GetSelectedEntity();
            if (selected && selected.is_alive() && selected.has<TransformComponent>()) {
                auto* transform = selected.try_get<TransformComponent>();
                m_gridCenter = transform->position;
            }
        }


        ImGui::EndPopup();


    }

    ImVec2 perspectiveButtonPos = ImVec2(
        gridButtonMax.x + 10,
        controlsPos.y
    );
    ImVec2 perspectiveButtonMax = ImVec2(perspectiveButtonPos.x + buttonSize, perspectiveButtonPos.y + buttonSize);

    bool isPerspectiveHovered = io.MousePos.x >= perspectiveButtonPos.x && io.MousePos.x <= perspectiveButtonMax.x &&
        io.MousePos.y >= perspectiveButtonPos.y && io.MousePos.y <= perspectiveButtonMax.y;

    auto cameraModule = m_world.try_get_mut<CameraModule>();
    auto mainCam = cameraModule->GetMainCamera();
    bool isPerspective = true;
    if (mainCam && mainCam.is_alive())
    {
        auto camComp = mainCam.try_get<CameraComponent>();
        if (camComp)
        {
            isPerspective = (camComp->projectionMode == ProjectionMode::PERSPECTIVE);
        }
    }

    ImVec4 perspectiveButtonColour;
    if (isPerspective) perspectiveButtonColour = isPerspectiveHovered ? buttonActive : buttonSnapped;
    else perspectiveButtonColour = isPerspectiveHovered ? buttonHovered : buttonNormal;

    drawList->AddRectFilled(perspectiveButtonPos, perspectiveButtonMax,
        IM_COL32(perspectiveButtonColour.x * 255, perspectiveButtonColour.y * 255, perspectiveButtonColour.z * 255, perspectiveButtonColour.w * 255),
        4.0f);

    if (isPerspective)
    {
        drawList->AddRect(perspectiveButtonPos, perspectiveButtonMax,
            IM_COL32(128, 128, 255, 255), 4.0f, 0, 2.0f);
    }
    else
    {
        drawList->AddRect(perspectiveButtonPos, perspectiveButtonMax,
            IM_COL32(80, 80, 80, 180), 4.0f, 0, 1.0f);
    }


    if (uiAtlas && uiAtlas->GetHandle() != 0)
    {
        ImVec2 iconSize = ImVec2(16.0f, 16.0f);
        ImVec2 iconPos = ImVec2(
            perspectiveButtonPos.x + (buttonSize - iconSize.x) * 0.5f,
            perspectiveButtonPos.y + (buttonSize - iconSize.y) * 0.5f
        );

        ImVec4 iconTint = isPerspective ? ImVec4(1.2f, 1.2f, 1.2f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);


        const int perspectiveIconIndex = 51;
        const int orthographicIconIndex = 51;

        int iconIndex = isPerspective ? perspectiveIconIndex : orthographicIconIndex;


        Cauda::UI::DrawImageAtlasIndex(
            (ImTextureID)(uintptr_t)uiAtlas->GetHandle(),
            iconPos, iconSize,
            iconIndex,
            atlasColumns, atlasRows,
            iconTint
        );
    }
    else
    {
        const char* perspectiveText = isPerspective ? "P" : "O";
        ImVec2 textSize = ImGui::CalcTextSize(perspectiveText);
        ImVec2 textPos = ImVec2(
            perspectiveButtonPos.x + (buttonSize - textSize.x) * 0.5f,
            perspectiveButtonPos.y + (buttonSize - textSize.y) * 0.5f
        );
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), perspectiveText);
    }

    if (isPerspectiveHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {

        if (mainCam && mainCam.is_alive())
        {
            auto camComp = mainCam.try_get_mut<CameraComponent>();
            if (camComp)
            {
                camComp->projectionMode = isPerspective ?
                    ProjectionMode::ORTHOGRAPHIC :
                    ProjectionMode::PERSPECTIVE;
            }
        }
    }

    if (isPerspectiveHovered)
    {
        const char* tooltip = isPerspective ?
            "Switch to Orthographic View" :
            "Switch to Perspective View";
        ImGui::SetTooltip("%s", tooltip);
    }
}

bool EditorModule::EntityOrChildMatchesFilter(flecs::entity entity, const ImGuiTextFilter& filter)
{
    const char* name = entity.name();
    if (name && filter.PassFilter(ResourceLibrary::GetStrippedName(name).c_str()))
    {
        return true;
    }

    bool childMatches = false;
    entity.children([&](flecs::entity child) {
        if (EntityOrChildMatchesFilter(child, filter))
        {
            childMatches = true;
        }
        return !childMatches;
        });

    return childMatches;
}

void EditorModule::DrawEntityNode(flecs::entity entity, std::set<flecs::entity_t>& processedEntities, const ImGuiTextFilter& filter)
{
    if (!entity.is_alive()) return;

    if (m_selectedEntity && m_selectedEntity.is_alive() && m_selectedEntity.id() != entity.id())
    {
        flecs::entity currentParent = m_selectedEntity.parent();
        while (currentParent.is_alive() && currentParent.id() != m_sceneRoot.id())
        {
            if (currentParent.id() == entity.id())
            {
                ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                break;
            }
            currentParent = currentParent.parent();
        }
    }

    std::string displayName = ResourceLibrary::GetStrippedName(entity.name().c_str());

    std::vector<flecs::entity> children;
    entity.children([&children](flecs::entity child)
        {
            children.push_back(child);
            return true;
        });

    std::sort(children.begin(), children.end(), [](const flecs::entity& a, const flecs::entity& b)
        {
            return a.id() < b.id();
        });

    std::vector<flecs::entity> visibleChildren;
    bool shouldBeVisible = true;

    if (filter.IsActive())
    {
        bool selfMatches = filter.PassFilter(displayName.c_str());

        if (!selfMatches)
        {
            bool hasMatchingChild = false;
            for (auto& child : children)
            {
                if (EntityOrChildMatchesFilter(child, filter))
                {
                    hasMatchingChild = true;
                    ImGui::SetNextItemOpen(true, ImGuiCond_Always);
                    break;
                }
            }
            shouldBeVisible = hasMatchingChild;
        }
    }

    if (!shouldBeVisible) return;

    for (auto& child : children)
    {
        if (processedEntities.find(child.id()) == processedEntities.end())
        {
            if (!filter.IsActive() || EntityOrChildMatchesFilter(child, filter))
            {
                visibleChildren.push_back(child);
                processedEntities.insert(child.id());
            }
        }
    }

    ImGuiTreeNodeFlags flags = m_baseFlags;
    if (visibleChildren.empty())
        flags |= ImGuiTreeNodeFlags_Leaf;
    if (m_selectedEntity && entity.id() == m_selectedEntity.id())
        flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImVec4 entityColour = GetEntityColour(entity);
    bool needsPopColour = false;

    if (entityColour.w > 0.0f)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, entityColour);
        needsPopColour = true;
    }

    bool nodeOpen = ImGui::TreeNodeEx((void*)(uintptr_t)entity.id(), flags, "%s", displayName.c_str());

    if (needsPopColour)
        ImGui::PopStyleColor();

    if (m_selectedEntity && entity.id() == m_selectedEntity.id() && s_scrollToSelection)
    {
        ImGui::SetScrollHereY(0.5f);
        s_scrollToSelection = false;
    }

    bool isSceneRoot = (entity.id() == m_sceneRoot.id());
    if (!isSceneRoot && ImGui::BeginDragDropSource())
    {
        flecs::entity_t entityId = entity.id();
        ImGui::SetDragDropPayload("ENTITY_NODE", &entityId, sizeof(flecs::entity_t));
        ImGui::Text("Moving: %s", displayName.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_NODE"))
        {
            flecs::entity_t draggedEntityId = *(const flecs::entity_t*)payload->Data;
            flecs::entity draggedEntity(m_world, draggedEntityId);

            if (draggedEntity.is_alive() && CanReparentEntity(draggedEntity, entity))
            {
                ReparentEntity(draggedEntity, entity);
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (ImGui::IsItemClicked())
    {
        SelectEntity(entity);
        s_scrollToSelection = false;
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        const auto* targetTransform = entity.try_get<TransformComponent>();
        auto cameraModule = m_world.try_get_mut<CameraModule>();
        flecs::entity mainCam = cameraModule->GetMainCamera();

        if (targetTransform && mainCam.is_alive())
        {
            auto* camTrans = mainCam.try_get_mut<TransformComponent>();
            if (camTrans)
            {
                glm::vec3 forward = cameraModule->GetForwardVector(mainCam);

                float focusDistance = 50.0f;
                camTrans->position = Math::GetWorldPosition(entity) - (forward * focusDistance);

                mainCam.modified<TransformComponent>();
            }
        }
    }

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Rename"))
        {
        }
        if (ImGui::MenuItem("Duplicate"))
        {
            flecs::entity duplicated = DuplicateEntity(entity, entity.parent());
            if (duplicated != m_selectedEntity && duplicated)
            {
                SelectEntity(duplicated);
            }
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Delete"))
        {
            DeleteEntity(entity);
            if (m_selectedEntity && m_selectedEntity.id() == entity.id())
                m_selectedEntity = flecs::entity::null();
            ImGui::EndPopup();
            if (nodeOpen) ImGui::TreePop();
            return;
        }
        if (ImGui::MenuItem("Create Child"))
        {
            CreateEntity("Entity", entity);
        }
        if (ImGui::MenuItem("Create Prefab"))
        {
            CreatePrefabFromEntity(entity, entity.name().c_str());
        }
        ImGui::EndPopup();
    }

    if (nodeOpen)
    {
        for (auto& child : visibleChildren)
        {
            DrawEntityNode(child, processedEntities, filter);
        }
        ImGui::TreePop();
    }
}

void EditorModule::ShowEntityProperties(flecs::entity entity)
{
    bool isSceneRoot = (entity.id() == m_sceneRoot.id());
    if (isSceneRoot)
    {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::TextColored(ImVec4(0.60f, 0.85f, 0.70f, 1.00f), "Scene Root");
        ImGui::PopFont();

        ImGui::TextDisabled("Entity ID: %i | Version: %i", (int32_t)entity.id(), entity.id() >> 32);
        return;
    }

    const char* entityName = entity.name();
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::Text("%s", entityName ? entityName : "Unnamed Entity");
    ImGui::PopFont();

    ImGui::TextDisabled("Entity ID: %i | Version: %i", (int32_t)entity.id(), entity.id() >> 32);

    bool noGizmo = entity.has<NoGizmoTag>();
    if (ImGui::Checkbox("Disable Gizmo", &noGizmo)) 
    {
        if (noGizmo) 
        {
            entity.add<NoGizmoTag>();
        }
        else 
        {
            entity.remove<NoGizmoTag>();
        }
    }

    if (ImGui::BeginTable("##basic_properties", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        char nameBuf[256] = "";
        if (entity.name())
        {
            std::string strippedName = ResourceLibrary::GetStrippedName(entity.name().c_str());
            strcpy_s(nameBuf, sizeof(nameBuf), strippedName.c_str());
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Name");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);


        if (entity.has(flecs::Prefab))
        {
            ImGui::BeginDisabled();
        }

        ImGui::InputText("##Name", nameBuf, sizeof(nameBuf));

        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            std::string requestedName = nameBuf;
            std::string currentName = ResourceLibrary::GetStrippedName(entity.name().c_str());

            if (currentName != requestedName)
            {
                bool nameUsed = false;
                if (entity.parent())
                {
                    nameUsed = ResourceLibrary::DoesNameExistInScope(entity, ResourceLibrary::FormatRelationshipName(requestedName, entity.parent().name().c_str()));
                }
                else
                {
                    nameUsed = m_world.lookup(requestedName.c_str());
                }

                if (!nameUsed)
                {
                    Cauda::HistoryStack::Execute(new Cauda::RenameEntityCommand(entity, entity.parent(), requestedName, m_world));
                }
                currentName = ResourceLibrary::GetStrippedName(entity.name().c_str());
                strcpy_s(nameBuf, sizeof(nameBuf), currentName.c_str());
                
            }
        }

        if (entity.has(flecs::Prefab))
        {
            ImGui::EndDisabled();
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Parent");
        ImGui::TableNextColumn();

        if (entity.has(flecs::ChildOf, flecs::Wildcard))
        {
            flecs::entity parent = entity.target(flecs::ChildOf);
            bool isParentSceneRoot = (parent.id() == m_sceneRoot.id());

            ImGui::Text("%s%s",
                parent.name() ? parent.name() : "Unnamed",
                isParentSceneRoot ? " (Scene Root)" : "");

            ImGui::SameLine();
            if (ImGui::SmallButton("Select##SelectParent"))
            {
                SelectEntity(parent);
            }
        }
        else
        {
            if (!entity.has(flecs::Prefab))
            {
                entity.add(flecs::ChildOf, m_sceneRoot);
                std::vector<flecs::entity_t> children;
                m_sceneRoot.children([&children](flecs::entity child)
                    {
                        children.push_back(child.id());
                    });

                m_sceneRoot.set_child_order(children.data(), children.size());
            }
        }

        ImGui::EndTable();
    }

    ImGui::Separator();

    bool drewAnyComponent = false;
    for (const auto& metadata : m_registeredComponents)
    {
        if (metadata.hasComponent(entity))
        {
            metadata.drawInspector(entity);
            drewAnyComponent = true;
        }
    }

    entity.each([&](flecs::id componentId)
    {
        if (!componentId.is_pair())
        {
            bool isRegistered = false;
            for (const auto& metadata : m_registeredComponents)
            {
                if (metadata.hasComponent(entity))
                {
                    isRegistered = true;
                    break;
                }
            }

            if (!isRegistered)
            {
                DrawDefaultComponent(entity, componentId);
                drewAnyComponent = true;
            }
        }
    });

    ImGui::Separator();
    if (ImGui::Button("Add Component", ImVec2(-1, 0)))
    {
        ImGui::OpenPopup("AddComponentPopup");
    }

    DrawAddComponentPopup();
}

void EditorModule::DrawAddComponentPopup()
{
    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        flecs::entity selectedEntity = m_selectedEntity;

        if (!selectedEntity || !selectedEntity.is_alive())
        {
            ImGui::EndPopup();
            return;
        }

        std::map<std::string, std::vector<const ComponentMetadata*>> componentsByCategory;

        for (const auto& metadata : m_registeredComponents)
        {
            if (!metadata.hasComponent(selectedEntity))
            {
                componentsByCategory[metadata.category].push_back(&metadata);
            }
        }

        for (const auto& [category, components] : componentsByCategory)
        {
            if (components.empty()) continue;

            if (componentsByCategory.size() > 1)
            {
                if (ImGui::BeginMenu(category.c_str()))
                {
                    for (const auto* metadata : components)
                    {
                        if (ImGui::MenuItem(metadata->name.c_str()))
                        {
                            metadata->addToEntity(selectedEntity);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::EndMenu();
                }
            }
            else
            {
                for (const auto* metadata : components)
                {
                    if (ImGui::MenuItem(metadata->name.c_str()))
                    {
                        metadata->addToEntity(selectedEntity);
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
        }

        ImGui::EndPopup();
    }
}

void EditorModule::DrawDefaultComponent(flecs::entity target, flecs::id componentId)
{
    if (!target.is_alive()) return;

    flecs::entity componentEntity = componentId.entity();
    const char* componentName = componentEntity.name();
    if (!componentName) return;

    if (ImGui::CollapsingHeader(componentName))
    {
        void* componentPtr = target.try_get_mut(componentId);
        if (!componentPtr) return;

        auto* componentData = componentEntity.try_get<flecs::Component>();
        if (!componentData || componentData->size == 0) return;

        flecs::cursor cursor(m_world, componentEntity, componentPtr);
        bool wasChanged = false;

        if (cursor.push())
        {
            while (cursor.next())
            {
                const char* memberName = cursor.get_member();
                flecs::entity memberType = cursor.get_type();
                void* memberPtr = cursor.get_ptr();

                ImGui::PushID(memberName);

                if (memberType == m_world.component<float>())
                {
                    if (ImGui::DragFloat(memberName, static_cast<float*>(memberPtr), 0.1f))
                        wasChanged = true;
                }
                else if (memberType == m_world.component<int>())
                {
                    if (ImGui::DragInt(memberName, static_cast<int*>(memberPtr)))
                        wasChanged = true;
                }
                else if (memberType == m_world.component<bool>())
                {
                    if (ImGui::Checkbox(memberName, static_cast<bool*>(memberPtr)))
                        wasChanged = true;
                }
                else if (memberType == m_world.component<glm::vec3>())
                {
                    if (ImGui::DragFloat3(memberName, glm::value_ptr(*static_cast<glm::vec3*>(memberPtr)), 0.1f))
                        wasChanged = true;
                }
                else
                {
                    ImGui::LabelText(memberName, "(%s)", memberType.name().c_str());
                }

                ImGui::PopID();
            }
            cursor.pop();
        }

        if (wasChanged)
        {
            target.modified(componentEntity);
        }
    }
}

void EditorModule::DrawGrid(ImVec2 viewportPos, ImVec2 viewportSize, CameraModule* camera)
{
    if (!m_gridEnabled || !camera) return;
    auto mainCam = camera->GetMainCamera();
    auto* camComp = mainCam.try_get<CameraComponent>();

    ImGuizmo::SetOrthographic(camComp->projectionMode == ProjectionMode::ORTHOGRAPHIC);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

    glm::mat4 viewMatrix = camera->GetViewMatrix(mainCam);
    glm::mat4 projectionMatrix = camera->GetProjectionMatrix(mainCam);

    glm::mat4 gridMatrix = glm::translate(glm::mat4(1.0f), m_gridCenter);

    glm::vec3 defaultNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 normal = glm::normalize(m_gridNormal);

    if (glm::length(normal - defaultNormal) > 0.001f)
    {
        glm::vec3 axis = glm::cross(defaultNormal, normal);
        if (glm::length(axis) > 0.001f)
        {
            float angle = acos(glm::clamp(glm::dot(defaultNormal, normal), -1.0f, 1.0f));
            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), angle, glm::normalize(axis));
            gridMatrix = gridMatrix * rotation;
        }
    }

    ImGuizmo::DrawGrid(
        glm::value_ptr(viewMatrix),
        glm::value_ptr(projectionMatrix),
        glm::value_ptr(gridMatrix),
        m_gridSize
    );
}

void EditorModule::CycleGizmoModes()
{
    switch (m_currentGizmoOperation)
    {
        case EditorModule::GizmoOperation::Translate:
            m_currentGizmoOperation = EditorModule::GizmoOperation::Rotate;
            break;
        case EditorModule::GizmoOperation::Rotate:
            m_currentGizmoOperation = EditorModule::GizmoOperation::Scale;
            break;
        case EditorModule::GizmoOperation::Scale:
            m_currentGizmoOperation = EditorModule::GizmoOperation::Translate;
            break;
        default:
            break;
    }
}

void EditorModule::CreatePrefabFromEntity(flecs::entity target, std::string prefabName)
{
    flecs::entity prefab = DuplicateEntity(target, target.parent());
    ResourceLibrary::SaveEntityAsPrefab(prefab, prefabName);
}

flecs::entity EditorModule::CreateEntityFromPrefab(const std::string& desiredName, const std::string& prefabName)
{
    auto prefab = ResourceLibrary::GetPrefab(prefabName.c_str());
    if (prefab)
    {
        std::string prefabName = desiredName + "Instance";
        flecs::entity newEntity = m_world.entity()
            .is_a(prefab)
            .child_of(m_sceneRoot);

        auto command = new Cauda::RenameEntityCommand(newEntity, m_sceneRoot, ResourceLibrary::GenerateUniqueName(prefabName.c_str(), m_sceneRoot.name().c_str()).c_str(), m_world);
        command->Execute();
        delete command;

        StripPrefabFromChildren(newEntity);

        return newEntity;
    }
    return flecs::entity::null();
}



void EditorModule::StripPrefabFromChildren(flecs::entity entity)
{
    std::vector<flecs::entity> children;
    entity.children([&](flecs::entity child)
        {
            children.push_back(child);
        });

    for (auto child : children)
    {
        if (child.has(flecs::Prefab))
        {
            child.remove(flecs::Prefab);
            StripPrefabFromChildren(child);
        }
    }
}

void EditorModule::HandleTransformGizmo(ImVec2 renderPos, ImVec2 renderSize, CameraModule* camera)
{
    if (!m_gizmoEnabled) return;

    flecs::entity selectedEntity = GetSelectedEntity();
    if (!selectedEntity || !selectedEntity.is_alive() || !selectedEntity.has<TransformComponent>())
        return;

    if (selectedEntity.has<NoGizmoTag>())
        return;

    auto transform = selectedEntity.try_get_mut<TransformComponent>();
    auto transformModule = m_world.try_get_mut<TransformModule>();
    if (!transform || !transformModule) return;

    auto mainCam = camera->GetMainCamera();
    glm::mat4 viewMatrix = camera->GetViewMatrix(mainCam);
    glm::mat4 projectionMatrix = camera->GetProjectionMatrix(mainCam);
    auto camComp = mainCam.try_get<CameraComponent>();

    if (camComp->projectionMode == PERSPECTIVE)
    {
        ImGuizmo::SetOrthographic(false);
    }
    else
	{
		ImGuizmo::SetOrthographic(true);
	}
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(renderPos.x, renderPos.y, renderSize.x, renderSize.y);


    ImGuizmo::OPERATION operation;
    switch (m_currentGizmoOperation)
    {
    case GizmoOperation::Translate: operation = ImGuizmo::TRANSLATE; break;
    case GizmoOperation::Rotate: operation = ImGuizmo::ROTATE; break;
    case GizmoOperation::Scale: operation = ImGuizmo::SCALE; break;
    default: operation = ImGuizmo::TRANSLATE; break;
    }

    ImGuizmo::MODE mode = (m_currentGizmoMode == GizmoMode::Local) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    glm::mat4 gizmoMatrix = Math::LocalToWorldMat(selectedEntity);

    float* snapPtr = nullptr;
    float snapValues[3] = { 0.0f, 0.0f, 0.0f };
    int currentOpIndex = static_cast<int>(m_currentGizmoOperation);
    bool snapEnabled = m_snapEnabled[currentOpIndex];
    bool temporarySnapToggle = (InputSystem::Inst()->IsKeyDown(PKey::X));
    bool useSnap = snapEnabled != temporarySnapToggle;

    if (useSnap)
    {
        float snapValue = m_snapValues[currentOpIndex];
        if (m_currentGizmoOperation == GizmoOperation::Translate)
        {
            snapValues[0] = snapValue;
            snapValues[1] = snapValue;
            snapValues[2] = snapValue;
            snapPtr = snapValues;
        }
        else
        {
            snapValues[0] = snapValue;
            snapPtr = snapValues;
        }
    }

    bool isManipulating = ImGuizmo::IsUsing();
    bool startedManipulating = isManipulating && !m_wasManipulating;
    bool finishedManipulating = !isManipulating && m_wasManipulating;

    bool isShiftHeld = InputSystem::Inst()->IsKeyDown(PKey::LShift) ||
        InputSystem::Inst()->IsKeyDown(PKey::RShift);

    if (startedManipulating)
    {
        m_cachedTransformForGizmo = *transform;
        m_manipulationDirty = true;

        if (isShiftHeld && !m_isDuplicateManipulation)
        {
            m_duplicatedEntity = DuplicateEntity(selectedEntity, selectedEntity.parent());
            if (m_duplicatedEntity && m_duplicatedEntity.is_alive())
            {
                SelectEntity(m_duplicatedEntity);
                selectedEntity = m_duplicatedEntity;
                transform = selectedEntity.try_get_mut<TransformComponent>();
                m_isDuplicateManipulation = true;

                if (m_currentGizmoMode == GizmoMode::World) 
                {
                    gizmoMatrix = Math::LocalToWorldMat(selectedEntity);
                }
                else
                {
                    if (transform)
                    {
                        glm::vec3 worldPos = Math::GetWorldPosition(selectedEntity);
                        glm::mat4 T = glm::translate(glm::mat4(1.0f), worldPos);
                        glm::mat4 R = glm::mat4_cast(transform->rotation);
                        glm::mat4 S = glm::scale(glm::mat4(1.0f), transform->scale);
                        gizmoMatrix = T * R * S;
                    }
                }
            }
        }
    }

    glm::mat4 originalMatrix = gizmoMatrix;

    bool wasManipulated = ImGuizmo::Manipulate(
        glm::value_ptr(viewMatrix),
        glm::value_ptr(projectionMatrix),
        operation,
        mode,
        glm::value_ptr(gizmoMatrix),
        nullptr,
        snapPtr
    );

    if (wasManipulated)
    {
        TransformComponent newWorldTransform = Math::MatrixToTransform(gizmoMatrix);

        if (m_currentGizmoOperation == GizmoOperation::Translate)
        {
            Math::SetWorldPosition(selectedEntity, newWorldTransform.position);
        }
        else if (m_currentGizmoOperation == GizmoOperation::Rotate)
        {
            Math::SetWorldRotation(selectedEntity, newWorldTransform.rotation);
        }
        else if (m_currentGizmoOperation == GizmoOperation::Scale)
        {
            TransformComponent originalWorldTransform = Math::MatrixToTransform(originalMatrix);

            glm::vec3 scaleChange = newWorldTransform.scale / originalWorldTransform.scale;
            transform->scale *= scaleChange;
        }

        if (transform)
        {
            transformModule->m_displayedTransform = *transform;
        }

        //if (m_currentGizmoOperation == GizmoOperation::Scale)
        //{
        //    auto* physicsModule = m_world.try_get_mut<PhysicsModule>();
        //    if (physicsModule) {
        //        physicsModule->SyncColliderWithTransform(selectedEntity);
        //    }
        //}
    }

    if (finishedManipulating)
    {
        if (m_manipulationDirty)
        {
            *transform = m_cachedTransformForGizmo;

            Cauda::HistoryStack::Execute(new Cauda::GizmoModifyCommand(
                selectedEntity,
                transform,
                &transformModule->m_displayedTransform
            ));

            m_manipulationDirty = false;
        }

        if (m_isDuplicateManipulation)
        {
            m_isDuplicateManipulation = false;
            m_duplicatedEntity = flecs::entity::null();
        }
    }

    m_wasManipulating = isManipulating;
}


flecs::entity EditorModule::DuplicateEntity(flecs::entity sourceEntity, flecs::entity parent)
{
    if (!sourceEntity || !sourceEntity.is_alive())
        return flecs::entity::null();

    if (sourceEntity.has(flecs::Prefab)) return flecs::entity::null();

    auto duplicatedEntity = sourceEntity.clone(true);

    duplicatedEntity.child_of(parent);
    Cauda::HistoryStack::Execute(new Cauda::RenameEntityCommand(duplicatedEntity, parent, ResourceLibrary::GenerateUniqueName(ResourceLibrary::GetStrippedName(sourceEntity.name().c_str()), parent.name().c_str()), m_world));

    if (!duplicatedEntity.has(flecs::IsA, flecs::Wildcard))
    {
        std::vector<flecs::entity> children;
        sourceEntity.children([&children](flecs::entity child)
            {
                children.push_back(child);
                return true;
            });

        for (auto& child : children)
        {
            auto duplicatedChild = DuplicateEntity(child, duplicatedEntity);
        }
    }

    return duplicatedEntity;
}

void EditorModule::RegisterEntityColour(std::function<ImVec4(flecs::entity)> colorProvider)
{
    m_colourProviders.push_back(colorProvider);
}

ImVec4 EditorModule::GetEntityColour(flecs::entity entity)
{
    ImVec4 colour(1.0f, 1.0f, 1.0f, 1.0f);

    for (const auto& provider : m_colourProviders)
    {
        ImVec4 providerColour = provider(entity);
        if (providerColour.w > 0.0f)
        {
            colour = providerColour;
            break;
        }
    }

    return colour;
}

bool EditorModule::CanReparentEntity(flecs::entity child, flecs::entity newParent)
{
    if (!child.is_alive() || !newParent.is_alive())
        return false;

    if (child.id() == newParent.id())
        return false;

    flecs::entity current = newParent;
    while (current.has(flecs::ChildOf, flecs::Wildcard))
    {
        current = current.target(flecs::ChildOf);
        if (current.id() == child.id())
            return false; 
    }

    return true;
}

void EditorModule::ReparentEntity(flecs::entity entity, flecs::entity newParent)
{
    if (!CanReparentEntity(entity, newParent))
        return;

    Cauda::HistoryStack::Execute(new Cauda::ReparentCommand(entity, newParent, entity.parent(), m_world));

}

bool EditorModule::SaveScene(const std::string& filePath)
{
    std::string path = g_ContentPath + "Scenes/" + filePath;
    return SceneIO::SaveSceneToFile(path, m_world);
}

bool EditorModule::LoadScene(const std::string& filePath)
{
    GrimoriumModule::SetLoading(true);

    std::string path = g_ContentPath + "Scenes/" + filePath;
    bool success = SceneIO::LoadSceneFromFile(path, m_world, m_sceneRoot);

    GrimoriumModule::SetLoading(false);
    return success;
}

void EditorModule::SetCurrentSceneName(const std::string& sceneName)
{
    m_currentSceneName = sceneName;
}

const std::string& EditorModule::GetCurrentSceneName()
{
    return m_currentSceneName;
}

void EditorModule::SetSceneRoot(flecs::entity root)
{
    if (root) m_sceneRoot = root;
}

void EditorModule::SetSelectedEntity(flecs::entity selected)
{
    if (selected)
    m_selectedEntity = selected;
}

flecs::entity EditorModule::GetSelectedEntity() const
{
    if (m_selectedEntity && m_selectedEntity.is_alive())
    {
        return m_selectedEntity;
    }
    return flecs::entity::null();
}

void EditorModule::SelectEntity(flecs::entity entity)
{
    if (entity.id() == m_selectedEntity.id()) return;
    if (entity.has<SelectedTag>())
    {
        entity.remove<SelectedTag>();
    }

    Cauda::HistoryStack::Execute(new Cauda::SelectEntityCommand(entity, m_selectedEntity));
    s_scrollToSelection = true;
}

void EditorModule::CreateEntity(const std::string& name, flecs::entity parent)
{
    if (!parent || !parent.is_alive())
    {
        parent = m_sceneRoot;
    }

    Cauda::HistoryStack::Execute(new Cauda::CreateEntityCommand("Entity", parent, m_world));

}

void EditorModule::DeleteEntity(flecs::entity entity)
{
    if (!entity.is_alive()) return;
    if (entity.id() == m_sceneRoot.id()) return;

    Cauda::HistoryStack::Execute(new Cauda::DeleteEntityCommand(entity));
}
