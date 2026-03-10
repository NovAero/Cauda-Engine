#pragma once
#include "Renderer/ComputeShader.h"
#include "Renderer/ShaderProgram.h"
#include "Renderer/Mesh.h"
#include "Renderer/SkeletalMesh.h"
#include "Renderer/Material.h"
#include "Renderer/Texture.h"

#include "Core/Audio/AudioAsset.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <filesystem>
#include <vector>
#include <string>
#include "imgui.h"
#include <Core/Math/Gizmos.h>
#include <ThirdParty/Flecs.h>

namespace fs = std::filesystem;
using AssetHandle = std::string;
struct Grimoire;

struct AssetInfo
{
    std::string handle;
    std::string filepath;
    std::string type;
    ImGuiID id;
    void* resource;

    AssetInfo(const std::string& h, const std::string& path, const std::string& t, void* res)
        : handle(h), filepath(path), type(t), resource(res)
    {
        static ImGuiID nextId = 1000;
        id = nextId++;
    }
};

class ResourceLibrary
{
public:
    ResourceLibrary() = default;
    ~ResourceLibrary() = default;

    static void LoadAllContent();

    static Mesh* GetMesh(AssetHandle name);
    static SkeletalMesh* GetSkeletalMesh(AssetHandle name);
    static Material* GetMaterial(AssetHandle name);
    static ComputeShader* GetComputeShader(AssetHandle name);
    static ShaderProgram* GetShader(AssetHandle name);
    static Texture* GetTexture(AssetHandle name);
    //static Grimoire* GetGrimoire(AssetHandle name);
    static flecs::entity GetPrefab(AssetHandle name);
    static fs::path GetScene(AssetHandle name);
    static SoundWave* GetSoundWave(AssetHandle name);
    static SoundStream* GetSoundStream(AssetHandle name);
    static AudioAsset* GetAudioAsset(AssetHandle name);

    static Mesh* LoadMesh(const std::string& name, const std::string& filepath = "");
    static SkeletalMesh* LoadSkeletalMesh(
        const std::string& name,
        const std::string& meshFile,
        const std::string& skeletonFile,
        float meshScale = 1.0f );
    static bool LoadSkeletalAnimation(const std::string& meshHandle,
        const std::string& animName,
        const std::string& animFile);
    static Material* LoadMaterial(const std::string& name, const std::string& vertPath, const std::string& fragPath);
    static Material* LoadMaterial(const std::string& name, ShaderProgram* shader);
    static ShaderProgram* LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath);
    static ComputeShader* LoadComputeShader(const std::string& name, const std::string& computeFilePath);
    static Texture* LoadTexture(const std::string& name, const std::string& filepath);
    //static Grimoire* LoadGrimoire(Grimoire script);
    static flecs::entity LoadPrefab(const std::string& name, const std::string& filepath);
    static fs::path LoadScene(const std::string& name, const std::string& filePath);
    static SoundWave* LoadSoundWave(const std::string& name, const std::string filePath);
    static SoundStream* LoadSoundStream(const std::string& name, const std::string filePath);

    static bool RegisterChildNames(flecs::entity entity);
    static void RegisterName(std::string name, std::string parent);
    static std::string GenerateUniqueName(std::string intendedName, std::string parentName);
    static std::string FormatRelationshipName(std::string child, std::string parent);
    static std::string GetStrippedName(std::string name);
    static bool DoesNameExistInScope(flecs::entity entity, std::string name);
    static void ResetUsednames();

    static void DrawAssetBrowser(bool* p_open = nullptr);
    static void ReloadAllShaders();
    static void Clear();
    static void SetInstance(ResourceLibrary* instance);
    static void SetWorld(flecs::world* world);

    static std::vector<const char*> GetAllMeshHandles();
    static std::vector<const char*> GetAllSkeletalMeshHandles();
    static std::vector<const char*> GetAllMaterialHandles();
    static std::vector<const char*> GetAllShaderHandles();
    static std::vector<const char*> GetAllTextureHandles();
    static std::vector<const char*> GetAllGrimoireHandles();
    static std::vector<const char*> GetAllPrefabHandles();
    static std::vector<const char*> GetAllSceneHandles();
    static std::vector<const char*> GetAllAudioHandles();
    static const std::vector<AssetInfo>& GetAllAssets();

    static void SaveEntityAsPrefab(flecs::entity entity, std::string prefabName);

    static void SaveMaterial(const std::string& materialName);
    static void LoadMaterial(const std::string& materialName, const std::string& filePath);
    static void SaveAllMaterials();

    static std::string GetTextureHandle(Texture* texture);
    static std::string GetShaderName(ShaderProgram* shader);

private:

    static void UpdateChildNames(flecs::entity entity);

    void LoadScenesFromFilesystem();
    void LoadPrefabsFromFilesystem();
    void LoadMeshesFromFilesystem();
    void LoadMaterialsFromFilesystem();
    void LoadTexturesFromFilesystem();
    void LoadAudioFromFilesystem();

    void UpdateLayoutSizes(float availWidth, ImVec2& itemSize, ImVec2& itemStep, int& columnCount);

    std::unordered_map<std::string, std::unique_ptr<Mesh>> m_meshes;
    std::unordered_map<std::string, std::unique_ptr<SkeletalMesh>> m_skeletalMeshes;
    std::unordered_map<std::string, std::unique_ptr<Material>> m_materials;
    std::unordered_map<std::string, std::unique_ptr<ComputeShader>> m_computeShaders;
    std::unordered_map<std::string, std::unique_ptr<ShaderProgram>> m_shaders;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
    std::unordered_map<std::string, std::unique_ptr<AudioAsset>> m_sounds; //Shorter sounds
    std::unordered_map<std::string, flecs::entity_t> m_prefabs; //Asset Handle and Entity Name
    std::unordered_map<std::string, fs::path> m_scenes; // Name and filepath
    std::unordered_map<std::string, std::unique_ptr<Grimoire>> m_scripts;
    std::unordered_map<std::string, int> m_usedNames;
    std::vector<AssetInfo> m_allAssets;

    float m_iconSize = 55.0f;
    int m_iconSpacing = 8;
    bool m_stretchSpacing = true;
    ImGuiTextFilter m_filter;

    flecs::world* m_world = nullptr;
    static ResourceLibrary* s_instance;

};