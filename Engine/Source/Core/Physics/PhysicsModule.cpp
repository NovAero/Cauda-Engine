#include "cepch.h"
#include "PhysicsModule.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Scene/CameraModule.h"
#include "Core/Physics/PhysicsUserData.h"
#include "Core/Physics/PhysicsShapes.h"
#include "Core/Character/CharacterModule.h"
#include "Core/Components/GrabComponent.h"
#include "Core/Math/TransformModule.h"
#include "Core/Editor/ResourceLibrary.h"
#include "Core/Editor/GrimoriumModule.h"
#include "Core/Utilities/STLModule.h"
#include "Platform/Win32/Application.h"
#include <iostream>
#include <chrono>
#include <algorithm>

#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include "ConstraintModule.h"

namespace ObjectLayers
{
	static constexpr JPH::ObjectLayer NON_MOVING(0);
	static constexpr JPH::ObjectLayer MOVING(1);
	static constexpr JPH::ObjectLayer NO_COLLISION(2);
	static constexpr JPH::ObjectLayer KINEMATIC_SENSOR(3);
	static constexpr JPH::ObjectLayer ROPE(4);
	static constexpr JPH::ObjectLayer ROPE_EVEN(5);
	static constexpr JPH::ObjectLayer CHARACTER(6);
	static constexpr JPH::ObjectLayer PROJECTILE(7);
	static constexpr JPH::ObjectLayer PROJECTILE_ONLY(8);
	static constexpr JPH::ObjectLayer NUM_LAYERS(9);
};

namespace BroadPhaseLayers
{
	static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
	static constexpr JPH::BroadPhaseLayer MOVING(1);
	static constexpr JPH::BroadPhaseLayer NO_COLLISION(2);
	static constexpr JPH::BroadPhaseLayer KINEMATIC_SENSOR(3);
	static constexpr JPH::BroadPhaseLayer ROPE(4);
	static constexpr JPH::BroadPhaseLayer ROPE_EVEN(5);
	static constexpr JPH::BroadPhaseLayer CHARACTER(6);
	static constexpr JPH::BroadPhaseLayer PROJECTILE(7);
	static constexpr JPH::BroadPhaseLayer PROJECTILE_ONLY(8);
	static constexpr JPH::uint NUM_LAYERS(9);
};

class BPLayerInterface final : public JPH::BroadPhaseLayerInterface
{
public:
	BPLayerInterface()
	{
		mObjectToBroadPhase[ObjectLayers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
		mObjectToBroadPhase[ObjectLayers::MOVING] = BroadPhaseLayers::MOVING;
		mObjectToBroadPhase[ObjectLayers::NO_COLLISION] = BroadPhaseLayers::NO_COLLISION;
		mObjectToBroadPhase[ObjectLayers::KINEMATIC_SENSOR] = BroadPhaseLayers::KINEMATIC_SENSOR;
		mObjectToBroadPhase[ObjectLayers::ROPE] = BroadPhaseLayers::ROPE;
		mObjectToBroadPhase[ObjectLayers::ROPE_EVEN] = BroadPhaseLayers::ROPE_EVEN;
		mObjectToBroadPhase[ObjectLayers::CHARACTER] = BroadPhaseLayers::CHARACTER;
		mObjectToBroadPhase[ObjectLayers::PROJECTILE] = BroadPhaseLayers::PROJECTILE;
		mObjectToBroadPhase[ObjectLayers::PROJECTILE_ONLY] = BroadPhaseLayers::PROJECTILE_ONLY;
	}

	virtual JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
	virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override { return mObjectToBroadPhase[inLayer]; }
	virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
	{
		switch ((JPH::BroadPhaseLayer::Type)inLayer)
		{
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NO_COLLISION: return "NO_COLLISION";
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::KINEMATIC_SENSOR: return "KINEMATIC_SENSOR";
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::ROPE: return "ROPE";
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::ROPE_EVEN: return "ROPE_EVEN";
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::CHARACTER: return "CHARACTER";
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::PROJECTILE: return "PROJECTILE";
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::PROJECTILE_ONLY: return "PROJECTILE_ONLY";
		default: JPH_ASSERT(false); return "INVALID";
		}
	}

private:
	JPH::BroadPhaseLayer mObjectToBroadPhase[ObjectLayers::NUM_LAYERS];
};

class ObjectVsBPLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
	{
		switch (inLayer1)
		{
		case ObjectLayers::NON_MOVING:
			return inLayer2 != BroadPhaseLayers::NON_MOVING &&
				inLayer2 != BroadPhaseLayers::NO_COLLISION &&
				inLayer2 != BroadPhaseLayers::PROJECTILE_ONLY;

		case ObjectLayers::MOVING:
			return inLayer2 != BroadPhaseLayers::NO_COLLISION &&
				inLayer2 != BroadPhaseLayers::PROJECTILE_ONLY;

		case ObjectLayers::NO_COLLISION:
			return false;

		case ObjectLayers::KINEMATIC_SENSOR:
			return inLayer2 != BroadPhaseLayers::NO_COLLISION &&
				inLayer2 != BroadPhaseLayers::PROJECTILE_ONLY;

		case ObjectLayers::ROPE:
			return inLayer2 != BroadPhaseLayers::NO_COLLISION &&
				inLayer2 != BroadPhaseLayers::PROJECTILE_ONLY;

		case ObjectLayers::ROPE_EVEN:
			return inLayer2 != BroadPhaseLayers::NO_COLLISION &&
				inLayer2 != BroadPhaseLayers::PROJECTILE_ONLY;

		case ObjectLayers::CHARACTER:
			return inLayer2 != BroadPhaseLayers::NO_COLLISION &&
				inLayer2 != BroadPhaseLayers::PROJECTILE_ONLY;

		case ObjectLayers::PROJECTILE:
			return inLayer2 != BroadPhaseLayers::NO_COLLISION &&
				inLayer2 != BroadPhaseLayers::PROJECTILE;

		case ObjectLayers::PROJECTILE_ONLY:
			return inLayer2 == BroadPhaseLayers::PROJECTILE;

		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
	{
		switch (inObject1)
		{
		case ObjectLayers::NON_MOVING:
			return inObject2 != ObjectLayers::NON_MOVING &&
				inObject2 != ObjectLayers::NO_COLLISION &&
				inObject2 != ObjectLayers::PROJECTILE_ONLY;

		case ObjectLayers::MOVING:
			return inObject2 != ObjectLayers::NO_COLLISION &&
				inObject2 != ObjectLayers::PROJECTILE_ONLY;

		case ObjectLayers::NO_COLLISION:
			return false;

		case ObjectLayers::KINEMATIC_SENSOR:
			return inObject2 != ObjectLayers::NO_COLLISION &&
				inObject2 != ObjectLayers::PROJECTILE_ONLY;

		case ObjectLayers::ROPE:
			return inObject2 != ObjectLayers::NO_COLLISION &&
				inObject2 != ObjectLayers::ROPE_EVEN &&
				inObject2 != ObjectLayers::PROJECTILE_ONLY;


		case ObjectLayers::ROPE_EVEN:
			return inObject2 != ObjectLayers::NO_COLLISION &&
				inObject2 != ObjectLayers::ROPE &&
				inObject2 != ObjectLayers::PROJECTILE_ONLY;

		case ObjectLayers::CHARACTER:
			return inObject2 != ObjectLayers::NO_COLLISION &&
				inObject2 != ObjectLayers::PROJECTILE_ONLY;

		case ObjectLayers::PROJECTILE:
			return inObject2 != ObjectLayers::NO_COLLISION &&
				inObject2 != ObjectLayers::PROJECTILE;

		case ObjectLayers::PROJECTILE_ONLY:
			return inObject2 == ObjectLayers::PROJECTILE;

		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};


PhysicsModule::PhysicsModule(flecs::world& world) : m_world(world)
{
	InitialisePhysics();
	SetupComponents();
	SetupSystems();
	SetupObservers();

	m_world.import<SensorModule>();
	m_world.import<ConstraintModule>();
	m_sensorModule = m_world.try_get_mut<SensorModule>();

	EditorConsole::Inst()->AddConsoleInfo("PhysicsModule initialised successfully", true);
}

PhysicsModule::~PhysicsModule()
{
	Logger::PrintLog("PhysicsModule shutdown starting...");

	if (m_resolveUnboundEntitiesSystem && m_resolveUnboundEntitiesSystem.is_alive())
		m_resolveUnboundEntitiesSystem.destruct();

	if (m_bodyAddObserver && m_bodyAddObserver.is_alive())
		m_bodyAddObserver.destruct();
	if (m_bodyRemoveObserver && m_bodyRemoveObserver.is_alive())
		m_bodyRemoveObserver.destruct();
	if (m_transformUpdateObserver && m_transformUpdateObserver.is_alive())
		m_transformUpdateObserver.destruct();
	if (m_colliderUpdateObserver && m_colliderUpdateObserver.is_alive())
		m_colliderUpdateObserver.destruct();
	if (m_bodyUpdateObserver && m_bodyUpdateObserver.is_alive())
		m_bodyUpdateObserver.destruct();

	m_debugRenderer.reset();

	if (m_physicsSystem)
	{
		JPH::Constraints constraints;
		constraints = m_physicsSystem->GetConstraints();

		ConstraintModule* cMod = m_world.try_get_mut<ConstraintModule>();
		cMod->CleanupManagedConstraints();

		constraints = m_physicsSystem->GetConstraints();
		for (JPH::Constraint* constraint : constraints)
		{
			m_physicsSystem->RemoveConstraint(constraint);
		}
	}

	if (m_cleanupQuery)
	{
		for (auto& [entityId, bodyId] : m_bodies)
		{
			if (m_physicsSystem && m_physicsSystem->GetBodyInterface().IsAdded(bodyId))
			{
				m_physicsSystem->GetBodyInterface().RemoveBody(bodyId);
				m_physicsSystem->GetBodyInterface().DestroyBody(bodyId);
			}
		}
		m_bodies.clear();
	}

	if (m_physicsSystem)
	{
		JPH::BodyIDVector body_ids;
		m_physicsSystem->GetBodies(body_ids);
		if (!body_ids.empty())
		{
			m_physicsSystem->GetBodyInterface().RemoveBodies(body_ids.data(), body_ids.size());
			m_physicsSystem->GetBodyInterface().DestroyBodies(body_ids.data(), body_ids.size());
		}
	}

	m_physicsSystem.reset();

	m_jobSystem.reset();
	m_tempAllocator.reset();

	m_objectLayerPairFilter.reset();
	m_objectVsBPLayerFilter.reset();
	m_broadPhaseInterface.reset();

	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;

	Logger::PrintLog("PhysicsModule shutdown completed.");
}

void PhysicsModule::InitialisePhysics()
{
	const uint32_t maxBodies = 65536;
	const uint32_t maxBodyPairs = 65536;
	const uint32_t maxContactConstraints = 10240;
	const uint32_t tempAllocatorSize = 10 * 1024 * 1024;

	JPH::RegisterDefaultAllocator();
	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(tempAllocatorSize);

	const unsigned int maxThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
	m_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, maxThreads);

	m_broadPhaseInterface = std::make_unique<BPLayerInterface>();
	m_objectVsBPLayerFilter = std::make_unique<ObjectVsBPLayerFilter>();
	m_objectLayerPairFilter = std::make_unique<ObjectLayerPairFilter>();

	m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();
	m_physicsSystem->Init(
		maxBodies,
		0, // Mutexes
		maxBodyPairs,
		maxContactConstraints,
		*m_broadPhaseInterface,
		*m_objectVsBPLayerFilter,
		*m_objectLayerPairFilter
	);

	m_physicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
	m_stats.gravity = glm::vec3(0.0f, -9.81f, 0.0f);

	m_physicsCasting = std::make_unique<PhysicsCasting>(m_world, this);

	m_cameraModule = m_world.try_get_mut<CameraModule>();
	InitialiseDebugRenderer();
}

void PhysicsModule::SetupEvents()
{

}

void PhysicsModule::SetupComponents()
{
	m_world.component<ShapeType>("ShapeType")
		.constant("Sphere", ShapeType::Sphere)
		.constant("Box", ShapeType::Box)
		.constant("Triangle", ShapeType::Triangle)
		.constant("Capsule", ShapeType::Capsule)
		.constant("TaperedCapsule", ShapeType::TaperedCapsule)
		.constant("Cylinder", ShapeType::Cylinder)
		.constant("ConvexHull", ShapeType::ConvexHull)
		.constant("StaticCompound", ShapeType::StaticCompound)
		.constant("MutableCompound", ShapeType::MutableCompound)
		.constant("RotatedTranslated", ShapeType::RotatedTranslated)
		.constant("Scaled", ShapeType::Scaled)
		.constant("OffsetCenterOfMass", ShapeType::OffsetCenterOfMass)
		.constant("Mesh", ShapeType::Mesh)
		.constant("HeightField", ShapeType::HeightField)
		.constant("SoftBody", ShapeType::SoftBody)
		.constant("User1", ShapeType::User1)
		.constant("User2", ShapeType::User2)
		.constant("User3", ShapeType::User3)
		.constant("User4", ShapeType::User4)
		.constant("User5", ShapeType::User5)
		.constant("User6", ShapeType::User6)
		.constant("User7", ShapeType::User7)
		.constant("User8", ShapeType::User8)
		.constant("UserConvex1", ShapeType::UserConvex1)
		.constant("UserConvex2", ShapeType::UserConvex2)
		.constant("UserConvex3", ShapeType::UserConvex3)
		.constant("UserConvex4", ShapeType::UserConvex4)
		.constant("UserConvex5", ShapeType::UserConvex5)
		.constant("UserConvex6", ShapeType::UserConvex6)
		.constant("UserConvex7", ShapeType::UserConvex7)
		.constant("UserConvex8", ShapeType::UserConvex8)
		.constant("Plane", ShapeType::Plane)
		.constant("TaperedCylinder", ShapeType::TaperedCylinder)
		.constant("Empty", ShapeType::Empty);

	m_world.component<MotionType>("MotionType")
		.constant("STATIC", MotionType::STATIC)
		.constant("KINEMATIC", MotionType::KINEMATIC)
		.constant("DYNAMIC", MotionType::DYNAMIC);

	m_world.component<CollisionLayer>("CollisionLayer")
		.constant("NON_MOVING", CollisionLayer::NON_MOVING)
		.constant("MOVING", CollisionLayer::MOVING)
		.constant("NO_COLLISION", CollisionLayer::NO_COLLISION)
		.constant("KINEMATIC_SENSOR", CollisionLayer::KINEMATIC_SENSOR)
		.constant("ROPE", CollisionLayer::ROPE)
		.constant("ROPE_EVEN", CollisionLayer::ROPE_EVEN)
		.constant("CHARACTER", CollisionLayer::CHARACTER)
		.constant("NUM_LAYERS", CollisionLayer::NUM_LAYERS);

	m_world.component<SubShapeData>("SubShapeData")
		.member<ShapeType>("shapeType")
		.member<glm::vec3>("position")
		.member<glm::quat>("rotation")
		.member<glm::vec3>("dimensions")
		.member<std::string>("meshAssetHandle")
		.member<uint32_t>("userData");

	m_world.component<std::vector<SubShapeData>>()
		.opaque(std_vector_ser<SubShapeData>);

	m_world.component<PhysicsBodyComponent>("PhysicsBodyComponent")
		.member<MotionType>("motionType")
		.member<CollisionLayer>("collisionLayer")
		.member<float>("mass")
		.member<float>("restitution")
		.member<float>("friction")
		.member<float>("gravityFactor")
		.add(flecs::With, m_world.component<ColliderComponent>());

	m_world.component<ColliderComponent>("ColliderComponent")
		.member<ShapeType>("shapeSubType")
		.member<glm::vec3>("dimensions")
		.member<glm::vec3>("offset")
		.member<bool>("isSensor")
		.member<std::string>("meshAssetHandle")
		.member<bool>("syncWithTransform")
		.member<std::vector<SubShapeData>>("subShapes");

	m_world.component<PhysicsStats>();

	RegisterWithEditor();

	m_syncFromPhysicsQuery = m_world.query_builder<PhysicsBodyComponent, TransformComponent>().build();
	m_cleanupQuery = m_world.query_builder<PhysicsBodyComponent>().build();
}

void PhysicsModule::SetupSystems()
{
	flecs::entity GameTick = Cauda::Application::GetGameTickSource();
	//flecs::entity PhysicsPhase = Cauda::Application::GetPhysicsPhase();
	//flecs::entity ResolvePhysicsPhase = Cauda::Application::ResolvePhysicsPhase();


	m_resolveUnboundEntitiesSystem = m_world.system<TransformComponent, ColliderComponent, PhysicsBodyComponent>("ResolveUnboundBodies")
		.kind(flecs::PreUpdate)
		.tick_source(GameTick)
		.each([this](flecs::entity e, TransformComponent& transform, ColliderComponent& collider, PhysicsBodyComponent& physics)
			{


				if (!e.is_alive() || m_bodies.contains(e.id()))
					return;

				this->CreateBody(e, transform, collider, physics);
			});

	m_updateSystem = m_world.system<>("PhysicsUpdate")
		.kind(flecs::OnUpdate)
		.tick_source(GameTick)
		.run([this](flecs::iter& it)
			{
				float deltaTime = it.delta_system_time();
				//std::cout << "Physics deltaTime: " << deltaTime << std::endl;

				auto start = std::chrono::high_resolution_clock::now();

#ifdef _DEBUG
				m_physicsSystem->Update(deltaTime, 1, m_tempAllocator.get(), m_jobSystem.get());
#else 
				m_physicsSystem->Update(deltaTime, 2, m_tempAllocator.get(), m_jobSystem.get());
#endif
				auto end = std::chrono::high_resolution_clock::now();
				m_stats.lastStepTime = std::chrono::duration<float, std::milli>(end - start).count();

				SyncTransformsFromPhysics();

				UpdatePhysicsStats();

				//std::cout << "Physics Tick" << std::endl;
				//m_sensorModule->Update(m_world.delta_time());
			});
}

void PhysicsModule::SetupObservers()
{

	m_bodyRemoveObserver = m_world.observer<PhysicsBodyComponent>()
		.event(flecs::OnRemove)
		.event(flecs::OnDelete)
		.each([this](flecs::entity entity, PhysicsBodyComponent& physics)
			{
				this->RemoveBody(entity);
			});

	m_world.observer<const TransformComponent>("NonPhysicsBodyUpdatePhysicsChildren")
		.without<ColliderComponent>().filter()
		.without<PhysicsBodyComponent>().filter()
		.event(flecs::OnSet)
		.each([this](flecs::entity entity, const TransformComponent&)
			{
				UpdateJoltBody(entity);
			});

	m_transformUpdateObserver = m_world.observer<const TransformComponent>("TransformUpdateObserver")
		.with<ColliderComponent>().filter()
		.with<PhysicsBodyComponent>().filter()
		.event(flecs::OnSet)
		.each([this](flecs::entity entity, const TransformComponent& transform)
			{
				auto collider = entity.try_get_mut<ColliderComponent>();
				if (collider && collider->syncWithTransform)
				{
					SyncColliderWithTransform(entity, *collider, transform);
				}

				UpdateJoltBody(entity);
			});

	m_world.observer<const ColliderComponent>("ColliderUpdateObserver")
		.with<PhysicsBodyComponent>().filter()
		.with<TransformComponent>().filter()
		.event(flecs::OnSet)
		.each([this](flecs::entity entity, const ColliderComponent& collider)
			{
				auto transform = entity.try_get<TransformComponent>();
				auto physics = entity.try_get<PhysicsBodyComponent>();

				UpdateJoltCollider(entity, collider, *transform);
			});

	m_world.observer<PhysicsBodyComponent>("PhysicsBodyUpdateObserver")
		.with<ColliderComponent>().filter()
		.with<TransformComponent>().filter()
		.event(flecs::OnSet)
		.each([this](flecs::entity entity, PhysicsBodyComponent& physics)
			{
				if (m_bodies.contains(entity.id()))
				{
					physics.bodyIdandSequence = m_bodies[entity.id()].GetIndexAndSequenceNumber();
				}

				auto collider = entity.try_get_mut<ColliderComponent>();
				UpdateJoltBodySettings(entity, *collider, physics);
			});

	m_world.observer<const TransformComponent>()
		.event<OnPhysicsUpdateTransform>()
		.each([this](flecs::entity entity, const TransformComponent& transform)
			{
				UpdateJoltBody(entity);
			});

}

void PhysicsModule::RegisterWithEditor()
{
	m_editorModule = m_world.try_get_mut<EditorModule>();
	if (!m_editorModule) return;

	m_editorModule->RegisterComponent<PhysicsBodyComponent>(
		"Physics Body",
		"Physics",
		[this](flecs::entity entity, PhysicsBodyComponent& component)
		{
			DrawPhysicsBodyInspector(entity, component);
		},
		[](flecs::entity entity) -> bool
		{
			return true;
		}
	);

	m_editorModule->RegisterComponent<ColliderComponent>(
		"Collider",
		"Physics",
		[this](flecs::entity entity, ColliderComponent& component)
		{
			DrawColliderInspector(entity, component);
		}
	);

	m_editorModule->RegisterEntityColour([this](flecs::entity entity) -> ImVec4
		{
			return GetPhysicsEntityColour(entity);
		});
}

void PhysicsModule::OnImGuiRender()
{
	if (m_showPhysicsWindow)
	{
		ImGui::Begin("Physics Control", &m_showPhysicsWindow);

		ImGui::Text("Simulation Control");
		ImGui::Separator();

		if (ImGui::DragFloat3("Gravity", glm::value_ptr(m_stats.gravity), 0.1f, -100.0f, 100.0f))
		{
			SetGravity(m_stats.gravity);
		}

		ImGui::Separator();

		ImGui::Text("Debug Windows");
		ImGui::Checkbox("Show Stats Window", &m_showStatsWindow);
		ImGui::Checkbox("Show Debug Settings", &m_showDebugWindow);

		ImGui::Separator();

		ImGui::End();
	}

	if (m_showStatsWindow)
	{
		DrawPhysicsStatsWindow();
	}

	if (m_showDebugWindow)
	{
		DrawDebugSettingsWindow();
	}
}

void PhysicsModule::EnablePhysics(bool enable)
{
	if (enable)
		m_updateSystem.enable();
	else
		m_updateSystem.disable();
}

void PhysicsModule::ResolveUnboundEntities()
{
	m_resolveUnboundEntitiesSystem.run();
}

JPH::ShapeSettings::ShapeResult PhysicsModule::ConstructJoltShape(flecs::entity entity, const ColliderComponent& collider, const TransformComponent& transform)
{
	JPH::ShapeSettings::ShapeResult baseShapeResult;
	bool needsScaling = false;

	switch (ToJoltShape(collider.shapeSubType))
	{
	case JPH::EShapeSubType::Box:
		baseShapeResult = PhysicsShapes::CreateBox(collider.dimensions);
		break;
	case JPH::EShapeSubType::Sphere:
		baseShapeResult = PhysicsShapes::CreateSphere(collider.dimensions.x);
		break;
	case JPH::EShapeSubType::Capsule:
		baseShapeResult = PhysicsShapes::CreateCapsule(collider.dimensions.x, collider.dimensions.y);
		break;
	case JPH::EShapeSubType::Cylinder:
		baseShapeResult = PhysicsShapes::CreateCylinder(collider.dimensions.x, collider.dimensions.y);
		break;
	case JPH::EShapeSubType::Plane:
	{
		glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
		glm::vec3 normal = transform.rotation * worldUp;
		normal = glm::normalize(normal);
		baseShapeResult = PhysicsShapes::CreatePlane(normal, collider.dimensions.x);
		break;
	}
	case JPH::EShapeSubType::ConvexHull:
	{
		if (collider.meshAssetHandle.empty())
		{
			JPH::ShapeSettings::ShapeResult result;
			result.SetError("No mesh asset handle specified for convex hull");
			return result;
		}

		Mesh* mesh = ResourceLibrary::GetMesh(collider.meshAssetHandle);
		if (!mesh || !mesh->HasPhysicsData())
		{
			JPH::ShapeSettings::ShapeResult result;
			result.SetError("Mesh asset not found for convex hull: " + collider.meshAssetHandle);
			return result;
		}

		const std::vector<glm::vec3>& vertices = mesh->GetVertices();
		baseShapeResult = PhysicsShapes::CreateConvexHull(vertices);
		needsScaling = true;
		break;
	}
	case JPH::EShapeSubType::Mesh:
	{
		if (collider.meshAssetHandle.empty())
		{
			JPH::ShapeSettings::ShapeResult result;
			result.SetError("No mesh asset handle specified");
			return result;
		}

		Mesh* mesh = ResourceLibrary::GetMesh(collider.meshAssetHandle);
		if (!mesh || !mesh->HasPhysicsData())
		{
			JPH::ShapeSettings::ShapeResult result;
			result.SetError("Mesh asset not found or has no physics data: " + collider.meshAssetHandle);
			return result;
		}

		const std::vector<glm::vec3>& vertices = mesh->GetVertices();
		const std::vector<uint32_t>& indices = mesh->GetIndices();
		baseShapeResult = PhysicsShapes::CreateMesh(vertices, indices);
		needsScaling = true;
		break;
	}
	case JPH::EShapeSubType::StaticCompound:
	{
		if (collider.subShapes.empty())
		{
			JPH::ShapeSettings::ShapeResult result;
			result.SetError("Static compound shape requires at least one sub-shape");
			return result;
		}

		baseShapeResult = PhysicsShapes::CreateStaticCompound(collider.subShapes);
		break;
	}
	default:
		std::cerr << "Error: Unsupported shape type for entity " << entity.name() << std::endl;
		JPH::ShapeSettings::ShapeResult result;
		result.SetError("Unsupported shape type");
		return result;
	}

	if (baseShapeResult.HasError())
	{
		return baseShapeResult;
	}

	if (needsScaling)
	{
		JPH::ScaledShapeSettings scaledSettings(baseShapeResult.Get(), ToJolt(collider.dimensions));
		baseShapeResult = scaledSettings.Create();
		if (baseShapeResult.HasError())
		{
			return baseShapeResult;
		}
	}

	glm::vec3 offset = collider.offset;
	if (glm::length(offset) > 1e-6f)
	{
		JPH::RotatedTranslatedShapeSettings offsetSettings(
			ToJolt(offset),
			JPH::Quat::sIdentity(),
			baseShapeResult.Get()
		);
		return offsetSettings.Create();
	}

	return baseShapeResult;
}


void PhysicsModule::DrawPhysicsStatsWindow()
{
	ImGui::Begin("Physics Statistics", &m_showStatsWindow);

	ImGui::Text("Performance Metrics");
	ImGui::Separator();
	ImGui::Text("Last Step Time: %.3f ms", m_stats.lastStepTime);
	ImGui::Text("Simulation Steps: %d", m_stats.simulationSteps);
	ImGui::Text("Fixed Time Step: %.6f s", m_stats.timeStep);

	float fps = m_stats.lastStepTime > 0.0f ? 1000.0f / m_stats.lastStepTime : 0.0f;
	ImGui::Text("Effective FPS: %.1f", fps);

	ImGui::Separator();

	ImGui::Text("Body Statistics");
	ImGui::Separator();
	ImGui::Text("Total Bodies: %d", m_stats.totalBodies);
	ImGui::Text("Active Bodies: %d", m_stats.activeBodies);
	ImGui::Text("Sleeping Bodies: %d", m_stats.sleepingBodies);

	ImGui::Separator();

	ImGui::Text("By Motion Type:");
	ImGui::Text("  Static Bodies: %d", m_stats.staticBodies);
	ImGui::Text("  Kinematic Bodies: %d", m_stats.kinematicBodies);
	ImGui::Text("  Dynamic Bodies: %d", m_stats.dynamicBodies);

	ImGui::Separator();

	ImGui::Text("Memory Usage");
	ImGui::Separator();
	ImGui::Text("Body Memory: %zu KB", m_stats.bodyMemoryUsage / 1024);
	ImGui::Text("Shape Memory: %zu KB", m_stats.shapeMemoryUsage / 1024);
	ImGui::Text("Total Memory: %zu KB", (m_stats.bodyMemoryUsage + m_stats.shapeMemoryUsage) / 1024);

	ImGui::Separator();

	ImGui::Text("World Settings");
	ImGui::Separator();
	ImGui::Text("Gravity: (%.2f, %.2f, %.2f)", m_stats.gravity.x, m_stats.gravity.y, m_stats.gravity.z);
	float gravityMagnitude = glm::length(m_stats.gravity);
	ImGui::Text("Gravity Magnitude: %.2f m/s�", gravityMagnitude);

	ImGui::End();
}

void PhysicsModule::DrawDebugSettingsWindow()
{
	ImGui::Begin("Physics Debug Settings", &m_showDebugWindow);

	ImGui::Checkbox("Enable Debug Rendering", &m_debugSettings.enabled);

	if (!m_debugSettings.enabled)
	{
		ImGui::BeginDisabled();
	}

	ImGui::Separator();

	if (ImGui::CollapsingHeader("Shape Rendering", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Draw Bodies", &m_debugSettings.drawBodies);
		ImGui::Checkbox("Draw Shapes", &m_debugSettings.drawShapes);
		ImGui::Checkbox("Draw Shape Wireframe", &m_debugSettings.drawShapeWireframe);
		ImGui::Checkbox("Draw Bounding Boxes", &m_debugSettings.drawBoundingBoxes);

		ImGui::Text("Shape Colour Mode:");
		const char* colourModes[] = {
			"Instance Colour",
			"Shape Type Colour",
			"Motion Type Colour",
			"Sleep State Colour",
			"Island Colour"
		};
		ImGui::Combo("##ColourMode", &m_debugSettings.shapeColourMode, colourModes, IM_ARRAYSIZE(colourModes));
	}

	if (ImGui::CollapsingHeader("Body Properties"))
	{
		ImGui::Checkbox("Draw Velocity", &m_debugSettings.drawVelocity);
		ImGui::Checkbox("Draw Mass and Inertia", &m_debugSettings.drawMassAndInertia);
		ImGui::Checkbox("Draw Sleep Stats", &m_debugSettings.drawSleepStats);
		ImGui::Checkbox("Draw Center of Mass", &m_debugSettings.drawCenterOfMassTransform);
		ImGui::Checkbox("Draw World Transform", &m_debugSettings.drawWorldTransform);
	}

	if (ImGui::CollapsingHeader("Advanced Debug"))
	{
		ImGui::Checkbox("Draw Support Function", &m_debugSettings.drawGetSupportFunction);

		ImGui::Checkbox("Draw Supporting Face", &m_debugSettings.drawGetSupportingFace);
	}

	if (ImGui::CollapsingHeader("Constraints"))
	{
		ImGui::Checkbox("Draw Constraints", &m_debugSettings.drawConstraints);
	}


	if (!m_debugSettings.enabled)
	{
		ImGui::EndDisabled();
	}

	ImGui::End();
}


void PhysicsModule::SyncTransformsFromPhysics()
{
	m_syncFromPhysicsQuery.each([this](flecs::entity e, PhysicsBodyComponent& body, TransformComponent& transform)
		{
			if (body.motionType == MotionType::STATIC) return;

			flecs::entity parent = e.parent();
			if (parent.is_alive() && parent.has<PhysicsBodyComponent>())
			{
				return;
			}

			auto it = m_bodies.find(e.id());
			if (it != m_bodies.end())
			{
				JPH::BodyID bodyId = it->second;
				if (GetBodyInterface().IsAdded(bodyId))
				{
					JPH::Vec3 physicsPos = GetBodyInterface().GetPosition(bodyId);
					JPH::Quat physicsRot = GetBodyInterface().GetRotation(bodyId);

					glm::vec3 worldPos = ToGLM(physicsPos);
					glm::quat worldRot = ToGLMQuat(physicsRot);

					Math::SetWorldPosition(e, worldPos);
					Math::SetWorldRotation(e, worldRot);

					m_world.event<OnPhysicsUpdateTransform>()
						.id<TransformComponent>()
						.id<PhysicsBodyComponent>()
						.entity(e)
						.emit();
				}
			}
		});
}

void PhysicsModule::SyncColliderWithTransform(flecs::entity entity, ColliderComponent& collider, const TransformComponent& transform)
{
	TransformComponent worldTransform = Math::LocalToWorldTform(entity);

	switch (collider.shapeSubType)
	{
	case ShapeType::Box:
		collider.dimensions = worldTransform.scale;
		break;
	case ShapeType::Sphere:
		collider.dimensions.x = (worldTransform.scale.x + worldTransform.scale.y + worldTransform.scale.z) / 6.0f;
		break;
	case ShapeType::Plane:
		break;
	default:
		collider.dimensions = worldTransform.scale;
		break;
	}

	UpdateJoltCollider(entity, collider, transform);
}

void PhysicsModule::UpdatePhysicsStats()
{
	if (!m_physicsSystem) return;

	auto start = std::chrono::high_resolution_clock::now();

	auto bodyStats = m_physicsSystem->GetBodyStats();

	m_stats.totalBodies = bodyStats.mNumBodies;
	m_stats.activeBodies = bodyStats.mNumActiveBodiesDynamic + bodyStats.mNumActiveBodiesKinematic;
	m_stats.staticBodies = bodyStats.mNumBodiesStatic;
	m_stats.kinematicBodies = bodyStats.mNumBodiesKinematic;
	m_stats.dynamicBodies = bodyStats.mNumBodiesDynamic;
	m_stats.sleepingBodies = bodyStats.mNumBodiesDynamic - bodyStats.mNumActiveBodiesDynamic;

	auto contactStats = m_physicsSystem->GetPhysicsSettings();
	m_stats.simulationSteps = contactStats.mNumVelocitySteps;

	const JPH::Constraints& constraints = m_physicsSystem->GetConstraints();
	m_stats.constraints = static_cast<int>(constraints.size());

	m_stats.bodyMemoryUsage = m_stats.totalBodies * sizeof(JPH::Body);
	m_stats.shapeMemoryUsage = m_stats.totalBodies * 100;

	m_stats.timeStep = m_fixedDeltaTime;

	auto end = std::chrono::high_resolution_clock::now();
	float statsUpdateTime = std::chrono::duration<float, std::milli>(end - start).count();

}

void PhysicsModule::CreateBody(flecs::entity entity, TransformComponent& transform, ColliderComponent& collider, PhysicsBodyComponent& physics)
{
	if (!entity.is_alive() || entity.has(flecs::Prefab) || !entity.has<TransformComponent>() || !entity.has<ColliderComponent>() || !entity.has<PhysicsBodyComponent>())
	{
		std::cout << "Has prefab: " << entity.has(flecs::Prefab) << '\n';
		return;
	}

	RemoveBody(entity);

	auto globalTform = Math::LocalToWorldTform(entity);

	JPH::ShapeSettings::ShapeResult shapeResult = ConstructJoltShape(entity, collider, globalTform);

	if (shapeResult.HasError())
	{
		std::cerr << "Failed to create shape for entity " << entity.name() << ": " << shapeResult.GetError().c_str() << std::endl;
		return;
	}

	JPH::Ref<JPH::Shape> shape = shapeResult.Get();
	JPH::BodyCreationSettings bodySettings(
		shape,
		ToJolt(globalTform.position),
		ToJoltQuat(globalTform.rotation),
		static_cast<JPH::EMotionType>(physics.motionType),
		JPH::ObjectLayer(physics.collisionLayer)
	);

	bodySettings.mAllowDynamicOrKinematic = true;

	if (physics.motionType == MotionType::DYNAMIC)
	{
		bodySettings.mMassPropertiesOverride.mMass = physics.mass;
		bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
	}

	bodySettings.mUserData = entity.id(); //Integration with flecs
	bodySettings.mRestitution = physics.restitution;
	bodySettings.mFriction = physics.friction;
	bodySettings.mGravityFactor = physics.gravityFactor;
	bodySettings.mIsSensor = collider.isSensor;

	if (collider.isSensor)
	{
		entity.add<SensorTag>();
		//m_sensorModule->RegisterSensor(entity);
	}

	JPH::Body* body = m_physicsSystem->GetBodyInterface().CreateBody(bodySettings);
	if (!body)
	{
		std::cerr << "Error: Failed to create body for entity " << entity.name() << std::endl;
		return;
	}

	m_physicsSystem->GetBodyInterface().AddBody(body->GetID(), JPH::EActivation::Activate);

	physics.bodyIdandSequence = body->GetID().GetIndexAndSequenceNumber();
	m_bodies[entity.id()] = body->GetID();
}


void PhysicsModule::RemoveBody(flecs::entity entity)
{
	// if (!entity.is_alive()) return;
	if (entity.has(flecs::Prefab)) return;

	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		JPH::BodyID bodyId = it->second;

		if (m_physicsSystem && m_physicsSystem->GetBodyInterface().IsAdded(bodyId))
		{

			JPH::Constraints constraints = m_physicsSystem->GetConstraints();
			for (JPH::Constraint* c : constraints)
			{
				if (c->GetType() == JPH::EConstraintType::TwoBodyConstraint)
				{
					auto* twoBodyC = static_cast<JPH::TwoBodyConstraint*>(c);

					if (twoBodyC->GetBody1() && twoBodyC->GetBody1()->GetID() == bodyId)
					{
						m_physicsSystem->RemoveConstraint(c);
						continue; 
					}

					if (twoBodyC->GetBody2() && twoBodyC->GetBody2()->GetID() == bodyId)
					{
						m_physicsSystem->RemoveConstraint(c);
					}
				}
			}

			m_physicsSystem->GetBodyInterface().DeactivateBody(bodyId);
			m_physicsSystem->GetBodyInterface().RemoveBody(bodyId);
			m_physicsSystem->GetBodyInterface().DestroyBody(bodyId);
		}

		m_bodies.erase(it);
	}

	if (entity.is_alive())
	{
		auto collider = entity.try_get<ColliderComponent>();
		if (collider && collider->isSensor)
		{
			entity.remove<SensorTag>();
		}

		if (auto* bodyComp = entity.try_get_mut<PhysicsBodyComponent>())
		{
			bodyComp->bodyIdandSequence = UINT32_MAX;
		}
	}
}

void PhysicsModule::UpdateJoltBody(flecs::entity entity)
{
	if (m_bodies.contains(entity.id())) {

		auto bodyID = m_bodies[entity.id()];

		auto globalTransform = Math::LocalToWorldTform(entity);
		m_physicsSystem->GetBodyInterface().SetPositionAndRotation(bodyID, ToJolt(globalTransform.position), ToJoltQuat(glm::normalize(globalTransform.rotation)), JPH::EActivation::Activate);
	}
	entity.children([&](flecs::entity child)
		{
			UpdateJoltBody(child);
		});
}

void PhysicsModule::UpdateJoltBodySettings(flecs::entity entity, const ColliderComponent& collider, PhysicsBodyComponent& physics)
{
	if (!m_bodies.contains(entity.id())) return;
	auto bodyID = m_bodies[entity.id()];

	JPH::ObjectLayer collisionLayer = (collider.isSensor && physics.motionType == MotionType::KINEMATIC) ? ObjectLayers::KINEMATIC_SENSOR : JPH::ObjectLayer(physics.collisionLayer);
	JPH::EMotionType motionType = static_cast<JPH::EMotionType>(physics.motionType);

	m_physicsSystem->GetBodyInterface().SetMotionType(bodyID, motionType, JPH::EActivation::DontActivate);
	m_physicsSystem->GetBodyInterface().SetObjectLayer(bodyID, collisionLayer);

	m_physicsSystem->GetBodyInterface().SetRestitution(bodyID, physics.restitution);
	m_physicsSystem->GetBodyInterface().SetFriction(bodyID, physics.friction);
	m_physicsSystem->GetBodyInterface().SetGravityFactor(bodyID, physics.gravityFactor);
}

void PhysicsModule::UpdateJoltCollider(flecs::entity entity, const ColliderComponent& collider, const TransformComponent& transform)
{
	if (!m_bodies.contains(entity.id())) return;
	auto bodyID = m_bodies[entity.id()];

	JPH::ShapeSettings::ShapeResult shapeResult = ConstructJoltShape(entity, collider, transform);

	if (shapeResult.HasError())
	{
		std::cerr << "Failed to update collider shape for entity " << entity.name() << ": " << shapeResult.GetError().c_str() << std::endl;
		return;
	}

	m_physicsSystem->GetBodyInterface().SetShape(bodyID, shapeResult.Get(), true, JPH::EActivation::DontActivate);
}

void PhysicsModule::UpdateMass(flecs::entity entity, const PhysicsBodyComponent& physics)
{
	if (!m_bodies.contains(entity.id())) return;
	auto bodyID = m_bodies[entity.id()];

	JPH::BodyLockWrite lock(m_physicsSystem->GetBodyLockInterface(), bodyID);
	if (lock.Succeeded()) {
		JPH::MotionProperties* motionProperties = lock.GetBody().GetMotionProperties();
		JPH::MassProperties massProperties = lock.GetBody().GetShape()->GetMassProperties();

		massProperties.ScaleToMass(physics.mass);
		motionProperties->SetMassProperties(JPH::EAllowedDOFs::All, massProperties);
	}
}

void PhysicsModule::UpdateIsSensor(flecs::entity entity, const ColliderComponent& collider, const PhysicsBodyComponent& physics)
{
	if (!m_bodies.contains(entity.id())) return;
	auto bodyID = m_bodies[entity.id()];

	JPH::BodyLockWrite lock(m_physicsSystem->GetBodyLockInterface(), bodyID);
	if (!lock.Succeeded())
	{
		std::cerr << "Failed to lock physics entity: " << entity.name() << std::endl;
		return;
	}

	if (lock.GetBody().IsSensor() == collider.isSensor) return;
	lock.GetBody().SetIsSensor(collider.isSensor);

	/*if (!collider.isSensor)
	{
		m_sensorModule->UnregisterSensor(entity);
		return;
	}

	m_sensorModule->RegisterSensor(entity);*/

	if (collider.isSensor)
	{
		entity.add<SensorTag>(); // Observer will register
	}
	else
	{
		entity.remove<SensorTag>(); // Observer will unregister
	}
}

void PhysicsModule::ClearAllBodies()
{
	for (auto& [entityId, bodyId] : m_bodies)
	{
		if (m_physicsSystem->GetBodyInterface().IsAdded(bodyId))
		{
			m_physicsSystem->GetBodyInterface().RemoveBody(bodyId);
			m_physicsSystem->GetBodyInterface().DestroyBody(bodyId);
		}
	}
	m_bodies.clear();

	m_world.each<PhysicsBodyComponent>([](flecs::entity entity, PhysicsBodyComponent& body)
		{
			body.bodyIdandSequence = UINT32_MAX;
		});
}

glm::vec3 PhysicsModule::GetGravity()
{
	//m_stats.gravity = ToGLM(m_physicsSystem->GetGravity());
	return m_stats.gravity;
}

void PhysicsModule::SetGravity(const glm::vec3& gravity)
{
	m_physicsSystem->SetGravity(ToJolt(gravity));
	m_stats.gravity = gravity;
}

void PhysicsModule::SetRestitution(flecs::entity entity, float value)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().SetRestitution(it->second, value);
	}
}

void PhysicsModule::SetFriction(flecs::entity entity, float value)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().SetFriction(it->second, value);
	}
}

void PhysicsModule::SetGravityFactor(flecs::entity entity, float value)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().SetGravityFactor(it->second, value);
	}
}

void PhysicsModule::AddForce(flecs::entity entity, const glm::vec3& force)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().AddForce(it->second, ToJolt(force));
	}
}

void PhysicsModule::AddImpulse(flecs::entity entity, const glm::vec3& impulse)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().AddImpulse(it->second, ToJolt(impulse));
	}
	else
	{
		//std::cout << entity.name() << ": physics body not found" << std::endl;
	}
}

void PhysicsModule::AddTorque(flecs::entity entity, const glm::vec3& torque)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().AddTorque(it->second, ToJolt(torque));
	}
}

void PhysicsModule::AddAngularImpulse(flecs::entity entity, const glm::vec3& angularImpulse)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().AddAngularImpulse(it->second, ToJolt(angularImpulse));
	}
}

void PhysicsModule::AddForceAtPosition(flecs::entity entity, const glm::vec3& force, const glm::vec3& position)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().AddForce(it->second, ToJolt(force), ToJolt(position));
	}
}

void PhysicsModule::AddImpulseAtPosition(flecs::entity entity, const glm::vec3& impulse, const glm::vec3& position)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().AddImpulse(it->second, ToJolt(impulse), ToJolt(position));
	}
}

void PhysicsModule::SetLinearVelocity(flecs::entity entity, const glm::vec3& velocity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().SetLinearVelocity(it->second, ToJolt(velocity));
	}
}


void PhysicsModule::SetAngularVelocity(flecs::entity entity, const glm::vec3& angularVelocity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().SetAngularVelocity(it->second, ToJolt(angularVelocity));
	}
}

glm::vec3 PhysicsModule::GetLinearVelocity(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		return ToGLM(m_physicsSystem->GetBodyInterface().GetLinearVelocity(it->second));
	}
	return glm::vec3(0.0f);
}

glm::vec3 PhysicsModule::GetAngularVelocity(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		return ToGLM(m_physicsSystem->GetBodyInterface().GetAngularVelocity(it->second));
	}
	return glm::vec3(0.0f);
}

void PhysicsModule::AddLinearVelocity(flecs::entity entity, const glm::vec3& velocity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		JPH::Vec3 currentVel = m_physicsSystem->GetBodyInterface().GetLinearVelocity(it->second);
		m_physicsSystem->GetBodyInterface().SetLinearVelocity(it->second, currentVel + ToJolt(velocity));
	}
}

void PhysicsModule::AddAngularVelocity(flecs::entity entity, const glm::vec3& angularVelocity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		JPH::Vec3 currentAngVel = m_physicsSystem->GetBodyInterface().GetAngularVelocity(it->second);
		m_physicsSystem->GetBodyInterface().SetAngularVelocity(it->second, currentAngVel + ToJolt(angularVelocity));
	}
}

void PhysicsModule::SetBodyPosition(flecs::entity entity, const glm::vec3& position)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().SetPosition(it->second, ToJolt(position), JPH::EActivation::DontActivate);
	}
}

void PhysicsModule::SetBodyRotation(flecs::entity entity, const glm::quat& rotation)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().SetRotation(it->second, ToJoltQuat(rotation), JPH::EActivation::DontActivate);
	}
}

void PhysicsModule::SetBodyPositionAndRotation(flecs::entity entity, const glm::vec3& position, const glm::quat& rotation)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().SetPositionAndRotation(it->second, ToJolt(position), ToJoltQuat(rotation), JPH::EActivation::DontActivate);
	}
}

glm::vec3 PhysicsModule::GetBodyPosition(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		return ToGLM(m_physicsSystem->GetBodyInterface().GetPosition(it->second));
	}
	return glm::vec3(0.0f);
}

glm::quat PhysicsModule::GetBodyRotation(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		return ToGLMQuat(m_physicsSystem->GetBodyInterface().GetRotation(it->second));
	}
	return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

glm::vec3 PhysicsModule::GetCenterOfMassPosition(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		return ToGLM(m_physicsSystem->GetBodyInterface().GetCenterOfMassPosition(it->second));
	}
	return glm::vec3(0.0f);
}

void PhysicsModule::ActivateBody(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().ActivateBody(it->second);
	}
}

void PhysicsModule::DeactivateBody(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().DeactivateBody(it->second);
	}
}

bool PhysicsModule::IsBodyActive(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		return m_physicsSystem->GetBodyInterface().IsActive(it->second);
	}
	return false;
}

bool PhysicsModule::IsBodyAdded(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		return m_physicsSystem->GetBodyInterface().IsAdded(it->second);
	}
	return false;
}

void PhysicsModule::SetBodyMotionType(flecs::entity entity, MotionType motionType)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().SetMotionType(it->second, static_cast<JPH::EMotionType>(motionType), JPH::EActivation::DontActivate);
	}
}

MotionType PhysicsModule::GetBodyMotionType(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), it->second);
		if (lock.Succeeded())
		{
			return static_cast<MotionType>(lock.GetBody().GetMotionType());
		}
	}
	return MotionType::STATIC;
}

void PhysicsModule::SetBodyMass(flecs::entity entity, float mass)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		JPH::BodyLockWrite lock(m_physicsSystem->GetBodyLockInterface(), it->second);
		if (lock.Succeeded())
		{
			JPH::MotionProperties* motionProps = lock.GetBody().GetMotionProperties();
			if (motionProps)
			{
				JPH::MassProperties massProps = lock.GetBody().GetShape()->GetMassProperties();
				massProps.ScaleToMass(mass);
				motionProps->SetMassProperties(JPH::EAllowedDOFs::All, massProps);
			}
		}
	}
}

float PhysicsModule::GetBodyMass(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), it->second);
		if (lock.Succeeded())
		{
			const JPH::MotionProperties* motionProps = lock.GetBody().GetMotionProperties();
			if (motionProps)
			{
				return 1.0f / motionProps->GetInverseMass();
			}
		}
	}
	return 0.0f;
}

void PhysicsModule::SetInverseMass(flecs::entity entity, float inverseMass)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		JPH::BodyLockWrite lock(m_physicsSystem->GetBodyLockInterface(), it->second);
		if (lock.Succeeded())
		{
			JPH::MotionProperties* motionProps = lock.GetBody().GetMotionProperties();
			if (motionProps)
			{
				motionProps->SetInverseMass(inverseMass);
			}
		}
	}
}

float PhysicsModule::GetInverseMass(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), it->second);
		if (lock.Succeeded())
		{
			const JPH::MotionProperties* motionProps = lock.GetBody().GetMotionProperties();
			if (motionProps)
			{
				return motionProps->GetInverseMass();
			}
		}
	}
	return 0.0f;
}

float PhysicsModule::GetRestitution(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		return m_physicsSystem->GetBodyInterface().GetRestitution(it->second);
	}
	return 0.0f;
}

float PhysicsModule::GetFriction(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		return m_physicsSystem->GetBodyInterface().GetFriction(it->second);
	}
	return 0.0f;
}

float PhysicsModule::GetGravityFactor(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), it->second);
		if (lock.Succeeded())
		{
			const JPH::MotionProperties* motionProps = lock.GetBody().GetMotionProperties();
			if (motionProps)
			{
				return motionProps->GetGravityFactor();
			}
		}
	}
	return 1.0f;
}

void PhysicsModule::SetObjectLayer(flecs::entity entity, CollisionLayer layer)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		m_physicsSystem->GetBodyInterface().SetObjectLayer(it->second, JPH::ObjectLayer(layer));
	}
}

CollisionLayer PhysicsModule::GetObjectLayer(flecs::entity entity)
{
	auto it = m_bodies.find(entity.id());
	if (it != m_bodies.end())
	{
		JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), it->second);
		if (lock.Succeeded())
		{
			return static_cast<CollisionLayer>(lock.GetBody().GetObjectLayer());
		}
	}
	return CollisionLayer::NON_MOVING;
}

RaycastHit PhysicsModule::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
{
	return m_physicsCasting->Raycast(origin, direction, maxDistance);
}

bool PhysicsModule::RaycastAny(const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
{
	return m_physicsCasting->RaycastAny(origin, direction, maxDistance);
}

std::vector<RaycastHit> PhysicsModule::RaycastAll(const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
{
	return m_physicsCasting->RaycastAll(origin, direction, maxDistance);
}

RaycastHit PhysicsModule::RaycastWithLayerFilter(const glm::vec3& origin, const glm::vec3& direction,
	float maxDistance, const std::vector<JPH::ObjectLayer>& ignoreLayers)
{
	return m_physicsCasting->RaycastWithLayerFilter(origin, direction, maxDistance, ignoreLayers);
}

std::vector<RaycastHit> PhysicsModule::RaycastAllWithLayerFilter(const glm::vec3& origin, const glm::vec3& direction,
	float maxDistance, const std::vector<JPH::ObjectLayer>& ignoreLayers)
{
	return m_physicsCasting->RaycastAllWithLayerFilter(origin, direction, maxDistance, ignoreLayers);
}

bool PhysicsModule::IsPointInside(const glm::vec3& point)
{
	return m_physicsCasting->IsPointInside(point);
}

std::vector<flecs::entity_t> PhysicsModule::GetEntitiesContainingPoint(const glm::vec3& point)
{
	return m_physicsCasting->GetEntitiesContainingPoint(point);
}

ShapecastHit PhysicsModule::ShapeCast(const JPH::Shape* shape, const glm::vec3& origin, const glm::quat& orientation,
	const glm::vec3& direction, float maxDistance)
{
	return m_physicsCasting->ShapeCast(shape, origin, orientation, direction, maxDistance);
}

bool PhysicsModule::ShapeCastAny(const JPH::Shape* shape, const glm::vec3& origin, const glm::quat& orientation,
	const glm::vec3& direction, float maxDistance)
{
	return m_physicsCasting->ShapeCastAny(shape, origin, orientation, direction, maxDistance);
}

ShapecastHit PhysicsModule::SweptShapeCast(const JPH::Shape* shape, const glm::vec3& startPos, const glm::vec3& endPos,
	const glm::quat& orientation)
{
	return m_physicsCasting->SweptShapeCast(shape, startPos, endPos, orientation);
}

ShapecastHit PhysicsModule::ShapeCastWithLayerFilter(const JPH::Shape* shape, const glm::vec3& origin,
	const glm::quat& orientation, const glm::vec3& direction, float maxDistance,
	const std::vector<JPH::ObjectLayer>& ignoreLayers)
{
	return m_physicsCasting->ShapeCastWithLayerFilter(shape, origin, orientation, direction, maxDistance, ignoreLayers);
}

ShapecastHit PhysicsModule::SphereCast(float radius, const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
{
	return m_physicsCasting->SphereCast(radius, origin, direction, maxDistance);
}

bool PhysicsModule::SphereCastAny(float radius, const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
{
	return m_physicsCasting->SphereCastAny(radius, origin, direction, maxDistance);
}

ShapecastHit PhysicsModule::SweptSphereCast(float radius, const glm::vec3& startPos, const glm::vec3& endPos)
{
	return m_physicsCasting->SweptSphereCast(radius, startPos, endPos);
}

ShapecastHit PhysicsModule::SphereCastWithLayerFilter(float radius, const glm::vec3& origin,
	const glm::vec3& direction, float maxDistance,
	const std::vector<JPH::ObjectLayer>& ignoreLayers)
{
	return m_physicsCasting->SphereCastWithLayerFilter(radius, origin, direction, maxDistance, ignoreLayers);
}

ShapecastHit PhysicsModule::BoxCast(const glm::vec3& halfExtents, const glm::vec3& origin, const glm::quat& orientation,
	const glm::vec3& direction, float maxDistance)
{
	return m_physicsCasting->BoxCast(halfExtents, origin, orientation, direction, maxDistance);
}

bool PhysicsModule::BoxCastAny(const glm::vec3& halfExtents, const glm::vec3& origin, const glm::quat& orientation,
	const glm::vec3& direction, float maxDistance)
{
	return m_physicsCasting->BoxCastAny(halfExtents, origin, orientation, direction, maxDistance);
}

ShapecastHit PhysicsModule::SweptBoxCast(const glm::vec3& halfExtents, const glm::vec3& startPos, const glm::vec3& endPos,
	const glm::quat& orientation)
{
	return m_physicsCasting->SweptBoxCast(halfExtents, startPos, endPos, orientation);
}

ShapecastHit PhysicsModule::BoxCastWithLayerFilter(const glm::vec3& halfExtents, const glm::vec3& origin,
	const glm::quat& orientation, const glm::vec3& direction, float maxDistance,
	const std::vector<JPH::ObjectLayer>& ignoreLayers)
{
	return m_physicsCasting->BoxCastWithLayerFilter(halfExtents, origin, orientation, direction, maxDistance, ignoreLayers);
}

std::vector<flecs::entity_t> PhysicsModule::OverlapSphere(const glm::vec3& center, float radius)
{
	return m_physicsCasting->OverlapSphere(center, radius);
}

bool PhysicsModule::OverlapSphereAny(const glm::vec3& center, float radius)
{
	return m_physicsCasting->OverlapSphereAny(center, radius);
}

std::vector<flecs::entity_t> PhysicsModule::OverlapBox(const glm::vec3& center, const glm::vec3& halfExtents,
	const glm::quat& orientation)
{
	return m_physicsCasting->OverlapBox(center, halfExtents, orientation);
}

bool PhysicsModule::OverlapBoxAny(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& orientation)
{
	return m_physicsCasting->OverlapBoxAny(center, halfExtents, orientation);
}

bool PhysicsModule::IsGrounded(flecs::entity entity, float checkDistance, float sphereRadius)
{
	if (!entity.has<TransformComponent>()) return false;

	glm::vec3 pos = Math::GetWorldPosition(entity);
	pos.y -= 0.1f;

	return SphereCastAny(sphereRadius, pos, glm::vec3(0, -1, 0), checkDistance);
}

RaycastHit PhysicsModule::GetGroundInfo(flecs::entity entity, float checkDistance)
{
	if (!entity.has<TransformComponent>()) return RaycastHit{};

	glm::vec3 pos = Math::GetWorldPosition(entity);
	return Raycast(pos, glm::vec3(0, -1, 0), checkDistance);
}

std::vector<flecs::entity_t> PhysicsModule::GetNearbyEntities(flecs::entity entity, float radius)
{
	if (!entity.has<TransformComponent>()) return {};

	glm::vec3 pos = Math::GetWorldPosition(entity);
	auto entities = OverlapSphere(pos, radius);

	entities.erase(std::remove(entities.begin(), entities.end(), entity.id()), entities.end());

	return entities;
}

bool PhysicsModule::IsEntityNearby(flecs::entity entity, flecs::entity_t targetEntityId, float radius)
{
	auto nearbyEntities = GetNearbyEntities(entity, radius);
	return std::find(nearbyEntities.begin(), nearbyEntities.end(), targetEntityId) != nearbyEntities.end();
}

JPH::Ref<JPH::DistanceConstraint> PhysicsModule::CreateDistanceConstraint(JPH::Body* bodyA, JPH::Body* bodyB, float distance)
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
	settings.mPoint1 = posA + direction * (distance * 0.1f);
	settings.mPoint2 = posB - direction * (distance * 0.1f);
	settings.mMaxDistance = distance;
	settings.mMinDistance = distance * 0.95f;

	return new JPH::DistanceConstraint(*bodyA, *bodyB, settings);
}

JPH::Ref<JPH::DistanceConstraint> PhysicsModule::CreatePinConstraint(JPH::Body* bodyA, JPH::Body* bodyB)
{
	JPH::DistanceConstraintSettings settings;

	settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
	settings.mPoint1 = bodyA->GetPosition() - bodyA->GetCenterOfMassPosition();
	settings.mPoint2 = settings.mPoint1;
	settings.mMaxDistance = 0.1f;
	settings.mMinDistance = 0.01f;

	return new JPH::DistanceConstraint(*bodyA, *bodyB, settings);
}

JPH::Ref<JPH::DistanceConstraint> PhysicsModule::CreateSpringConstraint(JPH::Body* bodyA, JPH::Body* bodyB, float distance, float stiffness, float damping)
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
	settings.mLimitsSpringSettings.mStiffness = stiffness;
	settings.mLimitsSpringSettings.mDamping = damping;
	settings.mPoint1 = posA + direction * (distance * 0.1f);
	settings.mPoint2 = posB - direction * (distance * 0.1f);
	settings.mMaxDistance = distance;
	settings.mMinDistance = distance * 0.95f;

	return new JPH::DistanceConstraint(*bodyA, *bodyB, settings);
}

void PhysicsModule::InitialiseDebugRenderer()
{
	m_debugRenderer = std::make_unique<PhysicsDebugRenderer>();

	m_debugRenderer->Initialise();
}

void PhysicsModule::RenderDebug()
{
	if (!m_debugRenderer || !m_physicsSystem) return;

	m_debugRenderer->Clear();

	if (m_debugSettings.enabled)
	{
		JPH::BodyManager::DrawSettings drawSettings;

		drawSettings.mDrawBoundingBox = m_debugSettings.drawBoundingBoxes;
		drawSettings.mDrawShapeWireframe = m_debugSettings.drawShapeWireframe;
		drawSettings.mDrawGetSupportFunction = m_debugSettings.drawGetSupportFunction;
		drawSettings.mDrawGetSupportingFace = m_debugSettings.drawGetSupportingFace;
		drawSettings.mDrawShape = m_debugSettings.drawShapes;
		drawSettings.mDrawVelocity = m_debugSettings.drawVelocity;
		drawSettings.mDrawMassAndInertia = m_debugSettings.drawMassAndInertia;
		drawSettings.mDrawSleepStats = m_debugSettings.drawSleepStats;
		drawSettings.mDrawCenterOfMassTransform = m_debugSettings.drawCenterOfMassTransform;
		drawSettings.mDrawWorldTransform = m_debugSettings.drawWorldTransform;

		switch (m_debugSettings.shapeColourMode)
		{
		case 0: drawSettings.mDrawShapeColor = JPH::BodyManager::EShapeColor::InstanceColor; break;
		case 1: drawSettings.mDrawShapeColor = JPH::BodyManager::EShapeColor::ShapeTypeColor; break;
		case 2: drawSettings.mDrawShapeColor = JPH::BodyManager::EShapeColor::MotionTypeColor; break;
		case 3: drawSettings.mDrawShapeColor = JPH::BodyManager::EShapeColor::SleepColor; break;
		case 4: drawSettings.mDrawShapeColor = JPH::BodyManager::EShapeColor::IslandColor; break;
		default: drawSettings.mDrawShapeColor = JPH::BodyManager::EShapeColor::InstanceColor; break;
		}

		if (m_debugSettings.drawBodies)
		{
			m_physicsSystem->DrawBodies(drawSettings, m_debugRenderer.get());
		}

		if (m_debugSettings.drawConstraints)
		{
			m_physicsSystem->DrawConstraints(m_debugRenderer.get());
		}
	}

	auto grimoriumModule = m_world.try_get_mut<GrimoriumModule>();
	if (grimoriumModule)
	{
		grimoriumModule->CallGrimoireDebugDraw();
	}

	m_cameraModule = m_world.try_get_mut<CameraModule>();
	if (m_cameraModule)
	{
		auto camera = m_cameraModule->GetMainCamera();
		m_debugRenderer->SetCameraPos(ToJolt(camera.try_get<TransformComponent>()->position));
		m_debugRenderer->Render(m_cameraModule->GetViewMatrix(camera), m_cameraModule->GetProjectionMatrix(camera));
	}
}

JPH::Color PhysicsModule::ToJoltColour(const glm::vec4& colour)
{
	return JPH::Color(
		static_cast<uint8_t>(glm::clamp(colour.r, 0.0f, 1.0f) * 255),
		static_cast<uint8_t>(glm::clamp(colour.g, 0.0f, 1.0f) * 255),
		static_cast<uint8_t>(glm::clamp(colour.b, 0.0f, 1.0f) * 255),
		static_cast<uint8_t>(glm::clamp(colour.a, 0.0f, 1.0f) * 255)
	);
}

void PhysicsModule::DrawLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& colour)
{
	if (!m_debugRenderer) return;
	m_debugRenderer->DrawLine(ToJolt(from), ToJolt(to), ToJoltColour(colour));
}

void PhysicsModule::DrawArrow(const glm::vec3& from, const glm::vec3& to, const glm::vec4& colour, float size)
{
	if (!m_debugRenderer) return;
	m_debugRenderer->DrawArrow(ToJolt(from), ToJolt(to), ToJoltColour(colour), size);
}

void PhysicsModule::DrawMarker(const glm::vec3& position, const glm::vec4& colour, float size)
{
	if (!m_debugRenderer) return;
	m_debugRenderer->DrawMarker(ToJolt(position), ToJoltColour(colour), size);
}

void PhysicsModule::DrawWireSphere(const glm::vec3& center, float radius, const glm::vec4& colour, int level)
{
	if (!m_debugRenderer) return;
	m_debugRenderer->DrawWireSphere(ToJolt(center), radius, ToJoltColour(colour), level);
}

void PhysicsModule::DrawSphere(const glm::vec3& center, float radius, const glm::vec4& colour, bool castShadow, bool wireframe)
{
	if (!m_debugRenderer) return;
	auto castShadowEnum = castShadow ? JPH::DebugRenderer::ECastShadow::On : JPH::DebugRenderer::ECastShadow::Off;
	auto drawMode = wireframe ? JPH::DebugRenderer::EDrawMode::Wireframe : JPH::DebugRenderer::EDrawMode::Solid;
	m_debugRenderer->DrawSphere(ToJolt(center), radius, ToJoltColour(colour), castShadowEnum, drawMode);
}

void PhysicsModule::DrawWireBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation, const glm::vec4& colour)
{
	if (!m_debugRenderer) return;

	JPH::RMat44 transform = JPH::RMat44::sTranslation(ToJolt(center)) * JPH::Mat44::sRotation(ToJoltQuat(rotation));
	JPH::AABox box(ToJolt(-halfExtents), ToJolt(halfExtents));

	m_debugRenderer->DrawWireBox(transform, box, ToJoltColour(colour));
}

void PhysicsModule::DrawBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation, const glm::vec4& colour, bool castShadow, bool wireframe)
{
	if (!m_debugRenderer) return;

	JPH::RMat44 transform = JPH::RMat44::sTranslation(ToJolt(center)) * JPH::Mat44::sRotation(ToJoltQuat(rotation));
	JPH::AABox box(ToJolt(-halfExtents), ToJolt(halfExtents));

	auto castShadowEnum = castShadow ? JPH::DebugRenderer::ECastShadow::On : JPH::DebugRenderer::ECastShadow::Off;
	auto drawMode = wireframe ? JPH::DebugRenderer::EDrawMode::Wireframe : JPH::DebugRenderer::EDrawMode::Solid;

	m_debugRenderer->DrawBox(transform, box, ToJoltColour(colour), castShadowEnum, drawMode);
}

void PhysicsModule::DrawWireTriangle(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec4& colour)
{
	if (!m_debugRenderer) return;
	m_debugRenderer->DrawWireTriangle(ToJolt(v1), ToJolt(v2), ToJolt(v3), ToJoltColour(colour));
}

void PhysicsModule::DrawTriangle(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec4& colour, bool castShadow)
{
	if (!m_debugRenderer) return;
	auto castShadowEnum = castShadow ? JPH::DebugRenderer::ECastShadow::On : JPH::DebugRenderer::ECastShadow::Off;
	m_debugRenderer->DrawTriangle(ToJolt(v1), ToJolt(v2), ToJolt(v3), ToJoltColour(colour), castShadowEnum);
}

void PhysicsModule::DrawCapsule(const glm::vec3& base, const glm::quat& rotation, float halfHeight, float radius, const glm::vec4& colour, bool castShadow, bool wireframe)
{
	if (!m_debugRenderer) return;

	JPH::RMat44 transform = JPH::RMat44::sTranslation(ToJolt(base)) * JPH::Mat44::sRotation(ToJoltQuat(rotation));

	auto castShadowEnum = castShadow ? JPH::DebugRenderer::ECastShadow::On : JPH::DebugRenderer::ECastShadow::Off;
	auto drawMode = wireframe ? JPH::DebugRenderer::EDrawMode::Wireframe : JPH::DebugRenderer::EDrawMode::Solid;

	m_debugRenderer->DrawCapsule(transform, halfHeight, radius, ToJoltColour(colour), castShadowEnum, drawMode);
}

void PhysicsModule::DrawCylinder(const glm::vec3& base, const glm::quat& rotation, float halfHeight, float radius, const glm::vec4& colour, bool castShadow, bool wireframe)
{
	if (!m_debugRenderer) return;

	JPH::RMat44 transform = JPH::RMat44::sTranslation(ToJolt(base)) * JPH::Mat44::sRotation(ToJoltQuat(rotation));

	auto castShadowEnum = castShadow ? JPH::DebugRenderer::ECastShadow::On : JPH::DebugRenderer::ECastShadow::Off;
	auto drawMode = wireframe ? JPH::DebugRenderer::EDrawMode::Wireframe : JPH::DebugRenderer::EDrawMode::Solid;

	m_debugRenderer->DrawCylinder(transform, halfHeight, radius, ToJoltColour(colour), castShadowEnum, drawMode);
}

void PhysicsModule::DrawCoordinateSystem(const glm::vec3& position, const glm::quat& rotation, float size)
{
	if (!m_debugRenderer) return;

	JPH::RMat44 transform = JPH::RMat44::sTranslation(ToJolt(position)) * JPH::Mat44::sRotation(ToJoltQuat(rotation));

	m_debugRenderer->DrawCoordinateSystem(transform, size);
}

void PhysicsModule::DrawPlane(const glm::vec3& point, const glm::vec3& normal, const glm::vec4& colour, float size)
{
	if (!m_debugRenderer) return;
	m_debugRenderer->DrawPlane(ToJolt(point), ToJolt(normal), ToJoltColour(colour), size);
}

void PhysicsModule::DrawPhysicsBodyInspector(flecs::entity entity, PhysicsBodyComponent& component)
{
	bool changed = false;
	bool hasPlaneCollider = false;

	if (entity.has<ColliderComponent>())
	{
		auto* collider = entity.try_get<ColliderComponent>();
		hasPlaneCollider = (ToJoltShape(collider->shapeSubType) == JPH::EShapeSubType::Plane);
	}

	if (ImGui::BeginTable("##physics_props", 2, ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("MotionType");
		ImGui::TableNextColumn();

		static const char* motionTypes[] = { "Static", "Kinematic", "Dynamic" };
		int selectedMotionType = (int)component.motionType;

		if (hasPlaneCollider)
		{
			ImGui::BeginDisabled(selectedMotionType == 2);
			ImGui::Combo("##MotionType", &selectedMotionType, motionTypes, IM_ARRAYSIZE(motionTypes));
			ImGui::EndDisabled();

			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "(Planes must be static)");

			if (component.motionType == MotionType::DYNAMIC)
			{
				component.motionType = MotionType::STATIC;
				component.mass = 0.0f;
				changed = true;
			}
		}
		else
		{
			if (ImGui::Combo("##MotionType", &selectedMotionType, motionTypes, IM_ARRAYSIZE(motionTypes)))
			{
				component.motionType = (MotionType)selectedMotionType;
				if (component.motionType == MotionType::DYNAMIC && component.mass <= 0.0f)
				{
					component.mass = 1.0f;
				}
				changed = true;
			}
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Collision Layer");
		ImGui::TableNextColumn();

		static const char* collisionLayers[] = { "NON_MOVING",
												"MOVING",
												"NO_COLLISION",
												"KINEMATIC_SENSOR",
												"ROPE",
												"ROPE_EVEN",
												"CHARACTER",
												"PROJECTILE",
												"PROJECTILE_ONLY"};

		int selectedCollisionLayer = (int)component.collisionLayer;

		if (ImGui::Combo("##CollisionLayer", &selectedCollisionLayer, collisionLayers, IM_ARRAYSIZE(collisionLayers)))
		{
			component.collisionLayer = (CollisionLayer)selectedCollisionLayer;
			changed = true;
		}

		if (component.motionType == MotionType::DYNAMIC)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Mass");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloat("##Mass", &component.mass, 0.01f, 0.001f, 1000.0f);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				UpdateMass(entity, component);
			}
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Restitution");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##Restitution", &component.restitution, 0.01f, 0.0f, 1.0f);
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			changed = true;
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Friction");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##Friction", &component.friction, 0.01f, 0.0f, 500.0f);
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			changed = true;
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Gravity Factor");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat("##GravityFactor", &component.gravityFactor, 0.01f, -10.0f, 10.0f);
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			changed = true;
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Body ID");
		ImGui::TableNextColumn();
		ImGui::Text("%u", component.bodyIdandSequence);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Sequence");
		ImGui::TableNextColumn();
		ImGui::Text("%u", component.bodyIdandSequence >> 23);

		if (component.bodyIdandSequence != UINT32_MAX)
		{
			auto it = m_bodies.find(entity.id());
			if (it != m_bodies.end())
			{
				JPH::BodyID bodyId = it->second;
				JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), bodyId);
				if (lock.Succeeded())
				{
					const JPH::Body& body = lock.GetBody();

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted("Active");
					ImGui::TableNextColumn();
					ImGui::TextColored(
						body.IsActive() ? ImVec4(0.60f, 0.85f, 0.70f, 1.00f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
						body.IsActive() ? "Yes" : "No");

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted("Static");
					ImGui::TableNextColumn();
					ImGui::Text(body.IsStatic() ? "Yes" : "No");

					if (component.motionType == MotionType::DYNAMIC)
					{
						JPH::Vec3 velocity = body.GetLinearVelocity();
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::AlignTextToFramePadding();
						ImGui::TextUnformatted("Velocity");
						ImGui::TableNextColumn();
						ImGui::Text("%.2f, %.2f, %.2f", velocity.GetX(), velocity.GetY(), velocity.GetZ());
					}
				}
			}

		}
		ImGui::EndTable();

		if (changed)
		{
			auto collider = entity.try_get<ColliderComponent>();
			UpdateJoltBodySettings(entity, *collider, component);
		}

		if (component.motionType == MotionType::DYNAMIC)
		{
			ImGui::Separator();
			ImGui::Text("Actions:");

			auto it = m_bodies.find(entity.id());
			if (it != m_bodies.end())
			{
				JPH::BodyID bodyId = it->second;

				if (ImGui::Button("Reset Velocity"))
				{
					m_physicsSystem->GetBodyInterface().SetLinearVelocity(bodyId, JPH::Vec3(0, 0, 0));
					m_physicsSystem->GetBodyInterface().SetAngularVelocity(bodyId, JPH::Vec3(0, 0, 0));
				}

				ImGui::SameLine();
				if (ImGui::Button("Apply Upward Force"))
				{
					AddImpulse(entity, glm::vec3(0.0f, component.mass * 10.0f, 0.0f));
				}

				ImGui::SameLine();
				if (ImGui::Button("Wake Up"))
				{
					m_physicsSystem->GetBodyInterface().ActivateBody(bodyId);
				}
			}
		}
	}
}

void PhysicsModule::DrawColliderInspector(flecs::entity entity, ColliderComponent& component)
{
	bool changed = false;

	if (ImGui::BeginTable("##collider_props", 2, ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Shape Type");
		ImGui::TableNextColumn();

		int currentType = 0;
		for (int i = 0; i < IM_ARRAYSIZE(ShapeTypes); ++i)
		{
			if (ShapeTypes[i] == ToJoltShape(component.shapeSubType))
			{
				currentType = i;
				break;
			}
		}

		if (ImGui::Combo("##ShapeType", &currentType, ShapeTypeNames, IM_ARRAYSIZE(ShapeTypeNames)))
		{
			JPH::EShapeSubType oldType = ToJoltShape(component.shapeSubType);
			component.shapeSubType = FromJoltShape(ShapeTypes[currentType]);

			switch (ToJoltShape(component.shapeSubType))
			{
			case JPH::EShapeSubType::Box:
				component.dimensions = glm::vec3(1.0f, 1.0f, 1.0f);
				break;
			case JPH::EShapeSubType::Sphere:
				component.dimensions = glm::vec3(0.5f, 0.0f, 0.0f);
				break;
			case JPH::EShapeSubType::Capsule:
				component.dimensions = glm::vec3(0.5f, 1.0f, 0.0f);
				break;
			case JPH::EShapeSubType::Cylinder:
				component.dimensions = glm::vec3(0.5f, 1.0f, 0.0f);
				break;
			case JPH::EShapeSubType::Plane:
				component.dimensions = glm::vec3(1000.0f, 0.0f, 0.0f);
				if (entity.has<PhysicsBodyComponent>())
				{
					auto* physicsBody = entity.try_get_mut<PhysicsBodyComponent>();
					if (physicsBody && physicsBody->motionType == MotionType::DYNAMIC)
					{
						physicsBody->motionType = MotionType::STATIC;
						physicsBody->mass = 0.0f;
					}
				}
				break;
			case JPH::EShapeSubType::ConvexHull:
				component.dimensions = glm::vec3(1.0f, 1.0f, 1.0f);
				component.meshAssetHandle = "";
				break;
			case JPH::EShapeSubType::Mesh:
				component.dimensions = glm::vec3(1.0f, 1.0f, 1.0f);
				component.meshAssetHandle = "";
				break;
			case JPH::EShapeSubType::StaticCompound:
				component.dimensions = glm::vec3(1.0f, 1.0f, 1.0f);
				component.subShapes.clear();
				{
					SubShapeData defaultSubShape;
					defaultSubShape.shapeType = ShapeType::Box;
					defaultSubShape.dimensions = glm::vec3(1.0f);
					component.subShapes.push_back(defaultSubShape);
				}
				break;
			}
			changed = true;
		}

		if (ToJoltShape(component.shapeSubType) == JPH::EShapeSubType::Plane && entity.has<PhysicsBodyComponent>())
		{
			auto* physicsBody = entity.try_get<PhysicsBodyComponent>();
			if (physicsBody && physicsBody->motionType == MotionType::DYNAMIC)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TableNextColumn();
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Warning: Planes should be static!");
			}
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();

		switch (ToJoltShape(component.shapeSubType))
		{
		case JPH::EShapeSubType::Box:
			ImGui::TextUnformatted("Dimensions");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloat3("##Dimensions", glm::value_ptr(component.dimensions), 0.1f, 0.01f, 100.0f);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				changed = true;
			}
			break;

		case JPH::EShapeSubType::Sphere:
			ImGui::TextUnformatted("Radius");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloat("##Radius", &component.dimensions.x, 0.1f, 0.01f, 100.0f);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				changed = true;
			}
			break;

		case JPH::EShapeSubType::Capsule:
			ImGui::TextUnformatted("Radius");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloat("##Radius", &component.dimensions.x, 0.1f, 0.01f, 100.0f);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				changed = true;
			}

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Height");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloat("##Height", &component.dimensions.y, 0.1f, 0.01f, 100.0f);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				changed = true;
			}
			break;

		case JPH::EShapeSubType::Cylinder:
			ImGui::TextUnformatted("Radius");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloat("##Radius", &component.dimensions.x, 0.1f, 0.01f, 100.0f);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				changed = true;
			}

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Height");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloat("##Height", &component.dimensions.y, 0.1f, 0.01f, 100.0f);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				changed = true;
			}
			break;

		case JPH::EShapeSubType::Plane:
			ImGui::TextUnformatted("Half Extent");
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloat("##HalfExtent", &component.dimensions.x, 1.0f, 1.0f, 10000.0f);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				changed = true;
			}
			break;

		case JPH::EShapeSubType::ConvexHull:
			ImGui::TextUnformatted("Mesh Asset");
			ImGui::TableNextColumn();

			{
				auto meshHandles = ResourceLibrary::GetAllMeshHandles();

				int currentIndex = -1;
				std::vector<const char*> displayHandles;
				displayHandles.push_back("None");

				for (int i = 0; i < meshHandles.size(); ++i)
				{
					displayHandles.push_back(meshHandles[i]);
					if (component.meshAssetHandle == meshHandles[i])
					{
						currentIndex = i + 1;
					}
				}

				if (currentIndex == -1) currentIndex = 0;

				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::Combo("##MeshAsset", &currentIndex, displayHandles.data(), displayHandles.size()))
				{
					if (currentIndex == 0)
					{
						component.meshAssetHandle = "";
					}
					else
					{
						component.meshAssetHandle = meshHandles[currentIndex - 1];
					}
					changed = true;
				}

				if (!component.meshAssetHandle.empty())
				{
					Mesh* mesh = ResourceLibrary::GetMesh(component.meshAssetHandle);
					ImGui::SameLine();
					if (mesh && mesh->HasPhysicsData())
					{
						ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "Active");
					}
					else
					{
						ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.6f, 1.0f), "Missing!");
					}
				}
			}
			break;

		case JPH::EShapeSubType::Mesh:
			ImGui::TextUnformatted("Mesh Asset");
			ImGui::TableNextColumn();

			{
				auto meshHandles = ResourceLibrary::GetAllMeshHandles();

				int currentIndex = -1;
				std::vector<const char*> displayHandles;
				displayHandles.push_back("None");

				for (int i = 0; i < meshHandles.size(); ++i)
				{
					displayHandles.push_back(meshHandles[i]);
					if (component.meshAssetHandle == meshHandles[i])
					{
						currentIndex = i + 1;
					}
				}

				if (currentIndex == -1) currentIndex = 0;

				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::Combo("##MeshAsset", &currentIndex, displayHandles.data(), displayHandles.size()))
				{
					if (currentIndex == 0)
					{
						component.meshAssetHandle = "";
					}
					else
					{
						component.meshAssetHandle = meshHandles[currentIndex - 1];
					}
					changed = true;
				}

				if (!component.meshAssetHandle.empty())
				{
					Mesh* mesh = ResourceLibrary::GetMesh(component.meshAssetHandle);
					ImGui::SameLine();
					if (mesh && mesh->HasPhysicsData())
					{
						ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "✓");
					}
					else
					{
						ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.6f, 1.0f), "Missing!");
					}
				}
			}
			break;

		case JPH::EShapeSubType::StaticCompound:
			ImGui::TextUnformatted("Sub-Shapes");
			ImGui::TableNextColumn();

			{
				ImGui::Text("%zu sub-shapes", component.subShapes.size());

				if (ImGui::Button("Add Sub-Shape"))
				{
					SubShapeData newSubShape;
					component.subShapes.push_back(newSubShape);
					changed = true;
				}

				ImGui::Separator();

				for (size_t i = 0; i < component.subShapes.size(); ++i)
				{
					ImGui::PushID(static_cast<int>(i));

					if (ImGui::TreeNode(("Sub-Shape " + std::to_string(i)).c_str()))
					{
						SubShapeData& subShape = component.subShapes[i];
						bool subShapeChanged = false;

						const char* subShapeTypeNames[] = { "Box", "Sphere", "Capsule", "Cylinder", "ConvexHull", "Mesh" };
						JPH::EShapeSubType subShapeTypes[] = {
							JPH::EShapeSubType::Box,
							JPH::EShapeSubType::Sphere,
							JPH::EShapeSubType::Capsule,
							JPH::EShapeSubType::Cylinder,
							JPH::EShapeSubType::ConvexHull,
							JPH::EShapeSubType::Mesh
						};

						int currentSubType = 0;
						for (int j = 0; j < IM_ARRAYSIZE(subShapeTypes); ++j)
						{
							if (subShapeTypes[j] == ToJoltShape(subShape.shapeType))
							{
								currentSubType = j;
								break;
							}
						}

						if (ImGui::Combo("Type", &currentSubType, subShapeTypeNames, IM_ARRAYSIZE(subShapeTypeNames)))
						{
							subShape.shapeType = FromJoltShape(subShapeTypes[currentSubType]);
							subShapeChanged = true;
						}


						if (ImGui::DragFloat3("Position", glm::value_ptr(subShape.position), 0.1f))
						{
							if (ImGui::IsItemDeactivatedAfterEdit()) subShapeChanged = true;
						}

						glm::vec3 eulerAngles = glm::degrees(glm::eulerAngles(subShape.rotation));
						if (ImGui::DragFloat3("Rotation", glm::value_ptr(eulerAngles), 1.0f))
						{
							if (ImGui::IsItemDeactivatedAfterEdit())
							{
								subShape.rotation = glm::quat(glm::radians(eulerAngles));
								subShapeChanged = true;
							}
						}

						switch (ToJoltShape(subShape.shapeType))
						{
						case JPH::EShapeSubType::Box:
							if (ImGui::DragFloat3("Dimensions", glm::value_ptr(subShape.dimensions), 0.1f, 0.01f, 100.0f))
							{
								if (ImGui::IsItemDeactivatedAfterEdit()) subShapeChanged = true;
							}
							break;
						case JPH::EShapeSubType::Sphere:
							if (ImGui::DragFloat("Radius", &subShape.dimensions.x, 0.1f, 0.01f, 100.0f))
							{
								if (ImGui::IsItemDeactivatedAfterEdit()) subShapeChanged = true;
							}
							break;
						case JPH::EShapeSubType::Capsule:
						case JPH::EShapeSubType::Cylinder:
							if (ImGui::DragFloat("Radius", &subShape.dimensions.x, 0.1f, 0.01f, 100.0f))
							{
								if (ImGui::IsItemDeactivatedAfterEdit()) subShapeChanged = true;
							}
							if (ImGui::DragFloat("Height", &subShape.dimensions.y, 0.1f, 0.01f, 100.0f))
							{
								if (ImGui::IsItemDeactivatedAfterEdit()) subShapeChanged = true;
							}
							break;
						case JPH::EShapeSubType::ConvexHull:
						case JPH::EShapeSubType::Mesh:
						{
							auto meshHandles = ResourceLibrary::GetAllMeshHandles();
							int currentIndex = -1;
							std::vector<const char*> displayHandles;
							displayHandles.push_back("None");

							for (int j = 0; j < meshHandles.size(); ++j)
							{
								displayHandles.push_back(meshHandles[j]);
								if (subShape.meshAssetHandle == meshHandles[j])
								{
									currentIndex = j + 1;
								}
							}

							if (currentIndex == -1) currentIndex = 0;

							if (ImGui::Combo("Mesh Asset", &currentIndex, displayHandles.data(), displayHandles.size()))
							{
								if (currentIndex == 0)
								{
									subShape.meshAssetHandle = "";
								}
								else
								{
									subShape.meshAssetHandle = meshHandles[currentIndex - 1];
								}
								subShapeChanged = true;
							}

							if (ImGui::DragFloat3("Scale", glm::value_ptr(subShape.dimensions), 0.1f, 0.01f, 100.0f))
							{
								if (ImGui::IsItemDeactivatedAfterEdit()) subShapeChanged = true;
							}
							break;
						}
						}

						int userDataInt = static_cast<int>(subShape.userData);
						if (ImGui::DragInt("User Data", &userDataInt, 1.0f, 0, INT_MAX))
						{
							if (ImGui::IsItemDeactivatedAfterEdit())
							{
								subShape.userData = static_cast<uint32_t>(userDataInt);
								subShapeChanged = true;
							}
						}

						if (ImGui::Button("Remove Sub-Shape"))
						{
							component.subShapes.erase(component.subShapes.begin() + i);
							changed = true;
							ImGui::TreePop();
							ImGui::PopID();
							break;
						}

						if (subShapeChanged) changed = true;

						ImGui::TreePop();
					}

					ImGui::PopID();
				}
			}
			break;
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Offset");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat3("##Offset", glm::value_ptr(component.offset), 0.1f, -100.0f, 100.0f);
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			changed = true;
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Is Sensor");
		ImGui::TableNextColumn();
		if (ImGui::Checkbox("##IsSensor", &component.isSensor))
		{
			auto physics = entity.try_get<PhysicsBodyComponent>();
			UpdateIsSensor(entity, component, *physics);
		}

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Sync with transform");
		ImGui::TableNextColumn();
		if (ImGui::Checkbox("##SyncWithTransform", &component.syncWithTransform))
		{
			if (component.syncWithTransform)
			{
				auto transform = entity.try_get<TransformComponent>();
				SyncColliderWithTransform(entity, component, *transform);
			}
		}

		ImGui::EndTable();
	}

	if (changed)
	{
		auto transform = entity.try_get<TransformComponent>();
		UpdateJoltCollider(entity, component, *transform);
	}
}

ImVec4 PhysicsModule::GetPhysicsEntityColour(flecs::entity entity)
{
	if (entity.has<PhysicsBodyComponent>())
	{
		const auto* body = entity.try_get<PhysicsBodyComponent>();

		switch (body->motionType)
		{
		case MotionType::STATIC:
			return ImVec4(0.85f, 0.65f, 0.65f, 1.0f);
		case MotionType::KINEMATIC:
			return ImVec4(0.65f, 0.65f, 1.0f, 1.0f);
		case MotionType::DYNAMIC:
			return ImVec4(0.65f, 0.85f, 0.65f, 1.0f);
		}
	}

	return ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
}

JPH::BodyID PhysicsModule::GetBodyID(flecs::entity_t entityId)
{
	if (m_bodies.contains(entityId))
	{
		return m_bodies.at(entityId);
	}
	else
	{
		return JPH::BodyID();
	}
}

flecs::entity PhysicsModule::BodyToEntity(const JPH::Body& body)
{
	return m_world.get_alive(body.GetUserData());
}

flecs::entity_t PhysicsModule::BodyIDToEntityID(JPH::BodyID bodyID)
{
	for (auto value : m_bodies)
	{
		if (value.second == bodyID) return value.first;
	}
	//Check the character module aswell
	auto charModule = m_world.try_get_mut<CharacterModule>();
	auto entityId = charModule->BodyIDtoEntityID(bodyID);

	return entityId; //returns 0 if it also couldnt be found in the character module
}

glm::vec3 PhysicsModule::ToGLM(const JPH::Vec3& vec)
{
	return glm::vec3(vec.GetX(), vec.GetY(), vec.GetZ());
}

glm::quat PhysicsModule::ToGLMQuat(const JPH::Quat& q)
{
	return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}

JPH::Vec3 PhysicsModule::ToJolt(const glm::vec3& vec)
{
	return JPH::Vec3(vec.x, vec.y, vec.z);
}

JPH::Quat PhysicsModule::ToJoltQuat(const glm::quat& q)
{
	return JPH::Quat(q.x, q.y, q.z, q.w);
}

JPH::EShapeSubType PhysicsModule::ToJoltShape(ShapeType shape)
{
	return (JPH::EShapeSubType)shape;
}

ShapeType PhysicsModule::FromJoltShape(JPH::EShapeSubType joltShape)
{
	return (ShapeType)joltShape;
}