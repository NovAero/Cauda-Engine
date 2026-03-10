#pragma once

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/quaternion.hpp>

#include <ozz/base/maths/soa_transform.h>
#include <ozz/animation/runtime/skeleton.h>
#include "Core/Physics/JoltPhysics.h"

#include "Core/Components/Components.h"

#undef max
#undef min

namespace Math

{
	glm::mat4						TransformToMatrix(TransformComponent transform);
	TransformComponent				MatrixToTransform(glm::mat4 matrix);

	glm::mat4						WorldToLocalMat(flecs::entity entity);
	glm::mat4						LocalToWorldMat(flecs::entity entity);

	TransformComponent				WorldToLocalTform(flecs::entity entity);
	TransformComponent			    LocalToWorldTform(flecs::entity entity);

	void							SetWorldPosition(flecs::entity entity, const glm::vec3& worldPos);
	void							SetWorldRotation(flecs::entity entity, const glm::quat& worldRot);

	glm::vec3						GetWorldPosition(flecs::entity entity);
	glm::quat						GetWorldRotation(flecs::entity entity);

	void						    Reparent(flecs::entity entity, flecs::entity parent);

	bool							IsEqual(const glm::quat& q1, const glm::quat& q2, float tolerance = 0.0001f);
	glm::vec3						ClampEulerAngles(const glm::vec3& eulerAngles);
	glm::quat						EulerToQuat(glm::vec3 eulerAngles);
	glm::vec3						GetEulerAngles(glm::quat q);
	float							GetPitch(glm::quat q);
	float							GetYaw(glm::quat q);
	float							GetRoll(glm::quat q);

	float							LerpFloat(float a, float b, float t);
	glm::vec3						LerpVec3(const glm::vec3& a, const glm::vec3& b, float t);
	glm::quat						SlerpShortest(const glm::quat& a, const glm::quat& b, float t);
	glm::quat						SlerpExpSmooth(const glm::quat& current, const glm::quat& target, float smoothing, float deltaTime);
	glm::quat						RotateAroundAxis(const glm::quat& current, const glm::vec3& axis, float angleDegrees);

	float							SmoothStep(float t);
	float							SmootherStep(float t);

	void							Print(glm::quat q);
	void							Print(glm::vec3 v);
	std::string						ToString(glm::quat q);
	std::string						ToString(glm::vec3 v);

	ozz::math::Float4x4				GlmToOzz(const glm::mat4& glmMatrix);
	glm::mat4						OzzToGlm(const ozz::math::Float4x4& ozzMatrix);

	JPH::Mat44						OzzToJolt(const ozz::math::Float4x4& ozzMatrix);
	ozz::math::Float4x4				JoltToOzz(const JPH::Mat44& joltMatrix);

	JPH::Mat44						GlmToJolt(const glm::mat4& glmMatrix);
	glm::mat4						JoltToGlm(const JPH::Mat44& joltMatrix);

	JPH::Quat						GlmToJolt(const glm::quat& glmQuat);
	glm::quat						JoltToGlm(const JPH::Quat& joltQuat);

	JPH::Vec3						GlmToJolt(const glm::vec3& glmVec);
	glm::vec3						JoltToGlm(const JPH::Vec3& joltVec);

	void							ExtractTransformFromSoA(const ozz::math::SoaTransform& soa, int index,
									glm::vec3& outTranslation, glm::quat& outRotation, glm::vec3& outScale);

	void							InsertTransformIntoSoA(ozz::math::SoaTransform& soa, int index,
									const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale);
}