#include "cepch.h"
#include "PhysicsCasting.h"
#include "PhysicsModule.h"
#include "Core/Character/CharacterModule.h"

#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollidePointResult.h>
#include <algorithm>

PhysicsCasting::PhysicsCasting(flecs::world& world, PhysicsModule* physicsModule)
    : m_world(world), m_physicsModule(physicsModule)
{
    if (!m_physicsModule) {
        Logger::PrintLog("Error: PhysicsCasting requires a valid PhysicsModule!");
    }
}

PhysicsCasting::~PhysicsCasting()
{
}

RaycastHit PhysicsCasting::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
{
    JPH::RRayCast ray(ToJolt(origin), ToJolt(direction * maxDistance));
    JPH::ClosestHitCollisionCollector<JPH::CastRayCollector> collector;

    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings{}, collector);

    if (collector.HadHit())
    {
        return ConvertRaycastResult(collector.mHit, origin, direction, maxDistance);
    }

    return RaycastHit{};
}

std::vector<RaycastHit> PhysicsCasting::RaycastAll(const glm::vec3& origin, const glm::vec3& direction,
    float maxDistance)
{
    JPH::RRayCast ray(ToJolt(origin), ToJolt(direction * maxDistance));
    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;

    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings{}, collector);

    std::vector<RaycastHit> hits;
    hits.reserve(collector.mHits.size());

    for (const auto& hit : collector.mHits)
    {
        hits.push_back(ConvertRaycastResult(hit, origin, direction, maxDistance));
    }

    std::sort(hits.begin(), hits.end(), [](const RaycastHit& a, const RaycastHit& b)
        {
            return a.distance < b.distance;
        });

    return hits;
}

RaycastHit PhysicsCasting::RaycastWithLayerFilter(const glm::vec3& origin, const glm::vec3& direction,
    float maxDistance, const std::vector<JPH::ObjectLayer>& ignoreLayers)
{
    JPH::RRayCast ray(ToJolt(origin), ToJolt(direction * maxDistance));
    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;

    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings{}, collector);

    if (!collector.HadHit())
    {
        return RaycastHit{};
    }

    RaycastHit closestValidHit;
    float closestFraction = FLT_MAX;

    for (const auto& hit : collector.mHits)
    {
        bool shouldIgnore = false;
        if (hit.mBodyID.IsInvalid() == false)
        {
            JPH::BodyLockRead lock(m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface(), hit.mBodyID);
            if (lock.Succeeded())
            {
                const JPH::Body& body = lock.GetBody();
                JPH::ObjectLayer layer = body.GetObjectLayer();

                for (JPH::ObjectLayer ignoreLayer : ignoreLayers)
                {
                    if (layer == ignoreLayer)
                    {
                        shouldIgnore = true;
                        break;
                    }
                }
            }
        }

        if (!shouldIgnore && hit.mFraction < closestFraction)
        {
            closestFraction = hit.mFraction;
            closestValidHit = ConvertRaycastResult(hit, origin, direction, maxDistance);
        }
    }

    return closestValidHit;
}

std::vector<RaycastHit> PhysicsCasting::RaycastAllWithLayerFilter(const glm::vec3& origin,
    const glm::vec3& direction, float maxDistance, const std::vector<JPH::ObjectLayer>& ignoreLayers)
{
    JPH::RRayCast ray(ToJolt(origin), ToJolt(direction * maxDistance));
    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;

    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings{}, collector);

    std::vector<RaycastHit> hits;

    for (const auto& hit : collector.mHits)
    {
        bool shouldIgnore = false;
        if (hit.mBodyID.IsInvalid() == false)
        {
            JPH::BodyLockRead lock(m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface(), hit.mBodyID);
            if (lock.Succeeded())
            {
                const JPH::Body& body = lock.GetBody();
                JPH::ObjectLayer layer = body.GetObjectLayer();

                for (JPH::ObjectLayer ignoreLayer : ignoreLayers)
                {
                    if (layer == ignoreLayer)
                    {
                        shouldIgnore = true;
                        break;
                    }
                }
            }
        }

        if (!shouldIgnore)
        {
            hits.push_back(ConvertRaycastResult(hit, origin, direction, maxDistance));
        }
    }

    std::sort(hits.begin(), hits.end(), [](const RaycastHit& a, const RaycastHit& b)
        {
            return a.distance < b.distance;
        });

    return hits;
}

bool PhysicsCasting::RaycastAny(const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
{
    JPH::RRayCast ray(ToJolt(origin), ToJolt(direction * maxDistance));
    JPH::AnyHitCollisionCollector<JPH::CastRayCollector> collector;

    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings{}, collector);
    return collector.HadHit();
}

bool PhysicsCasting::IsPointInside(const glm::vec3& point)
{
    JPH::AnyHitCollisionCollector<JPH::CollidePointCollector> collector;
    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CollidePoint(ToJolt(point), collector);
    return collector.HadHit();
}

std::vector<flecs::entity_t> PhysicsCasting::GetEntitiesContainingPoint(const glm::vec3& point)
{
    JPH::AllHitCollisionCollector<JPH::CollidePointCollector> collector;

    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CollidePoint(ToJolt(point), collector);

    std::vector<flecs::entity_t> entities;
    entities.reserve(collector.mHits.size());

    for (const auto& hit : collector.mHits)
    {
        flecs::entity_t entityId = m_physicsModule->BodyIDToEntityID(hit.mBodyID);
        if (entityId != 0)
        {
            entities.push_back(entityId);
        }
    }

    return entities;
}

ShapecastHit PhysicsCasting::ShapeCast(const JPH::Shape* shape, const glm::vec3& origin, const glm::quat& orientation,
    const glm::vec3& direction, float maxDistance)
{
    JPH::RShapeCast shapeCast(shape, JPH::Vec3::sReplicate(1.0f),
        JPH::RMat44::sRotationTranslation(ToJoltQuat(orientation), ToJolt(origin)),
        ToJolt(direction * maxDistance));

    JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
    JPH::ShapeCastSettings settings;

    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CastShape(
        shapeCast, settings, JPH::Vec3::sZero(), collector);

    if (collector.HadHit()) 
    {
        return ConvertShapecastResult(collector.mHit);
    }

    return ShapecastHit{};
}

bool PhysicsCasting::ShapeCastAny(const JPH::Shape* shape, const glm::vec3& origin, const glm::quat& orientation,
    const glm::vec3& direction, float maxDistance)
{
    JPH::RShapeCast shapeCast(shape, JPH::Vec3::sReplicate(1.0f),
        JPH::RMat44::sRotationTranslation(ToJoltQuat(orientation), ToJolt(origin)),
        ToJolt(direction * maxDistance));

    JPH::AnyHitCollisionCollector<JPH::CastShapeCollector> collector;
    JPH::ShapeCastSettings settings;

    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CastShape(
        shapeCast, settings, JPH::Vec3::sZero(), collector);

    return collector.HadHit();
}

ShapecastHit PhysicsCasting::SweptShapeCast(const JPH::Shape* shape, const glm::vec3& startPos,
    const glm::vec3& endPos, const glm::quat& orientation)
{
    glm::vec3 direction = endPos - startPos;
    float distance = glm::length(direction);

    if (distance < 1e-6f)
    {
        return ShapecastHit{};
    }

    direction /= distance;
    return ShapeCast(shape, startPos, orientation, direction, distance);
}

ShapecastHit PhysicsCasting::ShapeCastWithLayerFilter(const JPH::Shape* shape, const glm::vec3& origin,
    const glm::quat& orientation, const glm::vec3& direction, float maxDistance,
    const std::vector<JPH::ObjectLayer>& ignoreLayers)
{
    JPH::RShapeCast shapeCast(shape, JPH::Vec3::sReplicate(1.0f),
        JPH::RMat44::sRotationTranslation(ToJoltQuat(orientation), ToJolt(origin)),
        ToJolt(direction * maxDistance));

    JPH::AllHitCollisionCollector<JPH::CastShapeCollector> collector;
    JPH::ShapeCastSettings settings;

    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CastShape(
        shapeCast, settings, JPH::Vec3::sZero(), collector);

    if (!collector.HadHit())
    {
        return ShapecastHit{};
    }


    ShapecastHit closestValidHit;
    float closestFraction = FLT_MAX;

    for (const auto& hit : collector.mHits)
    {
        bool shouldIgnore = false;
        if (hit.mBodyID2.IsInvalid() == false)
        {
            JPH::BodyLockRead lock(m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface(), hit.mBodyID2);
            if (lock.Succeeded())
            {
                const JPH::Body& body = lock.GetBody();
                JPH::ObjectLayer layer = body.GetObjectLayer();

                for (JPH::ObjectLayer ignoreLayer : ignoreLayers)
                {
                    if (layer == ignoreLayer)
                    {
                        shouldIgnore = true;
                        break;
                    }
                }
            }
        }

        if (!shouldIgnore && hit.mFraction < closestFraction)
        {
            closestFraction = hit.mFraction;
            closestValidHit = ConvertShapecastResult(hit);
        }
    }

    return closestValidHit;
}

ShapecastHit PhysicsCasting::SphereCast(float radius, const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
{
    JPH::SphereShapeSettings sphereSettings(radius);
    JPH::ShapeSettings::ShapeResult sphereResult = sphereSettings.Create();

    if (sphereResult.HasError()) 
    {
        Logger::PrintLog("Error creating sphere shape for cast: %s", sphereResult.GetError().c_str());
        return ShapecastHit{};
    }

    return ShapeCast(sphereResult.Get(), origin, glm::quat(1, 0, 0, 0), direction, maxDistance);
}

bool PhysicsCasting::SphereCastAny(float radius, const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
{
    JPH::SphereShapeSettings sphereSettings(radius);
    JPH::ShapeSettings::ShapeResult sphereResult = sphereSettings.Create();

    if (sphereResult.HasError()) 
    {
        Logger::PrintLog("Error creating sphere shape for cast: %s", sphereResult.GetError().c_str());
        return false;
    }

    return ShapeCastAny(sphereResult.Get(), origin, glm::quat(1, 0, 0, 0), direction, maxDistance);
}

ShapecastHit PhysicsCasting::SweptSphereCast(float radius, const glm::vec3& startPos, const glm::vec3& endPos)
{
    JPH::SphereShapeSettings sphereSettings(radius);
    JPH::ShapeSettings::ShapeResult sphereResult = sphereSettings.Create();

    if (sphereResult.HasError()) 
    {
        Logger::PrintLog("Error creating sphere shape for swept cast: %s", sphereResult.GetError().c_str());
        return ShapecastHit{};
    }

    return SweptShapeCast(sphereResult.Get(), startPos, endPos, glm::quat(1, 0, 0, 0));
}

ShapecastHit PhysicsCasting::SphereCastWithLayerFilter(float radius, const glm::vec3& origin,
    const glm::vec3& direction, float maxDistance, const std::vector<JPH::ObjectLayer>& ignoreLayers)
{
    JPH::SphereShapeSettings sphereSettings(radius);
    JPH::ShapeSettings::ShapeResult sphereResult = sphereSettings.Create();

    if (sphereResult.HasError())
    {
        Logger::PrintLog("Error creating sphere shape for cast: %s", sphereResult.GetError().c_str());
        return ShapecastHit{};
    }

    return ShapeCastWithLayerFilter(sphereResult.Get(), origin, glm::quat(1, 0, 0, 0),
        direction, maxDistance, ignoreLayers);
}

ShapecastHit PhysicsCasting::BoxCast(const glm::vec3& halfExtents, const glm::vec3& origin,
    const glm::quat& orientation, const glm::vec3& direction, float maxDistance)
{
    JPH::BoxShapeSettings boxSettings(ToJolt(halfExtents));
    JPH::ShapeSettings::ShapeResult boxResult = boxSettings.Create();

    if (boxResult.HasError()) 
    {
        Logger::PrintLog("Error creating box shape for cast: %s", boxResult.GetError().c_str());
        return ShapecastHit{};
    }

    return ShapeCast(boxResult.Get(), origin, orientation, direction, maxDistance);
}

bool PhysicsCasting::BoxCastAny(const glm::vec3& halfExtents, const glm::vec3& origin,
    const glm::quat& orientation, const glm::vec3& direction, float maxDistance)
{
    JPH::BoxShapeSettings boxSettings(ToJolt(halfExtents));
    JPH::ShapeSettings::ShapeResult boxResult = boxSettings.Create();

    if (boxResult.HasError())
    {
        Logger::PrintLog("Error creating box shape for cast: %s", boxResult.GetError().c_str());
        return false;
    }

    return ShapeCastAny(boxResult.Get(), origin, orientation, direction, maxDistance);
}

ShapecastHit PhysicsCasting::SweptBoxCast(const glm::vec3& halfExtents, const glm::vec3& startPos,
    const glm::vec3& endPos, const glm::quat& orientation)
{
    JPH::BoxShapeSettings boxSettings(ToJolt(halfExtents));
    JPH::ShapeSettings::ShapeResult boxResult = boxSettings.Create();

    if (boxResult.HasError())
    {
        Logger::PrintLog("Error creating box shape for swept cast: %s", boxResult.GetError().c_str());
        return ShapecastHit{};
    }

    return SweptShapeCast(boxResult.Get(), startPos, endPos, orientation);
}

ShapecastHit PhysicsCasting::BoxCastWithLayerFilter(const glm::vec3& halfExtents, const glm::vec3& origin,
    const glm::quat& orientation, const glm::vec3& direction, float maxDistance,
    const std::vector<JPH::ObjectLayer>& ignoreLayers)
{
    JPH::BoxShapeSettings boxSettings(ToJolt(halfExtents));
    JPH::ShapeSettings::ShapeResult boxResult = boxSettings.Create();

    if (boxResult.HasError())
    {
        Logger::PrintLog("Error creating box shape for cast: %s", boxResult.GetError().c_str());
        return ShapecastHit{};
    }

    return ShapeCastWithLayerFilter(boxResult.Get(), origin, orientation,
        direction, maxDistance, ignoreLayers);
}

std::vector<flecs::entity_t> PhysicsCasting::OverlapSphere(const glm::vec3& center, float radius)
{
    JPH::SphereShapeSettings sphereSettings(radius);
    JPH::ShapeSettings::ShapeResult sphereResult = sphereSettings.Create();

    if (sphereResult.HasError())
    {
        Logger::PrintLog("Error creating sphere shape for overlap: %s", sphereResult.GetError().c_str());
        return {};
    }

    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;

    JPH::CollideShapeSettings settings;
    JPH::RMat44 transform = JPH::RMat44::sTranslation(ToJolt(center));

    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CollideShape(
        sphereResult.Get(), JPH::Vec3::sReplicate(1.0f), transform, settings,
        JPH::Vec3::sZero(), collector);

    std::vector<flecs::entity_t> entities;
    entities.reserve(collector.mHits.size());

    for (const auto& hit : collector.mHits)
    {
        flecs::entity_t entityId = m_physicsModule->BodyIDToEntityID(hit.mBodyID2);
        if (entityId != 0) 
        {
            entities.push_back(entityId);
        }
    }

    return entities;
}

std::vector<flecs::entity_t> PhysicsCasting::OverlapBox(const glm::vec3& center, const glm::vec3& halfExtents,
    const glm::quat& orientation)
{
    JPH::BoxShapeSettings boxSettings(ToJolt(halfExtents));
    JPH::ShapeSettings::ShapeResult boxResult = boxSettings.Create();

    if (boxResult.HasError()) 
    {
        Logger::PrintLog("Error creating box shape for overlap: %s", boxResult.GetError().c_str());
        return {};
    }

    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;

    JPH::CollideShapeSettings settings;
    JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(ToJoltQuat(orientation), ToJolt(center));

    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CollideShape(
        boxResult.Get(), JPH::Vec3::sReplicate(1.0f), transform, settings,
        JPH::Vec3::sZero(), collector);

    std::vector<flecs::entity_t> entities;
    entities.reserve(collector.mHits.size());

    for (const auto& hit : collector.mHits)
    {
        flecs::entity_t entityId = m_physicsModule->BodyIDToEntityID(hit.mBodyID2);
        if (entityId != 0) 
        {
            entities.push_back(entityId);
        }
    }

    return entities;
}

bool PhysicsCasting::OverlapSphereAny(const glm::vec3& center, float radius)
{
    JPH::SphereShapeSettings sphereSettings(radius);
    JPH::ShapeSettings::ShapeResult sphereResult = sphereSettings.Create();

    if (sphereResult.HasError()) {
        Logger::PrintLog("Error creating sphere shape for overlap: %s", sphereResult.GetError().c_str());
        return false;
    }

    JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;
    JPH::CollideShapeSettings settings;
    JPH::RMat44 transform = JPH::RMat44::sTranslation(ToJolt(center));

    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CollideShape(
        sphereResult.Get(), JPH::Vec3::sReplicate(1.0f), transform, settings,
        JPH::Vec3::sZero(), collector);

    return collector.HadHit();
}

bool PhysicsCasting::OverlapBoxAny(const glm::vec3& center, const glm::vec3& halfExtents,
    const glm::quat& orientation)
{
    JPH::BoxShapeSettings boxSettings(ToJolt(halfExtents));
    JPH::ShapeSettings::ShapeResult boxResult = boxSettings.Create();

    if (boxResult.HasError()) 
    {
        Logger::PrintLog("Error creating box shape for overlap: %s", boxResult.GetError().c_str());
        return false;
    }

    JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;
    JPH::CollideShapeSettings settings;
    JPH::RMat44 transform = JPH::RMat44::sRotationTranslation(ToJoltQuat(orientation), ToJolt(center));

    m_physicsModule->GetPhysicsSystem()->GetNarrowPhaseQuery().CollideShape(
        boxResult.Get(), JPH::Vec3::sReplicate(1.0f), transform, settings,
        JPH::Vec3::sZero(), collector);

    return collector.HadHit();
}

RaycastHit PhysicsCasting::ConvertRaycastResult(const JPH::RayCastResult& result,
    const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
{
    RaycastHit hit;
    hit.hasHit = true;
    hit.bodyId = result.mBodyID;
    hit.subShapeId = result.mSubShapeID2;
    hit.fraction = result.mFraction;

    hit.distance = result.mFraction * maxDistance;
    hit.point = origin + direction * (result.mFraction * maxDistance);

    JPH::BodyLockRead lock(m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded())
    {
        const JPH::Body& body = lock.GetBody();
        JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ToJolt(hit.point));
        hit.normal = ToGLM(normal);
        hit.hitEntity = m_physicsModule->BodyIDToEntityID(result.mBodyID);
    }

    return hit;
}

ShapecastHit PhysicsCasting::ConvertShapecastResult(const JPH::ShapeCastResult& result)
{
    ShapecastHit hit;
    hit.hasHit = true;
    hit.bodyId = result.mBodyID2;
    hit.subShapeId1 = result.mSubShapeID1;
    hit.subShapeId2 = result.mSubShapeID2;
    hit.fraction = result.mFraction;
    hit.contactPoint1 = ToGLM(result.mContactPointOn1);
    hit.contactPoint2 = ToGLM(result.mContactPointOn2);
    hit.penetrationAxis = ToGLM(result.mPenetrationAxis);
    hit.penetrationDepth = result.mPenetrationDepth;
    hit.hitEntity = m_physicsModule->BodyIDToEntityID(result.mBodyID2);

    return hit;
}

glm::vec3 PhysicsCasting::ToGLM(const JPH::Vec3& vec)
{
    return glm::vec3(vec.GetX(), vec.GetY(), vec.GetZ());
}

JPH::Vec3 PhysicsCasting::ToJolt(const glm::vec3& vec)
{
    return JPH::Vec3(vec.x, vec.y, vec.z);
}

JPH::Quat PhysicsCasting::ToJoltQuat(const glm::quat& quat)
{
    return JPH::Quat(quat.x, quat.y, quat.z, quat.w);
}