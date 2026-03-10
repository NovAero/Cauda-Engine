#include "cepch.h"
#include "ConstraintModule.h"
#include "Core/Utilities/STLModule.h"
#include "Core/Physics/PhysicsModule.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Physics/RopeModule.h"
#include "Core/Character/CharacterModule.h"

ConstraintModule::ConstraintModule(flecs::world& world) :
	m_world(world)
{
	m_world.import<CharacterModule>();
	m_physicsModule = m_world.try_get_mut<PhysicsModule>();

	SetupComponents();
	SetupObservers();
	SetupQueries();
	SetupSystems();

	RegisterWithEditor();
}

ConstraintModule::~ConstraintModule()
{
}

void ConstraintModule::SetupComponents()
{
	m_world.component<ReloadConstraints>();

	m_world.component<Cauda::ConstraintType>()
		.constant("NONE", Cauda::ConstraintType::NONE)
		.constant("PIN", Cauda::ConstraintType::PIN)
		.constant("DISTANCE", Cauda::ConstraintType::DISTANCE);

	m_world.component<Cauda::ConstraintSettings>()
		.member<Cauda::ConstraintType>("type")
		.member<std::string>("entityA")
		.member<std::string>("entityB")
		.member<bool>("worldSpace");

	m_world.component<Cauda::PinConstraintSettings>()
		.member<Cauda::ConstraintSettings>("inheritedSettings")
		.member<glm::vec3>("mPoint1")
		.member<glm::vec3>("mPoint2");
		
	m_world.component<Cauda::DistanceConstraintSettings>()
		.member<Cauda::ConstraintSettings>("inheritedSettings")
		.member<float>("minDistance")
		.member<float>("maxDistance");

	m_world.component<std::vector<Cauda::DistanceConstraintSettings>>()
		.opaque(std_vector_ser<Cauda::DistanceConstraintSettings>);

	m_world.component<std::vector<Cauda::PinConstraintSettings>>()
		.opaque(std_vector_ser<Cauda::PinConstraintSettings>);

	m_world.component<PinConstraintComponent>()
		.member<std::vector<Cauda::PinConstraintSettings>>("constraintSettings");

	m_world.component<DistanceConstraintComponent>()
		.member<std::vector<Cauda::DistanceConstraintSettings>>("constraintSettings");
}

void ConstraintModule::SetupObservers()
{
	m_physbodyDeletedCleanupObserver = m_world.observer<PhysicsBodyComponent>("OnRemovePhysicsBodyConstraintCleanup")
		.filter()
		.event(flecs::OnRemove)
		.each([&](flecs::entity entity, PhysicsBodyComponent&)
			{
				RemoveAllConstraintsFromEntity(entity);
			});

	m_characterDeletedCleanupObserver = m_world.observer<CharacterComponent>("OnCharacterRemovedConstraintCleanup")
		.filter()
		.event(flecs::OnRemove)
		.each([&](flecs::entity entity, CharacterComponent&)
			{
				RemoveAllConstraintsFromEntity(entity);
			});

	m_pinConstraintAddedObserver = m_world.observer<PinConstraintComponent>()
		.event<ReloadConstraints>()
		.each([&](flecs::entity entity, PinConstraintComponent& comp)
			{
				for (auto& settings : comp.m_constraintSettings)
				{
					auto entityB = m_world.lookup(settings.entityB.c_str());
					if (entityB)
					{
						std::cout << "Created managed pin constraint\n";
						CreateManagedPinConstraint(entity, entityB, settings);
					}
				}
			});

	m_pinConstraintRemovedObserver = m_world.observer<PinConstraintComponent>()
		.event(flecs::OnRemove)
		.each([&](flecs::entity entity, PinConstraintComponent& comp)
			{
				while(!comp.m_constraintSettings.empty())
				{
					//Constraint not created, just remove the settings
					if (comp.m_constraintSettings.back().index == -1)
					{
						comp.m_constraintSettings.pop_back();
						continue;
					}
					auto& info = comp.m_constraintSettings.back();
					auto entityB = m_world.lookup(info.entityB.c_str());
					if (entityB)
					{
						TryRemoveConstraintFromEntity({ entity, entityB }, info.index);

					}
					comp.m_constraintSettings.pop_back();
				}
			});

	m_distanceConstraintAddedObserver = m_world.observer<DistanceConstraintComponent>()
		.event<ReloadConstraints>()
		.each([&](flecs::entity entity, DistanceConstraintComponent& comp)
			{
				for (auto& settings : comp.m_constraintSettings)
				{
					auto entityB = m_world.lookup(settings.entityB.c_str());
					if (entityB)
					{
						std::cout << "Created managed distance constraint\n";
						CreateManagedDistanceConstraint(entity, entityB, settings);
					}
				}
			});

	m_distanceConstraintRemovedObserver = m_world.observer<DistanceConstraintComponent>()
		.event(flecs::OnRemove)
		.each([&](flecs::entity entity, DistanceConstraintComponent& comp)
			{
				while (!comp.m_constraintSettings.empty())
				{
					//Constraint not created, just remove the settings
					if (comp.m_constraintSettings.back().index == -1) 
					{
						comp.m_constraintSettings.pop_back();
						continue;
					}
					auto& info = comp.m_constraintSettings.back();
					auto entityB = m_world.lookup(info.entityB.c_str());
					if (entityB)
					{
						TryRemoveConstraintFromEntity({ entity, entityB }, info.index);

					}
					comp.m_constraintSettings.pop_back();
				}
			});
}

void ConstraintModule::SetupQueries()
{
	m_constrainableEntitiesQuery = m_world.query_builder<PhysicsBodyComponent, ColliderComponent>()
		.without<RopeSegment>().build();
	m_ropeSegmentEntityQuery = m_world.query_builder<RopeSegment>().build();
	m_characterEntityQuery = m_world.query_builder<CharacterComponent>()
		.build();
}

void ConstraintModule::SetupSystems()
{
}

void ConstraintModule::OnImGuiRender()
{
}

bool ConstraintModule::CreateManagedDistanceConstraint(flecs::entity entityA, flecs::entity entityB, float minDistance, float maxDistance)
{
	BodyPair pair = GetBodyPair(entityA, entityB);

	if (pair.bodyA && pair.bodyB)
	{
		ConstraintKey key = { entityA.id(), entityB.id() };
		ConstraintContainer* container = TryFindContainer(key, true);

		Cauda::DistanceConstraintSettings settings = {};
		settings.entityA = entityA.path();
		settings.entityB = entityB.path();
		settings.minDistance = minDistance;
		settings.maxDistance = maxDistance;

		JPH::Constraint* constraint = CreateDistanceConstraint(pair.bodyA, pair.bodyB, settings);
		if (constraint)
		{
			m_physicsModule->GetPhysicsSystem()->AddConstraint(constraint);
			settings.index = container->data.size();

			if (!entityA.has<DistanceConstraintComponent>())
			{
				entityA.set<DistanceConstraintComponent>({ settings });
			}
			else
			{
				auto pinComp = entityA.try_get_mut<DistanceConstraintComponent>();
				pinComp->m_constraintSettings.push_back(settings);
			}

			container->data.push_back(ConstraintData{ constraint, settings });
			SyncContainer(*container);
			return true;
		}
	}
	return false;
}

bool ConstraintModule::CreateManagedDistanceConstraint(flecs::entity entityA, flecs::entity entityB, Cauda::DistanceConstraintSettings& settings)
{
	BodyPair pair = GetBodyPair(entityA, entityB);

	if (pair.bodyA && pair.bodyB)
	{
		ConstraintKey key = { entityA.id(), entityB.id() };
		ConstraintContainer* container = TryFindContainer(key, true);

		JPH::Constraint* constraint = CreateDistanceConstraint(pair.bodyA, pair.bodyB, settings);
		if (constraint)
		{
			m_physicsModule->GetPhysicsSystem()->AddConstraint(constraint);
			settings.index = container->data.size();
			container->data.push_back(ConstraintData{ constraint, settings });
			SyncContainer(*container);
			return true;
		}
	}
	return false;
}

bool ConstraintModule::CreateManagedPinConstraint(flecs::entity entityA, flecs::entity entityB, glm::vec3 offsetA, glm::vec3 offsetB)
{
	BodyPair pair = GetBodyPair(entityA, entityB);

	if (pair.bodyA && pair.bodyB)
	{
		ConstraintKey key = { entityA.id(), entityB.id() };
		ConstraintContainer* container = TryFindContainer(key, true);
		
		Cauda::PinConstraintSettings settings;
		settings.type = Cauda::ConstraintType::PIN;
		settings.entityA = entityA.path();
		settings.entityB = entityB.path();
		settings.mPoint1 = offsetA;
		settings.mPoint2 = offsetB;

		JPH::Constraint* constraint = CreatePinConstraint(pair.bodyA, pair.bodyB, settings);
		if (constraint)
		{
			m_physicsModule->GetPhysicsSystem()->AddConstraint(constraint);
			settings.index = container->data.size();
			
			if (!entityA.has<PinConstraintComponent>())
			{
				entityA.set<PinConstraintComponent>({settings});
			}
			else
			{
				auto pinComp = entityA.try_get_mut<PinConstraintComponent>();
				pinComp->m_constraintSettings.push_back(settings);
			}

			container->data.push_back(ConstraintData{ constraint, settings });
			SyncContainer(*container);
			return true;
		}
	}
	return false;
}

bool ConstraintModule::CreateManagedPinConstraint(flecs::entity entityA, flecs::entity entityB, Cauda::PinConstraintSettings& settings)
{
	BodyPair pair = GetBodyPair(entityA, entityB);

	if (pair.bodyA && pair.bodyB)
	{
		ConstraintKey key = { entityA.id(), entityB.id() };
		ConstraintContainer* container = TryFindContainer(key, true);

		JPH::Constraint* constraint = CreatePinConstraint(pair.bodyA, pair.bodyB, settings);
		if (constraint)
		{
			m_physicsModule->GetPhysicsSystem()->AddConstraint(constraint);
			settings.index = container->data.size();
			container->data.push_back(ConstraintData{ constraint, settings });
			SyncContainer(*container);
			return true;
		}
	}
	return false;
}

void ConstraintModule::RemoveConstraintFromEntity(ConstraintKey key, Cauda::ConstraintSettings settings)
{
	if (settings.index == -1) return;
	auto container = TryFindContainer(key);
	if (container)
	{
		if (RemoveConstraintFromSystem(container->data.at(settings.index).constraint))
		{
			auto entityA = m_world.get_alive(key >> 0);

			//Find constraint settings with the correct key and index - remove it
			switch (settings.type)
			{
			case Cauda::ConstraintType::PIN:
			{
				auto pinComp = entityA.try_get_mut<PinConstraintComponent>();
				if (pinComp)
				{
					for (int i = 0; i < pinComp->m_constraintSettings.size(); ++i)
					{
						auto info = pinComp->m_constraintSettings[i];
						if (info.entityA == settings.entityA && info.entityB == settings.entityB && info.index == settings.index)
						{
							pinComp->m_constraintSettings.erase(pinComp->m_constraintSettings.begin() + i);
							break;
						}
					}
				}
				break;
			}
			case Cauda::ConstraintType::DISTANCE:
			{
				auto distComp = entityA.try_get_mut<DistanceConstraintComponent>();
				if (distComp)
				{
					for (int i = 0; i < distComp->m_constraintSettings.size(); ++i)
					{
						auto info = distComp->m_constraintSettings[i];
						if (info.entityA == settings.entityA && info.entityB == settings.entityB && info.index == settings.index)
						{
							distComp->m_constraintSettings.erase(distComp->m_constraintSettings.begin() + i);
							break;
						}
					}
				}
				break;
			} 
			}

			//Erase from settings
			container->data.erase(container->data.begin() + settings.index);

			//Resync all components that have their settings in this container
			SyncContainer(*container);
		}
	}
}

void ConstraintModule::RemoveAllConstraintsFromEntity(flecs::entity entity)
{
	auto constraints = GetConstraintsSettingsForEntity(entity);
	if (constraints.empty()) return;

	std::sort(constraints.begin(), constraints.end(), [&](Cauda::ConstraintSettings& settingsA, Cauda::ConstraintSettings& settingsB) -> bool
		{
			//Sort so higher index comes first
			return settingsA.index < settingsB.index;
		});

	while (!constraints.empty())
	{
		auto& data = constraints.back();

		auto entityA = m_world.lookup(data.entityA.c_str());
		auto entityB = m_world.lookup(data.entityB.c_str());

		if (entityA && entityB)
		{
			RemoveConstraintFromEntity({ entityA, entityB }, data);
		}

		constraints.pop_back();
	}
}

bool ConstraintModule::TryRemoveConstraintFromEntity(ConstraintKey key, int index)
{
	if (index < 0) return false;
	auto container = TryFindContainer(key);
	if (container)
	{
		if (index >= container->data.size()) return false;
		RemoveConstraintFromEntity(key, container->data[index].settings);
		std::cout << "Removed constraint\n";
		return true;
	}
	return false;
}

JPH::Constraint* ConstraintModule::CreateDistanceConstraint(JPH::Body* bodyA, JPH::Body* bodyB, Cauda::DistanceConstraintSettings settings)
{
	JPH::DistanceConstraintSettings constraintSettings;

	JPH::Vec3 posA = bodyA->GetPosition();
	JPH::Vec3 posB = bodyB->GetPosition();
	JPH::Vec3 dirVec = posB - posA;

	if (dirVec.IsClose(JPH::Vec3::sZero(), 1e-6f))
	{
		dirVec = JPH::Vec3(0, -1, 0);
	}

	//TODO - clamp min distance to not be able to go above max distance

	JPH::Vec3 direction = dirVec.Normalized();
	constraintSettings.mPoint1 = posA;
	constraintSettings.mPoint2 = posB;
	constraintSettings.mMaxDistance = settings.maxDistance;
	constraintSettings.mMinDistance = settings.minDistance;

	return new JPH::DistanceConstraint(*bodyA, *bodyB, constraintSettings);
}

JPH::Constraint* ConstraintModule::CreatePinConstraint(JPH::Body* bodyA, JPH::Body* bodyB, Cauda::PinConstraintSettings settings)
{
	JPH::PointConstraintSettings constraintSettings;

	constraintSettings.mSpace = (JPH::EConstraintSpace)settings.worldSpace;
	constraintSettings.mPoint1 = PhysicsModule::ToJolt(settings.mPoint2);
	constraintSettings.mPoint2 = PhysicsModule::ToJolt(settings.mPoint1);

	return new JPH::PointConstraint(*bodyA, *bodyB, constraintSettings);
}

JPH::Constraint* ConstraintModule::CreateSpringConstraint(JPH::Body* bodyA, JPH::Body* bodyB, Cauda::SpringConstraintSettings settings)
{
	JPH::DistanceConstraintSettings constraintSettings;

	JPH::Vec3 posA = bodyA->GetPosition();
	JPH::Vec3 posB = bodyB->GetPosition();
	JPH::Vec3 dirVec = posB - posA;

	if (dirVec.IsClose(JPH::Vec3::sZero(), 1e-6f))
	{
		dirVec = JPH::Vec3(0, -1, 0);
	}

	JPH::Vec3 direction = dirVec.Normalized();
	constraintSettings.mPoint1 = posA + direction * (settings.maxDistance * 0.1f);
	constraintSettings.mPoint2 = posB - direction * (settings.maxDistance * 0.1f);
	constraintSettings.mMaxDistance = settings.maxDistance;
	constraintSettings.mMinDistance = settings.minDistance;
	constraintSettings.mLimitsSpringSettings.mStiffness = settings.stiffness;
	constraintSettings.mLimitsSpringSettings.mDamping = settings.damping;

	return new JPH::DistanceConstraint(*bodyA, *bodyB, constraintSettings);
}

std::vector<const ConstraintData*> ConstraintModule::GetConstraintsForEntity(flecs::entity entity)
{
	std::vector<const ConstraintData*> ret;
	for (auto* container : m_managedConstraints)
	{
		for (auto& data : container->data)
		{
			if (container->key[entity.id()])
			{
				ret.push_back(&data);
			}
		}
	}
	return ret;
}

std::vector<const JPH::DistanceConstraint*> ConstraintModule::GetDistanceConstraintsForEntity(flecs::entity entity)
{
	std::vector<const JPH::DistanceConstraint*> ret;

	for (auto& container : m_managedConstraints)
	{
		for (auto& data : container->data)
		{
			if (container->key[entity.id()])
			{
				JPH::Constraint* rawPtr = data.constraint.GetPtr();

				if (rawPtr->GetSubType() == JPH::EConstraintSubType::Distance)
				{
					ret.push_back(static_cast<JPH::DistanceConstraint*>(rawPtr));
				}
			}
		}
	}
	return ret;
}

std::vector<const JPH::PointConstraint*> ConstraintModule::GetPinConstraintsForEntity(flecs::entity entity)
{

	std::vector<const JPH::PointConstraint*> ret;

	for (auto& container : m_managedConstraints)
	{
		for (auto& data : container->data)
		{
			if (container->key[entity.id()])
			{
				JPH::Constraint* rawPtr = data.constraint.GetPtr();

				if (rawPtr->GetSubType() == JPH::EConstraintSubType::Point)
				{
					ret.push_back(static_cast<JPH::PointConstraint*>(rawPtr));
				}
			}
		}
	}
	return ret;
	
}

ConstraintContainer* ConstraintModule::TryFindContainer(ConstraintKey key, bool createIfNot)
{
	ConstraintContainer* out = nullptr;
	//Find container
	for (auto& it : m_managedConstraints)
	{
		if (it->key == key)
		{
			out = it;
		}
	}
	//Create if not found
	if (!out && createIfNot)
	{
		out = new ConstraintContainer();
		out->key = key;
		m_managedConstraints.push_back(out);
	}

	return out;
}

ConstraintModule::BodyPair ConstraintModule::GetBodyPair(flecs::entity entityA, flecs::entity entityB)
{
	BodyPair pair;

	if (entityA.has<CharacterComponent>())
	{
		auto characterModule = m_world.try_get_mut<CharacterModule>();
		auto bodyID = characterModule->GetBodyID(entityA);
		if (bodyID != JPH::BodyID())
		{
			pair.bodyA = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface().TryGetBody(bodyID);
		}
	}
	else if (entityA.has<PhysicsBodyComponent>())
	{
		pair.bodyA = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface().TryGetBody((JPH::BodyID)entityA.get_mut<PhysicsBodyComponent>().bodyIdandSequence);
	}

	if (entityB.has<CharacterComponent>())
	{
		auto characterModule = m_world.try_get_mut<CharacterModule>();
		auto bodyID = characterModule->GetBodyID(entityB);
		if (bodyID != JPH::BodyID())
		{
			pair.bodyB = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface().TryGetBody(bodyID);
		}
	}
	else if (entityB.has<PhysicsBodyComponent>())
	{
		pair.bodyB = m_physicsModule->GetPhysicsSystem()->GetBodyLockInterface().TryGetBody((JPH::BodyID)entityB.get_mut<PhysicsBodyComponent>().bodyIdandSequence);
	}

	return pair;
}

bool ConstraintModule::RemoveConstraintFromSystem(JPH::Constraint* constraint)
{
	if (constraint)
	{
		m_physicsModule->GetPhysicsSystem()->RemoveConstraint(constraint);
		return true;
	}
	return false;
}

Cauda::PinConstraintSettings ConstraintModule::CreateEmptyPinConstraint(flecs::entity entity)
{
	Cauda::PinConstraintSettings settings = {};
	settings.entityA = entity.path();
	settings.type = Cauda::ConstraintType::PIN;

	return settings;
}

Cauda::DistanceConstraintSettings ConstraintModule::CreateEmptyDistanceConstraint(flecs::entity entity)
{
	Cauda::DistanceConstraintSettings settings = {};
	settings.entityA = entity.path();
	settings.type = Cauda::ConstraintType::DISTANCE;

	return settings;
}

void ConstraintModule::SyncContainer(ConstraintContainer& container)
{
	//Get entityA's component
	auto entityA = m_world.get_alive(container.key >> 0);
	auto entityB = m_world.get_alive(container.key >> 1);
	auto pinComp = entityA.try_get_mut<PinConstraintComponent>();
	auto distComp = entityA.try_get_mut<DistanceConstraintComponent>();

	std::vector<Cauda::ConstraintSettings*> settingsPtrs;

	//Find all the constraints with the same key and store them as pointers
	//We can assume entityA is the same amongst these, because it came from A's components
	if (pinComp)
	{
		for (auto& info : pinComp->m_constraintSettings)
		{
			if (info.entityB == entityB.path().c_str() && info.index != -1)
			{
				settingsPtrs.push_back(&info);
			}
		}
	}
	if (distComp)
	{
		for (auto& info : distComp->m_constraintSettings)
		{
			if (info.entityB == entityB.path().c_str() && info.index != -1)
			{
				settingsPtrs.push_back(&info);
			}
		}
	}

	//Update the indices in them
	if (settingsPtrs.size() != 0)
	{
		for (int i = 0; i < container.data.size(); i++)
		{
			container.data[i].settings.index = i;
			settingsPtrs[i]->index = i;
		}
	}
}

void ConstraintModule::UpdateConstraint(flecs::entity entity, Cauda::ConstraintSettings& settings)
{
	if (!entity || settings.entityA == "" || settings.entityB == "" || settings.entityA == settings.entityB) return;
	flecs::entity entityB = entity.world().lookup(settings.entityB.c_str());
	
	if (!entityB) return; //Skip if entityB doesnt exist

	BodyPair pair = GetBodyPair(entity, entityB);

	if (!pair.bodyA || !pair.bodyB) return; //Skip if they don't have assigned jolt bodies

	JPH::Constraint* constraint = CreateConstraint(pair.bodyA, pair.bodyB, &settings);

	if (!constraint) return;

	ConstraintKey key = { entity.id(), entityB.id() };
	ConstraintContainer* container = TryFindContainer(key, true);

	if (!container) return; //Something went horribly wrong

	if (container->data.size() == 0 || settings.index == -1)
	{
		settings.index = container->data.size();
		m_physicsModule->GetPhysicsSystem()->AddConstraint(constraint);
		ConstraintData newData = ConstraintData(constraint, settings);
		container->data.push_back(newData);
	}
	else
	{
		container->data.at(settings.index).settings = settings;

		//Recreate the constraint with the settings
		if (container->data.at(settings.index).constraint)
		{
			m_physicsModule->GetPhysicsSystem()->RemoveConstraint(container->data.at(settings.index).constraint);
			//delete container->data.at(settings.index).constraint; 
		}
		m_physicsModule->GetPhysicsSystem()->AddConstraint(constraint);
		container->data.at(settings.index).constraint = constraint;
		SyncContainer(*container);
	}
}

void ConstraintModule::PreUpdateConstraint(flecs::entity entityA, flecs::entity previous, Cauda::ConstraintSettings& settings)
{
	if (previous)
	{
		if (previous.path().c_str() != settings.entityB)
		{
			//Remove constraint from system
			if (auto container = TryFindContainer({ entityA, previous }))
			{
				RemoveConstraintFromSystem(container->data.at(settings.index).constraint);
				//Erase from settings
				container->data.erase(container->data.begin() + settings.index);
				//Resync all components that have their settings in this container
				SyncContainer(*container);

				auto newB = m_world.lookup(settings.entityB.c_str());
				if (newB)
				{
					container = TryFindContainer({ entityA, newB }, true);
					if (container)
					{
						settings.index = container->data.size();
						container->data.push_back({ nullptr, settings });
					}
				}
			}
		}
	}
}

JPH::Constraint* ConstraintModule::CreateConstraint(JPH::Body* bodyA, JPH::Body* bodyB, Cauda::ConstraintSettings* settings)
{
	if (!bodyA || !bodyB || !settings) return nullptr;

	switch (settings->type)
	{
	case Cauda::ConstraintType::PIN:
	{
		auto* pinPtr = reinterpret_cast<Cauda::PinConstraintSettings*>(settings);
		if (pinPtr)
		{
			return CreatePinConstraint(bodyA, bodyB, *pinPtr);
		}
		return nullptr;
	} break;
	case Cauda::ConstraintType::DISTANCE:
	{
		auto* distPtr = reinterpret_cast<Cauda::DistanceConstraintSettings*>(settings);
		if (distPtr)
		{
			return CreateDistanceConstraint(bodyA, bodyB, *distPtr);
		}
		return nullptr;
	}
	default:
		return nullptr;
	}
}

void ConstraintModule::CleanupManagedConstraints()
{
	auto* physicsSystem = m_physicsModule->GetPhysicsSystem();

	for (auto& container : m_managedConstraints)
	{
		for (auto& data : container->data)
		{
			if (data.constraint != nullptr)
			{
				physicsSystem->RemoveConstraint(data.constraint);
			}
		}
		container->data.clear(); 
		delete container;
	}
	m_managedConstraints.clear();
}

std::vector<Cauda::ConstraintSettings> ConstraintModule::GetConstraintsSettingsForEntity(flecs::entity entity)
{
	std::vector<Cauda::ConstraintSettings> ret;
	for (auto* container : m_managedConstraints)
	{
		for (auto& data : container->data)
		{
			if (container->key[entity.id()])
			{
				ret.push_back(data.settings);
			}
		}
	}
	return ret;
}

void ConstraintModule::DrawPinConstraintInspector(flecs::entity entity, PinConstraintComponent& component)
{
	bool changed = false;
	int changedIndex = 0;

	static bool showRopeEntities = false;
	ImGui::Checkbox("Show Rope Entities", &showRopeEntities);

	std::vector<flecs::entity> connectableEntities;
	std::vector<const char*> connectableEntitiesNames;
	m_characterEntityQuery.each([&](flecs::entity entity, CharacterComponent& character)
		{
			connectableEntities.push_back(entity);
			connectableEntitiesNames.push_back(entity.name());
		});
	m_constrainableEntitiesQuery.each([&](flecs::entity entity, PhysicsBodyComponent& body, ColliderComponent& collider)
		{
			connectableEntities.push_back(entity);
			connectableEntitiesNames.push_back(entity.name());
		});
	if (showRopeEntities)
	{
		m_ropeSegmentEntityQuery.each([&](flecs::entity entity, RopeSegment& segment)
			{
				connectableEntities.push_back(entity);
				connectableEntitiesNames.push_back(entity.name().c_str());
			});
	}
	std::vector<Cauda::PinConstraintSettings>& settings = component.m_constraintSettings;

	flecs::entity previousB;

	if (ImGui::BeginTable("##pin_constraints", 2, ImGuiTableFlags_Resizable | ImGuiTableRowFlags_Headers))
	{
		
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		for (int i = 0; i < settings.size(); ++i)
		{
			auto pointSettings = &settings[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushID(i);

			ImGui::Text("Constraint %i", i);
			ImGui::TextDisabled("Pair Index: %i", pointSettings->index);

			ImGui::TableNextColumn();

			if (i > 0 && settings.size() > 1)
			{
				ImGui::Separator();
			}

			static int selected_idx = 0;
			std::string displayedName = pointSettings->entityB;
			if (ImGui::BeginCombo("Entity B", displayedName.c_str()))
			{
				static ImGuiTextFilter filter;
				if (ImGui::IsWindowAppearing())
				{
					ImGui::SetKeyboardFocusHere();
					filter.Clear();
				}
				ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
				filter.Draw("##Filter", -FLT_MIN);

				for (int idx = 0; idx < connectableEntities.size(); ++idx)
				{
					if (connectableEntities[idx].id() == entity.id()) continue;
					const bool isSelected = (selected_idx == idx);
					if (filter.PassFilter(connectableEntitiesNames[idx]))
					{
						if (ImGui::Selectable(connectableEntitiesNames[idx], isSelected))
						{
							selected_idx = idx; 
							previousB = m_world.lookup(pointSettings->entityB.c_str());
							pointSettings->entityB = connectableEntities[selected_idx].path();
						}
						if (ImGui::IsItemDeactivatedAfterEdit()) {
							changedIndex = i;
							changed = true;
						}
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (pointSettings->index != -1)
			{
				ImGui::SliderFloat3("Point A", glm::value_ptr(pointSettings->mPoint1), -25.f, 25.f);
				if (ImGui::IsItemDeactivatedAfterEdit()) {
					changedIndex = i;
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("Reset A"))
				{
					pointSettings->mPoint1 = glm::vec3(0);
					changedIndex = i;
					changed = true;
				}

				ImGui::SliderFloat3("Point B", glm::value_ptr(pointSettings->mPoint2), -25.f, 25.f);
				if (ImGui::IsItemDeactivatedAfterEdit()) {
					changedIndex = i;
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("Reset B"))
				{
					pointSettings->mPoint2 = glm::vec3(0);
					changedIndex = i;
					changed = true;
				}

				if (ImGui::Checkbox("World Space", &pointSettings->worldSpace))
				{
					changedIndex = i;
					changed = true;
				}
			}

			if (ImGui::Button("Remove Constraint"))
			{
				if (pointSettings->index == -1)
				{
					settings.erase(settings.begin() + i);
				}
				else
				{
					auto entityB = m_world.lookup(pointSettings->entityB.c_str());
					if (entityB)
					{
						RemoveConstraintFromEntity({ entity, entityB }, *pointSettings);
					}
				}
				entity.modified<PinConstraintComponent>();
			}

			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (ImGui::Button("Add Constraint"))
	{
		settings.push_back(CreateEmptyPinConstraint(entity));
	}

	if (changed && changedIndex < component.m_constraintSettings.size() && changedIndex >= 0)
	{
		PreUpdateConstraint(entity, previousB, component.m_constraintSettings[changedIndex]);
		UpdateConstraint(entity, component.m_constraintSettings[changedIndex]);
		entity.modified<PinConstraintComponent>();
	}
}

void ConstraintModule::DrawDistanceConstraintInspector(flecs::entity entity, DistanceConstraintComponent& component)
{
	bool changed = false;
	int changedIndex = 0;

	static bool showRopeEntities = false;
	ImGui::Checkbox("Show Rope Entities", &showRopeEntities);

	std::vector<flecs::entity> connectableEntities;
	std::vector<const char*> connectableEntitiesNames;
	m_characterEntityQuery.each([&](flecs::entity entity, CharacterComponent& character)
		{
			connectableEntities.push_back(entity);
			connectableEntitiesNames.push_back(entity.name());
		});
	m_constrainableEntitiesQuery.each([&](flecs::entity entity, PhysicsBodyComponent& body, ColliderComponent& collider)
		{
			connectableEntities.push_back(entity);
			connectableEntitiesNames.push_back(entity.name());
		});
	if (showRopeEntities)
	{
		m_ropeSegmentEntityQuery.each([&](flecs::entity entity, RopeSegment& segment)
			{
				connectableEntities.push_back(entity);
				connectableEntitiesNames.push_back(entity.name().c_str());
			});
	}

	std::vector<Cauda::DistanceConstraintSettings>& settings = component.m_constraintSettings;

	flecs::entity previousB;

	if (ImGui::BeginTable("##distance_constraints", 2, ImGuiTableFlags_Resizable | ImGuiTableRowFlags_Headers))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		for (int i = 0; i < settings.size(); ++i)
		{
			auto distSettings = &settings[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushID(i);

			ImGui::Text("Constraint %i", i);
			ImGui::TextDisabled("Pair Index: %i", distSettings->index);

			ImGui::TableNextColumn();

			if (i > 0 && settings.size() > 1)
			{
				ImGui::Separator();
			}

			static int selected_idx = 0;
			std::string displayedName = distSettings->entityB;
			if (ImGui::BeginCombo("Entity B", displayedName.c_str()))
			{
				static ImGuiTextFilter filter;
				if (ImGui::IsWindowAppearing())
				{
					ImGui::SetKeyboardFocusHere();
					filter.Clear();
				}
				ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
				filter.Draw("##Filter", -FLT_MIN);

				for (int idx = 0; idx < connectableEntities.size(); ++idx)
				{
					if (connectableEntities[idx].id() == entity.id()) continue;
					const bool isSelected = (selected_idx == idx);
					if (filter.PassFilter(connectableEntitiesNames[idx]))
					{
						if (ImGui::Selectable(connectableEntitiesNames[idx], isSelected))
						{
							selected_idx = idx;
							previousB = m_world.lookup(distSettings->entityB.c_str());
							distSettings->entityB = connectableEntities[selected_idx].path();
						}
						if (ImGui::IsItemDeactivatedAfterEdit()) {
							changedIndex = i;
							changed = true;
						}
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if (distSettings->index != -1)
			{
				if (!distSettings->fixed) {

					ImGui::SliderFloat("Min Distance", &distSettings->minDistance, 0.f, 100.f);
					if (ImGui::IsItemDeactivatedAfterEdit()) {
						changedIndex = i;
						changed = true;
					}
					if (ImGui::Button("Reset Min"))
					{
						distSettings->minDistance = 0;
						changedIndex = i;
						changed = true;
					}

					ImGui::SliderFloat("Max Distance", &distSettings->maxDistance, 0.f, 100.f);
					if (ImGui::IsItemDeactivatedAfterEdit()) {
						changedIndex = i;
						changed = true;
					}
					if (ImGui::Button("Reset Max"))
					{
						distSettings->maxDistance = 0;
						changedIndex = i;
						changed = true;
					}
				}
				else
				{
					if (ImGui::SliderFloat("Distance", &distSettings->maxDistance, 0.f, 100.0f))
					{
						distSettings->minDistance = distSettings->maxDistance;
						changedIndex = i;
						changed = true;
					}

					if (ImGui::Button("Reset"))
					{
						distSettings->maxDistance = 1;
						distSettings->minDistance = 1;
						changedIndex = i;
						changed = true;
					}
				}

				if (ImGui::Checkbox("Fixed", &distSettings->fixed))
				{
					distSettings->minDistance = distSettings->maxDistance;
				}

				if (ImGui::Checkbox("World Space", &distSettings->worldSpace))
				{
					changedIndex = i;
					changed = true;
				}
			}

			if (ImGui::Button("Remove Constraint"))
			{
				if (distSettings->index == -1)
				{
					settings.erase(settings.begin() + i);
				}
				else
				{
					auto entityB = m_world.lookup(distSettings->entityB.c_str());
					if (entityB)
					{
						RemoveConstraintFromEntity({ entity, entityB }, *distSettings);
					}
				}
				entity.modified<DistanceConstraintComponent>();
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (ImGui::Button("Add Constraint"))
	{
		settings.push_back(CreateEmptyDistanceConstraint(entity));
	}

	if (changed && changedIndex < component.m_constraintSettings.size() && changedIndex >= 0)
	{
		PreUpdateConstraint(entity, previousB, component.m_constraintSettings[changedIndex]);
		UpdateConstraint(entity, component.m_constraintSettings[changedIndex]);
		entity.modified<DistanceConstraintComponent>();
	}
}

void ConstraintModule::RegisterWithEditor()
{
	auto editorModule = m_world.try_get_mut<EditorModule>();
	if (editorModule)
	{
		editorModule->RegisterComponent<PinConstraintComponent>("Pin Constraints", "Constraints",
			[this](flecs::entity entity, PinConstraintComponent& comp)
			{
				DrawPinConstraintInspector(entity, comp);
			},
			[](flecs::entity entity) -> bool
			{
				return true;
			});

		editorModule->RegisterComponent<DistanceConstraintComponent>("Distance Constraints", "Constraints",
			[this](flecs::entity entity, DistanceConstraintComponent& comp)
			{
				DrawDistanceConstraintInspector(entity, comp);
			},
			[](flecs::entity entity) -> bool
			{
				return true;
			});
	}
}