#pragma once
#include "ShaderProgram.h"
#include "Mesh.h"
#include "SkeletalMesh.h"
#include "AnimationModule.h"
#include "Material.h"
#include <ThirdParty/flecs.h>
#include "imgui.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include <concurrentqueue.h>

#include "Core/Utilities/Gradient.h"

struct TransformComponent;
class FrameBuffer;
class EditorModule;

using AssetHandle = std::string;

struct Frustum
{
    glm::vec4 planes[6]; 

    void ExtractFromMatrix(const glm::mat4& viewProj)
    {
        // Left plane
        planes[0] = glm::vec4(
            viewProj[0][3] + viewProj[0][0],
            viewProj[1][3] + viewProj[1][0],
            viewProj[2][3] + viewProj[2][0],
            viewProj[3][3] + viewProj[3][0]
        );

        // Right plane
        planes[1] = glm::vec4(
            viewProj[0][3] - viewProj[0][0],
            viewProj[1][3] - viewProj[1][0],
            viewProj[2][3] - viewProj[2][0],
            viewProj[3][3] - viewProj[3][0]
        );

        // Bottom plane
        planes[2] = glm::vec4(
            viewProj[0][3] + viewProj[0][1],
            viewProj[1][3] + viewProj[1][1],
            viewProj[2][3] + viewProj[2][1],
            viewProj[3][3] + viewProj[3][1]
        );

        // Top plane
        planes[3] = glm::vec4(
            viewProj[0][3] - viewProj[0][1],
            viewProj[1][3] - viewProj[1][1],
            viewProj[2][3] - viewProj[2][1],
            viewProj[3][3] - viewProj[3][1]
        );

        // Near plane
        planes[4] = glm::vec4(
            viewProj[0][3] + viewProj[0][2],
            viewProj[1][3] + viewProj[1][2],
            viewProj[2][3] + viewProj[2][2],
            viewProj[3][3] + viewProj[3][2]
        );

        // Far plane
        planes[5] = glm::vec4(
            viewProj[0][3] - viewProj[0][2],
            viewProj[1][3] - viewProj[1][2],
            viewProj[2][3] - viewProj[2][2],
            viewProj[3][3] - viewProj[3][2]
        );

        for (int i = 0; i < 6; ++i)
        {
            float length = glm::length(glm::vec3(planes[i]));
            planes[i] /= length;
        }
    }

    bool IntersectsAABB(const AABB& aabb, const glm::mat4& transform, float margin = 0.0f) const
    {
        glm::vec3 worldMin = glm::vec3(transform * glm::vec4(aabb.min, 1.0f));
        glm::vec3 worldMax = glm::vec3(transform * glm::vec4(aabb.max, 1.0f));

        for (int i = 0; i < 6; ++i)
        {
            glm::vec3 normal = glm::vec3(planes[i]);

            glm::vec3 pVertex = worldMin;
            if (normal.x >= 0) pVertex.x = worldMax.x;
            if (normal.y >= 0) pVertex.y = worldMax.y;
            if (normal.z >= 0) pVertex.z = worldMax.z;

            if (glm::dot(normal, pVertex) + planes[i].w < -margin)
                return false;
        }

        return true;
    }
};

struct MeshComponent
{
    std::string handle = "";
    bool visible = true;
    bool useInstancing = true;
    bool castShadows = true;
};

struct MaterialComponent
{
    std::string handle = "";
};

struct MaterialData
{
    std::string shaderName;
    std::unordered_map<std::string, float> floatUniforms;
    std::unordered_map<std::string, int> intUniforms;
    std::unordered_map<std::string, bool> boolUniforms;
    std::unordered_map<std::string, glm::vec2> vec2Uniforms;
    std::unordered_map<std::string, glm::vec3> vec3Uniforms;
    std::unordered_map<std::string, glm::vec4> vec4Uniforms;
    std::unordered_map<std::string, std::string> textureHandles;
};

struct SkeletalMeshComponent
{
    std::string handle = "";
    bool visible = true;
    bool castShadows = true;
};

struct PostProcessComponent
{
    bool enabled = true;

    bool enableFog = true;
    glm::vec3 fogExtinction = glm::vec3(0.002, 0.0025, 0.003);
    glm::vec3 fogInscatter = glm::vec3(0.0015, 0.002, 0.0025);
    glm::vec3 fogBaseColour = glm::vec3(0.5, 0.6, 0.7);
    glm::vec3 fogSunColour = glm::vec3(1.0, 0.9, 0.7);
    float fogSunPower = 8.0;

    float fogHeight = -15.0;
    float fogHeightFalloff = 0.2;
    float fogHeightDensity = 15.0;

    float colourFactor = 0.05;
    float brightnessFactor = 3.0;

    bool enableHDR = true;
    float exposure = 1.0;

    bool enableHSV = true;
    float hueShift = 0.0;
    float saturationAdjust = 1.0;
    float valueAdjust = 1.0;

    bool enableEdgeDetection = true;
    bool showEdgesOnly = false;
    float edgeStrength = 0.5;
    float depthThreshold = 0.0004;
    float normalThreshold = 0.4;
    float darkenAmount = 1.0;
    float lightenAmount = 1.0;
    glm::vec3 normalBias = glm::vec3(-1, -1, -1);
    float edgeWidth = 1.0;

    bool enableQuantization = false;
    int quantizationLevels = 4;
    bool enableDithering = true;
    float ditherStrength = 1.0f;
    std::string ditherTexture = "";

    std::string handle = "";
};

struct ShellTextureComponent
{
    bool enabled = true;
    int shellCount = 8;
    float shellHeight = 0.01f;
    float tilingAmount = 1.0f;
    float thickness = 0.01f;
    glm::vec3 baseColour = glm::vec3(0.3f, 0.6f, 0.2f);
    glm::vec3 tipColour = glm::vec3(0.5f, 0.8f, 0.3f);
    std::string noiseTexture = "";
    std::string maskTexture = "";
};

struct XrayComponent
{
    bool enabled = true;
    glm::vec3 xrayColour = glm::vec3(0.0f, 1.0f, 0.5f);
    float xrayAlpha = 0.5f;
    float rimPower = 2.0f;
    bool useRimLighting = true;
};

struct alignas(16) InstanceData
{
    glm::mat4 modelMatrix;
    glm::mat4 normalMatrix;
    uint32_t entityIdLow;  
    uint32_t entityIdHigh;
    uint32_t materialId;
    uint32_t padding;       
};

struct DrawElementsIndirectCommand
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    uint32_t baseVertex;
    uint32_t baseInstance;
};

namespace Cauda
{
    class RenderLayer;
}

class RendererModule
{
    friend class Cauda::RenderLayer;
public:
    RendererModule(flecs::world& ecs);
    ~RendererModule();

    RendererModule(const RendererModule&) = delete;
    RendererModule& operator=(const RendererModule&) = delete;
    RendererModule(RendererModule&&) = default;
    RendererModule& operator=(RendererModule&&) = default;

    void Render();
    void RenderShadows();
    void RenderBackfaces();
    void RenderShellTextures();
    void RenderXray();

    void RenderMesh(Mesh* mesh, Material* material, const glm::mat4& modelMatrix);

    void DrawRenderStats(bool* p_open = nullptr);

    void SetupComponents();
    void SetupObservers();
    void SetupSystems();
    void SetupQueries();

    glm::mat4 TransformToMatrix(flecs::entity entity);

    void RegisterWithEditor();

    Mesh* GetMesh(const MeshComponent& component) const;
    Material* GetMaterial(const MaterialComponent& component) const;

    void SetMesh(flecs::entity e, const AssetHandle& handle);
    void SetMaterial(flecs::entity e, const AssetHandle& handle);
    void SetVisibility(flecs::entity e, bool visible);

    static void SetEntityId(InstanceData& instanceData, flecs::entity_t entityId);
    static flecs::entity_t GetEntityId(const InstanceData& instanceData);

private:
    flecs::world& m_world;
    EditorModule* m_editorModule = nullptr;

    flecs::query<MeshComponent, MaterialComponent, TransformComponent> m_renderQuery;
    flecs::query<ShellTextureComponent, MeshComponent, TransformComponent> m_shellTextureQuery;
    flecs::query<XrayComponent, MeshComponent, TransformComponent> m_xrayQuery;
    flecs::query<XrayComponent, SkeletalMeshComponent, TransformComponent> m_xraySkeletalQuery;
    
    flecs::observer m_setMaterialCleanupObserver;
    flecs::observer m_setMeshCleanupObserver;

    struct BatchKey
    {
        AssetHandle meshHandle = "";
        AssetHandle materialHandle = "";
        bool useInstancing = true; 

        bool operator==(const BatchKey& other) const
        {
            return meshHandle == other.meshHandle &&
                materialHandle == other.materialHandle &&
                useInstancing == other.useInstancing;  
        }
    };

    struct BatchKeyHash
    {
        std::size_t operator()(const BatchKey& k) const
        {
            return std::hash<std::string>()(k.meshHandle) ^
                (std::hash<std::string>()(k.materialHandle) << 1) ^
                (std::hash<bool>()(k.useInstancing) << 2);  
        }
    };


    struct ShellTextureBatchKey
    {
        flecs::entity_t entityId;  

        bool operator==(const ShellTextureBatchKey& other) const
        {
            return entityId == other.entityId;
        }
    };

    struct ShellTextureBatchKeyHash
    {
        std::size_t operator()(const ShellTextureBatchKey& k) const
        {
            return std::hash<flecs::entity_t>()(k.entityId);
        }
    };

    struct RenderBatch
    {
        std::vector<InstanceData> instances;
        std::vector<flecs::entity> entities;
        uint32_t baseInstance = 0;
        uint32_t drawCommandIndex = 0;
        bool useInstancing = true;  

        Mesh* mesh = nullptr;
        Material* material = nullptr;
        ShaderProgram* shader = nullptr;
    };

    struct ShellTextureBatch
    {
        std::vector<InstanceData> instances;
        std::vector<flecs::entity> entities;
        uint32_t baseInstance = 0;
        uint32_t drawCommandIndex = 0;
        int shellCount = 0;
        AssetHandle noiseTexture = "";
        AssetHandle maskTexture = "";
        AssetHandle meshHandle = "";  
    };

    std::unordered_map<BatchKey, RenderBatch, BatchKeyHash> m_batches;
    std::unordered_map<ShellTextureBatchKey, ShellTextureBatch, ShellTextureBatchKeyHash> m_shellTextureBatches;
    std::vector<DrawElementsIndirectCommand> m_drawCommands;
    std::vector<DrawElementsIndirectCommand> m_shellDrawCommands;

    static constexpr int NUM_BUFFERS = 3;
    int m_currentBufferIndex = 0;

    GLuint m_instanceSSBO[NUM_BUFFERS] = { 0, 0, 0 };
    GLuint m_drawCommandBuffer[NUM_BUFFERS] = { 0, 0, 0 };
    GLsync m_bufferFences[NUM_BUFFERS] = { nullptr, nullptr, nullptr };

    GLuint m_shellInstanceSSBO[NUM_BUFFERS] = { 0, 0, 0 };
    GLuint m_shellDrawCommandBuffer[NUM_BUFFERS] = { 0, 0, 0 };
    GLsync m_shellBufferFences[NUM_BUFFERS] = { nullptr, nullptr, nullptr };

    InstanceData* m_instanceMappedPtr[NUM_BUFFERS] = { nullptr, nullptr, nullptr };
    DrawElementsIndirectCommand* m_drawCommandMappedPtr[NUM_BUFFERS] = { nullptr, nullptr, nullptr };
    InstanceData* m_shellInstanceMappedPtr[NUM_BUFFERS] = { nullptr, nullptr, nullptr };
    DrawElementsIndirectCommand* m_shellDrawCommandMappedPtr[NUM_BUFFERS] = { nullptr, nullptr, nullptr };

    std::vector<InstanceData> m_instanceStagingBuffer;
    std::vector<DrawElementsIndirectCommand> m_drawCommandStagingBuffer;

    size_t m_maxInstances = 16384;
    size_t m_maxDrawCommands = 256;
    size_t m_currentInstanceCount = 0;
    size_t m_currentDrawCommandCount = 0;

    size_t m_maxShellInstances = 8192;
    size_t m_maxShellDrawCommands = 128;
    size_t m_currentShellInstanceCount = 0;
    size_t m_currentShellDrawCommandCount = 0;

    static constexpr int MIN_INSTANCES_FOR_BATCHING = 50;
    static constexpr size_t INSTANCE_BUFFER_GROW_FACTOR = 2;

    struct RenderStats
    {
        uint32_t drawCalls = 0;
        uint32_t totalInstances = 0;
        uint32_t shellInstances = 0;
        uint32_t batchCount = 0;
        size_t bufferMemoryMB = 0;
        float bufferFlushTimeMs = 0.0f;
        float shellBufferFlushTimeMs = 0.0f;
        float minBufferFlushTimeMs = FLT_MAX;
        float maxBufferFlushTimeMs = 0.0f;
        float avgBufferFlushTimeMs = 0.0f;
        uint32_t flushSampleCount = 0;
    } m_stats;

    struct GLStateCache
    {
        ShaderProgram* boundShader = nullptr;
        GLuint boundVAO = 0;
        GLuint boundSSBO = 0;

        void BindShader(ShaderProgram* shader)
        {
            if (shader != boundShader)
            {
                shader->Use();
                boundShader = shader;
            }
        }

        void BindVAO(GLuint vao)
        {
            if (vao != boundVAO)
            {
                glBindVertexArray(vao);
                boundVAO = vao;
            }
        }

        void BindSSBO(GLuint binding, GLuint buffer)
        {
            if (buffer != boundSSBO)
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer);
                boundSSBO = buffer;
            }
        }

        void Reset()
        {
            boundShader = nullptr;
            boundVAO = 0;
            boundSSBO = 0;
        }
    } m_glState;

    struct QueuedInstance 
    {
        BatchKey key;
        InstanceData data;
        flecs::entity entity;
    };

    //moodycamel::ConcurrentQueue<QueuedInstance> m_renderQueue;
    //flecs::system m_collectionSystem;
    //std::atomic<bool> m_collectionComplete{ false };

  //  void ProcessRenderQueue();

    void InitialiseBuffers();
    void ResizeBuffers();
    void CollectRenderData(const Frustum* frustum = nullptr);
    void CollectShellTextureData();
    void FlushInstanceData();
    void FlushShellTextureData();

    void ExecuteDrawCalls();
    void ExecuteBackfaceDrawCalls();
    void ExecuteShellTextureDrawCalls();

    flecs::query<SkeletalMeshComponent, MaterialComponent, TransformComponent, AnimationComponent> m_skeletalMeshQuery;

    void RenderSkeletalMeshes();

    void DrawMeshComponentInspector(flecs::entity entity, MeshComponent& component);
    void DrawSkeletalMeshComponentInspector(flecs::entity entity, SkeletalMeshComponent& component);
    void DrawMaterialComponentInspector(flecs::entity entity, MaterialComponent& component);
    void DrawPostProcessInspector(flecs::entity entity, PostProcessComponent& component);
    void DrawShellTextureInspector(flecs::entity entity, ShellTextureComponent& component);
    void DrawXrayInspector(flecs::entity entity, XrayComponent& component);
    void DrawMaterialUniforms(Material* material);
    ImVec4 GetRenderEntityColour(flecs::entity entity);
};