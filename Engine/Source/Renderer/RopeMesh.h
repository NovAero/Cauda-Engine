#pragma once
#include "Mesh.h"
#include <glm/glm.hpp>
#include <vector>

class RopeMesh : public Mesh
{
public:
    RopeMesh();
    ~RopeMesh() = default;

    void Initialise(int segments = 20, int radialSegments = 8);

    void UpdateFromControlPoints(const std::vector<glm::vec3>& controlPoints,
        float thickness = 0.1f,
        bool closedLoop = false);

    void UpdateFromControlPoints(const std::vector<glm::vec3>& controlPoints,
        const std::vector<float>& thicknessValues,
        bool closedLoop = false);

    void SetUVTiling(float lengthTiling = 1.0f, float radialTiling = 1.0f);
    void SetGenerateCaps(bool generateCaps) { m_generateCaps = generateCaps; }

private:
    void GenerateMesh(const std::vector<glm::vec3>& splinePoints,
        const std::vector<float>& thicknessValues);

    void GenerateEndCaps(const std::vector<glm::vec3>& splinePoints,
        const std::vector<float>& thicknessValues,
        const std::vector<glm::vec3>& tangents,
        const std::vector<glm::vec3>& normals,
        const std::vector<glm::vec3>& binormals);

    glm::vec3 CatmullRomSpline(const glm::vec3& p0, const glm::vec3& p1,
        const glm::vec3& p2, const glm::vec3& p3,
        float t);

    void CalculateFrenetFrames(const std::vector<glm::vec3>& points,
        std::vector<glm::vec3>& tangents,
        std::vector<glm::vec3>& normals,
        std::vector<glm::vec3>& binormals);

    int m_segmentCount;
    int m_radialSegments;
 
    float m_lengthTiling;
    float m_radialTiling;
    bool m_generateCaps;

    std::vector<Vertex> m_vertices;
    std::vector<unsigned int> m_indices;
};