#pragma once
#include "JoltPhysics.h"
#include <ThirdParty/flecs.h>
#include <unordered_map>
#include <concurrentqueue.h>
#include <map>

class PhysicsModule;
struct ColliderComponent;

constexpr int UPDATES_BEFORE_REMOVE = 5;

struct SensorData
{
	flecs::entity owningEntity;
	std::map<flecs::entity, int> overlappedEntities = {};
};

struct SensorEvent
{
	enum class Type {
		ADDED,
		PERSISTED,
		REMOVED,
		GRIMOIRE_ADDED,
		GRIMOIRE_PERSISTED,
		GRIMOIRE_REMOVED
	};

	uint64_t ownerId;      
	uint64_t sensedEntityId; 
	Type type;
};

struct SensorTag {};

class SensorModule : JPH::ContactListener
{
public:
	SensorModule(flecs::world& world);
	~SensorModule();

	void ProcessEvents();

	virtual JPH::ValidateResult	OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override;
	virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;
	virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;
	virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;

	void RegisterSensor(flecs::entity owner);
	void UnregisterSensor(flecs::entity owner);
	const SensorData& GetSensorDataForEntity(flecs::entity entity);

private:
	void PollRemovedEvents();

	flecs::world& m_world;
	PhysicsModule* m_physicsModule;

	moodycamel::ConcurrentQueue<SensorEvent> m_eventQueue;

	std::unordered_map<flecs::entity_t, SensorData*> m_storedSensorData;

	void SetupComponents();
	void SetupObservers();
	void SetupSystems();

	void DispatchSensorData();
	void CallGrimoireCollisions(flecs::entity entity, flecs::entity other, SensorEvent::Type eventType);
};