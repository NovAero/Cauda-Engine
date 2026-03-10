#pragma once

#include "common/caribbean_buffers.h"
#include "common/randgen_buffers.h"


class EditorModule;
class PhysicsModule;
class RendererModule;
class UniformBuffer;
class ShaderStorageBuffer;

namespace ECaribbeanMode
{
    enum Type : int
    {
        Generic,
        Fluid,
        END
    };

    static const char* Labels[Type::END] =
    {
        "Generic",
        "Fluid"
    };
}

struct CaribbeanComponent
{
    // Serialised
    ECaribbeanMode::Type computeMode = ECaribbeanMode::Generic;
    bool enabled = false;
    bool visible = true;
    int maxSteps = 4;
    float fixedTimeStep = 0.02f;
    glm::vec3 originOffset = glm::vec3(0.f);

    bool hasBounds = false;
    glm::vec3 boxHalfExtent = glm::vec3(2.f);

    int maxParticles = 8192;
    int spawnCount = 10;
    float spawnInterval = 0.05f;
    glm::vec3 spawnOffset = glm::vec3(0.f);
    glm::vec3 particleColour = glm::vec3(0.9f, 0.1f, 0.1f);
    glm::vec3 accForce = glm::vec3(0.f);
    
    // Spawning Parameters
    float minRadius = 0.25f;
    float maxRadius = 0.25f;
    float minLifetime = 1.f;
    float maxLifetime = 1.f;
    float minInitSpeed = 5.f;
    float maxInitSpeed = 5.f;

    // Non-Serialised
    float accumulatedSpawnTime = 0.f;
    float accumulatedSimTime = 0.f;
};

class CaribbeanModule
{
public:
    CaribbeanModule(flecs::world& world);
    ~CaribbeanModule();

    CaribbeanModule(const CaribbeanModule&) = delete;
    CaribbeanModule& operator=(const CaribbeanModule&) = delete;
    CaribbeanModule(CaribbeanModule&&) = delete;
    CaribbeanModule& operator=(CaribbeanModule&&) = delete;

    void Render();

    // Helper Functions/Methods
    void TryEnableParticlesFromEntity(flecs::entity entity, bool isEnabled);
    void TryEnableParticlesFromComponent(CaribbeanComponent& component, bool isEnabled);
    void TryRenderParticlesFromEntity(flecs::entity entity, bool isVisible);
    void TryRenderParticlesFromComponent(CaribbeanComponent& component, bool isVisible);
    void TrySpawnParticlesFromEntity(flecs::entity entity, int spawnCount);

    //void RenderShadows(const glm::mat4& lightSpaceMatrix);

private:
    unsigned int m_emptyVAO;

    std::unique_ptr<ShaderStorageBuffer> m_randomGenSSBO;

    std::unordered_map<flecs::entity_t, std::unique_ptr<UniformBuffer>> m_particleUBOs;
    std::unordered_map<flecs::entity_t, std::unique_ptr<ShaderStorageBuffer>> m_particleSSBOs;
    CaribbeanParticle defaultParticle;
    CaribbeanParticle m_particleBuffer[PARTICLE_WORKGROUP_SIZE];

    flecs::world& m_world;
    EditorModule* m_editorModule = nullptr;
    PhysicsModule* m_physicsModule = nullptr;
    RendererModule* m_rendererModule = nullptr;

    flecs::system m_computeParticlesSystem;
    flecs::observer m_caribbeanAddObserver;
    flecs::observer m_caribbeanUpdateObserver;
    flecs::observer m_caribbeanRemoveObserver;
    flecs::query<CaribbeanComponent> m_caribbeanQuery;

    class ComputeShader* m_cachedParticleComputeShader = nullptr;
    class ShaderProgram* m_cachedParticleRenderShader = nullptr;

private:
    void SetupRandomGenSSBO();
    void SetupParticleBuffer();
    void SetupShaders();
    void SetupComputeShaders();

    void SetupComponents();
    void SetupSystems();
    void SetupObservers();
    void SetupQueries();

    void RegisterWithEditor();

    void AdjustComponentData(flecs::entity entity, CaribbeanComponent& caribbean);
    void UpdateUBOData(flecs::entity entity, const CaribbeanComponent& caribbean);
    void SpawnParticles(flecs::entity entity, const CaribbeanComponent& caribbean, int spawnCount, glm::vec3 spawnOffset = glm::vec3(0.f));

    void CreateCaribbeanSystem(flecs::entity entity, const CaribbeanComponent& caribbean);
    void RemoveCaribbeanSystem(flecs::entity entity);

    void DrawCaribbeanInspector(flecs::entity entity, CaribbeanComponent& component);
};