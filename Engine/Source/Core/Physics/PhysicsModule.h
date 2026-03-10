#pragma once
#include "JoltPhysics.h"
#include "Core/Physics/PhysicsCasting.h"

#include <ThirdParty/flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <thread>
#include <functional>
#include <unordered_map>

#include "Renderer/PhysicsDebugRenderer.h"
#include "SensorModule.h"
#include "imgui.h"

struct TransformComponent;
class TranformModule;
class EditorModule;
class CameraModule;

enum class ShapeType
{
    Sphere,
    Box,
    Triangle,
    Capsule,
    TaperedCapsule,
    Cylinder,
    ConvexHull,
    StaticCompound,
    MutableCompound,
    RotatedTranslated,
    Scaled,
    OffsetCenterOfMass,
    Mesh,
    HeightField,
    SoftBody,
    User1,
    User2,
    User3,
    User4,
    User5,
    User6,
    User7,
    User8,
    UserConvex1,
    UserConvex2,
    UserConvex3,
    UserConvex4,
    UserConvex5,
    UserConvex6,
    UserConvex7,
    UserConvex8,
    Plane,
    TaperedCylinder,
    Empty
};

enum class MotionType
{
    STATIC,
    KINEMATIC,
    DYNAMIC
};

enum class CollisionLayer
{
    NON_MOVING,
    MOVING,
    NO_COLLISION,
    KINEMATIC_SENSOR,
    ROPE,
    ROPE_EVEN,
    CHARACTER,
    PROJECTILE,
    PROJECTILE_ONLY,
    NUM_LAYERS
};

struct PhysicsBodyComponent
{
    //Serialised
    MotionType motionType = MotionType::STATIC;
    CollisionLayer collisionLayer = CollisionLayer::NON_MOVING;
    float mass = 1.0f;
    float restitution = 0.2f;
    float friction = 0.5f;
    float gravityFactor = 1.0f;

    // Non-Serialised
    uint32_t bodyIdandSequence = UINT32_MAX;
};

struct SubShapeData
{
    ShapeType shapeType = ShapeType::Box;
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 dimensions = glm::vec3(1.0f);
    std::string meshAssetHandle = "";
    uint32_t userData = 0;
};

struct ColliderComponent
{
    ShapeType shapeSubType = ShapeType::Box;
    glm::vec3 dimensions = glm::vec3(1.0f);
    glm::vec3 offset = glm::vec3(0.0f);
    bool isSensor = false;
    std::string meshAssetHandle = "";
    bool syncWithTransform = false;

    std::vector<SubShapeData> subShapes;

    std::function<void(const SensorData&, flecs::entity)> onContactAdded = nullptr;
    std::function<void(const SensorData&, flecs::entity)> onContactPersisted = nullptr;
    std::function<void(const SensorData&, flecs::entity)> onContactRemoved = nullptr;
};

struct PhysicsStats
{
    float lastStepTime = 0.0f;
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);

    int totalBodies = 0;
    int activeBodies = 0;
    int staticBodies = 0;
    int kinematicBodies = 0;
    int dynamicBodies = 0;
    int sleepingBodies = 0;

    int constraints = 0;

    size_t bodyMemoryUsage = 0;
    size_t shapeMemoryUsage = 0;

    int simulationSteps = 1;
    float timeStep = 1.0f / 60.0f;
};

struct DebugRenderSettings
{
    bool enabled = false;

    bool drawBodies = true;
    bool drawShapes = true;
    bool drawShapeWireframe = true;
    bool drawBoundingBoxes = false;
    bool drawShapeColour = true;

    bool drawVelocity = false;
    bool drawMassAndInertia = false;
    bool drawSleepStats = false;
    bool drawCenterOfMassTransform = false;
    bool drawWorldTransform = false;

    bool drawGetSupportFunction = false;
    bool drawGetSupportingFace = false;

    bool drawConstraints = true;

    int shapeColourMode = 0; 

};

struct OnPhysicsUpdateTransform {};

class PhysicsModule
{
public:
    PhysicsModule(flecs::world& world);
    ~PhysicsModule();

    PhysicsModule(const PhysicsModule&) = delete;
    PhysicsModule& operator=(const PhysicsModule&) = delete;
    PhysicsModule(PhysicsModule&&) = delete;
    PhysicsModule& operator=(PhysicsModule&&) = delete;

    //void Update(float deltaTime) {};

    void DrawPhysicsStatsWindow();
    void DrawDebugSettingsWindow();
    void OnImGuiRender();

    void EnablePhysics(bool enable = true);
    void DisablePhysics() { EnablePhysics(false); }
    void ResolveUnboundEntities();

    JPH::ShapeSettings::ShapeResult ConstructJoltShape(flecs::entity, const ColliderComponent& collider, const TransformComponent& transform);

    void CreateBody(flecs::entity entity, TransformComponent& transform, ColliderComponent& collider, PhysicsBodyComponent& physics);
    void RemoveBody(flecs::entity entity);

    void UpdateJoltBody(flecs::entity entity);
    void UpdateJoltBodySettings(flecs::entity entity, const ColliderComponent& collider, PhysicsBodyComponent& physics);
    void UpdateJoltCollider(flecs::entity entity, const ColliderComponent& collider, const TransformComponent& transform);

    void UpdateMass(flecs::entity entity, const PhysicsBodyComponent& physics);
    void UpdateIsSensor(flecs::entity entity, const ColliderComponent& collider, const PhysicsBodyComponent& physics);

    void ClearAllBodies();

    void SetGravity(const glm::vec3& gravity);
    glm::vec3 GetGravity();

    // Body function wrappers
    void SetRestitution(flecs::entity entity, float value);
    void SetFriction(flecs::entity entity, float value);
    void SetGravityFactor(flecs::entity entity, float value);
    float GetRestitution(flecs::entity entity);
    float GetFriction(flecs::entity entity);
    float GetGravityFactor(flecs::entity entity);

    void AddForce(flecs::entity entity, const glm::vec3& force);
    void AddImpulse(flecs::entity entity, const glm::vec3& impulse);
    void AddTorque(flecs::entity entity, const glm::vec3& torque);
    void AddAngularImpulse(flecs::entity entity, const glm::vec3& angularImpulse);
    void AddForceAtPosition(flecs::entity entity, const glm::vec3& force, const glm::vec3& position);
    void AddImpulseAtPosition(flecs::entity entity, const glm::vec3& impulse, const glm::vec3& position);

    void SetLinearVelocity(flecs::entity entity, const glm::vec3& velocity);
    void SetAngularVelocity(flecs::entity entity, const glm::vec3& angularVelocity);
    glm::vec3 GetLinearVelocity(flecs::entity entity);
    glm::vec3 GetAngularVelocity(flecs::entity entity);
    void AddLinearVelocity(flecs::entity entity, const glm::vec3& velocity);
    void AddAngularVelocity(flecs::entity entity, const glm::vec3& angularVelocity);

    void SetBodyPosition(flecs::entity entity, const glm::vec3& position);
    void SetBodyRotation(flecs::entity entity, const glm::quat& rotation);
    void SetBodyPositionAndRotation(flecs::entity entity, const glm::vec3& position, const glm::quat& rotation);

    glm::vec3 GetBodyPosition(flecs::entity entity);
    glm::quat GetBodyRotation(flecs::entity entity);
    glm::vec3 GetCenterOfMassPosition(flecs::entity entity);

    void SetBodyMass(flecs::entity entity, float mass);
    float GetBodyMass(flecs::entity entity);
    void SetInverseMass(flecs::entity entity, float inverseMass);
    float GetInverseMass(flecs::entity entity);

    void ActivateBody(flecs::entity entity);
    void DeactivateBody(flecs::entity entity);
    bool IsBodyActive(flecs::entity entity);
    bool IsBodyAdded(flecs::entity entity);

    void SetBodyMotionType(flecs::entity entity, MotionType motionType);
    MotionType GetBodyMotionType(flecs::entity entity);

    void SetObjectLayer(flecs::entity entity, CollisionLayer layer);
    CollisionLayer GetObjectLayer(flecs::entity entity);

    // Casting wrappers
    RaycastHit Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance = 1000.0f);
    bool RaycastAny(const glm::vec3& origin, const glm::vec3& direction, float maxDistance = 1000.0f);
    std::vector<RaycastHit> RaycastAll(const glm::vec3& origin, const glm::vec3& direction, float maxDistance = 1000.0f);
    RaycastHit RaycastWithLayerFilter(const glm::vec3& origin, const glm::vec3& direction,
        float maxDistance, const std::vector<JPH::ObjectLayer>& ignoreLayers);
    std::vector<RaycastHit> RaycastAllWithLayerFilter(const glm::vec3& origin, const glm::vec3& direction,
        float maxDistance, const std::vector<JPH::ObjectLayer>& ignoreLayers);

    bool IsPointInside(const glm::vec3& point);
    std::vector<flecs::entity_t> GetEntitiesContainingPoint(const glm::vec3& point);

    ShapecastHit ShapeCast(const JPH::Shape* shape, const glm::vec3& origin, const glm::quat& orientation,
            const glm::vec3& direction, float maxDistance);
    bool ShapeCastAny(const JPH::Shape* shape, const glm::vec3& origin, const glm::quat& orientation,
            const glm::vec3& direction, float maxDistance);
    ShapecastHit SweptShapeCast(const JPH::Shape* shape, const glm::vec3& startPos, const glm::vec3& endPos,
            const glm::quat& orientation);
    ShapecastHit ShapeCastWithLayerFilter(const JPH::Shape* shape, const glm::vec3& origin,
        const glm::quat& orientation, const glm::vec3& direction, float maxDistance,
        const std::vector<JPH::ObjectLayer>& ignoreLayers);

    ShapecastHit SphereCast(float radius, const glm::vec3& origin, const glm::vec3& direction, float maxDistance);
    bool SphereCastAny(float radius, const glm::vec3& origin, const glm::vec3& direction, float maxDistance);
    ShapecastHit SweptSphereCast(float radius, const glm::vec3& startPos, const glm::vec3& endPos);
    ShapecastHit SphereCastWithLayerFilter(float radius, const glm::vec3& origin,
        const glm::vec3& direction, float maxDistance,
        const std::vector<JPH::ObjectLayer>& ignoreLayers);

    ShapecastHit BoxCast(const glm::vec3& halfExtents, const glm::vec3& origin, const glm::quat& orientation,
        const glm::vec3& direction, float maxDistance);
    bool BoxCastAny(const glm::vec3& halfExtents, const glm::vec3& origin, const glm::quat& orientation,
        const glm::vec3& direction, float maxDistance);
    ShapecastHit SweptBoxCast(const glm::vec3& halfExtents, const glm::vec3& startPos, const glm::vec3& endPos,
        const glm::quat& orientation);
    ShapecastHit BoxCastWithLayerFilter(const glm::vec3& halfExtents, const glm::vec3& origin,
        const glm::quat& orientation, const glm::vec3& direction, float maxDistance,
        const std::vector<JPH::ObjectLayer>& ignoreLayers);

    std::vector<flecs::entity_t> OverlapSphere(const glm::vec3& center, float radius);
    bool OverlapSphereAny(const glm::vec3& center, float radius);
    std::vector<flecs::entity_t> OverlapBox(const glm::vec3& center, const glm::vec3& halfExtents,
        const glm::quat& orientation = glm::quat(1, 0, 0, 0));
    bool OverlapBoxAny(const glm::vec3& center, const glm::vec3& halfExtents,
        const glm::quat& orientation = glm::quat(1, 0, 0, 0));

    bool IsGrounded(flecs::entity entity, float checkDistance = 0.1f, float sphereRadius = 0.1f);
    RaycastHit GetGroundInfo(flecs::entity entity, float checkDistance = 2.0f);

    std::vector<flecs::entity_t> GetNearbyEntities(flecs::entity entity, float radius);
    bool IsEntityNearby(flecs::entity entity, flecs::entity_t targetEntityId, float radius);

    //Constraint module move soon
    JPH::Ref<JPH::DistanceConstraint> CreateDistanceConstraint(JPH::Body* bodyA, JPH::Body* bodyB, float distance);
    JPH::Ref<JPH::DistanceConstraint> CreatePinConstraint(JPH::Body* bodyA, JPH::Body* bodyB);
    JPH::Ref<JPH::DistanceConstraint> CreateSpringConstraint(JPH::Body* bodyA, JPH::Body* bodyB, float distance, float stiffness = 10.f, float damping = 0.f);

    void InitialiseDebugRenderer();
    void RenderDebug();
    void SetDebugDrawEnabled(bool enabled) { m_debugSettings.enabled = enabled; }
    bool IsDebugDrawEnabled() const { return m_debugSettings.enabled; }

    void DrawLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& colour);
    void DrawArrow(const glm::vec3& from, const glm::vec3& to, const glm::vec4& colour, float size = 0.1f);
    void DrawMarker(const glm::vec3& position, const glm::vec4& colour, float size = 0.1f);

    void DrawWireSphere(const glm::vec3& center, float radius, const glm::vec4& colour, int level = 3);
    void DrawSphere(const glm::vec3& center, float radius, const glm::vec4& colour, bool castShadow = false, bool wireframe = false);

    void DrawWireBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation, const glm::vec4& colour);
    void DrawBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation, const glm::vec4& colour, bool castShadow = false, bool wireframe = false);

    void DrawWireTriangle(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec4& colour);
    void DrawTriangle(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec4& colour, bool castShadow = false);

    void DrawCapsule(const glm::vec3& base, const glm::quat& rotation, float halfHeight, float radius, const glm::vec4& colour, bool castShadow = false, bool wireframe = false);
    void DrawCylinder(const glm::vec3& base, const glm::quat& rotation, float halfHeight, float radius, const glm::vec4& colour, bool castShadow = false, bool wireframe = false);

    void DrawCoordinateSystem(const glm::vec3& position, const glm::quat& rotation, float size = 1.0f);
    void DrawPlane(const glm::vec3& point, const glm::vec3& normal, const glm::vec4& colour, float size = 10.0f);


    JPH::PhysicsSystem* GetPhysicsSystem() { return m_physicsSystem.get(); }
    JPH::BodyInterface& GetBodyInterface() { return m_physicsSystem->GetBodyInterface(); }
    PhysicsDebugRenderer* GetDebugRenderer() { return m_debugRenderer.get(); }
    JPH::BodyID GetBodyID(flecs::entity_t entityId);
    flecs::entity BodyToEntity(const JPH::Body& body);
    flecs::entity_t BodyIDToEntityID(JPH::BodyID bodyID);

    static JPH::Color ToJoltColour(const glm::vec4& colour);
    static glm::vec3 ToGLM(const JPH::Vec3& vec);
    static glm::quat ToGLMQuat(const JPH::Quat& quat);
    static JPH::Vec3 ToJolt(const glm::vec3& vec);
    static JPH::Quat ToJoltQuat(const glm::quat& quat);
    static JPH::EShapeSubType ToJoltShape(ShapeType shape);
    static ShapeType FromJoltShape(JPH::EShapeSubType joltShape);

    void RegisterWithEditor();

private:
    void InitialisePhysics();

    void SetupEvents();
    void SetupComponents();
    void SetupSystems();
    void SetupObservers();

    void DrawPhysicsBodyInspector(flecs::entity entity, PhysicsBodyComponent& component);
    void DrawColliderInspector(flecs::entity entity, ColliderComponent& component);

    void SyncColliderWithTransform(flecs::entity entity, ColliderComponent& collider, const TransformComponent& transform);

    void UpdatePhysicsStats();
    void SyncTransformsFromPhysics();

    ImVec4 GetPhysicsEntityColour(flecs::entity entity);

private:
    flecs::world& m_world;
    EditorModule* m_editorModule = nullptr;
    CameraModule* m_cameraModule = nullptr;
    SensorModule* m_sensorModule = nullptr;

    flecs::query<PhysicsBodyComponent> m_cleanupQuery;
    flecs::query<PhysicsBodyComponent, TransformComponent> m_syncFromPhysicsQuery;
    flecs::system m_updateSystem;
    flecs::system m_resolveUnboundEntitiesSystem;

    flecs::observer m_bodyAddObserver;
    flecs::observer m_bodyRemoveObserver;
    flecs::observer m_transformUpdateObserver;
    flecs::observer m_colliderUpdateObserver;
    flecs::observer m_bodyUpdateObserver;

    std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;
    std::unique_ptr<JPH::TempAllocator> m_tempAllocator;
    std::unique_ptr<JPH::JobSystem> m_jobSystem;
    std::unique_ptr<JPH::BroadPhaseLayerInterface> m_broadPhaseInterface;
    std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> m_objectVsBPLayerFilter;
    std::unique_ptr<JPH::ObjectLayerPairFilter> m_objectLayerPairFilter;

    std::unique_ptr<PhysicsDebugRenderer> m_debugRenderer;
    std::unique_ptr<PhysicsCasting> m_physicsCasting;

    std::unordered_map<flecs::entity_t, JPH::BodyID> m_bodies;

    DebugRenderSettings m_debugSettings;

    float m_fixedDeltaTime = 1.0f / 60.0f;
    float m_accumulator = 0.0f;

    PhysicsStats m_stats;

    bool m_showPhysicsWindow = true;
    bool m_showStatsWindow = true;
    bool m_showDebugWindow = true;
};

static const char* ShapeTypeNames[] = {
    "Sphere",
    "Box",
    "Triangle",
    "Capsule",
    "Tapered Capsule",
    "Cylinder",
    "Convex Hull",
    "Static Compound",
    "Mutable Compound",
    "Rotated Translated",
    "Scaled",
    "Offset Center of Mass",
    "Mesh",
    "Height Field",
    "Soft Body",
    "User Shape 1",
    "User Shape 2",
    "User Shape 3",
    "User Shape 4",
    "User Shape 5",
    "User Shape 6",
    "User Shape 7",
    "User Shape 8",
    "User Convex Shape 1",
    "User Convex Shape 2",
    "User Convex Shape 3",
    "User Convex Shape 4",
    "User Convex Shape 5",
    "User Convex Shape 6",
    "User Convex Shape 7",
    "User Convex Shape 8",
    "Plane",
    "Tapered Cylinder",
    "Empty Shape"
};

static const JPH::EShapeSubType ShapeTypes[] = {
    JPH::EShapeSubType::Sphere,
    JPH::EShapeSubType::Box,
    JPH::EShapeSubType::Triangle,
    JPH::EShapeSubType::Capsule,
    JPH::EShapeSubType::TaperedCapsule,
    JPH::EShapeSubType::Cylinder,
    JPH::EShapeSubType::ConvexHull,
    JPH::EShapeSubType::StaticCompound,
    JPH::EShapeSubType::MutableCompound,
    JPH::EShapeSubType::RotatedTranslated,
    JPH::EShapeSubType::Scaled,
    JPH::EShapeSubType::OffsetCenterOfMass,
    JPH::EShapeSubType::Mesh,
    JPH::EShapeSubType::HeightField,
    JPH::EShapeSubType::SoftBody,
    JPH::EShapeSubType::User1,
    JPH::EShapeSubType::User2,
    JPH::EShapeSubType::User3,
    JPH::EShapeSubType::User4,
    JPH::EShapeSubType::User5,
    JPH::EShapeSubType::User6,
    JPH::EShapeSubType::User7,
    JPH::EShapeSubType::User8,
    JPH::EShapeSubType::UserConvex1,
    JPH::EShapeSubType::UserConvex2,
    JPH::EShapeSubType::UserConvex3,
    JPH::EShapeSubType::UserConvex4,
    JPH::EShapeSubType::UserConvex5,
    JPH::EShapeSubType::UserConvex6,
    JPH::EShapeSubType::UserConvex7,
    JPH::EShapeSubType::UserConvex8,
    JPH::EShapeSubType::Plane,
    JPH::EShapeSubType::TaperedCylinder,
    JPH::EShapeSubType::Empty,
};