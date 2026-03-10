#include "cepch.h"
#include "SensorModule.h"
#include "PhysicsModule.h"
#include "Core/Physics/RopeModule.h"
#include "Core/Components/GrabComponent.h"
#include "Core/Physics/PhysicsUserData.h"
#include "Core/Editor/GrimoriumModule.h"

SensorModule::SensorModule(flecs::world& world) :
	m_world(world)
{
	m_physicsModule = m_world.try_get_mut<PhysicsModule>();

	SetupComponents();
	SetupObservers();
	SetupSystems();

	m_physicsModule->GetPhysicsSystem()->SetContactListener(this);
}

SensorModule::~SensorModule()
{
	for (auto sensor : m_storedSensorData)
	{
		delete sensor.second;
	}
}

void SensorModule::ProcessEvents()
{
	PollRemovedEvents();
	DispatchSensorData();
}

JPH::ValidateResult SensorModule::OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult)
{
	return JPH::ValidateResult::AcceptContact;
}

void SensorModule::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
	uint64_t id1 = inBody1.GetUserData();
	uint64_t id2 = inBody2.GetUserData();

	if (id1 == 0 || id2 == 0) return; 

	if (inBody1.IsSensor() && !inBody2.IsSensor())
	{
		m_eventQueue.enqueue(SensorEvent{ id1, id2, SensorEvent::Type::ADDED });
	}
	else if (!inBody1.IsSensor() && inBody2.IsSensor())
	{
		m_eventQueue.enqueue(SensorEvent{ id2, id1, SensorEvent::Type::ADDED });
	}

	m_eventQueue.enqueue(SensorEvent{ id1, id2, SensorEvent::Type::GRIMOIRE_ADDED });
	m_eventQueue.enqueue(SensorEvent{ id2, id1, SensorEvent::Type::GRIMOIRE_ADDED });
}

void SensorModule::OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
	uint64_t id1 = inBody1.GetUserData();
	uint64_t id2 = inBody2.GetUserData();

	if (id1 == 0 || id2 == 0) return;

	if (inBody1.IsSensor() && !inBody2.IsSensor())
	{
		m_eventQueue.enqueue(SensorEvent{ id1, id2, SensorEvent::Type::PERSISTED });
	}
	else if (!inBody1.IsSensor() && inBody2.IsSensor())
	{
		m_eventQueue.enqueue(SensorEvent{ id2, id1, SensorEvent::Type::PERSISTED });
	}

	m_eventQueue.enqueue(SensorEvent{ id1, id2, SensorEvent::Type::GRIMOIRE_PERSISTED });
	m_eventQueue.enqueue(SensorEvent{ id2, id1, SensorEvent::Type::GRIMOIRE_PERSISTED });
}

void SensorModule::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair)
{
	// We handle removals via polling in PollRemovedEvents
}

void SensorModule::RegisterSensor(flecs::entity owner)
{
	auto collider = owner.try_get_mut<ColliderComponent>();
	if (collider)
	{
		SensorData* data = new SensorData{ .owningEntity = owner };

		m_storedSensorData.try_emplace(owner.id(), data);
		if (!m_storedSensorData.contains(owner.id()))
		{
			delete data;
		}
	}
}

void SensorModule::UnregisterSensor(flecs::entity owner)
{
	auto collider = owner.try_get_mut<ColliderComponent>();
	if (collider)
	{
		SensorData* data = nullptr;

		auto it = m_storedSensorData.find(owner.id());
		if (it != m_storedSensorData.end())
		{
			data = m_storedSensorData.at(owner.id());
		}
		m_storedSensorData.erase(owner.id());

		if (data)
			delete data;
	}
}

const SensorData& SensorModule::GetSensorDataForEntity(flecs::entity entity)
{
	if (!m_storedSensorData.contains(entity.id()))
	{
		static const SensorData emptyData = {};
		return emptyData;
	}
	const auto& data = *m_storedSensorData.at(entity.id());
	return data;
}

void SensorModule::PollRemovedEvents()
{
	for (auto& [owner, sensorData] : m_storedSensorData)
	{
		if (!owner)
		{
			std::cout << "entity doesnt exist\n";
			continue;
		}
		std::vector<flecs::entity> entitiesToRemove;

		for (auto& [entity, ticks] : sensorData->overlappedEntities)
		{
			if (!entity.is_alive())
			{
				entitiesToRemove.push_back(entity);
				continue;
			}

			ticks++;
			if (ticks > UPDATES_BEFORE_REMOVE)
			{
				entitiesToRemove.push_back(entity);
			}
		}

		for (const auto& entity : entitiesToRemove)
		{
			if (sensorData->overlappedEntities.contains(entity))
			{
				m_eventQueue.enqueue(SensorEvent{
					sensorData->owningEntity.id(),
					entity.id(),
					SensorEvent::Type::REMOVED
					});
				m_eventQueue.enqueue(SensorEvent{
					sensorData->owningEntity.id(),
					entity.id(),
					SensorEvent::Type::GRIMOIRE_REMOVED
					});
				m_eventQueue.enqueue(SensorEvent{
					entity.id(),
					sensorData->owningEntity.id(),
					SensorEvent::Type::GRIMOIRE_REMOVED
					});
			}
		}
	}
}

void SensorModule::SetupComponents()
{
	m_world.component<SensorTag>();
}

void SensorModule::SetupObservers()
{
	m_world.observer<SensorTag>("OnAddSensor")
		.event(flecs::OnAdd)
		.each([this](flecs::entity e, SensorTag sensorTag)
		{
			RegisterSensor(e);
		});

	m_world.observer<SensorTag>("OnRemoveSensor")
		.event(flecs::OnRemove)
		.event(flecs::OnDelete)
		.each([this](flecs::entity e, SensorTag sensorTag)
		{
			UnregisterSensor(e);
		});
}

void SensorModule::SetupSystems()
{
	flecs::entity GameTick = Cauda::Application::GetGameTickSource();

	m_world.system("SensorModuleUpdate")
		.kind(flecs::OnValidate)
		.tick_source(GameTick)
		.run([this](flecs::iter& it)
		{
			ProcessEvents();
		});

	//m_world.system("PollRemovedEvents")
	//	.kind(flecs::OnValidate)
	//	.tick_source(GameTick)
	//	.run([this](flecs::iter& it)
	//	{
	//		PollRemovedEvents();
	//	});
}

void SensorModule::DispatchSensorData()
{
	SensorEvent event;

	while (m_eventQueue.try_dequeue(event))
	{
		flecs::entity owner(m_world, event.ownerId);
		flecs::entity sensedEntity(m_world, event.sensedEntityId);

		if (!owner.is_alive() || !sensedEntity.is_alive())
		{
			continue;
		}

		if (owner.parent() == sensedEntity || sensedEntity.parent() == owner)
		{
			continue;
		}

		switch (event.type)
		{
		case SensorEvent::Type::GRIMOIRE_ADDED:
			CallGrimoireCollisions(owner, sensedEntity, SensorEvent::Type::ADDED);

			continue; 
		case SensorEvent::Type::GRIMOIRE_PERSISTED:
			CallGrimoireCollisions(owner, sensedEntity, SensorEvent::Type::PERSISTED);

			continue; 
		case SensorEvent::Type::GRIMOIRE_REMOVED:
			CallGrimoireCollisions(owner, sensedEntity, SensorEvent::Type::REMOVED);

			continue; 
		default:
			break;
		}


		if (!m_storedSensorData.contains(owner.id()))
		{
			continue;
		}

		auto collider = owner.try_get_mut<ColliderComponent>();
		if (collider)
		{
			switch (event.type)
			{
			case SensorEvent::Type::ADDED:
			{
				auto& overlapped = m_storedSensorData.at(owner.id())->overlappedEntities;
				if (!overlapped.contains(sensedEntity))
				{
					overlapped.insert({ sensedEntity, 0 });
					if (collider->onContactAdded)
					{
						collider->onContactAdded(GetSensorDataForEntity(owner), sensedEntity);
					}
				}
			} break;
			case SensorEvent::Type::PERSISTED:
			{
				auto& overlapped = m_storedSensorData.at(owner.id())->overlappedEntities;
				if (overlapped.contains(sensedEntity)) 
				{
					overlapped[sensedEntity] = 0; 
				}

				if (collider->onContactPersisted)
				{
					collider->onContactPersisted(GetSensorDataForEntity(owner), sensedEntity);
				}
			} break;
			case SensorEvent::Type::REMOVED:
			{
				auto& overlapped = m_storedSensorData.at(owner.id())->overlappedEntities;

				if (overlapped.contains(sensedEntity))
				{
					overlapped.erase(sensedEntity);

					if (collider->onContactRemoved)
					{
						collider->onContactRemoved(GetSensorDataForEntity(owner), sensedEntity);
					}
				}
			} break;
			}
		}
	}
}

void SensorModule::CallGrimoireCollisions(flecs::entity entity, flecs::entity other, SensorEvent::Type eventType)
{
	if (!entity.is_valid() || !entity.has<GrimoireComponent>()) return;
	auto* grimorium = m_world.try_get_mut<GrimoriumModule>();
	if (!grimorium) return;

	const auto& scripts = grimorium->GetRegisteredScripts();
	for (const auto& script : scripts)
	{
		if (script.has(entity))
		{
			if (Grimoire* grimoire = script.get_grimoire(entity))
			{
				switch (eventType)
				{
				case SensorEvent::Type::ADDED:
					grimoire->OnCollisionEnter(entity, other);
					break;
				case SensorEvent::Type::PERSISTED:
					grimoire->OnCollisionStay(entity, other);
					break;
				case SensorEvent::Type::REMOVED:
					grimoire->OnCollisionExit(entity, other);
					break;
				}
			}
		}
	}
}