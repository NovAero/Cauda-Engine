#pragma once
#include <ThirdParty/flecs.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Core/Physics/JoltPhysics.h"
#include <vector>
#include <string>
#include <unordered_map>

class PhysicsModule;
class AnimationModule;
struct AnimationComponent;
struct SkeletalMeshComponent;


namespace ozz
{
	namespace animation
	{
		class Skeleton;
	}
}

enum class RagdollBodyShape
{
	Capsule,
	Box,
	Sphere
};

struct RagdollBoneConfig
{
	std::string boneName;
	RagdollBodyShape shape = RagdollBodyShape::Capsule;
	glm::vec3 dimensions = glm::vec3(0.5f, 0.5f, 0.5f);
	float mass = 1.0f;
};

struct RagdollJointConfig
{
	std::string parentBone;
	std::string childBone;
	glm::vec3 twistAxisLocal = glm::vec3(0, 1, 0);
	float twistMinAngle = -glm::radians(45.0f);
	float twistMaxAngle = glm::radians(45.0f);
	float normalHalfConeAngle = glm::radians(30.0f);
	float planeHalfConeAngle = glm::radians(30.0f);
};

struct RagdollBone
{
	int boneIndex = -1;
	std::string boneName;
};

struct RagdollComponent
{
	JPH::Ref<JPH::Ragdoll> joltRagdoll;
	std::vector<RagdollBone> bones;
	std::unordered_map<std::string, int> boneNameToIndex;

	float motorStrength = 300000.0f;
	float motorDamping = 10000.0f;

	bool isInitialised = false;
	bool enabled = true;

	bool useKinematicMode = true;
};

class RagdollModule
{
public:
	RagdollModule(flecs::world& world);
	~RagdollModule();

	bool CreateRagdoll(flecs::entity entity,
		const std::vector<RagdollBoneConfig>& boneConfigs,
		const std::vector<RagdollJointConfig>& jointConfigs);

	void DestroyRagdoll(flecs::entity entity);

	void AddForceAtBone(flecs::entity entity, const std::string& boneName, const glm::vec3& force);
	void AddImpulseAtBone(flecs::entity entity, const std::string& boneName, const glm::vec3& impulse);

	void SetMotorStrength(flecs::entity entity, float strength);
	void SetMotorDamping(flecs::entity entity, float damping);

	JPH::BodyID GetBoneBodyID(flecs::entity entity, const std::string& boneName) const;

	bool CreateRagdollFromSkeleton(flecs::entity entity, float defaultBoneRadius = .1f, float defaultBoneMass = 1.0f);

private:
	flecs::world& m_world;
	PhysicsModule* m_physicsModule = nullptr;
	AnimationModule* m_animModule = nullptr;

	flecs::query<RagdollComponent, AnimationComponent, SkeletalMeshComponent> m_ragdollQuery;

	void SetupComponents();
	void SetupSystems();
	void SetupObservers();
	void SetupQueries();
	void RegisterWithEditor();

	JPH::Ref<JPH::Skeleton> CreateJoltSkeleton(const ozz::animation::Skeleton* ozzSkeleton,
		const std::vector<RagdollBoneConfig>& boneConfigs);

	void UpdateRagdoll(flecs::entity entity, RagdollComponent& ragdoll,
		AnimationComponent& anim, const SkeletalMeshComponent& mesh, float deltaTime);

	void DrawRagdollInspector(flecs::entity entity, RagdollComponent& component);

};