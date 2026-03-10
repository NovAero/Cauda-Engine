#include "cepch.h"
#include "Core/Math/EngineMath.h"
#include "Core/Components/Components.h"

namespace Math
{
	glm::mat4 TransformToMatrix(TransformComponent transform)
	{
		glm::mat4 T = glm::translate(glm::mat4(1.0f), transform.position);
		glm::mat4 R = glm::mat4_cast(transform.rotation);
		glm::mat4 S = glm::scale(glm::mat4(1.0f), transform.scale);

		return T * R * S;
	}

	TransformComponent MatrixToTransform(glm::mat4 matrix)
	{
		TransformComponent transform;

		transform.position = glm::vec3(matrix[3]);

		glm::vec3 scale;
		scale.x = glm::length(glm::vec3(matrix[0]));
		scale.y = glm::length(glm::vec3(matrix[1]));
		scale.z = glm::length(glm::vec3(matrix[2]));
		transform.scale = scale;

		glm::mat3 rotationMatrix;
		rotationMatrix[0] = glm::vec3(matrix[0]) / scale.x;
		rotationMatrix[1] = glm::vec3(matrix[1]) / scale.y;
		rotationMatrix[2] = glm::vec3(matrix[2]) / scale.z;
		transform.rotation = glm::quat_cast(rotationMatrix);

		transform.cachedEuler = Math::GetEulerAngles(transform.rotation);

		return transform;
	}

	glm::mat4 WorldToLocalMat(flecs::entity entity)
	{
		auto worldMatrix = LocalToWorldMat(entity);

		flecs::entity parent = entity.parent();
		if (parent && parent.has<TransformComponent>())
		{
			glm::mat4 parentWorldMatrix = LocalToWorldMat(parent);
			return glm::inverse(parentWorldMatrix) * worldMatrix;
		}

		return worldMatrix;
	}

	glm::mat4 LocalToWorldMat(flecs::entity entity)
	{
		glm::mat4 worldMatrix = glm::mat4(1.0f);

		std::vector<flecs::entity> hierarchy;
		flecs::entity current = entity;

		while (current && current.has<TransformComponent>())
		{
			hierarchy.push_back(current);
			current = current.parent();
		}

		for (auto it = hierarchy.rbegin(); it != hierarchy.rend(); ++it)
		{
			auto transform = it->try_get<TransformComponent>();
			if (transform)
			{
				worldMatrix = worldMatrix * TransformToMatrix(*transform);
			}
		}

		return worldMatrix;
	}

	TransformComponent WorldToLocalTform(flecs::entity entity)
	{
		return MatrixToTransform(WorldToLocalMat(entity));
	}

	TransformComponent LocalToWorldTform(flecs::entity entity)
	{
		return MatrixToTransform(LocalToWorldMat(entity));
	}

	void SetWorldPosition(flecs::entity entity, const glm::vec3& worldPos)
	{
		auto* transform = entity.try_get_mut<TransformComponent>();
		if (!transform) return;

		flecs::entity parent = entity.parent();
		if (parent && parent.has<TransformComponent>())
		{
			glm::mat4 parentWorldMatrix = LocalToWorldMat(parent);

			TransformComponent parentWorldTransform = MatrixToTransform(parentWorldMatrix);

			glm::vec3 relativePos = worldPos - parentWorldTransform.position;

			glm::quat invParentRot = glm::inverse(parentWorldTransform.rotation);
			glm::vec3 rotatedPos = invParentRot * relativePos;

			glm::vec3 localPos = rotatedPos / parentWorldTransform.scale;

			transform->position = localPos;
		}
		else
		{
			transform->position = worldPos;
		}
	}

	void SetWorldRotation(flecs::entity entity, const glm::quat& worldRot)
	{
		auto transform = entity.try_get_mut<TransformComponent>();
		if (!transform) return;

		flecs::entity parent = entity.parent();
		if (parent && parent.has<TransformComponent>())
		{
			TransformComponent parentWorldTransform = LocalToWorldTform(parent);
			glm::quat invParentRot = glm::inverse(parentWorldTransform.rotation);
			transform->rotation = invParentRot * worldRot;
		}
		else
		{
			transform->rotation = worldRot;
		}

		transform->cachedEuler = Math::GetEulerAngles(transform->rotation);
	}

	glm::vec3 GetWorldPosition(flecs::entity entity)
	{
		return LocalToWorldTform(entity).position;
	}

	glm::quat GetWorldRotation(flecs::entity entity)
	{
		return LocalToWorldTform(entity).rotation;
	}

	void Reparent(flecs::entity entity, flecs::entity parent)
	{
		glm::mat4 oldWorldMatrix = Math::LocalToWorldMat(entity);

		entity.child_of(parent);

		glm::mat4 parentWorldMatrix = Math::LocalToWorldMat(parent);
		glm::mat4 newLocalMatrix = glm::inverse(parentWorldMatrix) * oldWorldMatrix;

		auto* transform = entity.try_get_mut<TransformComponent>();

		if (transform)
		{
			entity.get_mut<TransformComponent>() = Math::MatrixToTransform(newLocalMatrix);

			entity.modified<TransformComponent>();
		}
	}


	bool IsEqual(const glm::quat& q1, const glm::quat& q2, float tolerance)
	{
		bool result = glm::abs(glm::abs(glm::dot(q1, q2)) - 1.f) < tolerance;
		return result;
	}

	glm::vec3 ClampEulerAngles(const glm::vec3& eulerAngles)
	{
		return (glm::fract((eulerAngles + 180.f) / 360.f)) * 360.f - 180.f;
	}

	glm::quat EulerToQuat(glm::vec3 eulerAngles)
	{
		glm::vec3 radians = glm::radians(eulerAngles);

		glm::vec3 c = glm::cos(radians * 0.5f);
		glm::vec3 s = glm::sin(radians * 0.5f);

		glm::quat q;
		q.w = c.x * c.y * c.z + s.x * s.y * s.z;
		q.x = s.x * c.y * c.z + c.x * s.y * s.z;
		q.y = c.x * s.y * c.z - s.x * c.y * s.z;
		q.z = c.x * c.y * s.z - s.x * s.y * c.z;

		return glm::normalize(q);
	}

	glm::vec3 GetEulerAngles(glm::quat q)
	{
		q = glm::normalize(q);
		if (abs(q.y * q.z - q.w * q.x) > 0.5f)
			q.w = -q.w;

		return glm::vec3(GetPitch(q), GetYaw(q), GetRoll(q));
	}

	float GetPitch(glm::quat q)
	{
		float sinp = -2.f * (q.y * q.z - q.w * q.x);
		return glm::degrees(std::asin(std::clamp(sinp, -0.9999f, 0.9999f)));
	}

	float GetYaw(glm::quat q)
	{
		float siny_cosp = 2.f * (q.w * q.y + q.x * q.z);
		float cosy_cosp = 1.f - 2.f * (q.x * q.x + q.y * q.y);

		return glm::degrees(std::atan2(siny_cosp, cosy_cosp));
	}

	float GetRoll(glm::quat q)
	{
		float sinr_cosp = 2.f * (q.w * q.z + q.x * q.y);
		float cosr_cosp = 1.f - 2.f * (q.x * q.x + q.z * q.z);

		return glm::degrees(std::atan2(sinr_cosp, cosr_cosp));
	}

	float LerpFloat(float a, float b, float t)
	{
		return a + ((b - a) * t);
	}

	glm::vec3 LerpVec3(const glm::vec3& a, const glm::vec3& b, float t)
	{
		return a + ((b - a) * t);
	}

	glm::quat SlerpShortest(const glm::quat& a, const glm::quat& b, float t)
	{
		glm::quat correctedB = b;

		if (glm::dot(a, b) < 0.0f)
		{
			correctedB = -b;
		}

		return glm::slerp(a, correctedB, t);
	}

	float SmoothStep(float t)
	{
		return t * t * (3.0f - 2.0f * t);
	}

	float SmootherStep(float t)
	{
		return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
	}

	glm::quat SlerpExpSmooth(const glm::quat& current, const glm::quat& target, float smoothing, float deltaTime)
	{
		float factor = 1.0f - glm::exp(-smoothing * deltaTime);
		return SlerpShortest(current, target, factor);
	}

	glm::quat RotateAroundAxis(const glm::quat& current, const glm::vec3& axis, float angleDegrees)
	{
		glm::quat rotation = glm::angleAxis(glm::radians(angleDegrees), glm::normalize(axis));
		return current * rotation;
	}

	void Print(glm::quat q)
	{
		std::cout << "w: " << q.w << ", x: " << q.x << ", y: " << q.y << ", z: " << q.z << std::endl;
	}

	void Print(glm::vec3 v)
	{
		std::cout << "x: " << v.x << ", y : " << v.y << ", z : " << v.z << std::endl;
	}

	std::string ToString(glm::quat q)
	{
		std::string result =
			"w: " + std::to_string(q.w) +
			", x : " + std::to_string(q.x) +
			", y : " + std::to_string(q.y) +
			", z : " + std::to_string(q.z);

		return result;
	}

	std::string ToString(glm::vec3 v)
	{
		std::string result =
			"x: " + std::to_string(v.x) +
			", y : " + std::to_string(v.y) +
			", z : " + std::to_string(v.z);

		return result;
	}

	glm::mat4 OzzToGlm(const ozz::math::Float4x4& ozzMatrix)
	{
		glm::mat4 result;
		for (int col = 0; col < 4; ++col) {
			for (int row = 0; row < 4; ++row) {
				result[col][row] = ozzMatrix.cols[col].m128_f32[row];
			}
		}
		return result;
	}

	ozz::math::Float4x4 GlmToOzz(const glm::mat4& glmMatrix)
	{
		ozz::math::Float4x4 result;
		for (int col = 0; col < 4; ++col) {
			result.cols[col] = ozz::math::simd_float4::Load(
				glmMatrix[col][0], glmMatrix[col][1],
				glmMatrix[col][2], glmMatrix[col][3]);
		}
		return result;
	}

	JPH::Mat44 OzzToJolt(const ozz::math::Float4x4& ozzMatrix)
	{
		JPH::Mat44 result;
		for (int col = 0; col < 4; ++col) {
			for (int row = 0; row < 4; ++row) {
				result(row, col) = ozzMatrix.cols[col].m128_f32[row];
			}
		}
		return result;
	}

	ozz::math::Float4x4 JoltToOzz(const JPH::Mat44& joltMatrix)
	{
		ozz::math::Float4x4 result;
		for (int col = 0; col < 4; ++col) {
			JPH::Vec4 column = joltMatrix.GetColumn4(col);
			result.cols[col] = ozz::math::simd_float4::Load(
				column.GetX(), column.GetY(),
				column.GetZ(), column.GetW());
		}
		return result;
	}

	JPH::Mat44 GlmToJolt(const glm::mat4& glmMatrix)
	{
		return JPH::Mat44(
			JPH::Vec4(glmMatrix[0][0], glmMatrix[0][1], glmMatrix[0][2], glmMatrix[0][3]),
			JPH::Vec4(glmMatrix[1][0], glmMatrix[1][1], glmMatrix[1][2], glmMatrix[1][3]),
			JPH::Vec4(glmMatrix[2][0], glmMatrix[2][1], glmMatrix[2][2], glmMatrix[2][3]),
			JPH::Vec4(glmMatrix[3][0], glmMatrix[3][1], glmMatrix[3][2], glmMatrix[3][3])
		);
	}

	glm::mat4 JoltToGlm(const JPH::Mat44& joltMatrix)
	{
		glm::mat4 result;
		for (int col = 0; col < 4; ++col) {
			JPH::Vec4 column = joltMatrix.GetColumn4(col);
			result[col][0] = column.GetX();
			result[col][1] = column.GetY();
			result[col][2] = column.GetZ();
			result[col][3] = column.GetW();
		}
		return result;
	}

	glm::quat JoltToGlm(const JPH::Quat& joltQuat)
	{
		return glm::quat(joltQuat.GetW(), joltQuat.GetX(), joltQuat.GetY(), joltQuat.GetZ());
	}

	JPH::Quat GlmToJolt(const glm::quat& glmQuat)
	{
		return JPH::Quat(glmQuat.x, glmQuat.y, glmQuat.z, glmQuat.w);
	}

	glm::vec3 JoltToGlm(const JPH::Vec3& joltVec)
	{
		return glm::vec3(joltVec.GetX(), joltVec.GetY(), joltVec.GetZ());
	}

	JPH::Vec3 GlmToJolt(const glm::vec3& glmVec)
	{
		return JPH::Vec3(glmVec.x, glmVec.y, glmVec.z);
	}

	glm::vec3 JoltRVecToGlm(const JPH::RVec3& joltRVec)
	{
		return glm::vec3(
			static_cast<float>(joltRVec.GetX()),
			static_cast<float>(joltRVec.GetY()),
			static_cast<float>(joltRVec.GetZ())
		);
	}

	JPH::RVec3 GlmVecToJoltR(const glm::vec3& glmVec)
	{
		return JPH::RVec3(
			static_cast<JPH::Real>(glmVec.x),
			static_cast<JPH::Real>(glmVec.y),
			static_cast<JPH::Real>(glmVec.z)
		);
	}


	void ExtractTransformFromSoA(const ozz::math::SoaTransform& soa, int index,
		glm::vec3& outTranslation, glm::quat& outRotation, glm::vec3& outScale)
	{
		assert(index >= 0 && index < 4);

		outTranslation = glm::vec3(
			soa.translation.x.m128_f32[index],
			soa.translation.y.m128_f32[index],
			soa.translation.z.m128_f32[index]
		);

		outRotation = glm::quat(
			soa.rotation.w.m128_f32[index], 
			soa.rotation.x.m128_f32[index],
			soa.rotation.y.m128_f32[index],
			soa.rotation.z.m128_f32[index]
		);

		outScale = glm::vec3(
			soa.scale.x.m128_f32[index],
			soa.scale.y.m128_f32[index],
			soa.scale.z.m128_f32[index]
		);
	}

	void InsertTransformIntoSoA(ozz::math::SoaTransform& soa, int index,
		const glm::vec3& translation, const glm::quat& rotation, const glm::vec3& scale)
	{
		assert(index >= 0 && index < 4);

		soa.translation.x.m128_f32[index] = translation.x;
		soa.translation.y.m128_f32[index] = translation.y;
		soa.translation.z.m128_f32[index] = translation.z;

		soa.rotation.x.m128_f32[index] = rotation.x;
		soa.rotation.y.m128_f32[index] = rotation.y;
		soa.rotation.z.m128_f32[index] = rotation.z;
		soa.rotation.w.m128_f32[index] = rotation.w;

		soa.scale.x.m128_f32[index] = scale.x;
		soa.scale.y.m128_f32[index] = scale.y;
		soa.scale.z.m128_f32[index] = scale.z;
	}
}