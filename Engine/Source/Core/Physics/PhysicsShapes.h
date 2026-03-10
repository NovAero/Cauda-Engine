#pragma once
#include "JoltPhysics.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

enum class ShapeType;
struct SubShapeData;

class PhysicsShapes
{
public:
    static JPH::ShapeSettings::ShapeResult CreateBox(const glm::vec3& dimensions);
    static JPH::ShapeSettings::ShapeResult CreateSphere(float radius);
    static JPH::ShapeSettings::ShapeResult CreateCapsule(float radius, float height);
    static JPH::ShapeSettings::ShapeResult CreateCylinder(float radius, float height);
    static JPH::ShapeSettings::ShapeResult CreatePlane(const glm::vec3& normal, float halfExtent = JPH::PlaneShapeSettings::cDefaultHalfExtent);
    static JPH::ShapeSettings::ShapeResult CreateMesh(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices);
    static JPH::ShapeSettings::ShapeResult CreateConvexHull(const std::vector<glm::vec3>& vertices, float maxConvexRadius = 0.05f);
    static JPH::ShapeSettings::ShapeResult CreateStaticCompound(const std::vector<SubShapeData>& subShapes);

private:
    static JPH::Vec3 ToJolt(const glm::vec3& vec);
    static JPH::Quat ToJoltQuat(const glm::quat& quat);
    static glm::vec3 ToGLM(const JPH::Vec3& vec);
    static glm::quat ToGLMQuat(const JPH::Quat& quat);
    static glm::vec3 ValidateDimensions(const glm::vec3& dimensions, ShapeType shapeType);
    static bool ValidateMeshData(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices);
    static bool ValidateConvexHullData(const std::vector<glm::vec3>& vertices);
};