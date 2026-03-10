#include "cepch.h"
#include "PhysicsShapes.h"
#include "Core/Physics/PhysicsModule.h"
#include "Core/Editor/ResourceLibrary.h"

#include <iostream>
#include <algorithm>

JPH::ShapeSettings::ShapeResult PhysicsShapes::CreateBox(const glm::vec3& dimensions)
{
    glm::vec3 validDim = glm::max(dimensions, glm::vec3(0.01f));
    JPH::BoxShapeSettings settings(ToJolt(validDim * 0.5f));
    return settings.Create();
}

JPH::ShapeSettings::ShapeResult PhysicsShapes::CreateSphere(float radius)
{
    float validRadius = std::max(radius, 0.01f);
    JPH::SphereShapeSettings settings(validRadius);
    return settings.Create();
}

JPH::ShapeSettings::ShapeResult PhysicsShapes::CreateCapsule(float radius, float height)
{
    float validRadius = std::max(radius, 0.01f);
    float validHeight = std::max(height, 0.02f);
    JPH::CapsuleShapeSettings settings(validHeight, validRadius);
    return settings.Create();
}

JPH::ShapeSettings::ShapeResult PhysicsShapes::CreateCylinder(float radius, float height)
{
    float validRadius = std::max(radius, 0.01f);
    float validHeight = std::max(height, 0.01f);
    JPH::CylinderShapeSettings settings(validHeight * 0.5f, validRadius);
    return settings.Create();
}

JPH::ShapeSettings::ShapeResult PhysicsShapes::CreatePlane(const glm::vec3& normal, float halfExtent)
{
    glm::vec3 normalizedNormal = glm::normalize(normal);
    JPH::Plane plane(ToJolt(normalizedNormal), 0.0f);
    float validHalfExtent = halfExtent > 0.0f ? halfExtent : JPH::PlaneShapeSettings::cDefaultHalfExtent;
    JPH::PlaneShapeSettings settings(plane, nullptr, validHalfExtent);
    return settings.Create();
}

JPH::ShapeSettings::ShapeResult PhysicsShapes::CreateMesh(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices)
{
    if (!ValidateMeshData(vertices, indices))
    {
        JPH::ShapeSettings::ShapeResult result;
        result.SetError("Invalid mesh data provided");
        return result;
    }

    JPH::VertexList joltVertices;
    joltVertices.reserve(vertices.size());
    for (const auto& vertex : vertices)
    {
        joltVertices.push_back(JPH::Float3(vertex.x, vertex.y, vertex.z));
    }

    JPH::IndexedTriangleList joltTriangles;
    joltTriangles.reserve(indices.size() / 3);
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        if (i + 2 < indices.size())
        {
            joltTriangles.push_back(JPH::IndexedTriangle(indices[i], indices[i + 1], indices[i + 2]));
        }
    }


    JPH::MeshShapeSettings settings(joltVertices, joltTriangles);
    return settings.Create();
}

JPH::ShapeSettings::ShapeResult PhysicsShapes::CreateConvexHull(const std::vector<glm::vec3>& vertices, float maxConvexRadius)
{
    if (!ValidateConvexHullData(vertices))
    {
        JPH::ShapeSettings::ShapeResult result;
        result.SetError("Invalid convex hull data provided");
        return result;
    }

    JPH::Array<JPH::Vec3> joltPoints;
    joltPoints.reserve(vertices.size());
    for (const auto& vertex : vertices)
    {
        joltPoints.push_back(ToJolt(vertex));
    }

    float validMaxConvexRadius = std::max(maxConvexRadius, 0.001f);
    JPH::ConvexHullShapeSettings settings(joltPoints, validMaxConvexRadius);
    return settings.Create();
}

JPH::ShapeSettings::ShapeResult PhysicsShapes::CreateStaticCompound(const std::vector<SubShapeData>& subShapes)
{
    JPH::ShapeSettings::ShapeResult result;

    if (subShapes.empty())
    {
        result.SetError("Static compound shape requires at least one sub-shape");
        return result;
    }

    JPH::StaticCompoundShapeSettings compoundSettings;

    for (const SubShapeData& subShape : subShapes)
    {
        JPH::ShapeSettings::ShapeResult subShapeResult;

        switch (subShape.shapeType)
        {
        case ShapeType::Box:
            subShapeResult = CreateBox(subShape.dimensions);
            break;

        case ShapeType::Sphere:
            subShapeResult = CreateSphere(subShape.dimensions.x);
            break;

        case ShapeType::Capsule:
            subShapeResult = CreateCapsule(subShape.dimensions.x, subShape.dimensions.y);
            break;

        case ShapeType::Cylinder:
            subShapeResult = CreateCylinder(subShape.dimensions.x, subShape.dimensions.y);
            break;

        case ShapeType::ConvexHull:
        {
            if (subShape.meshAssetHandle.empty())
            {
                result.SetError("ConvexHull sub-shape requires a mesh asset handle");
                return result;
            }

            Mesh* mesh = ResourceLibrary::GetMesh(subShape.meshAssetHandle);
            if (!mesh || !mesh->HasPhysicsData())
            {
                result.SetError("Mesh asset not found for convex hull: " + subShape.meshAssetHandle);
                return result;
            }

            const std::vector<glm::vec3>& vertices = mesh->GetVertices();
            subShapeResult = CreateConvexHull(vertices);

            if (!subShapeResult.HasError())
            {
                JPH::ScaledShapeSettings scaledSettings(subShapeResult.Get(), ToJolt(subShape.dimensions));
                subShapeResult = scaledSettings.Create();
            }
            break;
        }

        case ShapeType::Mesh:
        {
            if (subShape.meshAssetHandle.empty())
            {
                result.SetError("Mesh sub-shape requires a mesh asset handle");
                return result;
            }

            Mesh* mesh = ResourceLibrary::GetMesh(subShape.meshAssetHandle);
            if (!mesh || !mesh->HasPhysicsData())
            {
                result.SetError("Mesh asset not found: " + subShape.meshAssetHandle);
                return result;
            }

            const std::vector<glm::vec3>& vertices = mesh->GetVertices();
            const std::vector<uint32_t>& indices = mesh->GetIndices();
            subShapeResult = CreateMesh(vertices, indices);

            if (!subShapeResult.HasError())
            {
                JPH::ScaledShapeSettings scaledSettings(subShapeResult.Get(), ToJolt(subShape.dimensions));
                subShapeResult = scaledSettings.Create();
            }
            break;
        }

        default:
            result.SetError("Unsupported shape type for compound sub-shape");
            return result;
        }

        if (subShapeResult.HasError())
        {
            result.SetError("Failed to create sub-shape: " + std::string(subShapeResult.GetError().c_str()));
            return result;
        }

        compoundSettings.AddShape(
            ToJolt(subShape.position),
            ToJoltQuat(subShape.rotation),
            subShapeResult.Get(),
            subShape.userData
        );
    }

    return compoundSettings.Create();
}


JPH::Vec3 PhysicsShapes::ToJolt(const glm::vec3& vec)
{
    return JPH::Vec3(vec.x, vec.y, vec.z);
}

JPH::Quat PhysicsShapes::ToJoltQuat(const glm::quat& quat)
{
    return JPH::Quat(quat.x, quat.y, quat.z, quat.w);
}

glm::vec3 PhysicsShapes::ToGLM(const JPH::Vec3& vec)
{
    return glm::vec3(vec.GetX(), vec.GetY(), vec.GetZ());
}

glm::quat PhysicsShapes::ToGLMQuat(const JPH::Quat& quat)
{
    return glm::quat(quat.GetW(), quat.GetX(), quat.GetY(), quat.GetZ());
}

glm::vec3 PhysicsShapes::ValidateDimensions(const glm::vec3& dimensions, ShapeType shapeType)
{
    glm::vec3 result = dimensions;

    switch (shapeType)
    {
    case ShapeType::Box:
        result = glm::max(result, glm::vec3(0.01f));
        break;

    case ShapeType::Sphere:
        result.x = std::max(result.x, 0.01f);
        break;

    case ShapeType::Capsule:
        result.x = std::max(result.x, 0.01f);
        result.y = std::max(result.y, 0.02f);
        break;

    case ShapeType::Cylinder:
        result.x = std::max(result.x, 0.01f);
        result.y = std::max(result.y, 0.01f);
        break;

    case ShapeType::Plane:
        result.x = result.x > 0.0f ? result.x : JPH::PlaneShapeSettings::cDefaultHalfExtent;
        break;

    default:
        result = glm::max(result, glm::vec3(0.01f));
        break;
    }

    return result;
}

bool PhysicsShapes::ValidateMeshData(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices)
{
    if (vertices.empty())
    {
        std::cerr << "Error: Mesh vertices cannot be empty" << std::endl;
        return false;
    }

    if (indices.empty() || indices.size() % 3 != 0)
    {
        std::cerr << "Error: Mesh indices must be non-empty and divisible by 3" << std::endl;
        return false;
    }

    uint32_t maxIndex = static_cast<uint32_t>(vertices.size() - 1);
    for (uint32_t index : indices)
    {
        if (index > maxIndex)
        {
            std::cerr << "Error: Mesh index " << index << " is out of range (max: " << maxIndex << ")" << std::endl;
            return false;
        }
    }

    return true;
}

bool PhysicsShapes::ValidateConvexHullData(const std::vector<glm::vec3>& vertices)
{
    if (vertices.empty())
    {
        std::cerr << "Error: Convex hull vertices cannot be empty" << std::endl;
        return false;
    }

    if (vertices.size() < 4)
    {
        std::cerr << "Error: Convex hull requires at least 4 vertices to form a 3D shape" << std::endl;
        return false;
    }

    if (vertices.size() >= 4)
    {
        bool allSameX = true,
            allSameY = true,
            allSameZ = true;

        float firstX = vertices[0].x,
            firstY = vertices[0].y,
            firstZ = vertices[0].z;

        for (size_t i = 1; i < vertices.size() && (allSameX || allSameY || allSameZ); ++i)
        {
            if (std::abs(vertices[i].x - firstX) > 1e-6f) allSameX = false;
            if (std::abs(vertices[i].y - firstY) > 1e-6f) allSameY = false;
            if (std::abs(vertices[i].z - firstZ) > 1e-6f) allSameZ = false;
        }

        if (allSameX || allSameY || allSameZ)
        {
            std::cerr << "Error: Convex hull vertices are coplanar or collinear" << std::endl;
            return false;
        }
    }

    return true;
}