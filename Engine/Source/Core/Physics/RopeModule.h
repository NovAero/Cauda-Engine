#pragma once
#include <ThirdParty/flecs.h>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include "Core/Physics/PhysicsModule.h"
#include "Renderer/RopeMesh.h"
#include "imgui.h"

class EditorModule;
class PhysicsModule;
class CharacterModule;
class RendererModule;

struct RopeSegment
{
    flecs::entity_t owner;
    int index;
};

struct ConnectedAsB
{
    flecs::entity_t which;
};

struct RopeComponent
{
    std::string entityAName = "";
    std::string entityBName = "";
    int numSegments = 50;
    float length = 12.0f;
    float segmentMass = 2.0f;
    float stiffness = 0.f;
    bool useSkipConstraints = true;
    float skipConstraintStiffness = 0.7f;
    int basePriority = 1000;
    int priorityStep = 5;
    int forwardPriorityBoost = 600;
    int backwardPriorityBoost = 100;
    int velocityIterations = 0;
    int positionIterations = 25;
    float segmentRadius = 0.1f;
    float segmentHalfHeight = 0.2f;
    float segmentOverlap = 0.f;

    float gravityScale = 1.0f;
    float friction = 0.8f;
    float restitution = 0.1f;

    bool renderRope = true;
    float ropeThickness = 0.15f;
    glm::vec3 ropeColour = glm::vec3(0.6f, 0.4f, 0.2f);
    int segmentSubdivisions = 4;
    int radialSegments = 6;
    float uvTilingLength = 32.0f;
    float uvTilingRadial = 1.0f;
    bool generateEndCaps = true;
    ShapeType segmentType = ShapeType::Sphere;

    glm::vec3 entityAOffset = glm::vec3(0.0f);
    glm::vec3 entityBOffset = glm::vec3(0.0f);

    flecs::entity entityA = flecs::entity::null();
    flecs::entity entityB = flecs::entity::null();
    JPH::BodyID cachedBodyIdA = JPH::BodyID();
    JPH::BodyID cachedBodyIdB = JPH::BodyID();
    bool needsRecreation = false;

    std::string ropeMaterial = "rope_material";
};

class RopeModule
{
public:
    RopeModule(flecs::world& world);
    ~RopeModule();

    RopeModule(const RopeModule&) = delete;
    RopeModule& operator=(const RopeModule&) = delete;
    RopeModule(RopeModule&&) = delete;
    RopeModule& operator=(RopeModule&&) = delete;

    bool ConnectRope(flecs::entity ropeEntity, flecs::entity targetEntity, bool connectToA);
    void DisconnectRope(flecs::entity ropeEntity, bool disconnectA);

    //void Update(float deltaTime) {}

    void Render();
    void RenderShadows();//const glm::mat4& lightSpaceMatrix);
    void RenderBackfaces();//const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void RenderXray();

    float [[nodiscard]] QueryRopeTension(flecs::entity entity, int end = -1);

    std::vector<flecs::entity> GetSegmentsForRope(flecs::entity entity);

private:
    struct RopeConstraintData
    {
        std::vector<JPH::Ref<JPH::DistanceConstraint>> forwardConstraints;
        std::vector<JPH::Ref<JPH::DistanceConstraint>> backwardConstraints;
        std::vector<JPH::Ref<JPH::DistanceConstraint>> skipConstraints;
    };

    struct RopeVisualData
    {
        std::unique_ptr<RopeMesh> ropeMesh;
        glm::mat4 worldMatrix = glm::mat4(1.0f);
        bool needsUpdate = true;
    };

    flecs::world& m_world;
    EditorModule* m_editorModule = nullptr;
    PhysicsModule* m_physicsModule = nullptr;
    CharacterModule* m_characterModule = nullptr;
    RendererModule* m_rendererModule = nullptr;

    std::unordered_set<std::string> m_persistentEntityNames;
    std::unordered_map<flecs::entity_t, std::vector<JPH::BodyID>> m_ropeSegments;
    std::unordered_map<flecs::entity_t, RopeConstraintData> m_ropeConstraints;
    std::unordered_map<flecs::entity_t, RopeVisualData> m_ropeVisuals;

    flecs::system m_updateVisualsSystem;
    flecs::system m_resolveConnectedSystem;
    flecs::observer m_ropeAddObserver;
    flecs::observer m_ropeRemoveObserver;
    flecs::observer m_entityDeleteObserver;

    flecs::query<> m_connectableQuery;
    //flecs::query<RopeComponent> m_ropeUpdateQuery;

private:
    const char* GetPersistentEntityName(flecs::entity entity);
    void CleanupRopeData(flecs::entity_t entityId);

    void SetupComponents();
    void SetupSystems();
    void SetupObservers();
    void SetupQueries();
    void RegisterWithEditor();

    void CreateRope(flecs::entity entity);
    void DestroyRope(flecs::entity entity);

    void CreateRopeVisuals(flecs::entity entity);
    void DestroyRopeVisuals(flecs::entity entity);
    void UpdateRopeVisuals(flecs::entity entity);

    // For constraints between rope segments
    JPH::DistanceConstraint* CreateForwardConstraint(JPH::Body* bodyA, JPH::Body* bodyB,
        const RopeComponent& rope, int segmentIndex, float distance);
    JPH::DistanceConstraint* CreateBackwardConstraint(JPH::Body* bodyA, JPH::Body* bodyB,
        const RopeComponent& rope, int segmentIndex, float distance);
    JPH::DistanceConstraint* CreateSkipConstraint(JPH::Body* bodyA, JPH::Body* bodyB,
        const RopeComponent& rope, int segmentIndex, float distance);

    // For constraints between a rope segment and another body
    JPH::DistanceConstraint* CreateForwardConstraint(JPH::Body* bodyA, JPH::Body* bodyB,
        const RopeComponent& rope, int segmentIndex, float distance, int velocityIterations, int positionIterations);
    JPH::DistanceConstraint* CreateBackwardConstraint(JPH::Body* bodyA, JPH::Body* bodyB,
        const RopeComponent& rope, int segmentIndex, float distance, int velocityIterations, int positionIterations);

    void DrawRopeInspector(flecs::entity entity, RopeComponent& component);
    ImVec4 GetRopeEntityColour(flecs::entity entity);
    bool ValidateRopeEntity(flecs::entity entity, flecs::entity_t targetId, JPH::BodyID& outBodyId);
};