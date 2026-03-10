#pragma once
#include "Graphics.h"
#include "glm/glm.hpp"
#include <vector>

class Material;

struct AABB
{
    glm::vec3 min;
    glm::vec3 max;

    AABB() : min(FLT_MAX), max(-FLT_MAX) {}
    AABB(const glm::vec3& min, const glm::vec3& max) : min(min), max(max) {}

    void Expand(const glm::vec3& point)
    {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    glm::vec3 GetCenter() const { return (min + max) * 0.5f; }
    glm::vec3 GetExtents() const { return (max - min) * 0.5f; }
};

class Mesh
{
public:
    Mesh() : m_triCount(0), m_indexCount(0), m_VAO(0), m_VBO(0), m_IBO(0) {}
    virtual ~Mesh();

    struct Vertex
    {
        glm::vec4 position;
        glm::vec4 normal;
        glm::vec2 texCoord;
        glm::vec4 tangent;
        glm::vec4 bitangent;
        glm::vec4 colour;
    };

    void Initialise(unsigned int vertexCount, const Vertex* vertices, unsigned int indexCount = 0, unsigned int* indices = nullptr);
    void LoadFromFile(const char* filename);
    void CreateQuad();
    void CreatePlane(float width, float depth, unsigned int segmentsW = 1, unsigned int segmentsD = 1);
    void CreateBox(float width, float height, float depth);
    void CreateSphere(float radius, unsigned int rings = 16, unsigned int sectors = 32);
    void CreateCapsule(float radius, float height, unsigned int segments = 32);
    void CreateCylinder(float radius, float height, unsigned int segments);
    void CreateCone(float radius, float height, int segments = 32);

    unsigned int GetTriCount() const { return m_triCount; }
    unsigned int GetVAO() const { return m_VAO; }
    unsigned int GetVBO() const { return m_VBO; }
    unsigned int GetIBO() const { return m_IBO; }

    const std::vector<glm::vec3>& GetVertices() const { return m_vertices; }
    const std::vector<uint32_t>& GetIndices() const { return m_indices; }
    bool HasPhysicsData() const { return !m_vertices.empty(); }

    const AABB& GetBoundingBox() const { return m_boundingBox; }

    virtual void Render();
    void RenderInstanced(int instanceCount);
    void SetupInstancedAttributes(GLuint instanceVBO);

protected:
    unsigned int m_triCount;
    unsigned int m_VAO, m_VBO, m_IBO;
    unsigned int m_indexCount;
    bool m_hasInstancedAttributes = false;

    std::vector<glm::vec3> m_vertices;
    std::vector<uint32_t> m_indices;

    AABB m_boundingBox;

private:
    void StorePhysicsData(unsigned int vertexCount, const Vertex* vertices, unsigned int indexCount, const unsigned int* indices);
    void CalculateBoundingBox(unsigned int vertexCount, const Vertex* vertices);
};