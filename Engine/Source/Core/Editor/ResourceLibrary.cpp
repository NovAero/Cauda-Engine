#include "cepch.h"
#include "ResourceLibrary.h"
#include "GrimoriumModule.h"
#include "Core/Utilities/Serialiser.h"
#include "Core/Scene/SceneSerialiser.h"
#include "Core/Editor/EditorModule.h"
#include "Renderer/RendererModule.h"
#include "EditorCommands.h"
#include "Core/Audio/AudioModule.h"
#include "Core/Utilities/ImGuiUtilities.h"
#include <iostream>

ResourceLibrary* ResourceLibrary::s_instance = nullptr;

void ResourceLibrary::SetInstance(ResourceLibrary* instance)
{
    s_instance = instance;
}

void ResourceLibrary::LoadAllContent()
{
    auto assetSettings = g_EngineSettingsEntity.try_get_mut<AssetBrowserSettings>();

    if (assetSettings)
    {
        s_instance->m_iconSize = assetSettings->iconSize;
        s_instance->m_iconSpacing = assetSettings->iconSpacing;
        s_instance->m_stretchSpacing = assetSettings->stretchSpacing;
    }

    s_instance->LoadScenesFromFilesystem();
    s_instance->LoadPrefabsFromFilesystem();
    s_instance->LoadAudioFromFilesystem();
    s_instance->LoadMaterialsFromFilesystem();
}

Mesh* ResourceLibrary::GetMesh(AssetHandle name)
{
    if (!s_instance || name == "") return nullptr;
    auto it = s_instance->m_meshes.find(name);
    return (it != s_instance->m_meshes.end()) ? it->second.get() : nullptr;
}

SkeletalMesh* ResourceLibrary::GetSkeletalMesh(AssetHandle name)
{
    if (!s_instance || name == "") return nullptr;
    auto it = s_instance->m_skeletalMeshes.find(name);
    return (it != s_instance->m_skeletalMeshes.end()) ? it->second.get() : nullptr;
}

Material* ResourceLibrary::GetMaterial(AssetHandle name)
{
    if (!s_instance || name == "") return nullptr;
    auto it = s_instance->m_materials.find(name);
    return (it != s_instance->m_materials.end()) ? it->second.get() : nullptr;
}

ComputeShader* ResourceLibrary::GetComputeShader(AssetHandle name)
{
    if (!s_instance || name == "") return nullptr;
    auto it = s_instance->m_computeShaders.find(name);
    return (it != s_instance->m_computeShaders.end()) ? it->second.get() : nullptr;
}

ShaderProgram* ResourceLibrary::GetShader(AssetHandle name)
{
    if (!s_instance || name == "") return nullptr;
    auto it = s_instance->m_shaders.find(name);
    return (it != s_instance->m_shaders.end()) ? it->second.get() : nullptr;
}

Texture* ResourceLibrary::GetTexture(AssetHandle name)
{
    if (!s_instance || name == "") return nullptr;
    auto it = s_instance->m_textures.find(name);
    return (it != s_instance->m_textures.end()) ? it->second.get() : nullptr;
}

//Grimoire* ResourceLibrary::GetGrimoire(AssetHandle name)
//{
//    if (!s_instance) return nullptr;
//    if (name == "")
//    {
//        auto it = s_instance->m_scripts.find("None");
//        return (it != s_instance->m_scripts.end()) ? it->second.get() : nullptr;
//    }
//    auto it = s_instance->m_scripts.find(std::string(name));
//    return (it != s_instance->m_scripts.end()) ? it->second.get() : nullptr;
//}

flecs::entity ResourceLibrary::GetPrefab(AssetHandle name)
{
    if (!s_instance || name == "") return flecs::entity::null();
    std::string prefabString = name;
    if (prefabString.find("PF_") == std::string::npos)
    {
        prefabString = "PF_" + prefabString;
    }
    auto it = s_instance->m_prefabs.find(prefabString);
    if (it != s_instance->m_prefabs.end())
    {
        return s_instance->m_world->get_alive(s_instance->m_prefabs[prefabString]);
    }
    return flecs::entity::null();
}

fs::path ResourceLibrary::GetScene(AssetHandle name)
{
    if (!s_instance || name == "") return fs::path();
    auto it = s_instance->m_scenes.find(name);
    if (it != s_instance->m_scenes.end())
    {
        return s_instance->m_scenes[name];
    }
    return fs::path();
}

SoundWave* ResourceLibrary::GetSoundWave(AssetHandle name)
{
    if (!s_instance || name == "") return nullptr;
    auto it = s_instance->m_sounds.find(name);
    if (it == s_instance->m_sounds.end()) return nullptr;
    if (it->second->settings.type != SoundType::WAVE) return nullptr;
    return reinterpret_cast<SoundWave*>(it->second.get());
}

SoundStream* ResourceLibrary::GetSoundStream(AssetHandle name)
{
    if (!s_instance || name == "") return nullptr;
    auto it = s_instance->m_sounds.find(name);
    if (it == s_instance->m_sounds.end()) return nullptr;
    if (it->second->settings.type != SoundType::STREAM) return nullptr;
    return reinterpret_cast<SoundStream*>(it->second.get());
}

AudioAsset* ResourceLibrary::GetAudioAsset(AssetHandle name)
{
    if (!s_instance || name == "") return nullptr;
    auto it = s_instance->m_sounds.find(name);
    return (it != s_instance->m_sounds.end()) ? it->second.get() : nullptr;
}

Mesh* ResourceLibrary::LoadMesh(const std::string& name, const std::string& filepath)
{
    if (!s_instance) return nullptr;
    auto mesh = std::make_unique<Mesh>();
    if (!filepath.empty())
        mesh->LoadFromFile(filepath.c_str());

    s_instance->m_allAssets.emplace_back(name, filepath, "Mesh", mesh.get());

    Mesh* ptr = mesh.get();
    s_instance->m_meshes[name] = std::move(mesh);
    return ptr;
}

SkeletalMesh* ResourceLibrary::LoadSkeletalMesh(
    const std::string& name,
    const std::string& meshFile,
    const std::string& skeletonFile,
    float unitScale)
{
    if (!s_instance) return nullptr;

    auto mesh = std::make_unique<SkeletalMesh>();

    if (!mesh->LoadSkeleton(skeletonFile.c_str()))
    {
        std::cerr << "Failed to load skeleton: " << skeletonFile << std::endl;
        return nullptr;
    }

    if (!mesh->LoadMesh(meshFile.c_str(), unitScale))
    {
        std::cerr << "Failed to load skeletal mesh: " << meshFile << std::endl;
        return nullptr;
    }

    s_instance->m_allAssets.emplace_back(name, meshFile, "SkeletalMesh", mesh.get());

    SkeletalMesh* ptr = mesh.get();
    s_instance->m_skeletalMeshes[name] = std::move(mesh);

    return ptr;
}

bool ResourceLibrary::LoadSkeletalAnimation(const std::string& meshHandle,
    const std::string& animName,
    const std::string& animFile)
{
    auto* mesh = GetSkeletalMesh(meshHandle.c_str());
    if (!mesh)
    {
        std::cerr << "Skeletal mesh '" << meshHandle << "' not found" << std::endl;
        return false;
    }

    return mesh->LoadAnimation(animName, animFile.c_str());
}

Material* ResourceLibrary::LoadMaterial(const std::string& name, const std::string& vertPath, const std::string& fragPath)
{
    if (!s_instance) return nullptr;
    auto shader = LoadShader(name + "_Shader", vertPath, fragPath);
    auto material = std::make_unique<Material>(shader);

    s_instance->m_allAssets.emplace_back(name, vertPath + " + " + fragPath, "Material", material.get());

    Material* ptr = material.get();
    s_instance->m_materials[name] = std::move(material);
    return ptr;
}

Material* ResourceLibrary::LoadMaterial(const std::string& name, ShaderProgram* shader)
{
    if (!s_instance) return nullptr;
    auto material = std::make_unique<Material>(shader);

    std::string shaderPath = "";
    if (shader)
    {
        shaderPath = shader->m_vertexFilename + " + " + shader->m_fragmentFilename;
    }

    s_instance->m_allAssets.emplace_back(name, shaderPath, "Material", material.get());

    Material* ptr = material.get();
    s_instance->m_materials[name] = std::move(material);
    return ptr;
}

ShaderProgram* ResourceLibrary::LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath)
{
    if (!s_instance) return nullptr;
    auto shader = std::make_unique<ShaderProgram>(vertPath, fragPath);

    s_instance->m_allAssets.emplace_back(name, vertPath + " + " + fragPath, "Shader", shader.get());

    ShaderProgram* ptr = shader.get();
    s_instance->m_shaders[name] = std::move(shader);
    return ptr;
}

ComputeShader* ResourceLibrary::LoadComputeShader(const std::string& name, const std::string& computePath)
{
    if (!s_instance) return nullptr;
    auto computeShader = std::make_unique<ComputeShader>(computePath);

    s_instance->m_allAssets.emplace_back(name, computePath, "ComputeShader", computeShader.get());

    ComputeShader* ptr = computeShader.get();
    s_instance->m_computeShaders[name] = std::move(computeShader);
    return ptr;
}

Texture* ResourceLibrary::LoadTexture(const std::string& name, const std::string& filepath)
{
    if (!s_instance) return nullptr;
    auto texture = std::make_unique<Texture>();
    if (!filepath.empty())
    {
        if (!texture->LoadFromFile(filepath))
        {
            std::cerr << "Failed to load texture: " << filepath << std::endl;
            return nullptr;
        }
    }

    s_instance->m_allAssets.emplace_back(name, filepath, "Texture", texture.get());

    Texture* ptr = texture.get();
    s_instance->m_textures[name] = std::move(texture);
    return ptr;
}
//
//Grimoire* ResourceLibrary::LoadGrimoire(Grimoire script)
//{
//    if (!s_instance) return nullptr;
//    auto grimoire = std::make_unique<Grimoire>();
//
//    *grimoire = script;
//
//    s_instance->m_allAssets.emplace_back(script.scriptName, "", "Grimoire", grimoire.get());
//
//    Grimoire* ptr = grimoire.get();
//    s_instance->m_scripts[script.scriptName] = std::move(grimoire);
//
//    return ptr;
//}

flecs::entity ResourceLibrary::LoadPrefab(const std::string& name, const std::string& filepath)
{
    std::string formattedName = name;
    if (size_t i = name.find("PF_") == std::string::npos)
    {
        formattedName = "PF_" + name;
    }

    flecs::entity prefab = s_instance->m_world->lookup(formattedName.c_str());

    if (!prefab)
    {
        if (fs::exists(filepath))
        {
            std::string json = CerealBox::LoadFileAsString(filepath);
            s_instance->m_world->from_json(json.c_str());
            prefab = s_instance->m_world->lookup(formattedName.c_str());

            prefab.modified<TransformComponent>();

            if (!prefab)
            {
                EditorConsole::Inst()->AddConsoleError("Failed to load prefab: " + formattedName, true);
                return flecs::entity::null();
            }
        }
    }

    s_instance->RegisterName(formattedName, "");
    s_instance->m_prefabs[formattedName] = prefab.id();
    s_instance->m_allAssets.emplace_back(formattedName, filepath, "Prefab", &s_instance->m_prefabs[formattedName]);

    return prefab;
}

fs::path ResourceLibrary::LoadScene(const std::string& name, const std::string& filePath)
{
    fs::path scenePath = filePath;

    if (fs::exists(scenePath))
    {
        s_instance->m_scenes[name] = scenePath;
        s_instance->m_allAssets.emplace_back(name, scenePath.string(), "Scene", &s_instance->m_scenes[name]);
        return scenePath;
    }
    else {
        return fs::path();
    }
}

SoundWave* ResourceLibrary::LoadSoundWave(const std::string& name, const std::string filePath)
{
    if (!s_instance) return nullptr;
    auto cue = std::make_unique<SoundWave>();
    if (!filePath.empty())
    {
        if (cue->data.load(filePath.c_str()) != 0)
        {
            std::cerr << "Failed to load audio: " << filePath << std::endl;
            return nullptr;
        }
    }

    cue->name = name;
    cue->settings.type = SoundType::WAVE;

    s_instance->m_allAssets.emplace_back(name, filePath, "Sound Wave", cue.get());

    SoundWave* ptr = cue.get();
    s_instance->m_sounds[name] = std::move(cue);
    return ptr;
}

SoundStream* ResourceLibrary::LoadSoundStream(const std::string& name, const std::string filePath)
{
    if (!s_instance) return nullptr;
    auto cue = std::make_unique<SoundStream>();
    if (!filePath.empty())
    {
        if (cue->data.load(filePath.c_str()) != 0)
        {
            std::cerr << "Failed to load audio: " << filePath << std::endl;
            return nullptr;
        }
    }

    cue->name = name;
    cue->settings.type = SoundType::STREAM;

    s_instance->m_allAssets.emplace_back(name, filePath, "Sound Stream", cue.get());

    SoundStream* ptr = cue.get();
    s_instance->m_sounds[name] = std::move(cue);
    return ptr;
}

bool ResourceLibrary::RegisterChildNames(flecs::entity entity)
{
    bool hasChildren = false;
    RegisterName(entity.name().c_str(), entity.parent().name().c_str());
    entity.children([&](flecs::entity child)
        {
            hasChildren = true;
            if (hasChildren)
            {
                RegisterChildNames(child);
            }
        });
    return hasChildren;
}

void ResourceLibrary::RegisterName(std::string name, std::string parent)
{
    //Remove the parent's name from the child
    size_t i = name.find(parent);
    if (i != std::string::npos)
    {
        name.erase(i, i + parent.size());
    }
    i = name.find("(");
    if (i != std::string::npos)
    {
        name.erase(i, name.size() - i);
    }
    i = parent.find("(");
    if (i != std::string::npos)
    {
        parent.erase(i, parent.size() - i);
    }

    //Erase all '_' and number characters in the intended name
    std::string cleanIntended = name;
    cleanIntended.erase(std::remove_if(cleanIntended.begin(), cleanIntended.end(), [](char c)
        {
            return c == '|' || c == ' ';
        }), cleanIntended.end());

    std::string cleanParent = parent;
    cleanParent.erase(std::remove_if(cleanParent.begin(), cleanParent.end(), [](char c)
        {
            return c == '|' || c == ' ';
        }), cleanParent.end());

    if (cleanParent == "Root") cleanParent = "";
    s_instance->m_usedNames[cleanParent + cleanIntended]++;
}

std::string ResourceLibrary::GenerateUniqueName(std::string intendedName, std::string parentName)
{
    if (parentName == "" || parentName == "Root")
    {
        //Erase and format name
        std::string cleanIntended = intendedName;
        cleanIntended.erase(std::remove_if(cleanIntended.begin(), cleanIntended.end(), [](char c)
            {
                return c == '|' || c == ' ';
            }), cleanIntended.end());

        size_t i = cleanIntended.find("(");
        if (i != std::string::npos)
        {
            cleanIntended.erase(i, cleanIntended.size() - i);
        }

        if (s_instance->m_usedNames[cleanIntended] > 0)
        {
            s_instance->m_usedNames[cleanIntended]++;
            return cleanIntended + '(' + std::to_string(s_instance->m_usedNames[cleanIntended]) + ')';
        }
        else
        {
            s_instance->m_usedNames[cleanIntended]++;
            return intendedName;
        }
    }
    else
    {
        //Remove the parent's name from the child
        size_t i = intendedName.find(parentName);
        if (i != std::string::npos)
        {
            intendedName.erase(i, i + parentName.size());
        }

        //Erase all '_' and number characters in the intended name
        std::string cleanIntended = intendedName;
        cleanIntended.erase(std::remove_if(cleanIntended.begin(), cleanIntended.end(), [](char c)
            {
                return std::isdigit(c) || c == '|' || c == ' ';
            }), cleanIntended.end());

        i = cleanIntended.find("(");
        if (i != std::string::npos)
        {
            cleanIntended.erase(i, cleanIntended.size() - i);
        }

        if (s_instance->m_usedNames[parentName + cleanIntended] > 0)
        {
            s_instance->m_usedNames[parentName + cleanIntended]++;
            return parentName + "|" + cleanIntended + '(' + std::to_string(s_instance->m_usedNames[parentName + cleanIntended]) + ')';
        }
        else
        {
            s_instance->m_usedNames[parentName + cleanIntended]++;
            return parentName + "|" + cleanIntended;
        }
    }
}

std::string ResourceLibrary::FormatRelationshipName(std::string child, std::string parent)
{
    child.erase(std::remove_if(child.begin(), child.end(), [](char c)
        {
            return c == '|' || c == ' ';
        }), child.end());

    int i = child.find(parent);
    int j = child.find(child);
    if (i != std::string::npos && j != i)
    {
        child.erase(i, i + parent.size());
    }

    if (parent == "Root" || parent == "")
    {
        return child;
    }
    else
    {
        return parent + "|" + child;
    }
}

std::string ResourceLibrary::GetStrippedName(std::string name)
{
    int i = name.find_last_of("|");
    if (i != std::string::npos)
    {
        name.erase(0, i + 1);
    }

    return name;
}

bool ResourceLibrary::DoesNameExistInScope(flecs::entity entity, std::string name)
{
    bool nameUsed = false;
    entity.children([&](flecs::entity child)
        {
            std::string childName = child.name().c_str();
            if (childName == name)
            {
                nameUsed = true;
            }
        });
    return nameUsed;
}


void ResourceLibrary::ResetUsednames()
{
    s_instance->m_usedNames.clear();
}

void ResourceLibrary::DrawAssetBrowser(bool* p_open)
{
    if (p_open && !(*p_open)) return;
    if (!s_instance) return;

    ImGui::SetNextWindowSize(ImVec2(s_instance->m_iconSize * 15, s_instance->m_iconSize * 10), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Library", p_open, ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        return;
    }

    static Texture* uiAtlas = nullptr;
    if (!uiAtlas)
    {
        uiAtlas = ResourceLibrary::GetTexture("icon_atlas");
        if (!uiAtlas)
        {
            uiAtlas = ResourceLibrary::LoadTexture("icon_atlas", "content/textures/icon_atlas.png");
        }
    }

    const int atlasColumns = 10;
    const int atlasRows = 10;

    const int meshIconIndex = 45;
    const int materialIconIndex = 42;
    const int shaderIconIndex = 48;
    const int textureIconIndex = 42;
    const int prefabIconIndex = 45;
    const int sceneIconIndex = 46;
    const int soundWaveIconIndex = 40;
    const int soundStreamIconIndex = 40;
    const int grimoireIconIndex = 41;
    const int computeShaderIconIndex = 48;

    if (ImGui::BeginMenuBar())
    {
        auto assetSettings = g_EngineSettingsEntity.try_get_mut<AssetBrowserSettings>();

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Reload Shaders"))
                ReloadAllShaders();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            ImGui::SliderFloat("Icon Size", &s_instance->m_iconSize, 32.0f, 128.0f);
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                assetSettings->iconSize = s_instance->m_iconSize;
                g_EngineSettingsEntity.modified<AssetBrowserSettings>();
            }
            ImGui::SliderInt("Icon Spacing", &s_instance->m_iconSpacing, 0, 32);
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                assetSettings->iconSpacing = s_instance->m_iconSpacing;
                g_EngineSettingsEntity.modified<AssetBrowserSettings>();
            }
            if (ImGui::Checkbox("Stretch Spacing", &s_instance->m_stretchSpacing))
            {
                assetSettings->stretchSpacing = s_instance->m_stretchSpacing;
                g_EngineSettingsEntity.modified<AssetBrowserSettings>();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    s_instance->m_filter.Draw("Filter", -100.0f);

    if (ImGui::BeginChild("AssetGrid", ImVec2(0, 0), ImGuiChildFlags_Borders))
    {
        const float availWidth = ImGui::GetContentRegionAvail().x;
        ImVec2 itemSize, itemStep;
        int columnCount;
        s_instance->UpdateLayoutSizes(availWidth, itemSize, itemStep, columnCount);

        ImVec2 startPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        int itemIndex = 0;
        for (const auto& asset : s_instance->m_allAssets)
        {
            if (!s_instance->m_filter.PassFilter(asset.handle.c_str()))
                continue;

            int column = itemIndex % columnCount;
            int row = itemIndex / columnCount;

            ImVec2 pos = ImVec2(
                startPos.x + column * itemStep.x,
                startPos.y + row * itemStep.y
            );

            ImGui::SetCursorScreenPos(pos);
            ImGui::PushID(asset.id);

            bool isSelected = false;
            if (ImGui::Selectable("##asset", isSelected, ImGuiSelectableFlags_None, itemSize))
            {
                std::cout << "Selected: " << asset.handle << std::endl;
                if (asset.type == "Prefab")
                {
                    auto prefab = GetPrefab(asset.handle.c_str());

                    auto query = s_instance->m_world->query_builder<SelectedTag>().build();
                    flecs::entity previous;
                    query.each([&](flecs::entity entity, SelectedTag)
                        {
                            previous = entity;
                        });

                    Cauda::HistoryStack::Execute(new Cauda::SelectEntityCommand(prefab, previous));
                    if (prefab.has<SelectedTag>())
                    {
                        std::cout << "added selected tag\n";
                    }
                }
            }

            ImVec2 iconMin = pos;
            ImVec2 iconMax = ImVec2(pos.x + itemSize.x, pos.y + itemSize.y);

            ImU32 bgColour = ImGui::GetColorU32(IM_COL32(60, 60, 60, 255));
            drawList->AddRectFilled(iconMin, iconMax, bgColour, 4.0f);

            if (uiAtlas && uiAtlas->GetHandle() != 0)
            {
                float textAreaHeight = ImGui::GetFontSize() * 2.0f + 8.0f;
                float iconPadding = 8.0f;

                float availableIconSpace = glm::min(itemSize.x - iconPadding * 2, itemSize.y - textAreaHeight - iconPadding);
                ImVec2 iconSize = ImVec2(availableIconSpace, availableIconSpace);

                ImVec2 iconPos = ImVec2(
                    iconMin.x + (itemSize.x - iconSize.x) * 0.5f,
                    iconMin.y + iconPadding
                );

                int iconIndex = meshIconIndex;
                ImVec4 iconColour = Cauda::Colour::TextWhite;

                if (asset.type == "Mesh")
                {
                    iconIndex = meshIconIndex;
                    iconColour = Cauda::Colour::HumbleBlue;
                }
                else if (asset.type == "Material")
                {
                    iconIndex = materialIconIndex;
                    iconColour = Cauda::Colour::HumbleRed;
                }
                else if (asset.type == "Shader")
                {
                    iconIndex = shaderIconIndex;
                    iconColour = Cauda::Colour::HumblePurple;
                }
                else if (asset.type == "Texture")
                {
                    Texture* texture = static_cast<Texture*>(asset.resource);
                    if (texture && texture->GetHandle() != 0)
                    {
                        ImGui::SetCursorScreenPos(iconPos);
                        ImGui::Image(
                            (ImTextureID)(uintptr_t)texture->GetHandle(),
                            iconSize
                        );
                    }
                    else
                    {
                        iconIndex = textureIconIndex;
                        iconColour = Cauda::Colour::TextWhite;
                        Cauda::UI::DrawImageAtlasIndex(
                            (ImTextureID)(uintptr_t)uiAtlas->GetHandle(),
                            iconPos, iconSize,
                            iconIndex,
                            atlasColumns, atlasRows,
                            iconColour
                        );
                    }
                }
                else if (asset.type == "Prefab")
                {
                    iconIndex = prefabIconIndex;
                    iconColour = Cauda::Colour::HumblePink;
                }
                else if (asset.type == "Scene")
                {
                    iconIndex = sceneIconIndex;
                    iconColour = Cauda::Colour::HumbleGreen;
                }
                else if (asset.type == "Sound Wave")
                {
                    iconIndex = soundWaveIconIndex;
                    iconColour = Cauda::Colour::HumbleYellow;
                }
                else if (asset.type == "Sound Stream")
                {
                    iconIndex = soundStreamIconIndex;
                    iconColour = Cauda::Colour::HumbleYellow;
                }
                else if (asset.type == "Grimoire")
                {
                    iconIndex = grimoireIconIndex;
                    iconColour = Cauda::Colour::HumblePurple;
                }
                else if (asset.type == "ComputeShader")
                {
                    iconIndex = computeShaderIconIndex;
                    iconColour = Cauda::Colour::HumblePurple;
                }

                if (asset.type != "Texture")
                {
                    Cauda::UI::DrawImageAtlasIndex(
                        (ImTextureID)(uintptr_t)uiAtlas->GetHandle(),
                        iconPos, iconSize,
                        iconIndex,
                        atlasColumns, atlasRows,
                        iconColour
                    );
                }
            }
            else
            {
                if (asset.type == "Texture")
                {
                    Texture* texture = static_cast<Texture*>(asset.resource);
                    if (texture && texture->GetHandle() != 0)
                    {
                        ImVec2 textureMin = ImVec2(iconMin.x + 4, iconMin.y + 4);
                        ImVec2 textureMax = ImVec2(iconMax.x - 4, iconMax.y - 4);

                        ImGui::SetCursorScreenPos(textureMin);
                        ImGui::Image(
                            (ImTextureID)(uintptr_t)texture->GetHandle(),
                            ImVec2(textureMax.x - textureMin.x, textureMax.y - textureMin.y)
                        );
                    }
                }

                ImU32 typeColour = asset.type == "Mesh" ? IM_COL32(100, 150, 255, 255) :
                    asset.type == "Material" ? IM_COL32(255, 150, 100, 255) :
                    asset.type == "Shader" ? IM_COL32(150, 255, 100, 255) :
                    asset.type == "Texture" ? IM_COL32(255, 100, 255, 255) :
                    IM_COL32(200, 200, 200, 255);

                drawList->AddRectFilled(
                    ImVec2(iconMax.x - 8, iconMin.y + 2),
                    ImVec2(iconMax.x - 2, iconMin.y + 8),
                    typeColour
                );
            }

            const char* label = asset.handle.c_str();
            float textAreaHeight = ImGui::GetFontSize() * 2.0f + 4.0f;
            ImVec2 textAreaPos = ImVec2(pos.x + 4, pos.y + itemSize.y - textAreaHeight);
            ImVec2 textAreaSize = ImVec2(itemSize.x - 8, textAreaHeight);

            ImVec2 fullTextSize = ImGui::CalcTextSize(label);
            if (fullTextSize.x <= textAreaSize.x)
            {
                ImVec2 textPos = ImVec2(
                    textAreaPos.x + (textAreaSize.x - fullTextSize.x) * 0.5f,
                    textAreaPos.y + (textAreaSize.y - ImGui::GetFontSize()) * 0.5f
                );
                drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), label);
            }
            else
            {
                std::string labelStr = label;
                std::vector<std::string> lines;

                std::string currentLine = "";
                std::istringstream words(labelStr);
                std::string word;

                while (words >> word)
                {
                    std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
                    ImVec2 testSize = ImGui::CalcTextSize(testLine.c_str());

                    if (testSize.x <= textAreaSize.x)
                    {
                        currentLine = testLine;
                    }
                    else
                    {
                        if (!currentLine.empty())
                        {
                            lines.push_back(currentLine);
                            currentLine = word;
                        }
                        else
                        {
                            lines.push_back(word);
                        }
                    }
                }

                if (!currentLine.empty())
                {
                    lines.push_back(currentLine);
                }

                for (int i = 0; i < glm::min(2, (int)lines.size()); ++i)
                {
                    const std::string& line = lines[i];
                    ImVec2 lineSize = ImGui::CalcTextSize(line.c_str());
                    ImVec2 linePos = ImVec2(
                        textAreaPos.x + (textAreaSize.x - lineSize.x) * 0.5f,
                        textAreaPos.y + i * ImGui::GetFontSize() + 2
                    );

                    if (i == 1 && lines.size() > 2)
                    {
                        std::string truncatedLine = line;
                        while (truncatedLine.length() > 3)
                        {
                            truncatedLine = truncatedLine.substr(0, truncatedLine.length() - 1);
                            std::string testLine = truncatedLine + "...";
                            ImVec2 testSize = ImGui::CalcTextSize(testLine.c_str());
                            if (testSize.x <= textAreaSize.x)
                            {
                                drawList->AddText(
                                    ImVec2(textAreaPos.x + (textAreaSize.x - testSize.x) * 0.5f, linePos.y),
                                    ImGui::GetColorU32(ImGuiCol_Text),
                                    testLine.c_str()
                                );
                                break;
                            }
                        }
                    }
                    else
                    {
                        drawList->AddText(linePos, ImGui::GetColorU32(ImGuiCol_Text), line.c_str());
                    }
                }
            }

            if (ImGui::BeginPopupContextItem(asset.handle.c_str()))
            {
                ImGui::Text("Asset: %s", asset.handle.c_str());
                ImGui::Separator();

                if (asset.type == "Shader" && ImGui::MenuItem("Reload Shader"))
                {
                    ShaderProgram* shader = static_cast<ShaderProgram*>(asset.resource);
                    if (shader)
                        shader->Reload();
                }
                else if (asset.type == "ComputeShader" && ImGui::MenuItem("Reload Shader"))
                {
                    ComputeShader* computeShader = static_cast<ComputeShader*>(asset.resource);
                    if (computeShader)
                        computeShader->Reload();
                }
                else if (asset.type == "Prefab")
                {
                    if (ImGui::MenuItem("Edit Prefab"))
                    {
                        auto editorModule = s_instance->m_world->try_get_mut<EditorModule>();
                        if (editorModule)
                        {
                            // Enter prefab editing scene
                        }
                    }
                    if (ImGui::MenuItem("Create Instance"))
                    {
                        auto editorModule = s_instance->m_world->try_get_mut<EditorModule>();
                        if (editorModule)
                        {
                            editorModule->CreateEntityFromPrefab(asset.handle, asset.handle);
                        }
                    }
                }
                else if (asset.type == "Sound Wave" || asset.type == "Sound Stream")
                {
                    if (ImGui::MenuItem("Edit Sound"))
                    {
                        auto audioModule = s_instance->m_world->try_get_mut<AudioModule>();
                        audioModule->SelectSoundAsset(static_cast<AudioAsset*>(asset.resource));
                    }

                    if (ImGui::MenuItem("Play Sound"))
                    {
                        auto audioModule = s_instance->m_world->try_get_mut<AudioModule>();
                        audioModule->ForceSound2D(static_cast<AudioAsset*>(asset.resource));
                    }
                }

                ImGui::EndPopup();
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("Name: %s", asset.handle.c_str());
                ImGui::Text("Type: %s", asset.type.c_str());
                if (!asset.filepath.empty())
                    ImGui::Text("Path: %s", asset.filepath.c_str());
                ImGui::EndTooltip();
            }

            ImGui::PopID();
            itemIndex++;
        }
    }
    ImGui::EndChild();

    ImGui::Text("Assets: %d", (int)s_instance->m_allAssets.size());
    ImGui::End();
}

void ResourceLibrary::ReloadAllShaders()
{
    if (!s_instance) return;
    for (auto& [name, shader] : s_instance->m_shaders)
        shader->Reload();

    for (auto& [name, computeShader] : s_instance->m_computeShaders)
        computeShader->Reload();
}

void ResourceLibrary::Clear()
{
    if (!s_instance) return;
    s_instance->m_meshes.clear();
    s_instance->m_skeletalMeshes.clear();
    s_instance->m_materials.clear();
    s_instance->m_shaders.clear();
    s_instance->m_textures.clear();
    s_instance->m_allAssets.clear();
}

void ResourceLibrary::SetWorld(flecs::world* world)
{
    s_instance->m_world = world;
}

std::vector<const char*> ResourceLibrary::GetAllMeshHandles()
{
    std::vector<const char*> handles;
    if (!s_instance) return handles;

    for (const auto& asset : s_instance->m_allAssets)
    {
        if (asset.type == "Mesh")
        {
            handles.push_back(asset.handle.c_str());
        }
    }

    std::sort(handles.begin(), handles.end(), [](const char* a, const char* b) {
        return strcmp(a, b) < 0;
        });

    return handles;
}

std::vector<const char*> ResourceLibrary::GetAllSkeletalMeshHandles()
{
    std::vector<const char*> handles;
    if (!s_instance) return handles;

    for (const auto& asset : s_instance->m_allAssets)
    {
        if (asset.type == "SkeletalMesh")
        {
            handles.push_back(asset.handle.c_str());
        }
    }

    std::sort(handles.begin(), handles.end(), [](const char* a, const char* b) {
        return strcmp(a, b) < 0;
        });

    return handles;
}

std::vector<const char*> ResourceLibrary::GetAllMaterialHandles()
{
    std::vector<const char*> handles;
    if (!s_instance) return handles;

    for (const auto& asset : s_instance->m_allAssets)
    {
        if (asset.type == "Material")
        {
            handles.push_back(asset.handle.c_str());
        }
    }

    std::sort(handles.begin(), handles.end(), [](const char* a, const char* b) {
        return strcmp(a, b) < 0;
        });

    return handles;
}

std::vector<const char*> ResourceLibrary::GetAllShaderHandles()
{
    std::vector<const char*> handles;
    if (!s_instance) return handles;

    for (const auto& asset : s_instance->m_allAssets)
    {
        if (asset.type == "Shader")
        {
            handles.push_back(asset.handle.c_str());
        }
    }

    std::sort(handles.begin(), handles.end(), [](const char* a, const char* b)
        {
            return strcmp(a, b) < 0;
        });

    return handles;
}

std::vector<const char*> ResourceLibrary::GetAllTextureHandles()
{
    std::vector<const char*> handles;
    if (!s_instance) return handles;

    for (const auto& asset : s_instance->m_allAssets)
    {
        if (asset.type == "Texture")
        {
            handles.push_back(asset.handle.c_str());
        }
    }

    std::sort(handles.begin(), handles.end(), [](const char* a, const char* b)
        {
            return strcmp(a, b) < 0;
        });

    return handles;
}

std::vector<const char*> ResourceLibrary::GetAllGrimoireHandles()
{
    std::vector<const char*> handles;
    if (!s_instance) return handles;

    for (const auto& asset : s_instance->m_allAssets)
    {
        if (asset.type == "Grimoire")
        {
            handles.push_back(asset.handle.c_str());
        }
    }

    std::sort(handles.begin(), handles.end(), [](const char* a, const char* b)
        {
            return strcmp(a, b) < 0;
        });

    return handles;
}

std::vector<const char*> ResourceLibrary::GetAllPrefabHandles()
{
    std::vector<const char*> handles;
    if (!s_instance) return handles;

    for (const auto& asset : s_instance->m_allAssets)
    {
        if (asset.type == "Prefab")
        {
            handles.push_back(asset.handle.c_str());
        }
    }

    std::sort(handles.begin(), handles.end(), [](const char* a, const char* b)
        {
            return strcmp(a, b) < 0;
        });

    return handles;
}

std::vector<const char*> ResourceLibrary::GetAllSceneHandles()
{
    std::vector<const char*> handles;
    if (!s_instance) return handles;

    for (const auto& asset : s_instance->m_allAssets)
    {
        if (asset.type == "Scene")
        {
            handles.push_back(asset.handle.c_str());
        }
    }

    std::sort(handles.begin(), handles.end(), [](const char* a, const char* b)
        {
            return strcmp(a, b) < 0;
        });

    return handles;
}

std::vector<const char*> ResourceLibrary::GetAllAudioHandles()
{
    std::vector<const char*> handles;
    if (!s_instance) return handles;

    for (const auto& asset : s_instance->m_allAssets)
    {
        if (asset.type == "Sound Wave" || asset.type == "Sound Stream")
        {
            handles.push_back(asset.handle.c_str());
        }
    }

    std::sort(handles.begin(), handles.end(), [](const char* a, const char* b)
        {
            return strcmp(a, b) < 0;
        });

    return handles;
}

const std::vector<AssetInfo>& ResourceLibrary::GetAllAssets()
{
    if (!s_instance)
    {
        static std::vector<AssetInfo> empty;
        return empty;
    }
    return s_instance->m_allAssets;
}

void ResourceLibrary::SaveEntityAsPrefab(flecs::entity entity, std::string prefabName)
{
    size_t prefix = prefabName.find("PF_");
    if (prefix == std::string::npos)
    {
        prefabName = "PF_" + prefabName;
    }

    if (s_instance->m_prefabs.contains(prefabName))
    {
        prefabName = GenerateUniqueName(prefabName, "");
    }

    if (s_instance->m_world->lookup(prefabName.c_str()))
    {
        auto command = new Cauda::RenameEntityCommand(entity, entity.parent(), prefabName.c_str(), *s_instance->m_world);
        command->Execute();
        delete command;
    }
    else
    {
        prefabName = GenerateUniqueName(prefabName, "");
        entity.set_name(prefabName.c_str());
        entity.children([](flecs::entity child)
            {
                UpdateChildNames(child);
            });
    }

    entity.remove(flecs::ChildOf, flecs::Wildcard);
    entity.remove<SelectedTag>();
    entity.add(flecs::Prefab);

    if (auto transform = entity.try_get_mut<TransformComponent>()) //Reset position
    {
        transform->position = glm::vec3(0);
    }

    flecs::entity_to_json_desc_t desc = {};
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

    std::string filePath = g_ContentPath + "Prefabs/";
    std::string jsonContent = "{\"results\":[";

    SceneIO::SerialiseEntityBranch(entity, desc, jsonContent);

    jsonContent.erase(jsonContent.find_last_of(","));
    jsonContent.append("]}");

    s_instance->m_prefabs[prefabName] = entity.id();
    s_instance->m_allAssets.emplace_back(prefabName, filePath + prefabName + ".json", "Prefab", &s_instance->m_prefabs[prefabName]);

    prefabName.append(".json");
    CerealBox::ClobberFile(jsonContent, prefabName, filePath.c_str());
}

void ResourceLibrary::SaveMaterial(const std::string& materialName)
{
    if (!s_instance) return;

    Material* material = GetMaterial(materialName.c_str());
    if (!material) return;

    nlohmann::json j;

    j["shaderName"] = GetShaderName(material->GetShader());
    j["floatUniforms"] = material->GetFloatUniforms();
    j["intUniforms"] = material->GetIntUniforms();
    j["boolUniforms"] = material->GetBoolUniforms();
    j["textureHandles"] = material->GetTextureHandles();

    nlohmann::json vec2Uniforms;
    for (const auto& [name, value] : material->GetVec2Uniforms()) 
    {
        vec2Uniforms[name] = { value.x, value.y };
    }
    j["vec2Uniforms"] = vec2Uniforms;

    nlohmann::json vec3Uniforms;
    for (const auto& [name, value] : material->GetVec3Uniforms())
    {
        vec3Uniforms[name] = { value.x, value.y, value.z };
    }
    j["vec3Uniforms"] = vec3Uniforms;

    nlohmann::json vec4Uniforms;
    for (const auto& [name, value] : material->GetVec4Uniforms())
    {
        vec4Uniforms[name] = { value.x, value.y, value.z, value.w };
    }
    j["vec4Uniforms"] = vec4Uniforms;

    std::string jsonStr = j.dump(2); 
    std::string filePath = g_ContentPath + "Materials/";
    std::string fileName = materialName + ".json";

    fs::create_directories(filePath);
    CerealBox::ClobberFile(jsonStr, fileName, filePath.c_str());

    std::cout << "Saved material: " << materialName << std::endl;
}

void ResourceLibrary::LoadMaterial(const std::string& materialName, const std::string& filePath)
{
    if (!s_instance) return;

    if (!fs::exists(filePath)) return;

    std::string jsonStr = CerealBox::LoadFileAsString(filePath);
    if (jsonStr.empty()) return;

    try 
    {
        nlohmann::json j = nlohmann::json::parse(jsonStr);

        Material* material = GetMaterial(materialName.c_str());
       /* if (!material) 
        {
            std::string shaderName = j.value("shaderName", "");
            ShaderProgram* shader = GetShader(shaderName.c_str());
            material = LoadMaterial(materialName, shader);
        }*/

        if (!material) return;

        if (j.contains("floatUniforms")) 
        {
            std::unordered_map<std::string, float> floatUniforms = j["floatUniforms"];
            material->SetFloatUniforms(floatUniforms);
        }

        if (j.contains("intUniforms"))
        {
            std::unordered_map<std::string, int> intUniforms = j["intUniforms"];
            material->SetIntUniforms(intUniforms);
        }

        if (j.contains("boolUniforms"))
        {
            std::unordered_map<std::string, bool> boolUniforms = j["boolUniforms"];
            material->SetBoolUniforms(boolUniforms);
        }

        if (j.contains("textureHandles"))
        {
            std::unordered_map<std::string, std::string> textureHandles = j["textureHandles"];
            material->SetTextureHandles(textureHandles);
        }

        if (j.contains("vec2Uniforms")) 
        {
            std::unordered_map<std::string, glm::vec2> vec2Uniforms;
            for (const auto& [name, values] : j["vec2Uniforms"].items()) {
                vec2Uniforms[name] = glm::vec2(values[0], values[1]);
            }
            material->SetVec2Uniforms(vec2Uniforms);
        }

        if (j.contains("vec3Uniforms")) {
            std::unordered_map<std::string, glm::vec3> vec3Uniforms;
            for (const auto& [name, values] : j["vec3Uniforms"].items()) {
                vec3Uniforms[name] = glm::vec3(values[0], values[1], values[2]);
            }
            material->SetVec3Uniforms(vec3Uniforms);
        }

        if (j.contains("vec4Uniforms")) {
            std::unordered_map<std::string, glm::vec4> vec4Uniforms;
            for (const auto& [name, values] : j["vec4Uniforms"].items()) {
                vec4Uniforms[name] = glm::vec4(values[0], values[1], values[2], values[3]);
            }
            material->SetVec4Uniforms(vec4Uniforms);
        }

        std::cout << "Loaded material: " << materialName << std::endl;

    }
    catch (const nlohmann::json::exception& e) {
        std::cerr << "Failed to parse material JSON: " << e.what() << std::endl;
    }
}

void ResourceLibrary::SaveAllMaterials()
{
    if (!s_instance) return;

    for (const auto& asset : s_instance->m_allAssets)
    {
        if (asset.type == "Material")
        {
            SaveMaterial(asset.handle);
        }
    }
}


std::string ResourceLibrary::GetTextureHandle(Texture* texture)
{
    if (!s_instance || !texture) return "";

    for (const auto& asset : s_instance->m_allAssets) 
    {
        if (asset.type == "Texture" && asset.resource == texture)
        {
            return asset.handle;
        }
    }
    return "";
}

std::string ResourceLibrary::GetShaderName(ShaderProgram* shader)
{
    if (!s_instance || !shader) return "";

    for (const auto& asset : s_instance->m_allAssets) {
        if (asset.type == "Shader" && asset.resource == shader)
        {
            return asset.handle;
        }
    }
    return "";
}

void ResourceLibrary::UpdateChildNames(flecs::entity entity)
{
    std::string formattedName = FormatRelationshipName(GetStrippedName(entity.name().c_str()), entity.parent().name().c_str());

    entity.set_name(formattedName.c_str());

    entity.children([&](flecs::entity child)
        {
            UpdateChildNames(child);
        });
}

void ResourceLibrary::LoadScenesFromFilesystem()
{
    fs::path scenePath{ g_ContentPath + "Scenes/" };

    try
    {
        for (const auto& entry : fs::directory_iterator(scenePath))
        {
            if (fs::is_regular_file(entry.path()))
            {
                LoadScene(entry.path().stem().string(), entry.path().string());
            }
        }
    }
    catch (const fs::filesystem_error& e)
    {
        std::string error = "Filesystem Error: ";
        error.append(e.what());
        EditorConsole::Inst()->AddConsoleError(error);
    }
}

void ResourceLibrary::LoadPrefabsFromFilesystem()
{
    fs::path scenePath{ g_ContentPath + "Prefabs/" };

    try
    {
        for (const auto& entry : fs::directory_iterator(scenePath))
        {
            if (fs::is_regular_file(entry.path()))
            {
                std::string prefabName = entry.path().stem().string();
                LoadPrefab(prefabName, entry.path().string());
            }
        }
    }
    catch (const fs::filesystem_error& e)
    {
        std::string error = "Filesystem Error: ";
        error.append(e.what());

        EditorConsole::Inst()->AddConsoleError(error);
    }
}

void ResourceLibrary::LoadMaterialsFromFilesystem()
{
    fs::path materialPath{ g_ContentPath + "Materials/" };

    try
    {
        if (!fs::exists(materialPath))
        {
            fs::create_directories(materialPath);
            return;
        }

        for (const auto& entry : fs::directory_iterator(materialPath))
        {
            if (fs::is_regular_file(entry.path()) && entry.path().extension() == ".json") 
            {
                std::string materialName = entry.path().stem().string();
                LoadMaterial(materialName, entry.path().string());
            }
        }
    }
    catch (const fs::filesystem_error& e) 
    {
        std::string error = "Material loading error: ";
        error.append(e.what());
        EditorConsole::Inst()->AddConsoleError(error);
    }
}

void ResourceLibrary::LoadMeshesFromFilesystem()
{
}

void ResourceLibrary::LoadTexturesFromFilesystem()
{
}

void ResourceLibrary::LoadAudioFromFilesystem()
{
    fs::path scenePath{ g_ContentPath + "Audio/" };

    try
    {
        for (const auto& entry : fs::directory_iterator(scenePath))
        {
            if (fs::is_regular_file(entry.path()))
            {
                SL::Wav wav;
                wav.load(entry.path().string().c_str());
                std::string audioName = entry.path().stem().string();

                if (wav.getLength() < 15.f)
                {
                    LoadSoundWave(audioName, entry.path().string());
                }
                else
                {
                    LoadSoundStream(audioName, entry.path().string());
                }
            }
        }
    }
    catch (const fs::filesystem_error& e)
    {
        std::string error = "Filesystem Error: ";
        error.append(e.what());

        EditorConsole::Inst()->AddConsoleError(error);
    }
}

void ResourceLibrary::UpdateLayoutSizes(float availWidth, ImVec2& itemSize, ImVec2& itemStep, int& columnCount)
{
    itemSize = ImVec2(m_iconSize, m_iconSize);
    float spacing = (float)m_iconSpacing;

    columnCount = glm::max((int)(availWidth / (itemSize.x + spacing)), 1);

    if (m_stretchSpacing && columnCount > 1)
        spacing = (availWidth - itemSize.x * columnCount) / (columnCount - 1);

    itemStep = ImVec2(itemSize.x + spacing, itemSize.y + spacing);
}