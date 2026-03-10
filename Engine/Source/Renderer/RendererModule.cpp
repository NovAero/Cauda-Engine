#include "cepch.h"
#include "RendererModule.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Editor/ResourceLibrary.h"
#include "Core/Scene/CameraModule.h"
#include "Graphics.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include "Platform/Win32/Application.h"

RendererModule::RendererModule(flecs::world& ecs) : m_world(ecs)
{

   // m_world.set_threads(std::thread::hardware_concurrency());
    m_editorModule = m_world.try_get_mut<EditorModule>();


    SetupComponents();
    SetupObservers();
    SetupSystems();
    SetupQueries();

    InitialiseBuffers();
    RegisterWithEditor();

    std::cout << "RendererModule initialised" << std::endl;
}

RendererModule::~RendererModule()
{
    for (int i = 0; i < NUM_BUFFERS; ++i)
    {
        if (m_bufferFences[i] != nullptr)
        {
            glDeleteSync(m_bufferFences[i]);
        }
        if (m_shellBufferFences[i] != nullptr)
        {
            glDeleteSync(m_shellBufferFences[i]);
        }
    }

    for (int i = 0; i < NUM_BUFFERS; ++i)
    {
        if (m_instanceMappedPtr[i])
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instanceSSBO[i]);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
        if (m_drawCommandMappedPtr[i])
        {
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCommandBuffer[i]);
            glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);
        }
        if (m_shellInstanceMappedPtr[i])
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_shellInstanceSSBO[i]);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
        if (m_shellDrawCommandMappedPtr[i])
        {
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_shellDrawCommandBuffer[i]);
            glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);
        }
    }

    for (int i = 0; i < NUM_BUFFERS; ++i)
    {
        glDeleteBuffers(1, &m_instanceSSBO[i]);
        glDeleteBuffers(1, &m_drawCommandBuffer[i]);
        glDeleteBuffers(1, &m_shellInstanceSSBO[i]);
        glDeleteBuffers(1, &m_shellDrawCommandBuffer[i]);
    }
}

void RendererModule::SetupComponents()
{

    m_world.component<MaterialComponent>()
        .member<std::string>("handle");

    m_world.component<MeshComponent>()
        .member<std::string>("handle")
        .member<bool>("visible")
        .member<bool>("useInstancing")
        .member<bool>("castShadows")
        .add(flecs::With, m_world.component<MaterialComponent>());

    m_world.component<SkeletalMeshComponent>("SkeletalMeshComponent")
        .member<std::string>("handle")
        .member<bool>("visible")
        .member<bool>("castShadows")
        .add(flecs::With, m_world.component<MaterialComponent>());

    m_world.component<ShellTextureComponent>("ShellTextureComponent")
        .member<bool>("enabled")
        .member<int>("shellCount")
        .member<float>("shellHeight")
        .member<float>("tilingAmount")
        .member<float>("thickness")
        .member<glm::vec3>("baseColour")
        .member<glm::vec3>("tipColour")
        .member<std::string>("noiseTexture")
        .member<std::string>("maskTexture");

    m_world.component<XrayComponent>("XrayComponent")
        .member<bool>("enabled")
        .member<glm::vec3>("xrayColour")
        .member<float>("xrayAlpha")
        .member<float>("rimPower")
        .member<bool>("useRimLighting");

    m_world.component<PostProcessComponent>("PostProcessComponent")
        .member<bool>("enabled")
        .member<bool>("enableFog")
        .member<glm::vec3>("fogExtinction")
        .member<glm::vec3>("fogInscatter")
        .member<glm::vec3>("fogBaseColour")
        .member<glm::vec3>("fogSunColour")
        .member<float>("fogSunPower")
        .member<float>("fogHeight")
        .member<float>("fogHeightFalloff")
        .member<float>("fogHeightDensity")
        .member<float>("colourFactor")
        .member<float>("brightnessFactor")
        .member<bool>("enableHDR")
        .member<float>("exposure")
        .member<bool>("enableHSV")
        .member<float>("hueShift")
        .member<float>("saturationAdjust")
        .member<float>("valueAdjust")
        .member<bool>("enableEdgeDetection")
        .member<bool>("showEdgesOnly")
        .member<float>("edgeStrength")
        .member<float>("depthThreshold")
        .member<float>("normalThreshold")
        .member<float>("darkenAmount")
        .member<float>("lightenAmount")
        .member<glm::vec3>("normalBias")
        .member<float>("edgeWidth")
        .member<bool>("enableQuantization")
        .member<int>("quantizationLevels")
        .member<bool>("enableDithering")
        .member<float>("ditherStrength")
        .member<std::string>("ditherTexture");
}

void RendererModule::SetupObservers()
{
    m_setMeshCleanupObserver = m_world.observer<MeshComponent>()
        .event(flecs::OnSet)
        .each([&](flecs::entity entity, MeshComponent& meshComp)
            {

            });

    m_world.observer<MaterialComponent>()
        .event(flecs::OnSet)
        .each([&](flecs::entity entity, MaterialComponent& materialComp)
            {
                if (!materialComp.handle.empty())
                {
                    Material* material = ResourceLibrary::GetMaterial(materialComp.handle.c_str());
                    if (material && material->GetShader())
                    {
                        material->GetUniformsFromShader();
                    }
                }
            });
}

void RendererModule::SetupSystems()
{
    //m_collectionSystem = m_world.system<MeshComponent, MaterialComponent, TransformComponent>("CollectRenderDataSystem")
    //    .multi_threaded()
    //    .kind(flecs::PreStore)
    //    .each([this](flecs::entity e, MeshComponent& mesh, MaterialComponent& mat, TransformComponent& transform)
    //        {
    //            if (!e.is_alive() || !mesh.visible || mesh.handle.empty() || mat.handle.empty())
    //                return;

    //            Mesh* meshPtr = ResourceLibrary::GetMesh(mesh.handle.c_str());
    //            Material* materialPtr = ResourceLibrary::GetMaterial(mat.handle.c_str());
    //            if (!meshPtr || !materialPtr)
    //                return;

    //            InstanceData instance;
    //            instance.modelMatrix = TransformToMatrix(e);
    //            instance.normalMatrix = glm::transpose(glm::inverse(glm::mat4(glm::mat3(instance.modelMatrix))));
    //            SetEntityId(instance, e.id());
    //            instance.materialId = 0;

    //            QueuedInstance qItem;
    //            qItem.key = { mesh.handle, mat.handle, mesh.useInstancing };
    //            qItem.data = instance;
    //            qItem.entity = e;  

    //            m_renderQueue.enqueue(qItem);
    //        });

    //m_world.system("MarkCollectionComplete")
    //    .kind(flecs::OnStore)  
    //    .run([this](flecs::iter&) 
    //        {
    //            if (m_renderQueue.size_approx() > 0)
    //            {
    //                m_collectionComplete.store(true, std::memory_order_release);
    //            }
    //        });
}

void RendererModule::SetupQueries()
{
    m_renderQuery = m_world.query_builder<MeshComponent, MaterialComponent, TransformComponent>().build();
    m_shellTextureQuery = m_world.query_builder<ShellTextureComponent, MeshComponent, TransformComponent>().build();
    m_xrayQuery = m_world.query_builder<XrayComponent, MeshComponent, TransformComponent>().build();
    m_xraySkeletalQuery = m_world.query_builder<XrayComponent, SkeletalMeshComponent, TransformComponent>().build();
    m_skeletalMeshQuery = m_world.query_builder<SkeletalMeshComponent, MaterialComponent, TransformComponent, AnimationComponent>().build();
}

void RendererModule::SetEntityId(InstanceData& instanceData, flecs::entity_t entityId)
{
    instanceData.entityIdLow = static_cast<uint32_t>(entityId & 0xFFFFFFFF);
    instanceData.entityIdHigh = static_cast<uint32_t>((entityId >> 32) & 0xFFFFFFFF);
}

flecs::entity_t RendererModule::GetEntityId(const InstanceData& instanceData)
{
    return (static_cast<flecs::entity_t>(instanceData.entityIdHigh) << 32) |
        static_cast<flecs::entity_t>(instanceData.entityIdLow);
}

//void RendererModule::ProcessRenderQueue()
//{
//
//
//    QueuedInstance item;
//    while (m_renderQueue.try_dequeue(item))
//    {
//        auto& batch = m_batches[item.key];
//        batch.instances.push_back(item.data);
//        batch.entities.push_back(item.entity);  
//        batch.useInstancing = item.key.useInstancing;
//    }
//}

void RendererModule::InitialiseBuffers()
{

    size_t instanceBufferSize = m_maxInstances * sizeof(InstanceData);
    size_t commandBufferSize = m_maxDrawCommands * sizeof(DrawElementsIndirectCommand);
    size_t shellInstanceBufferSize = m_maxShellInstances * sizeof(InstanceData);
    size_t shellCommandBufferSize = m_maxShellDrawCommands * sizeof(DrawElementsIndirectCommand);

    GLbitfield mapFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    GLbitfield createFlags = mapFlags | GL_DYNAMIC_STORAGE_BIT;

    for (int i = 0; i < NUM_BUFFERS; ++i)
    {
        glGenBuffers(1, &m_instanceSSBO[i]);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instanceSSBO[i]);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, instanceBufferSize, nullptr, createFlags);
        m_instanceMappedPtr[i] = static_cast<InstanceData*>(
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, instanceBufferSize, mapFlags));

        glGenBuffers(1, &m_drawCommandBuffer[i]);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCommandBuffer[i]);
        glBufferStorage(GL_DRAW_INDIRECT_BUFFER, commandBufferSize, nullptr, createFlags);
        m_drawCommandMappedPtr[i] = static_cast<DrawElementsIndirectCommand*>(
            glMapBufferRange(GL_DRAW_INDIRECT_BUFFER, 0, commandBufferSize, mapFlags));

        glGenBuffers(1, &m_shellInstanceSSBO[i]);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_shellInstanceSSBO[i]);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, shellInstanceBufferSize, nullptr, createFlags);
        m_shellInstanceMappedPtr[i] = static_cast<InstanceData*>(
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, shellInstanceBufferSize, mapFlags));

        glGenBuffers(1, &m_shellDrawCommandBuffer[i]);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_shellDrawCommandBuffer[i]);
        glBufferStorage(GL_DRAW_INDIRECT_BUFFER, shellCommandBufferSize, nullptr, createFlags);
        m_shellDrawCommandMappedPtr[i] = static_cast<DrawElementsIndirectCommand*>(
            glMapBufferRange(GL_DRAW_INDIRECT_BUFFER, 0, shellCommandBufferSize, mapFlags));

        m_bufferFences[i] = nullptr;
        m_shellBufferFences[i] = nullptr;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    m_instanceStagingBuffer.reserve(m_maxInstances);
    m_drawCommandStagingBuffer.reserve(m_maxDrawCommands);

    m_stats.bufferMemoryMB = (instanceBufferSize + commandBufferSize +
        shellInstanceBufferSize + shellCommandBufferSize) * NUM_BUFFERS / (1024 * 1024);
}

void RendererModule::ResizeBuffers()
{

    size_t newMaxInstances = m_maxInstances * INSTANCE_BUFFER_GROW_FACTOR;
    size_t newMaxCommands = m_maxDrawCommands * INSTANCE_BUFFER_GROW_FACTOR;

    size_t newInstanceBufferSize = newMaxInstances * sizeof(InstanceData);
    size_t newCommandBufferSize = newMaxCommands * sizeof(DrawElementsIndirectCommand);

    GLbitfield mapFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    GLbitfield createFlags = mapFlags | GL_DYNAMIC_STORAGE_BIT;

    for (int i = 0; i < NUM_BUFFERS; ++i)
    {
        if (m_bufferFences[i] != nullptr)
        {
            glClientWaitSync(m_bufferFences[i], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
            glDeleteSync(m_bufferFences[i]);
            m_bufferFences[i] = nullptr;
        }
    }

    for (int i = 0; i < NUM_BUFFERS; ++i)
    {
        if (m_instanceMappedPtr[i])
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instanceSSBO[i]);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            m_instanceMappedPtr[i] = nullptr;
        }
        if (m_drawCommandMappedPtr[i])
        {
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCommandBuffer[i]);
            glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);
            m_drawCommandMappedPtr[i] = nullptr;
        }

        glDeleteBuffers(1, &m_instanceSSBO[i]);
        glDeleteBuffers(1, &m_drawCommandBuffer[i]);
    }

    for (int i = 0; i < NUM_BUFFERS; ++i)
    {
        glGenBuffers(1, &m_instanceSSBO[i]);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instanceSSBO[i]);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER, newInstanceBufferSize, nullptr, createFlags);
        m_instanceMappedPtr[i] = static_cast<InstanceData*>(
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, newInstanceBufferSize, mapFlags));

        glGenBuffers(1, &m_drawCommandBuffer[i]);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCommandBuffer[i]);
        glBufferStorage(GL_DRAW_INDIRECT_BUFFER, newCommandBufferSize, nullptr, createFlags);
        m_drawCommandMappedPtr[i] = static_cast<DrawElementsIndirectCommand*>(
            glMapBufferRange(GL_DRAW_INDIRECT_BUFFER, 0, newCommandBufferSize, mapFlags));
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    m_maxInstances = newMaxInstances;
    m_maxDrawCommands = newMaxCommands;

    m_instanceStagingBuffer.reserve(m_maxInstances);
    m_drawCommandStagingBuffer.reserve(m_maxDrawCommands);

    m_stats.bufferMemoryMB = (newInstanceBufferSize + newCommandBufferSize) * NUM_BUFFERS / (1024 * 1024);

}

void RendererModule::CollectRenderData(const Frustum* frustum)
{
    m_batches.clear();
    m_drawCommands.clear();
    m_currentInstanceCount = 0;
    m_currentDrawCommandCount = 0;

    m_renderQuery.each([this, frustum](flecs::entity e, MeshComponent& meshComp, MaterialComponent& matComp, TransformComponent& transform)
        {
            if (!e.is_alive() || !meshComp.visible || meshComp.handle == "" || matComp.handle == "")
                return;

            if (!e.has<TransformComponent>())
                return;

            Mesh* mesh = ResourceLibrary::GetMesh(meshComp.handle.c_str());
            Material* material = ResourceLibrary::GetMaterial(matComp.handle.c_str());
            if (!mesh || !material)
                return;

            if (frustum)
            {
                glm::mat4 modelMatrix = TransformToMatrix(e);
                const AABB& meshBounds = mesh->GetBoundingBox();

                float cullingMargin = 50.0f; 
                if (!frustum->IntersectsAABB(meshBounds, modelMatrix, cullingMargin))
                {
                    return; 
                }
            }

            BatchKey key = { meshComp.handle.c_str(), matComp.handle.c_str(), meshComp.useInstancing };
            auto& batch = m_batches[key];

            batch.useInstancing = meshComp.useInstancing;

            InstanceData instance;
            instance.modelMatrix = TransformToMatrix(e);
            instance.normalMatrix = glm::transpose(glm::inverse(glm::mat4(glm::mat3(instance.modelMatrix))));

            SetEntityId(instance, e.id());

            instance.materialId = 0;

            batch.instances.push_back(instance);
            batch.entities.push_back(e);
            m_currentInstanceCount++;
        });

    uint32_t baseInstance = 0;
    for (auto& [key, batch] : m_batches)
    {
        batch.baseInstance = baseInstance;
        batch.drawCommandIndex = static_cast<uint32_t>(m_drawCommands.size());

        Mesh* mesh = ResourceLibrary::GetMesh(key.meshHandle);
        if (!mesh) continue;

        DrawElementsIndirectCommand cmd;
        cmd.indexCount = mesh->GetTriCount() * 3;
        cmd.instanceCount = static_cast<uint32_t>(batch.instances.size());
        cmd.firstIndex = 0;
        cmd.baseVertex = 0;
        cmd.baseInstance = baseInstance;

        m_drawCommands.push_back(cmd);
        baseInstance += cmd.instanceCount;
        m_currentDrawCommandCount++;
    }
}

//void RendererModule::CollectRenderData()
//{
//    if (m_collectionComplete.exchange(false, std::memory_order_acquire))
//    {
//        m_batches.clear();
//        m_drawCommands.clear();
//        m_currentInstanceCount = 0;
//        m_currentDrawCommandCount = 0;
//
//        ProcessRenderQueue();
//
//        uint32_t baseInstance = 0;
//        for (auto& [key, batch] : m_batches)
//        {
//            batch.baseInstance = baseInstance;
//            batch.drawCommandIndex = static_cast<uint32_t>(m_drawCommands.size());
//
//            Mesh* mesh = ResourceLibrary::GetMesh(key.meshHandle.c_str());
//            if (!mesh) continue;
//
//            DrawElementsIndirectCommand cmd;
//            cmd.indexCount = mesh->GetTriCount() * 3;
//            cmd.instanceCount = static_cast<uint32_t>(batch.instances.size());
//            cmd.firstIndex = 0;
//            cmd.baseVertex = 0;
//            cmd.baseInstance = baseInstance;
//
//            m_drawCommands.push_back(cmd);
//            baseInstance += cmd.instanceCount;
//            m_currentDrawCommandCount++;
//            m_currentInstanceCount += batch.instances.size();
//        }
//    }
//}

void RendererModule::CollectShellTextureData()
{
    m_shellTextureBatches.clear();
    m_shellDrawCommands.clear();
    m_currentShellInstanceCount = 0;
    m_currentShellDrawCommandCount = 0;

    m_shellTextureQuery.each([this](flecs::entity e, ShellTextureComponent& shellComp, MeshComponent& meshComp, TransformComponent& transform)
        {
            if (!shellComp.enabled || !meshComp.visible || meshComp.handle == "")
                return;

            Mesh* mesh = ResourceLibrary::GetMesh(meshComp.handle.c_str());
            if (!mesh) return;

            ShellTextureBatchKey key = { e.id() };
            auto& batch = m_shellTextureBatches[key];

            batch.shellCount = shellComp.shellCount;
            batch.noiseTexture = shellComp.noiseTexture.c_str();
            batch.maskTexture = shellComp.maskTexture.c_str();
            batch.meshHandle = meshComp.handle.c_str();  

            glm::mat4 modelMatrix = TransformToMatrix(e);
            glm::mat4 normalMatrix = glm::transpose(glm::inverse(glm::mat4(glm::mat3(modelMatrix))));

            for (int shellIndex = 1; shellIndex < shellComp.shellCount; ++shellIndex)
            {
                InstanceData instance;
                instance.modelMatrix = modelMatrix;
                instance.normalMatrix = normalMatrix;

                SetEntityId(instance, e.id());
                instance.materialId = shellIndex;  

                batch.instances.push_back(instance);
                m_currentShellInstanceCount++;
            }

            batch.entities.push_back(e);
        });

    uint32_t baseInstance = 0;
    for (auto& [key, batch] : m_shellTextureBatches)
    {
        batch.baseInstance = baseInstance;
        batch.drawCommandIndex = static_cast<uint32_t>(m_shellDrawCommands.size());

        Mesh* mesh = ResourceLibrary::GetMesh(batch.meshHandle.c_str());
        if (!mesh) continue;

        DrawElementsIndirectCommand cmd;
        cmd.indexCount = mesh->GetTriCount() * 3;
        cmd.instanceCount = static_cast<uint32_t>(batch.instances.size());  
        cmd.firstIndex = 0;
        cmd.baseVertex = 0;
        cmd.baseInstance = baseInstance;

        m_shellDrawCommands.push_back(cmd);
        baseInstance += cmd.instanceCount;
        m_currentShellDrawCommandCount++;
    }
}

void RendererModule::FlushInstanceData()
{
    if (m_currentInstanceCount == 0)
        return;

    auto t1 = std::chrono::high_resolution_clock::now();

    m_currentBufferIndex = (m_currentBufferIndex + 1) % NUM_BUFFERS;
    int bufferIndex = m_currentBufferIndex;

    if (m_bufferFences[bufferIndex] != nullptr)
    {
        GLenum result = glClientWaitSync(m_bufferFences[bufferIndex], 0, 1000000); 
        if (result == GL_TIMEOUT_EXPIRED || result == GL_WAIT_FAILED)
        {
        }
        glDeleteSync(m_bufferFences[bufferIndex]);
        m_bufferFences[bufferIndex] = nullptr;
    }

    InstanceData* instancePtr = m_instanceMappedPtr[bufferIndex];
    DrawElementsIndirectCommand* commandPtr = m_drawCommandMappedPtr[bufferIndex];

    size_t instanceOffset = 0;
    size_t commandOffset = 0;

    for (const auto& [key, batch] : m_batches)
    {
        size_t batchInstanceSize = batch.instances.size() * sizeof(InstanceData);
        std::memcpy(instancePtr + instanceOffset, batch.instances.data(), batchInstanceSize);
        instanceOffset += batch.instances.size();

        commandPtr[commandOffset] = m_drawCommands[commandOffset];
        commandOffset++;
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    float totalFlushTime = std::chrono::duration<float, std::milli>(t2 - t1).count();
    m_stats.bufferFlushTimeMs = totalFlushTime;

    m_stats.minBufferFlushTimeMs = std::min(m_stats.minBufferFlushTimeMs, totalFlushTime);
    m_stats.maxBufferFlushTimeMs = std::max(m_stats.maxBufferFlushTimeMs, totalFlushTime);
    m_stats.flushSampleCount++;

    float alpha = 0.05f;
    if (m_stats.flushSampleCount == 1)
    {
        m_stats.avgBufferFlushTimeMs = totalFlushTime;
    }
    else
    {
        m_stats.avgBufferFlushTimeMs = m_stats.avgBufferFlushTimeMs * (1.0f - alpha) + totalFlushTime * alpha;
    }
}

void RendererModule::FlushShellTextureData()
{
    if (m_currentShellInstanceCount == 0)
        return;

    auto t1 = std::chrono::high_resolution_clock::now();

    int bufferIndex = m_currentBufferIndex;

    if (m_shellBufferFences[bufferIndex] != nullptr)
    {
        GLenum result = glClientWaitSync(m_shellBufferFences[bufferIndex], 0, 1000000);
        if (result == GL_TIMEOUT_EXPIRED || result == GL_WAIT_FAILED)
        {
        }
        glDeleteSync(m_shellBufferFences[bufferIndex]);
        m_shellBufferFences[bufferIndex] = nullptr;
    }

    InstanceData* shellInstancePtr = m_shellInstanceMappedPtr[bufferIndex];
    DrawElementsIndirectCommand* shellCommandPtr = m_shellDrawCommandMappedPtr[bufferIndex];

    size_t instanceOffset = 0;
    size_t commandOffset = 0;

    for (const auto& [key, batch] : m_shellTextureBatches)
    {
        size_t batchInstanceSize = batch.instances.size() * sizeof(InstanceData);
        std::memcpy(shellInstancePtr + instanceOffset, batch.instances.data(), batchInstanceSize);
        instanceOffset += batch.instances.size();

        shellCommandPtr[commandOffset] = m_shellDrawCommands[commandOffset];
        commandOffset++;
    }


    auto t2 = std::chrono::high_resolution_clock::now();
    m_stats.shellBufferFlushTimeMs = std::chrono::duration<float, std::milli>(t2 - t1).count();
}

void RendererModule::ExecuteDrawCalls()
{
    if (m_batches.empty())
        return;

    int bufferIndex = m_currentBufferIndex;

    m_glState.Reset();
    m_glState.BindSSBO(0, m_instanceSSBO[bufferIndex]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCommandBuffer[bufferIndex]);

    m_stats.drawCalls = 0;
    m_stats.batchCount = 0;

    std::vector<RenderBatch*> sortedBatches;
    sortedBatches.reserve(m_batches.size());

    for (auto& [key, batch] : m_batches)
    {
        if (!batch.mesh)
        {
            batch.mesh = ResourceLibrary::GetMesh(key.meshHandle);
            batch.material = ResourceLibrary::GetMaterial(key.materialHandle);
            if (batch.material)
                batch.shader = batch.material->GetShader();
        }

        if (batch.mesh && batch.material && batch.shader)
        {
            sortedBatches.push_back(&batch);
        }
    }

    std::sort(sortedBatches.begin(), sortedBatches.end(),
        [](RenderBatch* a, RenderBatch* b) 
        {
            if (a->shader != b->shader)
                return a->shader < b->shader;
            if (a->material != b->material)
                return a->material < b->material;
            return a->mesh < b->mesh;
        });

    ShaderProgram* currentShader = nullptr;
    Material* currentMaterial = nullptr;
    Mesh* currentMesh = nullptr;

    size_t batchStart = 0;

    for (size_t i = 0; i <= sortedBatches.size(); ++i)
    {
        bool needsFlush = false;
        RenderBatch* batch = nullptr;

        if (i < sortedBatches.size())
        {
            batch = sortedBatches[i];

            if (batch->shader != currentShader || batch->material != currentMaterial)
            {
                needsFlush = (batchStart < i);
            }
        }
        else
        {
            needsFlush = (batchStart < i);
        }

        if (needsFlush)
        {
            size_t batchCount = i - batchStart;

            Mesh* sharedMesh = sortedBatches[batchStart]->mesh;
            bool sameMesh = true;
            for (size_t j = batchStart + 1; j < i; ++j)
            {
                if (sortedBatches[j]->mesh != sharedMesh)
                {
                    sameMesh = false;
                    break;
                }
            }

            if (sameMesh && batchCount > 1)
            {
                m_glState.BindVAO(sharedMesh->GetVAO());
                currentMesh = sharedMesh;

                currentShader->SetBoolUniform("useInstancing", true);

                bool consecutive = true;
                uint32_t firstCmd = sortedBatches[batchStart]->drawCommandIndex;
                for (size_t j = 1; j < batchCount; ++j)
                {
                    if (sortedBatches[batchStart + j]->drawCommandIndex != firstCmd + j)
                    {
                        consecutive = false;
                        break;
                    }
                }

                if (consecutive)
                {
                    glMultiDrawElementsIndirect(
                        GL_TRIANGLES,
                        GL_UNSIGNED_INT,
                        reinterpret_cast<void*>(firstCmd * sizeof(DrawElementsIndirectCommand)),
                        static_cast<GLsizei>(batchCount),
                        0
                    );
                    m_stats.drawCalls++;
                    m_stats.batchCount += batchCount;
                }
                else
                {
                    for (size_t j = batchStart; j < i; ++j)
                    {
                        glDrawElementsIndirect(
                            GL_TRIANGLES,
                            GL_UNSIGNED_INT,
                            reinterpret_cast<void*>(sortedBatches[j]->drawCommandIndex * sizeof(DrawElementsIndirectCommand))
                        );
                        m_stats.drawCalls++;
                    }
                    m_stats.batchCount += batchCount;
                }
            }
            else
            {
                for (size_t j = batchStart; j < i; ++j)
                {
                    RenderBatch* b = sortedBatches[j];

                    if (b->mesh != currentMesh)
                    {
                        m_glState.BindVAO(b->mesh->GetVAO());
                        currentMesh = b->mesh;
                    }


                    // bool useInstancing = b->instances.size() >= MIN_INSTANCES_FOR_BATCHING;

                    bool useInstancing = b->useInstancing && b->instances.size() >= MIN_INSTANCES_FOR_BATCHING;

                    currentShader->SetBoolUniform("useInstancing", useInstancing);

                    if (useInstancing)
                    {
                        glDrawElementsIndirect(
                            GL_TRIANGLES,
                            GL_UNSIGNED_INT,
                            reinterpret_cast<void*>(b->drawCommandIndex * sizeof(DrawElementsIndirectCommand))
                        );
                        m_stats.drawCalls++;
                        m_stats.batchCount++;
                    }
                    else
                    {
                        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

                        for (const auto& instance : b->instances)
                        {
                            currentShader->SetMat4Uniform("modelMat", instance.modelMatrix);

                            currentShader->SetUintUniform("entityIdLow", instance.entityIdLow);
                            currentShader->SetUintUniform("entityIdHigh", instance.entityIdHigh);

                            b->mesh->Render();
                            m_stats.drawCalls++;
                        }

                        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCommandBuffer[bufferIndex]);
                    }
                }
            }

            batchStart = i;
        }

        if (batch)
        {
            if (batch->shader != currentShader)
            {
                m_glState.BindShader(batch->shader);
                currentShader = batch->shader;
                currentMaterial = nullptr;
            }

            if (batch->material != currentMaterial)
            {
                /*bool useInstancing = batch->instances.size() >= MIN_INSTANCES_FOR_BATCHING;*/

                bool useInstancing = batch->useInstancing && batch->instances.size() >= MIN_INSTANCES_FOR_BATCHING;
                currentShader->SetBoolUniform("useInstancing", useInstancing);
                batch->material->ApplyUniforms();
                currentMaterial = batch->material;
            }
        }
    }

    glBindVertexArray(0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    if (m_bufferFences[bufferIndex] != nullptr)
    {
        glDeleteSync(m_bufferFences[bufferIndex]);
    }
    m_bufferFences[bufferIndex] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void RendererModule::ExecuteShellTextureDrawCalls()
{
    if (m_shellTextureBatches.empty())
        return;

    int bufferIndex = m_currentBufferIndex;

    auto shellShader = ResourceLibrary::GetShader("grass_shell");
    if (!shellShader) return;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_shellInstanceSSBO[bufferIndex]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_shellDrawCommandBuffer[bufferIndex]);

    shellShader->Use();
    shellShader->SetBoolUniform("useInstancing", true);

    Mesh* lastMesh = nullptr;

    for (const auto& [key, batch] : m_shellTextureBatches)
    {
        Mesh* mesh = ResourceLibrary::GetMesh(batch.meshHandle.c_str());
        if (!mesh) continue;

        shellShader->SetIntUniform("shellCount", batch.shellCount);

        if (batch.noiseTexture != "")
        {
            Texture* noiseTexture = ResourceLibrary::GetTexture(batch.noiseTexture);
            if (noiseTexture) 
            {
                noiseTexture->Bind(0);
                shellShader->SetIntUniform("noiseTexture", 0);
            }
        }

        if (batch.maskTexture != "")
        {
            Texture* maskTexture = ResourceLibrary::GetTexture(batch.maskTexture);
            if (maskTexture)
            {
                maskTexture->Bind(1);
                shellShader->SetIntUniform("maskTexture", 1);
            }
        }

        if (!batch.entities.empty())
        {
            flecs::entity entity = batch.entities[0];
            if (entity.has<ShellTextureComponent>())
            {
                auto* shellComp = entity.try_get<ShellTextureComponent>();
                shellShader->SetFloatUniform("shellHeight", shellComp->shellHeight);
                shellShader->SetFloatUniform("tilingAmount", shellComp->tilingAmount);
                shellShader->SetFloatUniform("thickness", shellComp->thickness);
                shellShader->SetVec3Uniform("grassColour", shellComp->baseColour);
                shellShader->SetVec3Uniform("tipColour", shellComp->tipColour);
            }
        }

        if (mesh != lastMesh)
        {
            glBindVertexArray(mesh->GetVAO());
            lastMesh = mesh;
        }

        glDrawElementsIndirect(
            GL_TRIANGLES,
            GL_UNSIGNED_INT,
            reinterpret_cast<void*>(batch.drawCommandIndex * sizeof(DrawElementsIndirectCommand)));

        m_stats.drawCalls++;
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    if (m_shellBufferFences[bufferIndex] != nullptr)
    {
        glDeleteSync(m_shellBufferFences[bufferIndex]);
    }
    m_shellBufferFences[bufferIndex] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void RendererModule::RenderSkeletalMeshes()
{
    auto shader = ResourceLibrary::GetShader("skeletal_geometry");
    if (!shader) return;

    shader->Use();
    shader->SetBoolUniform("useInstancing", false);
    shader->SetBoolUniform("useSkinning", true);

    m_skeletalMeshQuery.each([&](flecs::entity e,
        SkeletalMeshComponent& skelComp,
        MaterialComponent& matComp,
        TransformComponent& transform,
        AnimationComponent& animState)  
        {
            if (!skelComp.visible || skelComp.handle == "")
                return;

            SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(skelComp.handle.c_str());
            Material* material = ResourceLibrary::GetMaterial(matComp.handle.c_str());

            if (!mesh || !material)
                return;

            glm::mat4 modelMatrix = TransformToMatrix(e);
            shader->SetMat4Uniform("modelMat", modelMatrix);

            shader->SetIntUniform("entityIdLow", static_cast<uint32_t>(e.id() & 0xFFFFFFFF));
            shader->SetIntUniform("entityIdHigh", static_cast<uint32_t>((e.id() >> 32) & 0xFFFFFFFF));

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, animState.boneSSBO);

            material->ApplyUniforms();

            mesh->Render();

            m_stats.drawCalls++;
        });

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
}

void RendererModule::Render()
{
    CameraModule* cameraModule = m_world.try_get_mut<CameraModule>();
    Frustum* frustum = nullptr;
    Frustum cameraFrustum;

    if (cameraModule)
    {
        flecs::entity mainCamera = cameraModule->GetMainCamera();
        if (mainCamera.is_alive())
        {
            glm::mat4 viewMatrix = cameraModule->GetViewMatrix(mainCamera);
            glm::mat4 projMatrix = cameraModule->GetProjectionMatrix(mainCamera);
            glm::mat4 viewProj = projMatrix * viewMatrix;

            cameraFrustum.ExtractFromMatrix(viewProj);
            frustum = &cameraFrustum;
        }
    }

    CollectRenderData(frustum);
    CollectShellTextureData();

    if (m_currentInstanceCount > m_maxInstances * 0.9f ||
        m_currentDrawCommandCount > m_maxDrawCommands * 0.9f)
    {
        ResizeBuffers();
    }

    FlushInstanceData();
    FlushShellTextureData();

    // glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

    ExecuteDrawCalls();

    RenderSkeletalMeshes();

    ExecuteShellTextureDrawCalls();

    m_stats.totalInstances = static_cast<uint32_t>(m_currentInstanceCount);
    m_stats.shellInstances = static_cast<uint32_t>(m_currentShellInstanceCount);
}

void RendererModule::RenderShadows()
{
    auto shadowShader = ResourceLibrary::GetShader("shadow_depth");
    if (!shadowShader) return;

    //CollectRenderData();
    FlushInstanceData();

    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT);

    shadowShader->Use();

    int bufferIndex = m_currentBufferIndex;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_instanceSSBO[bufferIndex]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCommandBuffer[bufferIndex]);

    for (const auto& [key, batch] : m_batches)
    {
        Mesh* mesh = ResourceLibrary::GetMesh(key.meshHandle);
        if (!mesh) continue;

        std::vector<InstanceData> shadowCastingInstances;
        for (size_t i = 0; i < batch.entities.size(); ++i)
        {
            flecs::entity entity = batch.entities[i];
            if (entity.is_alive() && entity.has<MeshComponent>())
            {
                const auto* meshComp = entity.try_get<MeshComponent>();
                if (meshComp && meshComp->castShadows)
                {
                    shadowCastingInstances.push_back(batch.instances[i]);
                }
            }
        }

        if (shadowCastingInstances.empty())
            continue;

        glBindVertexArray(mesh->GetVAO());

        bool useInstancing = batch.useInstancing && shadowCastingInstances.size() >= MIN_INSTANCES_FOR_BATCHING;
        shadowShader->SetBoolUniform("useInstancing", useInstancing);

        if (useInstancing)
        {
            glDrawElementsIndirect(
                GL_TRIANGLES,
                GL_UNSIGNED_INT,
                reinterpret_cast<void*>(batch.drawCommandIndex * sizeof(DrawElementsIndirectCommand)));
        }
        else
        {
            for (const auto& instance : shadowCastingInstances)
            {
                shadowShader->SetMat4Uniform("modelMat", instance.modelMatrix);
                mesh->Render();
            }
        }
    }

    auto skeletalShadowShader = ResourceLibrary::GetShader("skeletal_shadow_depth");
    if (skeletalShadowShader)
    {
        skeletalShadowShader->Use();

        m_skeletalMeshQuery.each([&](flecs::entity e,
            SkeletalMeshComponent& skelComp,
            MaterialComponent& matComp,
            TransformComponent& transform,
            AnimationComponent& animState)
            {
                if (!skelComp.visible || !skelComp.castShadows || skelComp.handle == "")
                    return;

                SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(skelComp.handle.c_str());
                if (!mesh)
                    return;

                glm::mat4 modelMatrix = TransformToMatrix(e);
                skeletalShadowShader->SetMat4Uniform("modelMat", modelMatrix);

                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, animState.boneSSBO);

                glBindVertexArray(mesh->GetVAO());
                mesh->Render();
            });

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glCullFace(GL_BACK);
}

void RendererModule::RenderBackfaces()
{
    //CollectRenderData();

    if (m_currentInstanceCount > m_maxInstances * 0.9f ||
        m_currentDrawCommandCount > m_maxDrawCommands * 0.9f)
    {
        ResizeBuffers();
    }

    FlushInstanceData();

    ExecuteBackfaceDrawCalls();

    m_stats.totalInstances = static_cast<uint32_t>(m_currentInstanceCount);
}

void RendererModule::ExecuteBackfaceDrawCalls()
{
    if (m_batches.empty())
        return;

    auto shader = ResourceLibrary::GetShader("mesh_position");
    if (!shader) return;

    int bufferIndex = m_currentBufferIndex;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_instanceSSBO[bufferIndex]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCommandBuffer[bufferIndex]);

    shader->Use();

    m_stats.drawCalls = 0;
    m_stats.batchCount = 0;

    Mesh* lastMesh = nullptr;

    for (const auto& [key, batch] : m_batches)
    {
        Mesh* mesh = ResourceLibrary::GetMesh(key.meshHandle);
        if (!mesh) continue;

        /*bool useInstancing = batch.instances.size() >= MIN_INSTANCES_FOR_BATCHING;*/

        bool useInstancing = batch.useInstancing && batch.instances.size() >= MIN_INSTANCES_FOR_BATCHING;
        shader->SetBoolUniform("useInstancing", useInstancing);

        if (mesh != lastMesh)
        {
            glBindVertexArray(mesh->GetVAO());
            lastMesh = mesh;
        }

        if (useInstancing)
        {
            glDrawElementsIndirect(
                GL_TRIANGLES,
                GL_UNSIGNED_INT,
                reinterpret_cast<void*>(batch.drawCommandIndex * sizeof(DrawElementsIndirectCommand)));

            m_stats.drawCalls++;
            m_stats.batchCount++;
        }
        else
        {
            for (const auto& instance : batch.instances)
            {
                shader->SetMat4Uniform("modelMat", instance.modelMatrix);
                mesh->Render();
                m_stats.drawCalls++;
            }
        }
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void RendererModule::RenderShellTextures()
{
    CollectShellTextureData();

    if (m_currentShellInstanceCount > m_maxShellInstances * 0.9f ||
        m_currentShellDrawCommandCount > m_maxShellDrawCommands * 0.9f)
    {
        m_maxShellInstances = m_maxShellInstances * INSTANCE_BUFFER_GROW_FACTOR;
        m_maxShellDrawCommands = m_maxShellDrawCommands * INSTANCE_BUFFER_GROW_FACTOR;
    }

    FlushShellTextureData();
    ExecuteShellTextureDrawCalls();

    m_stats.shellInstances = static_cast<uint32_t>(m_currentShellInstanceCount);
}

void RendererModule::RenderXray()
{
    ShaderProgram* xrayShader = ResourceLibrary::GetShader("xray");
    if (!xrayShader) return;

    xrayShader->Use();
    xrayShader->SetBoolUniform("useInstancing", false);

    m_xrayQuery.each([&](flecs::entity entity, XrayComponent& xray, MeshComponent& meshComp, TransformComponent& transform)
    {
        if (!xray.enabled) return;

        Mesh* mesh = GetMesh(meshComp);
        if (!mesh) return;

        glm::mat4 modelMatrix = TransformToMatrix(entity);

        xrayShader->SetMat4Uniform("modelMat", modelMatrix);
        xrayShader->SetVec3Uniform("xrayColour", xray.xrayColour);
        xrayShader->SetFloatUniform("xrayAlpha", xray.xrayAlpha);
        xrayShader->SetFloatUniform("rimPower", xray.rimPower);
        xrayShader->SetBoolUniform("useRimLighting", xray.useRimLighting);

        glBindVertexArray(mesh->GetVAO());
        mesh->Render();
        glBindVertexArray(0);
    });

    xrayShader->SetBoolUniform("useSkinning", true);

    m_xraySkeletalQuery.each([&](flecs::entity entity, XrayComponent& xray, SkeletalMeshComponent& skelComp, TransformComponent& transform)
    {
        if (!xray.enabled || !skelComp.visible || skelComp.handle == "") return;

        SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(skelComp.handle.c_str());
        if (!mesh) return;

        glm::mat4 modelMatrix = TransformToMatrix(entity);

        xrayShader->SetMat4Uniform("modelMat", modelMatrix);
        xrayShader->SetVec3Uniform("xrayColour", xray.xrayColour);
        xrayShader->SetFloatUniform("xrayAlpha", xray.xrayAlpha);
        xrayShader->SetFloatUniform("rimPower", xray.rimPower);
        xrayShader->SetBoolUniform("useRimLighting", xray.useRimLighting);

        if (entity.has<AnimationComponent>())
        {
            auto* animComp = entity.try_get<AnimationComponent>();
            if (animComp && animComp->boneSSBO != 0)
            {
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, animComp->boneSSBO);
            }
        }

        glBindVertexArray(mesh->GetVAO());
        mesh->Render();
        glBindVertexArray(0);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    });

    xrayShader->SetBoolUniform("useSkinning", false);
}

void RendererModule::RenderMesh(Mesh* mesh, Material* material, const glm::mat4& modelMatrix)
{
    if (!mesh || !material) return;

    material->GetShader()->Use();

    material->GetShader()->SetBoolUniform("useInstancing", false);
    material->GetShader()->SetMat4Uniform("modelMat", modelMatrix);

    material->GetShader()->SetUintUniform("entityIdLow", 0u);
    material->GetShader()->SetUintUniform("entityIdHigh", 0u);

    material->ApplyUniforms();

    glBindVertexArray(mesh->GetVAO());
    mesh->Render();
    glBindVertexArray(0);
}

void RendererModule::DrawRenderStats(bool* p_open)
{
    if (p_open && !(*p_open)) return;

    //ImGui::Begin("Renderer Statistics");
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Renderer Statistics", p_open)) { ImGui::End(); return; }

    ImGui::Text("Performance:");
    ImGui::Separator();

    ImGui::Text("Draw Calls: %u", m_stats.drawCalls);
    ImGui::Text("Batches: %u", m_stats.batchCount);
    ImGui::Text("Total Instances: %u", m_stats.totalInstances);
    ImGui::Text("Shell Instances: %u", m_stats.shellInstances);
    ImGui::Text("Regular Instances: %u", m_stats.totalInstances - m_stats.shellInstances);

    float avgInstancesPerBatch = m_stats.batchCount > 0 ?
        (float)m_stats.totalInstances / m_stats.batchCount : 0.0f;
    ImGui::Text("Avg Instances/Batch: %.1f", avgInstancesPerBatch);

    ImGui::Separator();
    ImGui::Text("Buffer Flush Times:");
    ImGui::Text("Current: %.3f ms (Inst: %.3f + Shell: %.3f)",
        m_stats.bufferFlushTimeMs + m_stats.shellBufferFlushTimeMs,
        m_stats.bufferFlushTimeMs,
        m_stats.shellBufferFlushTimeMs);

    ImGui::Spacing();
    ImGui::Text("Min: %.3f ms", m_stats.minBufferFlushTimeMs);
    ImGui::Text("Max: %.3f ms", m_stats.maxBufferFlushTimeMs);
    ImGui::Text("Avg: %.3f ms", m_stats.avgBufferFlushTimeMs);
    ImGui::Text("Samples: %u", m_stats.flushSampleCount);

    if (ImGui::Button("Reset Min/Max"))
    {
        m_stats.minBufferFlushTimeMs = FLT_MAX;
        m_stats.maxBufferFlushTimeMs = 0.0f;
        m_stats.flushSampleCount = 0;
    }

    ImGui::Spacing();

    float maxTime = m_stats.maxBufferFlushTimeMs;
    if (maxTime > 0.5f)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "HITCHES DETECTED: Max %.3f ms", maxTime);
    }
    else if (maxTime > 0.2f)
    {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Occasional spikes: Max %.3f ms", maxTime);
    }
    else
    {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Stable performance");
    }

    ImGui::Separator();
    ImGui::Text("Buffer Usage:");
    ImGui::ProgressBar((float)m_currentInstanceCount / m_maxInstances, ImVec2(-1, 0),
        "Instance Buffer");
    ImGui::Text("%zu / %zu instances", m_currentInstanceCount, m_maxInstances);

    ImGui::ProgressBar((float)m_currentDrawCommandCount / m_maxDrawCommands, ImVec2(-1, 0),
        "Command Buffer");
    ImGui::Text("%zu / %zu commands", m_currentDrawCommandCount, m_maxDrawCommands);

    ImGui::Text("GPU Memory: %zu MB", m_stats.bufferMemoryMB);

    ImGui::End();
}

glm::mat4 RendererModule::TransformToMatrix(flecs::entity entity)
{
    return Math::LocalToWorldMat(entity);
}

void RendererModule::RegisterWithEditor()
{
    if (!m_editorModule) return;

    m_editorModule->RegisterComponent<MeshComponent>(
        "Static Mesh",
        "Rendering",
        [this](flecs::entity entity, MeshComponent& component)
        {
            DrawMeshComponentInspector(entity, component);
        }
    );

    m_editorModule->RegisterComponent<MaterialComponent>(
        "Material",
        "Rendering",
        [this](flecs::entity entity, MaterialComponent& component)
        {
            DrawMaterialComponentInspector(entity, component);
        }
    );

    m_editorModule->RegisterComponent<SkeletalMeshComponent>(
        "Skeletal Mesh",
        "Rendering",
        [this](flecs::entity entity, SkeletalMeshComponent& component)
        {
            DrawSkeletalMeshComponentInspector(entity, component);
        }
    );

    m_editorModule->RegisterComponent<ShellTextureComponent>(
        "Shell Texture",
        "Rendering",
        [this](flecs::entity entity, ShellTextureComponent& component)
        {
            DrawShellTextureInspector(entity, component);
        }
    );

    m_editorModule->RegisterComponent<XrayComponent>(
        "X-Ray Vision",
        "Rendering",
        [this](flecs::entity entity, XrayComponent& component)
        {
            DrawXrayInspector(entity, component);
        }
    );

    m_editorModule->RegisterComponent<PostProcessComponent>(
        "Post Process",
        "Rendering",
        [this](flecs::entity entity, PostProcessComponent& component)
        {
            DrawPostProcessInspector(entity, component);
        }
    );

    m_editorModule->RegisterEntityColour([this](flecs::entity entity) -> ImVec4
        {
            return GetRenderEntityColour(entity);
        });
}

Mesh* RendererModule::GetMesh(const MeshComponent& component) const
{
    return ResourceLibrary::GetMesh(component.handle.c_str());
}

Material* RendererModule::GetMaterial(const MaterialComponent& component) const
{
    return ResourceLibrary::GetMaterial(component.handle.c_str());
}

void RendererModule::SetMesh(flecs::entity e, const AssetHandle& handle)
{
    e.set<MeshComponent>({ handle, true });
}

void RendererModule::SetMaterial(flecs::entity e, const AssetHandle& handle)
{
    e.set<MaterialComponent>({ handle });
}

void RendererModule::SetVisibility(flecs::entity e, bool visible)
{
    if (auto* meshComp = e.try_get_mut<MeshComponent>())
    {
        meshComp->visible = visible;
    }
}

void RendererModule::DrawMeshComponentInspector(flecs::entity entity, MeshComponent& component)
{
    if (ImGui::BeginTable("##mesh_component_props", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Visible");
        ImGui::TableNextColumn();
        if (ImGui::Checkbox("##Visible", &component.visible))
        {
            SetVisibility(entity, component.visible);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Use Instancing");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##UseInstancing", &component.useInstancing);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Cast Shadows");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##CastShadows", &component.castShadows);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Mesh");
        ImGui::TableNextColumn();

        std::string currentMeshName = component.handle == "" ? "None" : component.handle.c_str();


        ImGuiComboFlags comboFlags = ImGuiComboFlags_HeightLargest;

        if (ImGui::BeginCombo("##MeshSelect", currentMeshName.c_str(), comboFlags))
        {
            bool isSelected = (component.handle != "");
            if (ImGui::Selectable("None", isSelected))
            {
                SetMesh(entity, nullptr);
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();

            auto availableMeshes = ResourceLibrary::GetAllMeshHandles();
            for (const char* meshHandle : availableMeshes)
            {
                isSelected = (component.handle != "" && component.handle == meshHandle);
                if (ImGui::Selectable(meshHandle, isSelected))
                {
                    SetMesh(entity, meshHandle);
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (component.handle != "")
        {
            Mesh* mesh = GetMesh(component);
            if (mesh)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Handle");
                ImGui::TableNextColumn();
                ImGui::Text("%s", component.handle.c_str());

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Triangles");
                ImGui::TableNextColumn();
                ImGui::Text("%d", mesh->GetTriCount());

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Vertices");
                ImGui::TableNextColumn();
                ImGui::Text("%d", mesh->GetTriCount() * 3);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Status");
                ImGui::TableNextColumn();
                ImGui::TextColored(ImVec4(0.60f, 0.85f, 0.70f, 1.00f), "Loaded");
            }
            else
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Handle");
                ImGui::TableNextColumn();
                ImGui::Text("%s", component.handle.c_str());

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Status");
                ImGui::TableNextColumn();
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Missing mesh asset");
            }
        }
        else
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Status");
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No mesh assigned");
        }

        ImGui::EndTable();
    }

    if (!entity.has<MaterialComponent>() && !entity.has<ShellTextureComponent>())
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Note: No material or shell texture component - mesh won't render");
    }
}

void RendererModule::DrawSkeletalMeshComponentInspector(flecs::entity entity, SkeletalMeshComponent& component)
{
    if (ImGui::BeginTable("##skeletal_mesh_props", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Visible");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##Visible", &component.visible);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Cast Shadows");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##CastShadows", &component.castShadows);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Mesh");
        ImGui::TableNextColumn();

        std::string currentMeshName = component.handle == "" ? "None" : component.handle;
        if (ImGui::BeginCombo("##MeshSelect", currentMeshName.c_str()))
        {
            if (ImGui::Selectable("None"))
                component.handle = "";

            auto handles = ResourceLibrary::GetAllSkeletalMeshHandles();
            for (const char* handle : handles)
            {
                bool isSelected = (component.handle == handle);
                if (ImGui::Selectable(handle, isSelected))
                {
                    component.handle = handle;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (component.handle != "")
        {
            SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(component.handle.c_str());
            if (mesh)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Triangles");
                ImGui::TableNextColumn();
                ImGui::Text("%d", mesh->GetTriCount());
            }
        }

        ImGui::EndTable();
    }

    if (!entity.has<MaterialComponent>())
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Note: No material component - mesh won't render");
    }
}

void RendererModule::DrawMaterialComponentInspector(flecs::entity entity, MaterialComponent& component)
{
    if (ImGui::BeginTable("##material_component_props", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Material");
        ImGui::TableNextColumn();

        std::string currentMaterialName = component.handle.empty() ? "None" : component.handle;

        if (ImGui::BeginCombo("##MaterialSelect", currentMaterialName.c_str(), ImGuiComboFlags_HeightLargest))
        {
            bool isSelected = (component.handle.empty());
            if (ImGui::Selectable("None", isSelected))
            {
                SetMaterial(entity, "");
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();

            auto availableMaterials = ResourceLibrary::GetAllMaterialHandles();
            for (const char* materialHandle : availableMaterials)
            {
                isSelected = (!component.handle.empty() && component.handle == materialHandle);
                if (ImGui::Selectable(materialHandle, isSelected))
                {
                    SetMaterial(entity, materialHandle);
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        if (!component.handle.empty())
        {
            Material* material = GetMaterial(component);
            if (material)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Handle");
                ImGui::TableNextColumn();
                ImGui::Text("%s", component.handle.c_str());

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Status");
                ImGui::TableNextColumn();
                ImGui::TextColored(ImVec4(0.60f, 0.85f, 0.70f, 1.00f), "Loaded");

                if (material->GetShader())
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("Shader");
                    ImGui::TableNextColumn();
                    ImGui::Text("Loaded");
                }

                ImGui::EndTable();

                ImGui::Separator();

                if (ImGui::CollapsingHeader("Material Properties", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    DrawMaterialUniforms(material);
                }

                ImGui::Separator();
                if (ImGui::Button("Save Material"))
                {
                    ResourceLibrary::SaveMaterial(component.handle);
                }
            }
            else
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Status");
                ImGui::TableNextColumn();
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Missing material asset");

                ImGui::EndTable();
            }
        }
        else
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Status");
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No material assigned");

            ImGui::EndTable();
        }
    }

    if (!entity.has<MeshComponent>() && !entity.has<SkeletalMeshComponent>())
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Note: No mesh component - material won't be used");
    }
}


void RendererModule::DrawPostProcessInspector(flecs::entity entity, PostProcessComponent& component)
{
    if (ImGui::BeginTable("##postprocess_props", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Enabled");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##Enabled", &component.enabled);

        if (component.enabled)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Separator();
            if (ImGui::TreeNodeEx("Fog Settings", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::TableNextColumn();
                ImGui::Separator();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Enable Fog");
                ImGui::TableNextColumn();
                if (ImGui::Checkbox("##EnableFog", &component.enableFog))
                    entity.modified<PostProcessComponent>();

                if (component.enableFog)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Extinction");
                    ImGui::SetItemTooltip("Controls fog density per color channel");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::DragFloat3("##FogExtinction", glm::value_ptr(component.fogExtinction), 0.001f, 0.0f, 1.0f, "%.4f"))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Inscatter");
                    ImGui::SetItemTooltip("Light scattering in fog");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::DragFloat3("##FogInscatter", glm::value_ptr(component.fogInscatter), 0.001f, 0.0f, 1.0f, "%.4f"))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Base Colour");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::ColorEdit3("##FogBaseColour", glm::value_ptr(component.fogBaseColour), ImGuiColorEditFlags_NoInputs))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Sun Colour");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::ColorEdit3("##FogSunColour", glm::value_ptr(component.fogSunColour), ImGuiColorEditFlags_NoInputs))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Sun Power");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::DragFloat("##FogSunPower", &component.fogSunPower, 0.5f, 0.0f, 100.0f, "%.1f"))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Fog Height");
                    ImGui::SetItemTooltip("Base height for fog calculations");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::DragFloat("##FogHeight", &component.fogHeight, 0.1f, -100.0f, 100.0f, "%.1f"))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Height Falloff");
                    ImGui::SetItemTooltip("How quickly fog dissipates with height");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::DragFloat("##FogHeightFalloff", &component.fogHeightFalloff, 0.01f, 0.0f, 10.0f, "%.2f"))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Height Density");
                    ImGui::SetItemTooltip("Fog density at base height");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::DragFloat("##FogHeightDensity", &component.fogHeightDensity, 0.1f, 0.0f, 100.0f, "%.1f"))
                        entity.modified<PostProcessComponent>();
                }
                ImGui::TreePop();
            }
            else
            {
                ImGui::TableNextColumn();
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Separator();
            if (ImGui::TreeNodeEx("Quantization & Dithering", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::TableNextColumn();
                ImGui::Separator();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Enable Quantization");
                ImGui::TableNextColumn();
                if(ImGui::Checkbox("##EnableQuantization", &component.enableQuantization))
                    entity.modified<PostProcessComponent>();

                if (component.enableQuantization)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Quantization Levels");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::SliderInt("##QuantizationLevels", &component.quantizationLevels, 2, 32))
                    {
                        component.quantizationLevels = glm::clamp(component.quantizationLevels, 2, 32);
                        entity.modified<PostProcessComponent>();
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Enable Dithering");
                    ImGui::TableNextColumn();
                    if(ImGui::Checkbox("##EnableDithering", &component.enableDithering))
                        entity.modified<PostProcessComponent>();

                    if (component.enableDithering)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Dither Strength");
                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if(ImGui::SliderFloat("##DitherStrength", &component.ditherStrength, 0.1f, 2.0f, "%.2f"))
                            entity.modified<PostProcessComponent>();

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Dither Texture");
                        ImGui::TableNextColumn();

                        std::string currentDitherTextureName = component.ditherTexture != "" ? component.ditherTexture : "None";
                        if (ImGui::BeginCombo("##DitherTexture", currentDitherTextureName.c_str()))
                        {
                            bool isSelected = (component.ditherTexture == "");
                            if (ImGui::Selectable("None", isSelected))
                            {
                                component.ditherTexture = "";
                                entity.modified<PostProcessComponent>();
                            }
                            if (isSelected)
                                ImGui::SetItemDefaultFocus();

                            auto availableTextures = ResourceLibrary::GetAllTextureHandles();
                            for (const char* textureHandle : availableTextures)
                            {
                                bool isTextureSelected = (component.ditherTexture != "" && component.ditherTexture == textureHandle);
                                if (ImGui::Selectable(textureHandle, isTextureSelected))
                                {
                                    component.ditherTexture = textureHandle;
                                    entity.modified<PostProcessComponent>();
                                }
                                if (isTextureSelected)
                                    ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        if (component.ditherTexture == "")
                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TableNextColumn();
                            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Warning: No dither texture selected");
                        }
                    }

      
                }
                ImGui::TreePop();
            }
            else
            {
                ImGui::TableNextColumn();
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Separator();
            if (ImGui::TreeNodeEx("Colour & HDR", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::TableNextColumn();
                ImGui::Separator();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Colour Factor");
                ImGui::SetItemTooltip("Blend between original and processed colour");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                if(ImGui::SliderFloat("##ColourFactor", &component.colourFactor, 0.0f, 1.0f, "%.2f"))
                    entity.modified<PostProcessComponent>();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Brightness");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                if(ImGui::SliderFloat("##Brightness", &component.brightnessFactor, 0.5f, 3.0f, "%.2f"))
                    entity.modified<PostProcessComponent>();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Enable HDR");
                ImGui::TableNextColumn();
                if(ImGui::Checkbox("##EnableHDR", &component.enableHDR))
                    entity.modified<PostProcessComponent>();

                if (component.enableHDR)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Exposure");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::SliderFloat("##Exposure", &component.exposure, 0.1f, 5.0f, "%.2f"))
                        entity.modified<PostProcessComponent>();
                }
                ImGui::TreePop();
            }
            else
            {
                ImGui::TableNextColumn();
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Separator();
            if (ImGui::TreeNodeEx("HSV Adjustment", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::TableNextColumn();
                ImGui::Separator();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Enable HSV");
                ImGui::TableNextColumn();
                if(ImGui::Checkbox("##EnableHSV", &component.enableHSV))
                    entity.modified<PostProcessComponent>();

                if (component.enableHSV)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Hue Shift");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::SliderFloat("##HueShift", &component.hueShift, -1.0f, 1.0f, "%.2f"))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Saturation");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::SliderFloat("##Saturation", &component.saturationAdjust, 0.0f, 2.0f, "%.2f"))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Value");
                    ImGui::SetItemTooltip("Brightness in HSV space");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::SliderFloat("##Value", &component.valueAdjust, 0.0f, 2.0f, "%.2f"))
                        entity.modified<PostProcessComponent>();

                    if (ImGui::Button("Reset HSV"))
                    {
                        component.hueShift = 0.0f;
                        component.saturationAdjust = 1.0f;
                        component.valueAdjust = 1.0f;
                        entity.modified<PostProcessComponent>();
                    }
                }
                ImGui::TreePop();
            }
            else
            {
                ImGui::TableNextColumn();
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Separator();
            if (ImGui::TreeNodeEx("Edge Detection", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::TableNextColumn();
                ImGui::Separator();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Enable Edges");
                ImGui::TableNextColumn();
                if(ImGui::Checkbox("##EnableEdges", &component.enableEdgeDetection))
                    entity.modified<PostProcessComponent>();

                if (component.enableEdgeDetection)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Show Edges Only");
                    ImGui::SetItemTooltip("Display only the edge lines");
                    ImGui::TableNextColumn();
                    if(ImGui::Checkbox("##EdgesOnly", &component.showEdgesOnly))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Edge Strength");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::SliderFloat("##EdgeStrength", &component.edgeStrength, 0.0f, 1.0f, "%.2f"))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Edge Width");
                    ImGui::SetItemTooltip("Width of edge detection kernel");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::SliderFloat("##EdgeWidth", &component.edgeWidth, 1.0f, 10.0f, "%1.f"))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Depth Threshold");
                    ImGui::SetItemTooltip("Sensitivity to depth changes");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::DragFloat("##DepthThresh", &component.depthThreshold, 0.00001f, 0.0f, 0.01f, "%.5f"))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Normal Threshold");
                    ImGui::SetItemTooltip("Sensitivity to normal changes");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::SliderFloat("##NormalThresh", &component.normalThreshold, 0.0f, 1.0f, "%.2f"))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Darken Amount");
                    ImGui::SetItemTooltip("How much to darken edges");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::SliderFloat("##DarkenAmount", &component.darkenAmount, 0.0f, 3.0f, "%.2f"))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Lighten Amount");
                    ImGui::SetItemTooltip("How much to lighten non-edges");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::SliderFloat("##LightenAmount", &component.lightenAmount, 0.0f, 3.0f, "%.2f"))
                        entity.modified<PostProcessComponent>();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Normal Bias");
                    ImGui::SetItemTooltip("Bias for normal-based edge detection");
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if(ImGui::DragFloat3("##NormalBias", glm::value_ptr(component.normalBias), 0.01f, -2.0f, 2.0f, "%.2f"))
                        entity.modified<PostProcessComponent>();
                }
                ImGui::TreePop();
            }
            else
            {
                ImGui::TableNextColumn();
            }
        }

        ImGui::EndTable();
    }
}

void RendererModule::DrawShellTextureInspector(flecs::entity entity, ShellTextureComponent& component)
{
    if (ImGui::BeginTable("##shell_texture_props", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Enabled");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##Enabled", &component.enabled);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Separator();
        ImGui::Text("Shell Settings");
        ImGui::TableNextColumn();
        ImGui::Separator();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Shell Count");
        ImGui::SetItemTooltip("Number of layers (affects quality and performance)");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderInt("##ShellCount", &component.shellCount, 4, 32))
        {
            component.shellCount = glm::clamp(component.shellCount, 4, 32);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Shell Height");
        ImGui::SetItemTooltip("Total height of the shell layers");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::DragFloat("##ShellHeight", &component.shellHeight, 0.005f, 0.01f, 5.0f, "%.2f");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Separator();
        ImGui::Text("Appearance");
        ImGui::TableNextColumn();
        ImGui::Separator();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Tiling");
        ImGui::SetItemTooltip("Controls texture density/tiling");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::DragFloat("##Tiling", &component.tilingAmount, component.tilingAmount < 1.0f ? 0.01f : 1.0f, 0.01f, 512.0f, "%.01f");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Thickness");
        ImGui::SetItemTooltip("Base thickness of texture layers");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##Thickness", &component.thickness, 0.01f, 1.0f, "%.2f");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Base Colour");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::ColorEdit3("##BaseColour", glm::value_ptr(component.baseColour),
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Tip Colour");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::ColorEdit3("##TipColour", glm::value_ptr(component.tipColour),
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Noise Texture");
        ImGui::TableNextColumn();

        std::string currentTextureName = component.noiseTexture != "" ? component.noiseTexture : "None";
        if (ImGui::BeginCombo("##NoiseTexture", currentTextureName.c_str()))
        {
            auto availableTextures = ResourceLibrary::GetAllTextureHandles();
            for (const char* textureHandle : availableTextures)
            {
                bool isSelected = (component.noiseTexture != "" && component.noiseTexture == textureHandle);
                if (ImGui::Selectable(textureHandle, isSelected))
                {
                    component.noiseTexture = textureHandle;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Mask Texture");
        ImGui::TableNextColumn();

        std::string currentMaskTextureName = component.maskTexture != "" ? component.maskTexture : "None";
        if (ImGui::BeginCombo("##MaskTexture", currentMaskTextureName.c_str()))
        {
            bool isSelected = (component.maskTexture != "");
            if (ImGui::Selectable("None", isSelected))
            {
                component.maskTexture = "";
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();

            auto availableTextures = ResourceLibrary::GetAllTextureHandles();
            for (const char* textureHandle : availableTextures)
            {
                bool isSelected = (component.maskTexture != "" && component.maskTexture == textureHandle);
                if (ImGui::Selectable(textureHandle, isSelected))
                {
                    component.maskTexture = textureHandle;
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Performance Info:");

    if (entity.has<MeshComponent>())
    {
        auto* meshComp = entity.try_get<MeshComponent>();
        if (meshComp && meshComp->handle.c_str())
        {
            Mesh* mesh = GetMesh(*meshComp);
            if (mesh)
            {
                int verticesPerShell = mesh->GetTriCount() * 3;
                int totalVertices = verticesPerShell * component.shellCount;
                ImGui::Text("  Vertices per shell: %d", verticesPerShell);
                ImGui::Text("  Total instances: %d", component.shellCount);
                ImGui::Text("  Total vertices: %d", totalVertices);
            }
        }
    }

    if (!entity.has<MeshComponent>() && !entity.has<SkeletalMeshComponent>())
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Note: No mesh component - shell texture won't render");
    }
}

void RendererModule::DrawMaterialUniforms(Material* material)
{

    if (!material || !material->GetShader()) {
        ImGui::TextDisabled("No shader available");
        return;
    }

    const auto& uniforms = material->GetExposedUniforms();

    if (uniforms.empty())
    {
        ImGui::TextDisabled("No custom uniforms found");
        return;
    }

    bool changed = false;

    for (const auto& uniformInfo : uniforms)
    {
        ImGui::PushID(uniformInfo.name.c_str());

        switch (uniformInfo.type)
        {
        case UniformType::Float:
        {
            float value = material->GetFloat(uniformInfo.name);
            if (ImGui::DragFloat(uniformInfo.name.c_str(), &value, 0.01f))
            {
                material->SetFloat(uniformInfo.name, value);
                changed = true;
            }
            break;
        }
        case UniformType::Int:
        {
            int value = material->GetInt(uniformInfo.name);
            if (ImGui::DragInt(uniformInfo.name.c_str(), &value))
            {
                material->SetInt(uniformInfo.name, value);
                changed = true;
            }
            break;
        }
        case UniformType::Bool:
        {
            bool value = material->GetBool(uniformInfo.name);
            if (ImGui::Checkbox(uniformInfo.name.c_str(), &value))
            {
                material->SetBool(uniformInfo.name, value);
                changed = true;
            }
            break;
        }
        case UniformType::Vec2:
        {
            glm::vec2 value = material->GetVec2(uniformInfo.name);
            if (ImGui::DragFloat2(uniformInfo.name.c_str(), &value.x, 0.01f))
            {
                material->SetVec2(uniformInfo.name, value);
                changed = true;
            }
            break;
        }
        case UniformType::Vec3:
        {
            glm::vec3 value = material->GetVec3(uniformInfo.name);

            if (uniformInfo.name.find("olour") != std::string::npos ||
                uniformInfo.name.find("olor") != std::string::npos)
            {
                if (ImGui::ColorEdit3(uniformInfo.name.c_str(), &value.x))
                {
                    material->SetVec3(uniformInfo.name, value);
                    changed = true;
                }
            }
            else
            {
                if (ImGui::DragFloat3(uniformInfo.name.c_str(), &value.x, 0.01f))
                {
                    material->SetVec3(uniformInfo.name, value);
                    changed = true;
                }
            }
            break;
        }
        case UniformType::Vec4:
        {
            glm::vec4 value = material->GetVec4(uniformInfo.name);

            if (uniformInfo.name.find("olour") != std::string::npos ||
                uniformInfo.name.find("olor") != std::string::npos)
            {
                if (ImGui::ColorEdit4(uniformInfo.name.c_str(), &value.x))
                {
                    material->SetVec4(uniformInfo.name, value);
                    changed = true;
                }
            }
            else
            {
                if (ImGui::DragFloat4(uniformInfo.name.c_str(), &value.x, 0.01f))
                {
                    material->SetVec4(uniformInfo.name, value);
                    changed = true;
                }
            }
            break;
        }
        case UniformType::Sampler2D:
        {
            Texture* currentTexture = material->GetTexture(uniformInfo.name);
            std::string textureName = currentTexture ? "Loaded" : "None";

            if (ImGui::BeginCombo(uniformInfo.name.c_str(), textureName.c_str()))
            {
                if (ImGui::Selectable("None"))
                {
                    material->SetTexture(uniformInfo.name, nullptr);
                    changed = true;
                }

                auto availableTextures = ResourceLibrary::GetAllTextureHandles();
                for (const char* textureHandle : availableTextures)
                {
                    bool isSelected = false;
                    if (ImGui::Selectable(textureHandle, isSelected))
                    {
                        Texture* texture = ResourceLibrary::GetTexture(textureHandle);
                        material->SetTexture(uniformInfo.name, texture);
                        changed = true;
                    }
                }

                ImGui::EndCombo();
            }
            break;
        }
        }

        ImGui::PopID();
    }

    if (changed)
    {
        // ResourceLibrary::SaveMaterial(materialName);
    }
}

void RendererModule::DrawXrayInspector(flecs::entity entity, XrayComponent& component)
{
    if (ImGui::BeginTable("##xray_props", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Enabled");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##Enabled", &component.enabled);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Separator();
        ImGui::Text("X-Ray Settings");
        ImGui::TableNextColumn();
        ImGui::Separator();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("X-Ray Colour");
        ImGui::SetItemTooltip("Colour of the X-ray effect");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::ColorEdit3("##XrayColour", &component.xrayColour.x);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Alpha");
        ImGui::SetItemTooltip("Transparency of the X-ray effect");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderFloat("##XrayAlpha", &component.xrayAlpha, 0.0f, 1.0f, "%.2f");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Use Rim Lighting");
        ImGui::SetItemTooltip("Enable rim lighting effect for better visibility");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##UseRimLighting", &component.useRimLighting);

        if (component.useRimLighting)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Rim Power");
            ImGui::SetItemTooltip("Power of the rim lighting effect");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat("##RimPower", &component.rimPower, 0.5f, 5.0f, "%.2f");
        }

        ImGui::EndTable();
    }
}

ImVec4 RendererModule::GetRenderEntityColour(flecs::entity entity)
{
    if (entity.has<ShellTextureComponent>())
    {
        const auto* shellComp = entity.try_get<ShellTextureComponent>();
        if (shellComp->enabled)
        {
            return ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
        }
        else
        {
            return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        }
    }

    if (entity.has<MeshComponent>())
    {
        const auto* mesh = entity.try_get<MeshComponent>();
        if (mesh->visible && mesh->handle != "" && ResourceLibrary::GetMesh(mesh->handle.c_str()))
        {
            return ImVec4(0.65f, 0.65f, 0.75f, 1.0f);
        }
        else
        {
            return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        }
    }

    return ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
}