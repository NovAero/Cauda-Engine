#include "cepch.h"
#include "RagdollModule.h"
#include "Core/Physics/PhysicsModule.h"
#include "Renderer/AnimationModule.h"
#include "Renderer/SkeletalMesh.h"
#include "Renderer/RendererModule.h"
#include "Core/Editor/ResourceLibrary.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Math/EngineMath.h"
#include <imgui.h>
#include <iostream>
#include <unordered_set>
#include <iomanip>

using namespace Math;

namespace ObjectLayers
{
	static constexpr JPH::ObjectLayer NON_MOVING = 0;
	static constexpr JPH::ObjectLayer MOVING = 1;
	static constexpr JPH::ObjectLayer NO_COLLISION = 2;
	static constexpr JPH::ObjectLayer KINEMATIC_SENSOR(3);
	static constexpr JPH::ObjectLayer ROPE(4);
	static constexpr JPH::ObjectLayer ROPE_EVEN(5);
	static constexpr JPH::ObjectLayer CHARACTER = 6;
	static constexpr JPH::ObjectLayer NUM_LAYERS = 7;
};




static JPH::Vec3 GetPerpendicular(const JPH::Vec3& v)
{
	JPH::Vec3 c1 = v.Cross(JPH::Vec3(0, 0, 1));

	JPH::Vec3 c2 = v.Cross(JPH::Vec3(0, 1, 0));

	if (c1.LengthSq() > c2.LengthSq())
		return c1.Normalized();
	return c2.Normalized();
}

void OzzSoAToJoltJointState(const ozz::math::SoaTransform& soa, int index,
	JPH::Vec3& outTranslation, JPH::Quat& outRotation)
{
	int soa_index = index / 4;
	int element = index % 4;

	outTranslation = JPH::Vec3(
		soa.translation.x.m128_f32[element],
		soa.translation.y.m128_f32[element],
		soa.translation.z.m128_f32[element]
	);

	outRotation = JPH::Quat(
		soa.rotation.x.m128_f32[element],
		soa.rotation.y.m128_f32[element],
		soa.rotation.z.m128_f32[element],
		soa.rotation.w.m128_f32[element]
	);
}

RagdollModule::RagdollModule(flecs::world& world) : m_world(world)
{
	m_physicsModule = m_world.try_get_mut<PhysicsModule>();
	m_animModule = m_world.try_get_mut<AnimationModule>();

	SetupComponents();
	SetupSystems();
	SetupQueries();
	RegisterWithEditor();

}

RagdollModule::~RagdollModule()
{
	m_ragdollQuery.each([this](flecs::entity entity, RagdollComponent& ragdoll,
		AnimationComponent&, const SkeletalMeshComponent&)
		{
			DestroyRagdoll(entity);
		});
}

void RagdollModule::SetupComponents()
{
	m_world.component<RagdollComponent>("RagdollComponent");
}

void RagdollModule::SetupSystems()
{
	m_world.system<RagdollComponent, AnimationComponent, const SkeletalMeshComponent>(
		"UpdateRagdollSystem")
		.kind(flecs::PreUpdate)
		.each([this](flecs::entity entity, RagdollComponent& ragdoll,
			AnimationComponent& anim, const SkeletalMeshComponent& mesh)
			{
				if (!ragdoll.isInitialised || !ragdoll.enabled)
					return;
				UpdateRagdoll(entity, ragdoll, anim, mesh, m_world.delta_time());
			});
}

void RagdollModule::SetupObservers()
{
	m_world.observer<RagdollComponent>()
		.event(flecs::OnRemove)
		.each([this](flecs::entity entity, RagdollComponent& ragdoll)
			{
				if (ragdoll.joltRagdoll)
				{
					ragdoll.joltRagdoll->RemoveFromPhysicsSystem();
					ragdoll.joltRagdoll = nullptr;
				}

				ragdoll.bones.clear();
				ragdoll.boneNameToIndex.clear();
				ragdoll.isInitialised = false;
			});
}

void RagdollModule::SetupQueries()
{
	m_ragdollQuery = m_world.query_builder<RagdollComponent, AnimationComponent,
		SkeletalMeshComponent>().build();
}

void RagdollModule::RegisterWithEditor()
{
	auto* editorModule = m_world.try_get_mut<EditorModule>();
	if (!editorModule) return;

	editorModule->RegisterComponent<RagdollComponent>(
		"Ragdoll",
		"Physics",
		[this](flecs::entity entity, RagdollComponent& component)
		{
			DrawRagdollInspector(entity, component);
		}
	);
}


JPH::Ref<JPH::Skeleton> RagdollModule::CreateJoltSkeleton(
	const ozz::animation::Skeleton* ozzSkeleton,
	const std::vector<RagdollBoneConfig>& boneConfigs)
{
	if (!ozzSkeleton) return nullptr;

	JPH::Ref<JPH::Skeleton> joltSkeleton = new JPH::Skeleton();

	std::unordered_set<std::string> includedBones;
	for (const auto& config : boneConfigs)
	{
		includedBones.insert(config.boneName);
	}

	auto joint_names = ozzSkeleton->joint_names();
	auto joint_parents = ozzSkeleton->joint_parents();
	int numJoints = ozzSkeleton->num_joints();

	std::unordered_map<int, int> ozzToJoltIndex;
	int joltIndex = 0;

	for (int i = 0; i < numJoints; ++i)
	{
		std::string boneName = joint_names[i];

		if (includedBones.find(boneName) == includedBones.end())
			continue;

		int ozzParentIdx = joint_parents[i];
		int joltParentIdx = -1;

		if (ozzParentIdx != ozz::animation::Skeleton::kNoParent)
		{
			auto it = ozzToJoltIndex.find(ozzParentIdx);
			if (it != ozzToJoltIndex.end())
			{
				joltParentIdx = it->second;
			}
		}

		joltSkeleton->AddJoint(boneName.c_str(), joltParentIdx);
		ozzToJoltIndex[i] = joltIndex;
		joltIndex++;
	}

	joltSkeleton->CalculateParentJointIndices();
	return joltSkeleton;
}

bool RagdollModule::CreateRagdoll(flecs::entity entity,
	const std::vector<RagdollBoneConfig>& boneConfigs,
	const std::vector<RagdollJointConfig>& jointConfigs)
{
	if (!m_physicsModule || !m_animModule) return false;

	auto* meshComp = entity.try_get<SkeletalMeshComponent>();
	auto* ragdoll = entity.try_get_mut<RagdollComponent>();

	if (!meshComp || !ragdoll)
	{
		std::cerr << "Entity missing required components" << std::endl;
		return false;
	}

	SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp->handle.c_str());
	if (!mesh || !mesh->GetSkeleton())
	{
		std::cerr << "Invalid skeletal mesh" << std::endl;
		return false;
	}

	const ozz::animation::Skeleton* ozzSkeleton = mesh->GetSkeleton();

	if (ragdoll->isInitialised && ragdoll->joltRagdoll)
	{
		ragdoll->joltRagdoll->RemoveFromPhysicsSystem();
		ragdoll->joltRagdoll = nullptr;
	}
	ragdoll->bones.clear();
	ragdoll->boneNameToIndex.clear();

	JPH::Ref<JPH::Skeleton> joltSkeleton = CreateJoltSkeleton(ozzSkeleton, boneConfigs);
	if (!joltSkeleton)
	{
		std::cerr << "Failed to create Jolt skeleton" << std::endl;
		return false;
	}

	JPH::Ref<JPH::RagdollSettings> settings = new JPH::RagdollSettings();
	settings->mSkeleton = joltSkeleton;
	settings->mParts.resize(joltSkeleton->GetJointCount());

	std::unordered_map<std::string, int> boneNameToJoltIndex;
	for (int joltIdx = 0; joltIdx < joltSkeleton->GetJointCount(); ++joltIdx)
	{
		const JPH::Skeleton::Joint& joint = joltSkeleton->GetJoint(joltIdx);
		boneNameToJoltIndex[joint.mName.c_str()] = joltIdx;
	}

	std::unordered_map<int, float> boneHalfHeightMap;

	for (int joltIdx = 0; joltIdx < joltSkeleton->GetJointCount(); ++joltIdx)
	{
		const JPH::Skeleton::Joint& joint = joltSkeleton->GetJoint(joltIdx);
		float halfHeight = 0.05f;

		for (int childIdx = 0; childIdx < joltSkeleton->GetJointCount(); ++childIdx)
		{
			if (joltSkeleton->GetJoint(childIdx).mParentJointIndex == joltIdx)
			{
				int childOzzIdx = mesh->GetJointIndex(joltSkeleton->GetJoint(childIdx).mName.c_str());
				if (childOzzIdx >= 0)
				{
					const auto& rest_pose = ozzSkeleton->joint_rest_poses();
					ozz::math::SoaTransform transform = rest_pose[childOzzIdx / 4];
					int element = childOzzIdx % 4;
					JPH::Vec3 childLocalPos(
						transform.translation.x.m128_f32[element],
						transform.translation.y.m128_f32[element],
						transform.translation.z.m128_f32[element]
					);
					float len = childLocalPos.Length();
					if (len > 0.001f) halfHeight = len / 2.0f;
					break;
				}
			}
		}

		for (const auto& boneConfig : boneConfigs)
		{
			if (boneConfig.boneName == joint.mName.c_str())
			{
				if (boneConfig.dimensions.y / 2.0f > halfHeight || halfHeight == 0.05f) {
					halfHeight = boneConfig.dimensions.y / 2.0f;
				}
				break;
			}
		}
		boneHalfHeightMap[joltIdx] = halfHeight;
	}

	for (const auto& boneConfig : boneConfigs)
	{
		auto it = boneNameToJoltIndex.find(boneConfig.boneName);
		if (it == boneNameToJoltIndex.end()) continue;

		int joltIdx = it->second;
		auto& part = settings->mParts[joltIdx];
		float halfHeight = boneHalfHeightMap[joltIdx];

		JPH::Ref<JPH::Shape> shape;
		switch (boneConfig.shape)
		{
		case RagdollBodyShape::Capsule:
			shape = new JPH::CapsuleShape(halfHeight, boneConfig.dimensions.x);
			break;
		case RagdollBodyShape::Box:
			shape = new JPH::BoxShape(PhysicsModule::ToJolt(boneConfig.dimensions * 0.5f));
			break;
		case RagdollBodyShape::Sphere:
			shape = new JPH::SphereShape(boneConfig.dimensions.x);
			break;
		}
		part.SetShape(shape);
		part.mMassPropertiesOverride.mMass = boneConfig.mass;

		part.mPosition = JPH::Vec3(0, halfHeight, 0);
		part.mRotation = JPH::Quat::sIdentity();

		if (boneConfig.boneName.find("BoneMan") != std::string::npos || joltSkeleton->GetJoint(joltIdx).mParentJointIndex == -1)
		{
			part.mMotionType = JPH::EMotionType::Kinematic;
			part.mGravityFactor = 0.0f;
		}
		else
		{
			part.mMotionType = JPH::EMotionType::Dynamic;
			part.mGravityFactor = 1.0f;
		}

		int ozzIndex = mesh->GetJointIndex(boneConfig.boneName);
		ragdoll->bones.push_back({ ozzIndex, boneConfig.boneName });
		ragdoll->boneNameToIndex[boneConfig.boneName] = joltIdx;
	}

	for (const auto& jointConfig : jointConfigs)
	{
		auto parentIt = boneNameToJoltIndex.find(jointConfig.parentBone);
		auto childIt = boneNameToJoltIndex.find(jointConfig.childBone);
		if (parentIt == boneNameToJoltIndex.end() || childIt == boneNameToJoltIndex.end()) continue;

		int parentJoltIdx = parentIt->second;
		int childJoltIdx = childIt->second;

		int childOzzIdx = mesh->GetJointIndex(jointConfig.childBone);
		JPH::Vec3 twistAxis = JPH::Vec3(0, 1, 0);

		if (childOzzIdx >= 0)
		{
			const auto& rest_pose = ozzSkeleton->joint_rest_poses();
			ozz::math::SoaTransform transform = rest_pose[childOzzIdx / 4];
			int element = childOzzIdx % 4;
			JPH::Vec3 childLocalPos(
				transform.translation.x.m128_f32[element],
				transform.translation.y.m128_f32[element],
				transform.translation.z.m128_f32[element]
			);
			if (childLocalPos.LengthSq() > 0.001f) twistAxis = childLocalPos.Normalized();
		}

		JPH::Vec3 planeAxis = GetPerpendicular(twistAxis);

		JPH::SwingTwistConstraintSettings* constraint = new JPH::SwingTwistConstraintSettings();
		constraint->mSpace = JPH::EConstraintSpace::LocalToBodyCOM;

		constraint->mPosition1 = JPH::Vec3(0, boneHalfHeightMap[parentJoltIdx], 0);
		constraint->mPosition2 = JPH::Vec3(0, -boneHalfHeightMap[childJoltIdx], 0);

		constraint->mTwistAxis1 = twistAxis;
		constraint->mTwistAxis2 = twistAxis;
		constraint->mPlaneAxis1 = planeAxis;
		constraint->mPlaneAxis2 = planeAxis;

		constraint->mNormalHalfConeAngle = jointConfig.normalHalfConeAngle;
		constraint->mPlaneHalfConeAngle = jointConfig.planeHalfConeAngle;
		constraint->mTwistMinAngle = jointConfig.twistMinAngle;
		constraint->mTwistMaxAngle = jointConfig.twistMaxAngle;

		JPH::MotorSettings motor_settings(ragdoll->motorStrength, ragdoll->motorDamping);
		constraint->mSwingMotorSettings = motor_settings;
		constraint->mTwistMotorSettings = motor_settings;

		settings->mParts[childJoltIdx].mToParent = constraint;
	}

	if (!settings->Stabilize())
	{
		std::cerr << "Failed to stabilise ragdoll" << std::endl;
		return false;
	}

	settings->DisableParentChildCollisions();
	settings->CalculateBodyIndexToConstraintIndex();
	settings->CalculateConstraintIndexToBodyIdxPair();

	ragdoll->joltRagdoll = settings->CreateRagdoll(ObjectLayers::NO_COLLISION, 0, m_physicsModule->GetPhysicsSystem());

	if (!ragdoll->joltRagdoll)
	{
		std::cerr << "Failed to create ragdoll instance" << std::endl;
		return false;
	}

	ragdoll->joltRagdoll->AddToPhysicsSystem(JPH::EActivation::Activate);
	ragdoll->isInitialised = true;

	std::cout << "Successfully created ragdoll with " << joltSkeleton->GetJointCount() << " bodies." << std::endl;
	return true;
}

void RagdollModule::DestroyRagdoll(flecs::entity entity)
{
	auto* ragdoll = entity.try_get_mut<RagdollComponent>();
	if (!ragdoll || !ragdoll->isInitialised) return;

	if (ragdoll->joltRagdoll)
	{
		ragdoll->joltRagdoll->RemoveFromPhysicsSystem();
		ragdoll->joltRagdoll = nullptr;
	}

	ragdoll->bones.clear();
	ragdoll->boneNameToIndex.clear();
	ragdoll->isInitialised = false;
}

void RagdollModule::UpdateRagdoll(flecs::entity entity, RagdollComponent& ragdoll,
	AnimationComponent& anim, const SkeletalMeshComponent& mesh, float deltaTime)
{
	if (!ragdoll.joltRagdoll || !anim.blendedTransforms || anim.blendedTransforms->empty())
		return;

	const JPH::RagdollSettings* settings = ragdoll.joltRagdoll->GetRagdollSettings();
	if (!settings || !settings->GetSkeleton()) return;

	const JPH::Skeleton* joltSkeleton = settings->GetSkeleton();
	SkeletalMesh* skeletalMesh = ResourceLibrary::GetSkeletalMesh(mesh.handle.c_str());
	if (!skeletalMesh) return;

	JPH::SkeletonPose pose;
	pose.SetSkeleton(joltSkeleton);

	for (int joltIdx = 0; joltIdx < joltSkeleton->GetJointCount(); ++joltIdx)
	{
		const JPH::Skeleton::Joint& joint = joltSkeleton->GetJoint(joltIdx);
		int ozzIdx = skeletalMesh->GetJointIndex(joint.mName.c_str());

		if (ozzIdx >= 0 && ozzIdx < anim.blendedTransforms->size())
		{
			JPH::Vec3 translation;
			JPH::Quat rotation;
			OzzSoAToJoltJointState((*anim.blendedTransforms)[ozzIdx / 4], ozzIdx, translation, rotation);

			pose.GetJoint(joltIdx).mTranslation = translation;
			pose.GetJoint(joltIdx).mRotation = rotation;
		}
		else
		{
			pose.GetJoint(joltIdx).mTranslation = JPH::Vec3::sZero();
			pose.GetJoint(joltIdx).mRotation = JPH::Quat::sIdentity();
		}
	}

	glm::vec3 entityPos = Math::GetWorldPosition(entity);
	glm::quat entityRot = Math::GetWorldRotation(entity);

	pose.SetRootOffset(JPH::RVec3(entityPos.x, entityPos.y, entityPos.z));


	JPH::Quat worldRot = JPH::Quat(entityRot.x, entityRot.y, entityRot.z, entityRot.w);

	for (int i = 0; i < joltSkeleton->GetJointCount(); ++i)
	{
		if (joltSkeleton->GetJoint(i).mParentJointIndex == -1)
		{
			JPH::Quat localRot = pose.GetJoint(i).mRotation;

			pose.GetJoint(i).mRotation = worldRot * localRot;
		}
	}

	pose.CalculateJointMatrices();

	if (ragdoll.useKinematicMode)
	{
		ragdoll.joltRagdoll->DriveToPoseUsingKinematics(pose, deltaTime);
	}
	else
	{
		ragdoll.joltRagdoll->DriveToPoseUsingMotors(pose);

		JPH::RVec3 ragdollRootOffset;
		std::vector<JPH::Mat44> ragdollMatrices(joltSkeleton->GetJointCount());
		ragdoll.joltRagdoll->GetPose(ragdollRootOffset, ragdollMatrices.data(), true);

		if (entity.has<TransformComponent>() && !ragdollMatrices.empty())
		{
			auto* transform = entity.try_get_mut<TransformComponent>();

			transform->position = glm::vec3(ragdollRootOffset.GetX(), ragdollRootOffset.GetY(), ragdollRootOffset.GetZ());

			JPH::Mat44 invEntityMat = JPH::Mat44::sRotationTranslation(worldRot, ragdollRootOffset).Inversed();

			for (int joltIdx = 0; joltIdx < joltSkeleton->GetJointCount(); ++joltIdx)
			{
				const JPH::Skeleton::Joint& joint = joltSkeleton->GetJoint(joltIdx);
				int ozzIdx = skeletalMesh->GetJointIndex(joint.mName.c_str());

				if (ozzIdx >= 0 && anim.modelMatrices && ozzIdx < anim.modelMatrices->size())
				{
					JPH::Mat44 worldMat = ragdollMatrices[joltIdx];
					worldMat.PostTranslated(JPH::Vec3(ragdollRootOffset));

					const auto& part = settings->mParts[joltIdx];

					JPH::Vec3 comOffset = part.mPosition;

					JPH::Mat44 comCorrection = JPH::Mat44::sTranslation(comOffset).Inversed();
					JPH::Mat44 jointWorldMat = worldMat * comCorrection;

					JPH::Mat44 modelMat = invEntityMat * jointWorldMat;

					ozz::math::Float4x4& ozzMat = (*anim.modelMatrices)[ozzIdx];
					for (int c = 0; c < 4; ++c) {
						JPH::Vec4 col = modelMat.GetColumn4(c);
						ozzMat.cols[c] = ozz::math::simd_float4::Load(col.GetX(), col.GetY(), col.GetZ(), col.GetW());
					}
				}
			}
		}
	}
}

void RagdollModule::AddForceAtBone(flecs::entity entity, const std::string& boneName,
	const glm::vec3& force)
{
	JPH::BodyID bodyID = GetBoneBodyID(entity, boneName);
	if (!bodyID.IsInvalid())
	{
		m_physicsModule->GetBodyInterface().AddForce(bodyID, PhysicsModule::ToJolt(force));
	}
}

void RagdollModule::AddImpulseAtBone(flecs::entity entity, const std::string& boneName,
	const glm::vec3& impulse)
{
	JPH::BodyID bodyID = GetBoneBodyID(entity, boneName);
	if (!bodyID.IsInvalid())
	{
		m_physicsModule->GetBodyInterface().AddImpulse(bodyID, PhysicsModule::ToJolt(impulse));
	}
}

void RagdollModule::SetMotorStrength(flecs::entity entity, float strength)
{
	auto* ragdoll = entity.try_get_mut<RagdollComponent>();
	if (ragdoll)
	{
		ragdoll->motorStrength = strength;
	}
}

void RagdollModule::SetMotorDamping(flecs::entity entity, float damping)
{
	auto* ragdoll = entity.try_get_mut<RagdollComponent>();
	if (ragdoll)
	{
		ragdoll->motorDamping = damping;
	}
}

JPH::BodyID RagdollModule::GetBoneBodyID(flecs::entity entity, const std::string& boneName) const
{
	auto* ragdoll = entity.try_get<RagdollComponent>();
	if (!ragdoll || !ragdoll->joltRagdoll) return JPH::BodyID();

	auto it = ragdoll->boneNameToIndex.find(boneName);
	if (it != ragdoll->boneNameToIndex.end())
	{
		return ragdoll->joltRagdoll->GetBodyID(it->second);
	}

	return JPH::BodyID();
}

bool RagdollModule::CreateRagdollFromSkeleton(flecs::entity entity, float defaultBoneRadius, float defaultBoneMass)
{
	auto* meshComp = entity.try_get<SkeletalMeshComponent>();
	SkeletalMesh* mesh = ResourceLibrary::GetSkeletalMesh(meshComp->handle.c_str());
	if (!mesh || !mesh->GetSkeleton())
	{
		std::cerr << "Cannot create ragdoll: Invalid skeletal mesh." << std::endl;
		return false;
	}

	const ozz::animation::Skeleton* ozzSkeleton = mesh->GetSkeleton();
	const int numJoints = ozzSkeleton->num_joints();
	auto joint_names = ozzSkeleton->joint_names();
	auto joint_parents = ozzSkeleton->joint_parents();
	const auto& rest_pose = ozzSkeleton->joint_rest_poses();

	std::vector<RagdollBoneConfig> boneConfigs;
	std::vector<RagdollJointConfig> jointConfigs;

	std::unordered_map<int, std::vector<int>> child_map;
	for (int i = 0; i < numJoints; ++i) {
		if (joint_parents[i] != ozz::animation::Skeleton::kNoParent) {
			child_map[joint_parents[i]].push_back(i);
		}
	}

	auto shouldFilterBone = [](const std::string& name) -> bool 
	{
		return name.find("SK_") == 0 || name.find("_mesh") != std::string::npos;
	};

	for (int i = 0; i < numJoints; ++i)
	{
		std::string boneName = joint_names[i];

		if (shouldFilterBone(boneName))
		{
			std::cout << "Skipping non joint bone: " << boneName << std::endl;
			continue;
		}

		RagdollBoneConfig boneConfig;
		boneConfig.boneName = boneName;
		boneConfig.shape = RagdollBodyShape::Capsule;
		boneConfig.mass = defaultBoneMass;

		if (boneName.find("BoneMan") != std::string::npos)
		{
			boneConfig.mass = 0.0f;
		}

		ozz::math::SoaTransform transform = rest_pose[i / 4];
		glm::vec3 bonePos(transform.translation.x.m128_f32[i % 4],
			transform.translation.y.m128_f32[i % 4],
			transform.translation.z.m128_f32[i % 4]);

		float boneLength = defaultBoneRadius * 2.5f;
		if (child_map.count(i) > 0 && !child_map[i].empty())
		{
			int childIdx = child_map[i][0];
			ozz::math::SoaTransform childTransform = rest_pose[childIdx / 4];
			glm::vec3 childPos(childTransform.translation.x.m128_f32[childIdx % 4],
				childTransform.translation.y.m128_f32[childIdx % 4],
				childTransform.translation.z.m128_f32[childIdx % 4]);
			boneLength = glm::distance(glm::vec3(0), childPos);
		}

		boneConfig.dimensions = glm::vec3(defaultBoneRadius, boneLength, defaultBoneRadius);
		boneConfigs.push_back(boneConfig);

		if (joint_parents[i] != ozz::animation::Skeleton::kNoParent)
		{
			std::string parentName = joint_names[joint_parents[i]];
			if (shouldFilterBone(parentName))
			{
				std::cout << "Skipping joint to filtered parent: " << parentName << " -> " << boneName << std::endl;
				continue;
			}

			RagdollJointConfig jointConfig;
			jointConfig.parentBone = parentName;
			jointConfig.childBone = boneName;
			jointConfig.twistMinAngle = -glm::radians(30.0f);
			jointConfig.twistMaxAngle = glm::radians(30.0f);
			jointConfig.normalHalfConeAngle = glm::radians(25.0f);
			jointConfig.planeHalfConeAngle = glm::radians(25.0f);
			jointConfigs.push_back(jointConfig);
		}
	}

	return CreateRagdoll(entity, boneConfigs, jointConfigs);
}

void RagdollModule::DrawRagdollInspector(flecs::entity entity, RagdollComponent& component)
{
	auto* meshComp = entity.try_get<SkeletalMeshComponent>();
	auto* animComp = entity.try_get<AnimationComponent>();


	if (!component.isInitialised)
	{
		if (ImGui::Button("Create Ragdoll"))
		{
			CreateRagdollFromSkeleton(entity);
		}

		return;
	}

	ImGui::Checkbox("Enabled", &component.enabled);

	ImGui::Separator();
	ImGui::Text("Mode");
	if (ImGui::RadioButton("Kinematic", component.useKinematicMode))
	{
		component.useKinematicMode = true;
	}
	if (ImGui::RadioButton("Powered", !component.useKinematicMode))
	{
		component.useKinematicMode = false;
	}

	ImGui::Separator();
	ImGui::Text("Motor Settings");
	ImGui::DragFloat("Motor Strength", &component.motorStrength, 1.0f, 0.0f, 1000.0f);
	ImGui::DragFloat("Motor Damping", &component.motorDamping, 0.1f, 0.0f, 100.0f);

	ImGui::Separator();
	if (ImGui::Button("Destroy Ragdoll"))
	{
		DestroyRagdoll(entity);
	}
}