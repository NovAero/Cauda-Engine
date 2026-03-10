#include "cepch.h"
#include "RopeModule.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Physics/PhysicsModule.h"
#include "Core/Character/CharacterModule.h"
#include "Core/Physics/PhysicsUserData.h"
#include "Renderer/RendererModule.h"
#include "Renderer/RopeMesh.h"
#include "Core/Editor/ResourceLibrary.h"
#include "Core/Components/GrabComponent.h"
#include "Platform/Win32/Application.h"

#include <imgui.h>
#include <iostream>
#include <cstring>

namespace ObjectLayers
{
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer ROPE = 4;
    static constexpr JPH::ObjectLayer ROPE_EVEN = 5;
};

RopeModule::RopeModule(flecs::world& world)
    : m_world(world),
    m_editorModule(m_world.try_get_mut<EditorModule>()),
    m_physicsModule(m_world.try_get_mut<PhysicsModule>()),
    m_characterModule(m_world.try_get_mut<CharacterModule>()),
    m_rendererModule(m_world.try_get_mut<RendererModule>())
{
    SetupComponents();
    SetupSystems();
    SetupObservers();
    SetupQueries();

    RegisterWithEditor();
}

RopeModule::~RopeModule()
{
    if (m_ropeAddObserver && m_ropeAddObserver.is_alive())
        m_ropeAddObserver.destruct();
    if (m_ropeRemoveObserver && m_ropeRemoveObserver.is_alive())
        m_ropeRemoveObserver.destruct();
    if (m_entityDeleteObserver && m_entityDeleteObserver.is_alive())
        m_entityDeleteObserver.destruct();

    for (const auto& [entityId, constraintData] : m_ropeConstraints)
    {
        CleanupRopeData(entityId);
    }
    m_ropeConstraints.clear();
    m_ropeSegments.clear();
    m_ropeVisuals.clear();
}

void RopeModule::CleanupRopeData(flecs::entity_t entityId)
{
    auto constraintIt = m_ropeConstraints.find(entityId);
    if (constraintIt != m_ropeConstraints.end())
    {
        if (m_physicsModule && m_physicsModule->GetPhysicsSystem())
        {
            auto* system = m_physicsModule->GetPhysicsSystem();
            auto& data = constraintIt->second;

            auto BatchRemove = [&](std::vector<JPH::Ref<JPH::DistanceConstraint>>& constraints)
                {
                    for (int i = static_cast<int>(constraints.size()) - 1; i >= 0; --i)
                    {
                        if (constraints[i] != nullptr)
                        {
                            JPH::Constraint* constraint = constraints[i].GetPtr();
                            if (constraint && constraint->GetEnabled())
                            {
                                system->RemoveConstraint(constraint);
                            }
                        }
                    }
                    constraints.clear();
                };

            BatchRemove(data.forwardConstraints);
            BatchRemove(data.backwardConstraints);
            BatchRemove(data.skipConstraints);
        }
        m_ropeConstraints.erase(constraintIt);
    }

    auto segmentIt = m_ropeSegments.find(entityId);
    if (segmentIt != m_ropeSegments.end())
    {
        if (m_physicsModule)
        {
            //m_world.defer_suspend();
            auto& bodyInterface = m_physicsModule->GetBodyInterface();
            auto& lockInterface = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface();

            std::vector<JPH::BodyID> bodiesToDestroy;
            bodiesToDestroy.reserve(segmentIt->second.size());

            for (JPH::BodyID bodyId : segmentIt->second)
            {
                if (bodyId.IsInvalid()) continue;

                JPH::BodyLockRead lock(lockInterface, bodyId);
                if (lock.Succeeded())
                {
                    const JPH::Body& body = lock.GetBody();
                    flecs::entity segEntity = m_world.get_alive(body.GetUserData());
                    if (segEntity) segEntity.destruct();
                }

                if (bodyInterface.IsAdded(bodyId))
                {
                    bodiesToDestroy.push_back(bodyId);
                }
            }

            if (!bodiesToDestroy.empty())
            {
                bodyInterface.RemoveBodies(bodiesToDestroy.data(), (int)bodiesToDestroy.size());
                bodyInterface.DestroyBodies(bodiesToDestroy.data(), (int)bodiesToDestroy.size());
            }

            //m_world.defer_resume();
        }
        m_ropeSegments.erase(segmentIt);
    }

    auto visualIt = m_ropeVisuals.find(entityId);
    if (visualIt != m_ropeVisuals.end())
    {
        m_ropeVisuals.erase(visualIt);
    }
}

const char* RopeModule::GetPersistentEntityName(flecs::entity entity)
{
    if (!entity.is_alive()) return nullptr;

    std::string fullPath = entity.path().c_str();
    auto [it, inserted] = m_persistentEntityNames.insert(fullPath);
    return it->c_str();
}

void RopeModule::SetupComponents()
{
    m_world.component<RopeComponent>("RopeComponent")
        .member<std::string>("entityAName")
        .member<std::string>("entityBName")
        .member<int>("numSegments")
        .member<float>("length")
        .member<float>("segmentMass")
        .member<float>("stiffness")
        .member<bool>("useSkipConstraints")
        .member<float>("skipConstraintStiffness")
        .member<int>("basePriority")
        .member<int>("priorityStep")
        .member<int>("forwardPriorityBoost")
        .member<int>("backwardPriorityBoost")
        .member<int>("velocityIterations")
        .member<int>("positionIterations")
        .member<float>("segmentRadius")
        .member<float>("segmentHalfHeight")
        .member<float>("segmentOverlap")
        .member<float>("gravityScale")
        .member<float>("friction")
        .member<float>("restitution")
        .member<bool>("renderRope")
        .member<float>("ropeThickness")
        .member<glm::vec3>("ropeColour")
        .member<int>("segmentSubdivisions")
        .member<int>("radialSegments")
        .member<float>("uvTilingLength")
        .member<float>("uvTilingRadial")
        .member<bool>("generateEndCaps")
        .member<ShapeType>("segmentType")
        .member<glm::vec3>("entityAOffset")
        .member<glm::vec3>("entityBOffset");

    m_world.component<RopeSegment>();
    m_world.component<ConnectedAsB>();
}

void RopeModule::SetupSystems()
{
    m_updateVisualsSystem = m_world.system<RopeComponent>("UpdateVisuals")
        .kind(flecs::PreStore)
        .each([&](flecs::entity entity, RopeComponent& rope)
            {
                if (rope.renderRope) UpdateRopeVisuals(entity);
            });

    flecs::entity GameTick = Cauda::Application::GetGameTickSource();
    //   flecs::entity PhysicsPhase = Cauda::Application::GetPhysicsPhase();
    //   flecs::entity ResolvePhysicsPhase = Cauda::Application::ResolvePhysicsPhase();


    m_resolveConnectedSystem = m_world.system<RopeComponent>("ResolveConnectedEntities")
        .kind(flecs::PreUpdate)
        //.kind(flecs::OnValidate)
        .tick_source(GameTick)
        .each([this](flecs::entity e, RopeComponent& rope)
            {
                bool entitiesChanged = false;
                bool bodiesChanged = false;

                //Fallback for auto connect
                //if (rope.entityAName == "" && !rope.entityA)
                //{
                //    rope.entityAName = e.name();
                //    rope.entityA = e;
                //    entitiesChanged = true;
                //}

                if (rope.entityAName != "" && !rope.entityA)
                {
                    rope.entityA = e.world().lookup(rope.entityAName.c_str());
                    if (rope.entityA)
                    {
                        entitiesChanged = true;
                    }
                }

                if (rope.entityBName != "" && !rope.entityB)
                {
                    rope.entityB = e.world().lookup(rope.entityBName.c_str());
                    if (rope.entityB)
                    {
                        entitiesChanged = true;
                    }
                }

                if (rope.entityA || rope.entityB)
                {
                    JPH::BodyID currentBodyIdA, currentBodyIdB;
                    bool validA = ValidateRopeEntity(e, rope.entityA, currentBodyIdA);
                    bool validB = ValidateRopeEntity(e, rope.entityB, currentBodyIdB);

                    if (validA && currentBodyIdA != rope.cachedBodyIdA)
                    {
                        bodiesChanged = true;
                    }

                    if (validB && currentBodyIdB != rope.cachedBodyIdB)
                    {
                        bodiesChanged = true;
                    }

                    if (!rope.cachedBodyIdA.IsInvalid() && !validA)
                    {
                        bodiesChanged = true;
                    }

                    if (!rope.cachedBodyIdB.IsInvalid() && !validB)
                    {
                        bodiesChanged = true;
                    }
                }

                if (entitiesChanged || bodiesChanged || rope.needsRecreation)
                {
                    CreateRope(e);
                    rope.needsRecreation = false;
                }
            });
}

void RopeModule::SetupObservers()
{
    m_ropeAddObserver = m_world.observer<RopeComponent>()
        .event(flecs::OnSet)
        .each([this](flecs::entity entity, RopeComponent& rope)
            {
                this->CreateRope(entity);
            });

    m_ropeRemoveObserver = m_world.observer<RopeComponent>()
        .event(flecs::OnRemove)
        .each([this](flecs::entity entity, RopeComponent& rope)
            {
                this->DestroyRope(entity);

                m_world.each<RopeComponent>([this, entity](flecs::entity ropeEntity, RopeComponent& rope)
                    {
                        bool needsUpdate = false;
                        if (rope.entityA == entity)
                        {
                            rope.entityA = flecs::entity::null();
                            rope.entityAName = "";
                            rope.cachedBodyIdA = JPH::BodyID();
                            needsUpdate = true;
                        }
                        if (rope.entityB == entity)
                        {
                            rope.entityB = flecs::entity::null();
                            rope.entityBName = "";
                            rope.cachedBodyIdB = JPH::BodyID();
                            needsUpdate = true;
                        }
                        if (needsUpdate)
                        {
                            rope.needsRecreation = true;
                        }
                    });
            });

    //m_entityDeleteObserver = m_world.observer<RopeComponent>()
    //    .event(flecs::OnDelete)
    //    .each([this](flecs::entity entity, RopeComponent&)
    //        {
    //            CleanupRopeData(entity.id());

    //            m_world.each<RopeComponent>([this, entity](flecs::entity ropeEntity, RopeComponent& rope)
    //                {
    //                    bool needsUpdate = false;
    //                    if (rope.entityA == entity)
    //                    {
    //                        rope.entityA = flecs::entity::null();
    //                        rope.entityAName = "";
    //                        rope.cachedBodyIdA = JPH::BodyID();
    //                        needsUpdate = true;
    //                    }
    //                    if (rope.entityB == entity)
    //                    {
    //                        rope.entityB = flecs::entity::null();
    //                        rope.entityBName = "";
    //                        rope.cachedBodyIdB = JPH::BodyID();
    //                        needsUpdate = true;
    //                    }
    //                    if (needsUpdate)
    //                    {
    //                        rope.needsRecreation = true;
    //                    }
    //                });
    //        });

    m_world.observer<PhysicsBodyComponent>()
        .event(flecs::OnSet)
        .each([this](flecs::entity entity, PhysicsBodyComponent& body)
            {
                m_world.each<RopeComponent>([this, entity](flecs::entity ropeEntity, RopeComponent& rope)
                    {
                        if (rope.entityA == entity || rope.entityB == entity)
                        {
                            rope.needsRecreation = true;
                        }
                    });
            });

    m_world.observer<ConnectedAsB>("CleanupEntityBOnDestroy")
        .event(flecs::OnRemove)
        .event(flecs::OnDelete)
        .each([this](flecs::entity entity, ConnectedAsB& comp)
            {
                DisconnectRope(m_world.get_alive(comp.which), false);
                comp.which = 0;
            });
}

void RopeModule::SetupQueries()
{
    m_connectableQuery = m_world.query_builder<>()
        .with<PhysicsBodyComponent>().or_().with<CharacterComponent>()
        .without<RopeSegment>()
        .build();

    //m_ropeUpdateQuery = m_world.query_builder<RopeComponent>()
    //    .build();
}

void RopeModule::RegisterWithEditor()
{
    if (!m_editorModule) return;

    m_editorModule->RegisterComponent<RopeComponent>(
        "Rope",
        "Physics",
        [this](flecs::entity entity, RopeComponent& component)
        {
            DrawRopeInspector(entity, component);
        }
    );

    m_editorModule->RegisterEntityColour([this](flecs::entity entity) -> ImVec4
        {
            return GetRopeEntityColour(entity);
        });
}

//void RopeModule::Update(float deltaTime)
//{
//    m_ropeUpdateQuery.each([this](flecs::entity ropeEntity, RopeComponent& rope)
//        {
//            if (rope.renderRope)
//            {
//                UpdateRopeVisuals(ropeEntity);
//            }
//        });
//}

void RopeModule::Render()//const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
{
    if (!m_rendererModule) return;

    for (auto& [entityId, visualData] : m_ropeVisuals)
    {
        if (!visualData.ropeMesh) continue;

        auto ropeEntity = m_world.entity(entityId);
        if (!ropeEntity.is_alive()) continue;

        auto rope = ropeEntity.try_get<RopeComponent>();
        if (!rope || !rope->renderRope) continue;

        Material* material = ResourceLibrary::GetMaterial(rope->ropeMaterial);
        if (!material)
        {
            material = ResourceLibrary::GetMaterial("rope_material");
        }

        if (material)
        {
            //material->SetVec3("colourTint", rope->ropeColour);
            m_rendererModule->RenderMesh(
                visualData.ropeMesh.get(),
                material,
                glm::mat4(1.0f));
            //    viewMatrix,
            //    projectionMatrix
            //);
        }
    }
}

void RopeModule::RenderShadows()//const glm::mat4& lightSpaceMatrix)
{
    auto shadowShader = ResourceLibrary::GetShader("shadow_depth");
    if (!shadowShader || !m_rendererModule) return;

    shadowShader->Use();
    //shadowShader->SetMat4Uniform("lightSpaceMatrix", lightSpaceMatrix);
    shadowShader->SetBoolUniform("useInstancing", false);

    glCullFace(GL_FRONT);

    for (auto& [entityId, visualData] : m_ropeVisuals)
    {
        if (!visualData.ropeMesh) continue;

        auto ropeEntity = m_world.entity(entityId);
        if (!ropeEntity.is_alive()) continue;

        auto rope = ropeEntity.try_get<RopeComponent>();
        if (!rope || !rope->renderRope) continue;

        shadowShader->SetMat4Uniform("modelMat", visualData.worldMatrix);

        glBindVertexArray(visualData.ropeMesh->GetVAO());
        visualData.ropeMesh->Render();
        glBindVertexArray(0);
    }

    glCullFace(GL_BACK);
}

void RopeModule::RenderBackfaces()//const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
{
    auto shader = ResourceLibrary::GetShader("mesh_position");
    if (!shader) return;

    shader->Use();
    //shader->SetMat4Uniform("vpMat", projectionMatrix * viewMatrix);
    shader->SetBoolUniform("useInstancing", false);

    for (auto& [entityId, visualData] : m_ropeVisuals)
    {
        if (!visualData.ropeMesh) continue;

        auto ropeEntity = m_world.entity(entityId);
        if (!ropeEntity.is_alive()) continue;

        auto rope = ropeEntity.try_get<RopeComponent>();
        if (!rope || !rope->renderRope) continue;

        shader->SetMat4Uniform("modelMat", visualData.worldMatrix);

        glBindVertexArray(visualData.ropeMesh->GetVAO());
        visualData.ropeMesh->Render();
        glBindVertexArray(0);
    }
}

void RopeModule::RenderXray()
{
    auto xrayShader = ResourceLibrary::GetShader("xray");
    if (!xrayShader) return;

    xrayShader->Use();
    xrayShader->SetBoolUniform("useInstancing", false);
    xrayShader->SetBoolUniform("useSkinning", false);

    for (auto& [entityId, visualData] : m_ropeVisuals)
    {
        if (!visualData.ropeMesh) continue;

        auto ropeEntity = m_world.entity(entityId);
        if (!ropeEntity.is_alive()) continue;

        auto rope = ropeEntity.try_get<RopeComponent>();
        if (!rope || !rope->renderRope) continue;

        auto xray = ropeEntity.try_get<XrayComponent>();
        if (!xray || !xray->enabled) continue;

        xrayShader->SetMat4Uniform("modelMat", visualData.worldMatrix);
        xrayShader->SetVec3Uniform("xrayColour", xray->xrayColour);
        xrayShader->SetFloatUniform("xrayAlpha", xray->xrayAlpha);
        xrayShader->SetFloatUniform("rimPower", xray->rimPower);
        xrayShader->SetBoolUniform("useRimLighting", xray->useRimLighting);

        glBindVertexArray(visualData.ropeMesh->GetVAO());
        visualData.ropeMesh->Render();
        glBindVertexArray(0);
    }
}

float RopeModule::QueryRopeTension(flecs::entity entity, int end)
{
    if (!entity.has<RopeComponent>()) return 0.f;

    auto it = m_ropeConstraints.find(entity.id());
    if (it == m_ropeConstraints.end()) return 0.f;

    auto& data = it->second;

    float totalLambda = 0.f;
    int limit = (end == -1) ? (int)data.forwardConstraints.size() : std::min(end, (int)data.forwardConstraints.size());

    for (int i = 0; i < limit; ++i)
    {
        auto& constraintRef = data.forwardConstraints[i];

        if (constraintRef)
        {
            auto* distConstraint = static_cast<JPH::DistanceConstraint*>(constraintRef.GetPtr());
            if (distConstraint)
            {
                totalLambda += std::abs(distConstraint->GetTotalLambdaPosition());
            }
        }
    }

    if (limit == 0) return 0.f;
    return totalLambda / (float)limit;
}

std::vector<flecs::entity> RopeModule::GetSegmentsForRope(flecs::entity entity)
{
    if (!entity.is_alive()) return std::vector<flecs::entity>();
    if (!entity.has<RopeComponent>()) return std::vector<flecs::entity>();

    std::vector<flecs::entity> ropeSegments;

    flecs::entity_t entityID = entity.id();
    auto it = m_ropeSegments.find(entityID);
    if (it != m_ropeSegments.end())
    {
        for (auto id : it->second)
        {
            auto body = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface().TryGetBody(id);
            auto entity = m_world.get_alive(body->GetUserData());
            if (entity)
            {
                if (entity.has<RopeSegment>())
                    ropeSegments.push_back(entity);
            }
        }
    }
    return ropeSegments;
}

void RopeModule::CreateRope(flecs::entity entity)
{
    auto rope = entity.try_get_mut<RopeComponent>();
    if (!rope || rope->numSegments <= 0 || !m_physicsModule || entity.has(flecs::Prefab))
        return;

    DestroyRope(entity);

    flecs::entity_t entityId = entity.id();

    if (!rope->entityA)
    {
        rope->entityA = entity.has<PhysicsBodyComponent>() || entity.has<CharacterComponent>() ? entity : (entity.parent().has<PhysicsBodyComponent>() || entity.parent().has<CharacterComponent>() ? entity.parent() : flecs::entity::null());
        if (rope->entityA)
        {
            rope->entityAName = rope->entityA.name();
        }
    }
    if (rope->entityBName != "" && !rope->entityB)
    {
        rope->entityB = m_world.lookup(rope->entityBName.c_str());
    }

    JPH::BodyID bodyIdA, bodyIdB;
    bool validA = ValidateRopeEntity(entity, rope->entityA, bodyIdA);
    bool validB = ValidateRopeEntity(entity, rope->entityB, bodyIdB);

    if (!validA)
    {
        return;
    }

    rope->cachedBodyIdA = bodyIdA;
    rope->cachedBodyIdB = validB ? bodyIdB : JPH::BodyID();

    glm::vec3 startPos, endPos;

    startPos = PhysicsModule::ToGLM(m_physicsModule->GetBodyInterface().GetPosition(bodyIdA));

    if (validB)
    {
        endPos = PhysicsModule::ToGLM(m_physicsModule->GetBodyInterface().GetPosition(bodyIdB));
    }
    else
    {
        endPos = startPos;
    }

    glm::vec3 ropeVector = endPos - startPos;
    const float segmentLength = rope->segmentType == ShapeType::Capsule ? rope->segmentHalfHeight : rope->length / static_cast<float>(rope->numSegments);

    auto& segments = m_ropeSegments[entityId];
    segments.clear();

    for (int i = 1; i <= rope->numSegments; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(rope->numSegments + 1);
        glm::vec3 segmentPosition = startPos + t * ropeVector;

        ColliderComponent segmentCollider;
        JPH::Ref<JPH::Shape> shape;

        switch (rope->segmentType)
        {
        case ShapeType::Capsule:
        {
            segmentCollider = {
                .shapeSubType = ShapeType::Capsule,
                .dimensions = glm::vec3(rope->segmentRadius, rope->segmentRadius * 2, 0)
            };

            shape = new JPH::CapsuleShape(rope->segmentHalfHeight, rope->segmentRadius);
        }break;
        default:
        {
            segmentCollider = {
                .shapeSubType = ShapeType::Sphere,
                .dimensions = glm::vec3(rope->segmentRadius, 0, 0)
            };

            shape = new JPH::SphereShape(rope->segmentRadius);
        }break;
        }

        JPH::BodyCreationSettings segmentSettings(
            shape,
            PhysicsModule::ToJolt(segmentPosition),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            (rope->segmentType == ShapeType::Capsule && i % 2 == 0) ? ObjectLayers::ROPE_EVEN : ObjectLayers::ROPE);

        flecs::entity segmentEntity = m_world.entity()
            .set<RopeSegment>({ entity.id(), i });

        int threshold = glm::min(5, rope->numSegments - 2);
        if (i > threshold)
        {
            segmentEntity.add<Grabbable>();
        }

        segmentSettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        segmentSettings.mMassPropertiesOverride.mMass = rope->segmentMass;
        segmentSettings.mUserData = segmentEntity.id();
        segmentSettings.mAngularDamping = 1.0f;

        JPH::Body* currentBody = m_physicsModule->GetBodyInterface().CreateBody(segmentSettings);
        if (!currentBody)
        {
            segmentEntity.destruct();
            break;
        }

        m_physicsModule->GetBodyInterface().AddBody(currentBody->GetID(), JPH::EActivation::Activate);
        segments.push_back(currentBody->GetID());

        m_physicsModule->GetBodyInterface().SetFriction(currentBody->GetID(), rope->friction);
        m_physicsModule->GetBodyInterface().SetRestitution(currentBody->GetID(), rope->restitution);
        m_physicsModule->GetBodyInterface().SetGravityFactor(currentBody->GetID(), rope->gravityScale);

        PhysicsBodyComponent segmentBody = {
        .motionType = (MotionType)currentBody->GetMotionType(),
        .collisionLayer = (CollisionLayer)currentBody->GetObjectLayer(),
        .mass = rope->segmentMass,
        .restitution = currentBody->GetRestitution(),
        .friction = currentBody->GetFriction(),
        .bodyIdandSequence = currentBody->GetID().GetIndexAndSequenceNumber()
        };

        segmentEntity.set<PhysicsBodyComponent>(segmentBody);
        segmentEntity.set<ColliderComponent>(segmentCollider);
    }

    std::vector<JPH::Body*> allBodies;
    allBodies.push_back(m_physicsModule->GetPhysicsSystem()->GetBodyLockInterfaceNoLock().TryGetBody(bodyIdA));

    for (JPH::BodyID segmentId : segments)
    {
        allBodies.push_back(m_physicsModule->GetPhysicsSystem()->GetBodyLockInterfaceNoLock().TryGetBody(segmentId));
    }

    if (validB)
    {
        allBodies.push_back(m_physicsModule->GetPhysicsSystem()->GetBodyLockInterfaceNoLock().TryGetBody(bodyIdB));
    }

    RopeConstraintData& constraintData = m_ropeConstraints[entityId];
    constraintData.forwardConstraints.clear();
    constraintData.backwardConstraints.clear();
    constraintData.skipConstraints.clear();

    {
        if (!allBodies[0] || !allBodies[1]) return;

        JPH::Ref<JPH::DistanceConstraint> forwardConstraint = CreateForwardConstraint(
            allBodies[0], allBodies[1], *rope, 0, segmentLength, 24, 0
        );

        if (forwardConstraint)
        {
            m_physicsModule->GetPhysicsSystem()->AddConstraint(forwardConstraint);
            constraintData.forwardConstraints.push_back(forwardConstraint);
        }

        JPH::Ref<JPH::DistanceConstraint> backwardConstraint = CreateBackwardConstraint(
            allBodies[0], allBodies[1], *rope, 0, segmentLength, 4, 0
        );
        if (backwardConstraint)
        {
            m_physicsModule->GetPhysicsSystem()->AddConstraint(backwardConstraint);
            constraintData.backwardConstraints.push_back(backwardConstraint);
        }
    }

    {
        if (allBodies.size() < 2) return;
        int lastSegmentIndex = allBodies.size() - 2;

        if (!allBodies[lastSegmentIndex] || !allBodies[lastSegmentIndex + 1]) return;

        JPH::Ref<JPH::DistanceConstraint> forwardConstraint = CreateForwardConstraint(
            allBodies[lastSegmentIndex], allBodies[lastSegmentIndex + 1], *rope, lastSegmentIndex, segmentLength, 24, 0
        );

        if (forwardConstraint)
        {
            m_physicsModule->GetPhysicsSystem()->AddConstraint(forwardConstraint);
            constraintData.forwardConstraints.push_back(forwardConstraint);
        }

        JPH::Ref<JPH::DistanceConstraint> backwardConstraint = CreateBackwardConstraint(
            allBodies[lastSegmentIndex], allBodies[lastSegmentIndex + 1], *rope, lastSegmentIndex, segmentLength, 24, 0
        );
        if (backwardConstraint)
        {
            m_physicsModule->GetPhysicsSystem()->AddConstraint(backwardConstraint);
            constraintData.backwardConstraints.push_back(backwardConstraint);
        }
    }

    for (size_t i = 1; i < allBodies.size() - 2; ++i)
    {
        if (!allBodies[i] || !allBodies[i + 1]) continue;

        JPH::Ref<JPH::DistanceConstraint> forwardConstraint = CreateForwardConstraint(
            allBodies[i], allBodies[i + 1], *rope, i, segmentLength
        );
        if (forwardConstraint)
        {
            m_physicsModule->GetPhysicsSystem()->AddConstraint(forwardConstraint);
            constraintData.forwardConstraints.push_back(forwardConstraint);
        }

        JPH::Ref<JPH::DistanceConstraint> backwardConstraint = CreateBackwardConstraint(
            allBodies[i], allBodies[i + 1], *rope, i, segmentLength
        );
        if (backwardConstraint)
        {
            m_physicsModule->GetPhysicsSystem()->AddConstraint(backwardConstraint);
            constraintData.backwardConstraints.push_back(backwardConstraint);
        }
    }

    if (rope->useSkipConstraints)
    {
        for (size_t i = 0; i < allBodies.size() - 2; ++i)
        {
            if (!allBodies[i] || !allBodies[i + 2]) continue;

            JPH::Ref<JPH::DistanceConstraint> skipConstraint = CreateSkipConstraint(
                allBodies[i], allBodies[i + 2], *rope, i, (rope->segmentType == ShapeType::Capsule ? segmentLength * 4 : segmentLength * 2)
            );
            if (skipConstraint)
            {
                m_physicsModule->GetPhysicsSystem()->AddConstraint(skipConstraint);
                constraintData.skipConstraints.push_back(skipConstraint);
            }
        }
    }

    if (allBodies[0] && allBodies[allBodies.size() - 1])
    {
        JPH::DistanceConstraintSettings settings;

        JPH::Vec3 posA = allBodies[0]->GetPosition();
        JPH::Vec3 posB = allBodies[allBodies.size() - 1]->GetPosition();

        posA += allBodies[0]->GetRotation() * PhysicsModule::ToJolt(rope->entityAOffset);
        posB += allBodies[allBodies.size() - 1]->GetRotation() * PhysicsModule::ToJolt(rope->entityBOffset);

        settings.mPoint1 = posA;
        settings.mPoint2 = posB;
        settings.mMinDistance = 0.f;
        settings.mMaxDistance = rope->length * 1.5f;
        settings.mLimitsSpringSettings.mStiffness = 1.f;

        JPH::Ref<JPH::DistanceConstraint> ghostConstraint = new JPH::DistanceConstraint(*allBodies[0], *allBodies[allBodies.size() - 1], settings);

        m_physicsModule->GetPhysicsSystem()->AddConstraint(ghostConstraint);
        constraintData.forwardConstraints.push_back(ghostConstraint);
    }

    if (rope->renderRope)
    {
        CreateRopeVisuals(entity);
    }
}

void RopeModule::CreateRopeVisuals(flecs::entity entity)
{
    auto rope = entity.try_get<RopeComponent>();
    if (!rope) return;

    DestroyRopeVisuals(entity);

    flecs::entity_t entityId = entity.id();

    auto& visualData = m_ropeVisuals[entityId];

    int visualSegments = rope->numSegments * rope->segmentSubdivisions;

    visualData.ropeMesh = std::make_unique<RopeMesh>();
    visualData.ropeMesh->Initialise(visualSegments, rope->radialSegments);
    visualData.ropeMesh->SetUVTiling(rope->uvTilingLength, rope->uvTilingRadial);
    visualData.ropeMesh->SetGenerateCaps(rope->generateEndCaps);
    visualData.worldMatrix = glm::mat4(1.0f);
    visualData.needsUpdate = true;

    UpdateRopeVisuals(entity);
}

void RopeModule::UpdateRopeVisuals(flecs::entity entity)
{
    auto rope = entity.try_get<RopeComponent>();
    if (!rope || !rope->renderRope) return;

    flecs::entity_t entityId = entity.id();

    auto visualIt = m_ropeVisuals.find(entityId);
    if (visualIt == m_ropeVisuals.end() || !visualIt->second.ropeMesh)
    {
        CreateRopeVisuals(entity);
        return;
    }

    auto& visualData = visualIt->second;
    auto segmentIt = m_ropeSegments.find(entityId);
    if (segmentIt == m_ropeSegments.end()) return;

    auto& segments = segmentIt->second;

    std::vector<glm::vec3> controlPoints;

    if (rope->entityA.is_alive())
    {
        if (auto* transform = rope->entityA.try_get<TransformComponent>())
        {
            glm::vec3 basePos = Math::GetWorldPosition(rope->entityA);
            glm::quat rotation = Math::GetWorldRotation(rope->entityA);
            glm::vec3 rotatedOffset = rotation * rope->entityAOffset;
            controlPoints.push_back(basePos + rotatedOffset);
        }
        else if (!rope->cachedBodyIdA.IsInvalid() && m_physicsModule->GetBodyInterface().IsAdded(rope->cachedBodyIdA))
        {
            glm::vec3 basePos = PhysicsModule::ToGLM(m_physicsModule->GetBodyInterface().GetPosition(rope->cachedBodyIdA));
            JPH::Quat rotation = m_physicsModule->GetBodyInterface().GetRotation(rope->cachedBodyIdA);
            glm::vec3 worldOffset = PhysicsModule::ToGLM(rotation * PhysicsModule::ToJolt(rope->entityAOffset));
            controlPoints.push_back(basePos + worldOffset);
        }
    }

    for (const JPH::BodyID& bodyId : segments)
    {
        if (m_physicsModule->GetBodyInterface().IsAdded(bodyId))
        {
            controlPoints.push_back(PhysicsModule::ToGLM(m_physicsModule->GetBodyInterface().GetPosition(bodyId)));
        }
    }

    if (rope->entityB)
    {
        if (auto* transform = rope->entityB.try_get<TransformComponent>())
        {
            glm::vec3 basePos = Math::GetWorldPosition(rope->entityB);
            glm::quat rotation = Math::GetWorldRotation(rope->entityB);
            glm::vec3 rotatedOffset = rotation * rope->entityBOffset;
            controlPoints.push_back(basePos + rotatedOffset);
        }
        else if (!rope->cachedBodyIdB.IsInvalid() && m_physicsModule->GetBodyInterface().IsAdded(rope->cachedBodyIdB))
        {
            glm::vec3 basePos = PhysicsModule::ToGLM(m_physicsModule->GetBodyInterface().GetPosition(rope->cachedBodyIdB));
            JPH::Quat rotation = m_physicsModule->GetBodyInterface().GetRotation(rope->cachedBodyIdB);
            glm::vec3 worldOffset = PhysicsModule::ToGLM(rotation * PhysicsModule::ToJolt(rope->entityBOffset));
            controlPoints.push_back(basePos + worldOffset);
        }
    }

    if (controlPoints.size() >= 2)
    {
        float thickness = rope->segmentRadius * rope->ropeThickness;

        std::vector<float> thicknessValues;
        for (size_t i = 0; i < controlPoints.size(); ++i)
        {
            float t = static_cast<float>(i) / (controlPoints.size() - 1);
            float tapering = 1.0f - 0.2f * (1.0f - 4.0f * t * (1.0f - t));
            thicknessValues.push_back(thickness * tapering);
        }

        visualData.ropeMesh->UpdateFromControlPoints(controlPoints, thicknessValues, false);
        visualData.worldMatrix = glm::mat4(1.0f);
    }
}

void RopeModule::DestroyRopeVisuals(flecs::entity entity)
{
    flecs::entity_t entityId = entity.id();
    auto visualIt = m_ropeVisuals.find(entityId);
    if (visualIt != m_ropeVisuals.end())
    {
        m_ropeVisuals.erase(visualIt);
    }
}

void RopeModule::DestroyRope(flecs::entity entity)
{
    CleanupRopeData(entity.id());
}

JPH::DistanceConstraint* RopeModule::CreateForwardConstraint(JPH::Body* bodyA, JPH::Body* bodyB,
    const RopeComponent& rope,
    int segmentIndex,
    float distance)
{
    JPH::Vec3 posA, posB;

    switch (rope.segmentType)
    {
    case ShapeType::Capsule:
    {
        JPH::DistanceConstraintSettings settings;

        JPH::Vec3 aPin = bodyA->GetRotation() * JPH::Vec3(0, -1, 0);
        JPH::Vec3 bPin = bodyB->GetRotation() * JPH::Vec3(0, 1, 0);

        posA = bodyA->GetPosition() + (aPin * (distance - rope.segmentOverlap));
        posB = bodyB->GetPosition() + (bPin * distance);

        if (bodyA->GetID() == rope.cachedBodyIdA)
        {
            posA += bodyA->GetRotation() * PhysicsModule::ToJolt(rope.entityAOffset);
        }
        if (bodyB->GetID() == rope.cachedBodyIdB)
        {
            posB += bodyB->GetRotation() * PhysicsModule::ToJolt(rope.entityBOffset);
        }

        settings.mPoint1 = posA;
        settings.mPoint2 = posB;

        int totalSegments = rope.numSegments + (rope.entityA != 0 ? 1 : 0) + (rope.entityB != 0 ? 1 : 0) - 1;
        settings.mConstraintPriority = rope.basePriority + rope.forwardPriorityBoost +
            (totalSegments - segmentIndex) * rope.priorityStep;

        settings.mMaxDistance = 0.f;
        settings.mMinDistance = 0.f;

        settings.mNumVelocityStepsOverride = rope.velocityIterations;
        settings.mNumPositionStepsOverride = rope.positionIterations;

        return new JPH::DistanceConstraint(*bodyA, *bodyB, settings);
    }
    default:
    {
        JPH::DistanceConstraintSettings settings;

        JPH::Vec3 posA = bodyA->GetPosition();
        JPH::Vec3 posB = bodyB->GetPosition();
        JPH::Vec3 dirVec = posB - posA;

        if (dirVec.IsClose(JPH::Vec3::sZero(), 1e-6f))
        {
            dirVec = JPH::Vec3(0, -1, 0);
        }

        JPH::Vec3 direction = dirVec.Normalized();

        posA = posA + direction * (distance * 0.1f);
        posB = posB - direction * (distance * 0.1f);

        if (bodyA->GetID() == rope.cachedBodyIdA)
        {
            posA += bodyA->GetRotation() * PhysicsModule::ToJolt(rope.entityAOffset);
        }
        if (bodyB->GetID() == rope.cachedBodyIdB)
        {
            posB += bodyB->GetRotation() * PhysicsModule::ToJolt(rope.entityBOffset);
        }

        settings.mPoint1 = posA;
        settings.mPoint2 = posB;
        settings.mMaxDistance = distance;
        settings.mMinDistance = distance * 0.95f;

        int totalSegments = rope.numSegments + (rope.entityA != 0 ? 1 : 0) + (rope.entityB != 0 ? 1 : 0) - 1;
        settings.mConstraintPriority = rope.basePriority + rope.forwardPriorityBoost +
            (totalSegments - segmentIndex) * rope.priorityStep;

        settings.mNumVelocityStepsOverride = rope.velocityIterations;
        settings.mNumPositionStepsOverride = rope.positionIterations;

        return new JPH::DistanceConstraint(*bodyA, *bodyB, settings);
    }
    }
}

JPH::DistanceConstraint* RopeModule::CreateBackwardConstraint(JPH::Body* bodyA, JPH::Body* bodyB,
    const RopeComponent& rope,
    int segmentIndex,
    float distance)
{
    JPH::Vec3 posA, posB;

    switch (rope.segmentType)
    {
    case ShapeType::Capsule:
    {
        JPH::DistanceConstraintSettings settings;

        JPH::Vec3 aPin = bodyA->GetRotation() * JPH::Vec3(0, -1, 0);
        JPH::Vec3 bPin = bodyB->GetRotation() * JPH::Vec3(0, 1, 0);

        posA = bodyA->GetPosition() + (aPin * distance);
        posB = bodyB->GetPosition() + (bPin * (distance - rope.segmentOverlap));

        if (bodyA->GetID() == rope.cachedBodyIdA)
        {
            posA += bodyA->GetRotation() * PhysicsModule::ToJolt(rope.entityAOffset);
        }
        if (bodyB->GetID() == rope.cachedBodyIdB)
        {
            posB += bodyB->GetRotation() * PhysicsModule::ToJolt(rope.entityBOffset);
        }

        settings.mPoint1 = posA;
        settings.mPoint2 = posB;

        settings.mConstraintPriority = rope.basePriority + rope.backwardPriorityBoost +
            segmentIndex * rope.priorityStep;

        settings.mMaxDistance = 0.f;
        settings.mMinDistance = 0.f;

        settings.mNumVelocityStepsOverride = rope.velocityIterations;
        settings.mNumPositionStepsOverride = rope.positionIterations;

        return new JPH::DistanceConstraint(*bodyA, *bodyB, settings);
    }
    default:
    {
        JPH::DistanceConstraintSettings settings;

        JPH::Vec3 posA = bodyA->GetPosition();
        JPH::Vec3 posB = bodyB->GetPosition();
        JPH::Vec3 dirVec = posB - posA;

        if (dirVec.IsClose(JPH::Vec3::sZero(), 1e-6f))
        {
            dirVec = JPH::Vec3(0, -1, 0);
        }

        JPH::Vec3 direction = dirVec.Normalized();

        posA = posA - direction * (distance * 0.05f);
        posB = posB + direction * (distance * 0.05f);

        if (bodyA->GetID() == rope.cachedBodyIdA)
        {
            posA += bodyA->GetRotation() * PhysicsModule::ToJolt(rope.entityAOffset);
        }
        if (bodyB->GetID() == rope.cachedBodyIdB)
        {
            posB += bodyB->GetRotation() * PhysicsModule::ToJolt(rope.entityBOffset);
        }

        settings.mPoint1 = posA;
        settings.mPoint2 = posB;
        settings.mMaxDistance = distance;
        settings.mMinDistance = distance * 0.95f;

        settings.mConstraintPriority = rope.basePriority + rope.backwardPriorityBoost +
            segmentIndex * rope.priorityStep;

        settings.mNumVelocityStepsOverride = rope.velocityIterations;
        settings.mNumPositionStepsOverride = rope.positionIterations;

        return new JPH::DistanceConstraint(*bodyA, *bodyB, settings);
    }
    }
}

JPH::DistanceConstraint* RopeModule::CreateSkipConstraint(JPH::Body* bodyA, JPH::Body* bodyB,
    const RopeComponent& rope,
    int segmentIndex,
    float distance)
{
    JPH::DistanceConstraintSettings settings;

    JPH::Vec3 posA = bodyA->GetPosition();
    JPH::Vec3 posB = bodyB->GetPosition();

    if (bodyA->GetID() == rope.cachedBodyIdA)
    {
        posA += bodyA->GetRotation() * PhysicsModule::ToJolt(rope.entityAOffset);
    }
    if (bodyB->GetID() == rope.cachedBodyIdB)
    {
        posB += bodyB->GetRotation() * PhysicsModule::ToJolt(rope.entityBOffset);
    }

    settings.mPoint1 = posA;
    settings.mPoint2 = posB;
    settings.mMaxDistance = distance;
    settings.mMinDistance = distance * (0.8f + rope.skipConstraintStiffness * 0.2f);

    settings.mConstraintPriority = rope.basePriority - 200;
    settings.mNumVelocityStepsOverride = rope.velocityIterations * 0.7f;
    settings.mNumPositionStepsOverride = rope.positionIterations * 0.7f;

    return new JPH::DistanceConstraint(*bodyA, *bodyB, settings);
}

JPH::DistanceConstraint* RopeModule::CreateForwardConstraint(JPH::Body* bodyA, JPH::Body* bodyB, const RopeComponent& rope, int segmentIndex, float distance, int velocityIterations, int positionIterations)
{
    JPH::Vec3 posA, posB;

    switch (rope.segmentType)
    {
    case ShapeType::Capsule:
    {
        JPH::DistanceConstraintSettings settings;

        JPH::Vec3 aPin = bodyA->GetRotation() * JPH::Vec3(0, -1, 0);
        JPH::Vec3 bPin = bodyB->GetRotation() * JPH::Vec3(0, 1, 0);

        posA = bodyA->GetPosition() + (aPin * (distance - rope.segmentOverlap));
        posB = bodyB->GetPosition() + (bPin * distance);

        if (bodyA->GetID() == rope.cachedBodyIdA)
        {
            posA += bodyA->GetRotation() * PhysicsModule::ToJolt(rope.entityAOffset);
        }
        if (bodyB->GetID() == rope.cachedBodyIdB)
        {
            posB += bodyB->GetRotation() * PhysicsModule::ToJolt(rope.entityBOffset);
        }

        settings.mPoint1 = posA;
        settings.mPoint2 = posB;

        int totalSegments = rope.numSegments + (rope.entityA != 0 ? 1 : 0) + (rope.entityB != 0 ? 1 : 0) - 1;
        settings.mConstraintPriority = rope.basePriority + rope.forwardPriorityBoost +
            (totalSegments - segmentIndex) * rope.priorityStep;

        settings.mMaxDistance = 0.f;
        settings.mMinDistance = 0.f;

        settings.mNumVelocityStepsOverride = velocityIterations;
        settings.mNumPositionStepsOverride = positionIterations;

        return new JPH::DistanceConstraint(*bodyA, *bodyB, settings);
    }
    default:
    {
        JPH::DistanceConstraintSettings settings;

        JPH::Vec3 posA = bodyA->GetPosition();
        JPH::Vec3 posB = bodyB->GetPosition();
        JPH::Vec3 dirVec = posB - posA;

        if (dirVec.IsClose(JPH::Vec3::sZero(), 1e-6f))
        {
            dirVec = JPH::Vec3(0, -1, 0);
        }

        JPH::Vec3 direction = dirVec.Normalized();

        posA = posA + direction * (distance * 0.1f);
        posB = posB - direction * (distance * 0.1f);

        if (bodyA->GetID() == rope.cachedBodyIdA)
        {
            posA += bodyA->GetRotation() * PhysicsModule::ToJolt(rope.entityAOffset);
        }
        if (bodyB->GetID() == rope.cachedBodyIdB)
        {
            posB += bodyB->GetRotation() * PhysicsModule::ToJolt(rope.entityBOffset);
        }

        settings.mPoint1 = posA;
        settings.mPoint2 = posB;
        settings.mMaxDistance = distance;
        settings.mMinDistance = distance * 0.95f;

        int totalSegments = rope.numSegments + (rope.entityA != 0 ? 1 : 0) + (rope.entityB != 0 ? 1 : 0) - 1;
        settings.mConstraintPriority = rope.basePriority + rope.forwardPriorityBoost +
            (totalSegments - segmentIndex) * rope.priorityStep;

        settings.mNumVelocityStepsOverride = velocityIterations;
        settings.mNumPositionStepsOverride = positionIterations;

        return new JPH::DistanceConstraint(*bodyA, *bodyB, settings);
    }
    }
}

JPH::DistanceConstraint* RopeModule::CreateBackwardConstraint(JPH::Body* bodyA, JPH::Body* bodyB, const RopeComponent& rope, int segmentIndex, float distance, int velocityIterations, int positionIterations)
{
    JPH::Vec3 posA, posB;

    switch (rope.segmentType)
    {
    case ShapeType::Capsule:
    {
        JPH::DistanceConstraintSettings settings;

        JPH::Vec3 aPin = bodyA->GetRotation() * JPH::Vec3(0, -1, 0);
        JPH::Vec3 bPin = bodyB->GetRotation() * JPH::Vec3(0, 1, 0);

        posA = bodyA->GetPosition() + (aPin * distance);
        posB = bodyB->GetPosition() + (bPin * (distance - rope.segmentOverlap));

        if (bodyA->GetID() == rope.cachedBodyIdA)
        {
            posA += bodyA->GetRotation() * PhysicsModule::ToJolt(rope.entityAOffset);
        }
        if (bodyB->GetID() == rope.cachedBodyIdB)
        {
            posB += bodyB->GetRotation() * PhysicsModule::ToJolt(rope.entityBOffset);
        }

        settings.mPoint1 = posA;
        settings.mPoint2 = posB;

        settings.mConstraintPriority = rope.basePriority + rope.backwardPriorityBoost +
            segmentIndex * rope.priorityStep;

        settings.mMaxDistance = 0.f;
        settings.mMinDistance = 0.f;

        settings.mNumVelocityStepsOverride = velocityIterations;
        settings.mNumPositionStepsOverride = positionIterations;

        return new JPH::DistanceConstraint(*bodyA, *bodyB, settings);
    }
    default:
    {
        JPH::DistanceConstraintSettings settings;

        JPH::Vec3 posA = bodyA->GetPosition();
        JPH::Vec3 posB = bodyB->GetPosition();
        JPH::Vec3 dirVec = posB - posA;

        if (dirVec.IsClose(JPH::Vec3::sZero(), 1e-6f))
        {
            dirVec = JPH::Vec3(0, -1, 0);
        }

        JPH::Vec3 direction = dirVec.Normalized();

        posA = posA - direction * (distance * 0.05f);
        posB = posB + direction * (distance * 0.05f);

        if (bodyA->GetID() == rope.cachedBodyIdA)
        {
            posA += bodyA->GetRotation() * PhysicsModule::ToJolt(rope.entityAOffset);
        }
        if (bodyB->GetID() == rope.cachedBodyIdB)
        {
            posB += bodyB->GetRotation() * PhysicsModule::ToJolt(rope.entityBOffset);
        }

        settings.mPoint1 = posA;
        settings.mPoint2 = posB;
        settings.mMaxDistance = distance;
        settings.mMinDistance = distance * 0.95f;

        settings.mConstraintPriority = rope.basePriority + rope.backwardPriorityBoost +
            segmentIndex * rope.priorityStep;

        settings.mNumVelocityStepsOverride = velocityIterations;
        settings.mNumPositionStepsOverride = positionIterations;

        return new JPH::DistanceConstraint(*bodyA, *bodyB, settings);
    }
    }
}

bool RopeModule::ValidateRopeEntity(flecs::entity entity, flecs::entity_t targetId, JPH::BodyID& outBodyId)
{
    if (targetId == 0) return false;

    auto targetEntity = m_world.entity(targetId);
    if (!targetEntity.is_alive()) return false;

    if (auto body = targetEntity.try_get<PhysicsBodyComponent>())
    {
        JPH::BodyID id = m_physicsModule->GetBodyID(targetId);
        if (m_physicsModule->GetBodyInterface().IsAdded(id))
        {
            outBodyId = id;
            return true;
        }
    }

    if (auto character = targetEntity.try_get<CharacterComponent>())
    {
        auto charPtr = m_characterModule->GetCharacter(targetId);
        if (charPtr)
        {
            outBodyId = charPtr->GetBodyID();
            return m_physicsModule->GetBodyInterface().IsAdded(outBodyId);
        }
    }

    return false;
}

bool RopeModule::ConnectRope(flecs::entity ropeEntity, flecs::entity targetEntity, bool connectToA)
{
    auto rope = ropeEntity.try_get_mut<RopeComponent>();
    if (!rope || !targetEntity.is_alive()) return false;

    bool canConnect = targetEntity.has<PhysicsBodyComponent>() || targetEntity.has<CharacterComponent>();
    if (!canConnect) return false;

    const char* entityName = GetPersistentEntityName(targetEntity);
    if (!entityName) return false;

    if (connectToA)
    {
        rope->entityAName = entityName;
        rope->entityA = targetEntity;
    }
    else
    {
        rope->entityBName = entityName;
        rope->entityB = targetEntity;
        targetEntity.set<ConnectedAsB>({ ropeEntity.id() });
    }
    CreateRope(ropeEntity);
    return true;
}

void RopeModule::DisconnectRope(flecs::entity ropeEntity, bool disconnectA)
{
    if (!ropeEntity.is_alive()) return;

    auto rope = ropeEntity.try_get_mut<RopeComponent>();
    if (!rope) return;

    if (disconnectA)
    {
        rope->entityAName = "";
        rope->entityA = flecs::entity::null();
    }
    else
    {
        rope->entityBName = "";
        if (rope->entityB)
        {
            rope->entityB.remove<ConnectedAsB>();
        }
        rope->entityB = flecs::entity::null();
    }

    CreateRope(ropeEntity);
}

void RopeModule::DrawRopeInspector(flecs::entity entity, RopeComponent& component)
{
    auto findEntityIndex = [&](const flecs::entity& targetEntity, const std::vector<flecs::entity>& entities) -> int
        {
            if (!targetEntity)
            {
                return 0;
            }
            for (size_t i = 0; i < entities.size(); ++i)
            {
                if (entities[i] == targetEntity)
                {
                    return static_cast<int>(i + 1);
                }
            }
            return 0;
        };

    std::vector<const char*> connectableNames;
    std::vector<flecs::entity> connectableEntities;
    connectableNames.push_back("None");

    m_connectableQuery.each([&](flecs::entity e)
        {
            if (e != entity)
            {
                connectableEntities.push_back(e);
                connectableNames.push_back(e.name() ? e.name().c_str() : "Unnamed Entity");
            }
        });

    bool changed = false;

    if (ImGui::BeginTable("##rope_props", 2, ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Separator();
        if (ImGui::TreeNodeEx("Connection", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TableNextColumn();
            ImGui::Separator();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Entity A");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            int currentItemA = findEntityIndex(component.entityA, connectableEntities);

            if (ImGui::Combo("##EntityA", &currentItemA, connectableNames.data(), connectableNames.size(), std::min((int)connectableNames.size(), 30))) {
                if (currentItemA == 0)
                {
                    DisconnectRope(entity, true);
                }
                else
                {
                    ConnectRope(entity, connectableEntities[currentItemA - 1], true);
                }
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Entity B");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            int currentItemB = findEntityIndex(component.entityB, connectableEntities);
            if (ImGui::Combo("##EntityB", &currentItemB, connectableNames.data(), connectableNames.size(), std::min((int)connectableNames.size(), 30)))
            {
                if (currentItemB == 0)
                {
                    DisconnectRope(entity, false);
                }
                else
                {
                    ConnectRope(entity, connectableEntities[currentItemB - 1], false);
                }
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Entity A Offset");
            ImGui::SetItemTooltip("Local offset from Entity A center");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat3("##EntityAOffset", &component.entityAOffset.x, 0.1f, -10.0f, 10.0f, "%.1f");
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Entity B Offset");
            ImGui::SetItemTooltip("Local offset from Entity B center");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat3("##EntityBOffset", &component.entityBOffset.x, 0.1f, -10.0f, 10.0f, "%.1f");
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

            ImGui::TreePop();
        }
        else
        {
            ImGui::TableNextColumn();
        }

        static std::vector<const char*> shapeTypes = { "Sphere", "Capsule" };

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Separator();
        if (ImGui::TreeNodeEx("Physics", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TableNextColumn();
            ImGui::Separator();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Segment Type");
            ImGui::SetItemTooltip("Shape for rope segments");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);

            int currentShape = component.segmentType == ShapeType::Sphere ? 0 : 1;
            if (ImGui::Combo("##SelectedShape", &currentShape, shapeTypes.data(), shapeTypes.size())) {
                switch (currentShape)
                {
                case 0:
                    component.segmentType = ShapeType::Sphere;
                    break;
                case 1:
                    component.segmentType = ShapeType::Capsule;
                    break;
                }
                changed = true;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Physics Segments");
            ImGui::SetItemTooltip("Number of physics bodies in the rope");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragInt("##PhysicsSegments", &component.numSegments, 1, 2, 100);
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

            if (component.segmentType != ShapeType::Capsule)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Length");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##Length", &component.length, 0.1f, 0.1f, 100.0f, "%.1f");
                if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Segment Mass");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##SegmentMass", &component.segmentMass, 0.01f, 0.001f, 10.0f, "%.3f");
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Gravity Scale");
            ImGui::SetItemTooltip("Multiplier for gravity effect on rope segments");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##GravityScale", &component.gravityScale, 0.01f, 0.0f, 10.0f, "%.2f");
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Friction");
            ImGui::SetItemTooltip("Surface friction of rope segments");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##Friction", &component.friction, 0.01f, 0.0f, 2.0f, "%.2f");
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Restitution");
            ImGui::SetItemTooltip("Bounciness of rope segments (0=no bounce, 1=perfect bounce)");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##Restitution", &component.restitution, 0.01f, 0.0f, 1.0f, "%.2f");
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Segment Radius");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragFloat("##SegmentRadius", &component.segmentRadius, 0.01f, 0.01f, 1.0f, "%.2f");
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

            if (component.segmentType == ShapeType::Capsule)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Segment Half Height");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##SegmentHalfHeight", &component.segmentHalfHeight, 0.01f, 0.01f, 1.0f, "%.2f");
                if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Segment Overlap");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##SegmentOverlap", &component.segmentOverlap, 0.01f, 0.01f, 1.0f, "%.2f");
                if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Use Skip Constraints");
            ImGui::SetItemTooltip("Creates additional constraints that skip one segment");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##UseSkipConstraints", &component.useSkipConstraints);
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

            if (component.useSkipConstraints)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Skip Stiffness");
                ImGui::SetItemTooltip("Controls flexibility of skip constraints (0.1=flexible, 1.0=rigid)");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##SkipStiffness", &component.skipConstraintStiffness, 0.01f, 0.1f, 1.0f, "%.2f");
                if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Velocity Iterations");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragInt("##VelocityIterations", &component.velocityIterations, 1, 1, 50);
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Position Iterations");
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragInt("##PositionIterations", &component.positionIterations, 1, 1, 50);
            if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

            ImGui::TreePop();
        }
        else
        {
            ImGui::TableNextColumn();
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Separator();
        if (ImGui::TreeNodeEx("Visuals", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TableNextColumn();
            ImGui::Separator();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Render Rope");
            ImGui::TableNextColumn();
            ImGui::Checkbox("##RenderRope", &component.renderRope);
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                if (component.renderRope)
                    CreateRopeVisuals(entity);
                else
                    DestroyRopeVisuals(entity);
            }

            if (component.renderRope)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Rope Thickness");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##RopeThickness", &component.ropeThickness, 0.01f, 0.1f, 5.0f, "%.2f");
                if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Rope Colour");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::ColorEdit3("##RopeColour", &component.ropeColour[0], ImGuiColorEditFlags_NoInputs);
                if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Segment Multiplier");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragInt("##SegmentMultiplier", &component.segmentSubdivisions, 1, 1, 8);
                if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Radial Segments");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragInt("##RadialSegments", &component.radialSegments, 1, 3, 16);
                if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("UV Length Tiling");
                ImGui::SetItemTooltip("Texture repeat along rope length");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##UVLengthTiling", &component.uvTilingLength, 0.1f, 0.1f, 20.0f, "%.1f");
                if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("UV Radial Tiling");
                ImGui::SetItemTooltip("Texture repeat around rope circumference");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##UVRadialTiling", &component.uvTilingRadial, 0.1f, 0.1f, 5.0f, "%.1f");
                if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Generate End Caps");
                ImGui::TableNextColumn();
                ImGui::Checkbox("##GenerateEndCaps", &component.generateEndCaps);
                if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Material");
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                auto availableMaterials = ResourceLibrary::GetAllMaterialHandles();
                std::vector<const char*> materialNames;
                materialNames.push_back("default_material");
                for (const char* mat : availableMaterials)
                {
                    if (strcmp(mat, "default_material") != 0)
                        materialNames.push_back(mat);
                }

                int currentMaterial = 0;
                for (size_t i = 0; i < materialNames.size(); ++i)
                {
                    if (component.ropeMaterial != "" && component.ropeMaterial == materialNames[i])
                    {
                        currentMaterial = i;
                        break;
                    }
                }

                if (ImGui::Combo("##Material", &currentMaterial, materialNames.data(), materialNames.size()))
                {
                    component.ropeMaterial = materialNames[currentMaterial];
                    changed = true;
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
        if (ImGui::TreeNodeEx("Debug Info"))
        {
            ImGui::TableNextColumn();
            ImGui::Separator();

            flecs::entity_t entityId = entity.id();
            auto constraintIt = m_ropeConstraints.find(entityId);
            if (constraintIt != m_ropeConstraints.end())
            {
                const RopeConstraintData& data = constraintIt->second;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Forward Constraints");
                ImGui::TableNextColumn();
                ImGui::Text("%zu", data.forwardConstraints.size());

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Backward Constraints");
                ImGui::TableNextColumn();
                ImGui::Text("%zu", data.backwardConstraints.size());

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Skip Constraints");
                ImGui::TableNextColumn();
                ImGui::Text("%zu", data.skipConstraints.size());

                int totalConstraints = data.forwardConstraints.size() +
                    data.backwardConstraints.size() +
                    data.skipConstraints.size();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Total Constraints");
                ImGui::TableNextColumn();
                ImGui::Text("%d", totalConstraints);
            }

            auto visualIt = m_ropeVisuals.find(entityId);
            if (visualIt != m_ropeVisuals.end())
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Visual Mesh");
                ImGui::TableNextColumn();
                ImGui::Text("Active");

                if (visualIt->second.ropeMesh)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Mesh Triangles");
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", visualIt->second.ropeMesh->GetTriCount());

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Physics Segments");
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", component.numSegments);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted("Visual Segments");
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", component.numSegments * component.segmentSubdivisions);
                }
            }
            else
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Visual Mesh");
                ImGui::TableNextColumn();
                ImGui::Text("None");
            }

            ImGui::TreePop();
        }
        else
        {
            ImGui::TableNextColumn();
        }

        ImGui::EndTable();
    }

    if (changed)
    {
        CreateRope(entity);
    }
}

ImVec4 RopeModule::GetRopeEntityColour(flecs::entity entity)
{
    if (entity.has<RopeComponent>())
    {
        return ImVec4(0.85f, 0.65f, 0.85f, 1.0f);
    }
    return ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
}