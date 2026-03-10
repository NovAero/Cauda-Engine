#pragma once
#include "JoltPhysics.h"
#include <ThirdParty/flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>

class PhysicsModule;

struct RaycastHit
{
    flecs::entity_t hitEntity = 0;
    glm::vec3 point = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f);
    float distance = 0.0f;
    float fraction = 0.0f;
    JPH::BodyID bodyId;
    JPH::SubShapeID subShapeId;
    bool hasHit = false;
};

struct ShapecastHit
{
    flecs::entity_t hitEntity = 0;
    glm::vec3 contactPoint1 = glm::vec3(0.0f);
    glm::vec3 contactPoint2 = glm::vec3(0.0f);
    glm::vec3 penetrationAxis = glm::vec3(0.0f);
    float penetrationDepth = 0.0f;
    float fraction = 0.0f;
    JPH::BodyID bodyId;
    JPH::SubShapeID subShapeId1;
    JPH::SubShapeID subShapeId2;
    bool hasHit = false;
};

enum class BackFaceMode
{
    IgnoreBackFaces,
    CollideWithBackFaces
};

class PhysicsCasting
{
public:
    PhysicsCasting(flecs::world& world, PhysicsModule* physicsModule);
    ~PhysicsCasting();

    PhysicsCasting(const PhysicsCasting&) = delete;
    PhysicsCasting& operator=(const PhysicsCasting&) = delete;
    PhysicsCasting(PhysicsCasting&&) = delete;
    PhysicsCasting& operator=(PhysicsCasting&&) = delete;

    RaycastHit Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance = 1000.0f);
    bool RaycastAny(const glm::vec3& origin, const glm::vec3& direction, float maxDistance = 1000.0f);
    std::vector<RaycastHit> RaycastAll(const glm::vec3& origin, const glm::vec3& direction,
        float maxDistance = 1000.0f);
    RaycastHit RaycastWithLayerFilter(const glm::vec3& origin, const glm::vec3& direction,
        float maxDistance, const std::vector<JPH::ObjectLayer>& ignoreLayers);
    std::vector<RaycastHit> RaycastAllWithLayerFilter(const glm::vec3& origin, const glm::vec3& direction,
        float maxDistance, const std::vector<JPH::ObjectLayer>& ignoreLayers);






    bool IsPointInside(const glm::vec3& point);
    std::vector<flecs::entity_t> GetEntitiesContainingPoint(const glm::vec3& point);

    ShapecastHit ShapeCast(const JPH::Shape* shape, const glm::vec3& origin, const glm::quat& orientation,
        const glm::vec3& direction, float maxDistance = 1000.0f);
    bool ShapeCastAny(const JPH::Shape* shape, const glm::vec3& origin, const glm::quat& orientation,
        const glm::vec3& direction, float maxDistance = 1000.0f);
    ShapecastHit SweptShapeCast(const JPH::Shape* shape, const glm::vec3& startPos, const glm::vec3& endPos,
        const glm::quat& orientation);
    ShapecastHit ShapeCastWithLayerFilter(const JPH::Shape* shape, const glm::vec3& origin,
        const glm::quat& orientation, const glm::vec3& direction, float maxDistance,
        const std::vector<JPH::ObjectLayer>& ignoreLayers);

    ShapecastHit SphereCast(float radius, const glm::vec3& origin, const glm::vec3& direction, float maxDistance = 1000.0f);
    bool SphereCastAny(float radius, const glm::vec3& origin, const glm::vec3& direction, float maxDistance = 1000.0f);
    ShapecastHit SweptSphereCast(float radius, const glm::vec3& startPos, const glm::vec3& endPos);
    ShapecastHit SphereCastWithLayerFilter(float radius, const glm::vec3& origin,
        const glm::vec3& direction, float maxDistance,
        const std::vector<JPH::ObjectLayer>& ignoreLayers);

    ShapecastHit BoxCast(const glm::vec3& halfExtents, const glm::vec3& origin, const glm::quat& orientation,
        const glm::vec3& direction, float maxDistance = 1000.0f);
    bool BoxCastAny(const glm::vec3& halfExtents, const glm::vec3& origin, const glm::quat& orientation,
        const glm::vec3& direction, float maxDistance = 1000.0f);
    ShapecastHit SweptBoxCast(const glm::vec3& halfExtents, const glm::vec3& startPos, const glm::vec3& endPos,
        const glm::quat& orientation);
    ShapecastHit BoxCastWithLayerFilter(const glm::vec3& halfExtents, const glm::vec3& origin,
        const glm::quat& orientation, const glm::vec3& direction, float maxDistance,
        const std::vector<JPH::ObjectLayer>& ignoreLayers);

    std::vector<flecs::entity_t> OverlapSphere(const glm::vec3& center, float radius);
    std::vector<flecs::entity_t> OverlapBox(const glm::vec3& center, const glm::vec3& halfExtents,
        const glm::quat& orientation = glm::quat(1, 0, 0, 0));

    bool OverlapSphereAny(const glm::vec3& center, float radius);
    bool OverlapBoxAny(const glm::vec3& center, const glm::vec3& halfExtents,
        const glm::quat& orientation = glm::quat(1, 0, 0, 0));


private:
    RaycastHit ConvertRaycastResult(const JPH::RayCastResult& result, const glm::vec3& origin, const glm::vec3& direction, float maxDistance);
    ShapecastHit ConvertShapecastResult(const JPH::ShapeCastResult& result);

    static glm::vec3 ToGLM(const JPH::Vec3& vec);
    static JPH::Vec3 ToJolt(const glm::vec3& vec);
    static JPH::Quat ToJoltQuat(const glm::quat& quat);

private:
    flecs::world& m_world;
    PhysicsModule* m_physicsModule = nullptr;
};