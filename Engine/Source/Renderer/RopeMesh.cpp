#include "cepch.h"
#include "RopeMesh.h"
#include <glm/gtc/constants.hpp>

RopeMesh::RopeMesh()
    : m_segmentCount(20)
    , m_radialSegments(8)
    , m_lengthTiling(1.0f)
    , m_radialTiling(1.0f)
    , m_generateCaps(true)
{
}

void RopeMesh::Initialise(int segments, int radialSegments)
{
    m_segmentCount = segments;
    m_radialSegments = radialSegments;

    int vertexCount = (m_segmentCount + 1) * (m_radialSegments + 1);
    if (m_generateCaps)
    {
        vertexCount += 2 * (m_radialSegments + 1); 
    }

    int indexCount = m_segmentCount * m_radialSegments * 6;
    if (m_generateCaps)
    {
        indexCount += 2 * m_radialSegments * 3; 
    }

    m_vertices.reserve(vertexCount);
    m_indices.reserve(indexCount);

    std::vector<glm::vec3> initialPoints;
    for (int i = 0; i <= m_segmentCount; ++i)
    {
        float t = static_cast<float>(i) / m_segmentCount;
        initialPoints.push_back(glm::vec3(0, -t * 2.0f, 0));
    }

    std::vector<float> thickness(initialPoints.size(), 0.1f);
    GenerateMesh(initialPoints, thickness);
}

void RopeMesh::UpdateFromControlPoints(const std::vector<glm::vec3>& controlPoints,
    float thickness,
    bool closedLoop)
{
    if (controlPoints.size() < 2) return;

    std::vector<float> thicknessValues(controlPoints.size(), thickness);
    UpdateFromControlPoints(controlPoints, thicknessValues, closedLoop);
}

void RopeMesh::UpdateFromControlPoints(const std::vector<glm::vec3>& controlPoints,
    const std::vector<float>& thicknessValues,
    bool closedLoop)
{
    if (controlPoints.size() < 2) return;

    std::vector<glm::vec3> splinePoints;
    std::vector<float> splineThickness;

    int splineSegmentsPerControl = std::max(1, m_segmentCount / static_cast<int>(controlPoints.size() - 1));

    for (size_t i = 0; i < controlPoints.size() - 1; ++i)
    {
        glm::vec3 p0 = (i == 0) ? controlPoints[i] : controlPoints[i - 1];
        glm::vec3 p1 = controlPoints[i];
        glm::vec3 p2 = controlPoints[i + 1];
        glm::vec3 p3 = (i == controlPoints.size() - 2) ? controlPoints[i + 1] : controlPoints[i + 2];

        float t0 = (i < thicknessValues.size()) ? thicknessValues[i] : 0.1f;
        float t1 = (i + 1 < thicknessValues.size()) ? thicknessValues[i + 1] : 0.1f;

        for (int j = 0; j < splineSegmentsPerControl; ++j)
        {
            float t = static_cast<float>(j) / splineSegmentsPerControl;
            glm::vec3 point = CatmullRomSpline(p0, p1, p2, p3, t);
            float thick = glm::mix(t0, t1, t);

            splinePoints.push_back(point);
            splineThickness.push_back(thick);
        }
    }

    splinePoints.push_back(controlPoints.back());
    splineThickness.push_back(thicknessValues.empty() ? 0.1f : thicknessValues.back());

    GenerateMesh(splinePoints, splineThickness);
}

glm::vec3 RopeMesh::CatmullRomSpline(const glm::vec3& p0, const glm::vec3& p1,
    const glm::vec3& p2, const glm::vec3& p3,
    float t)
{
    float t2 = t * t;
    float t3 = t2 * t;

    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
        );
}

void RopeMesh::GenerateMesh(const std::vector<glm::vec3>& splinePoints,
    const std::vector<float>& thicknessValues)
{
    if (splinePoints.size() < 2) return;

    m_vertices.clear();
    m_indices.clear();

    std::vector<glm::vec3> tangents, normals, binormals;
    CalculateFrenetFrames(splinePoints, tangents, normals, binormals);

    float totalLength = 0.0f;
    std::vector<float> segmentLengths;

    for (size_t i = 0; i < splinePoints.size() - 1; ++i)
    {
        float segLength = glm::length(splinePoints[i + 1] - splinePoints[i]);
        segmentLengths.push_back(segLength);
        totalLength += segLength;
    }

    float currentLength = 0.0f;

    for (size_t i = 0; i < splinePoints.size(); ++i)
    {
        float uvV = (totalLength > 0.0f) ? (currentLength / totalLength) * m_lengthTiling : 0.0f;
        float radius = (i < thicknessValues.size()) ? thicknessValues[i] : 0.1f;

        for (int j = 0; j <= m_radialSegments; ++j)
        {
            float angle = (static_cast<float>(j) / m_radialSegments) * 2.0f * glm::pi<float>();
            float uvU = static_cast<float>(j) / m_radialSegments * m_radialTiling;

            glm::vec3 localPos = normals[i] * cosf(angle) + binormals[i] * sinf(angle);
            glm::vec3 position = splinePoints[i] + localPos * radius;

            glm::vec3 normal = glm::normalize(localPos);

            Vertex vertex;
            vertex.position = glm::vec4(position, 1.0f);
            vertex.normal = glm::vec4(normal, 0.0f);
            vertex.texCoord = glm::vec2(uvU, uvV);
            vertex.tangent = glm::vec4(tangents[i], 0.0f);
            vertex.bitangent = glm::vec4(binormals[i], 0.0f);
            vertex.colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

            m_vertices.push_back(vertex);
        }

        if (i < splinePoints.size() - 1)
        {
            currentLength += segmentLengths[i];
        }
    }

    for (size_t i = 0; i < splinePoints.size() - 1; ++i)
    {
        for (int j = 0; j < m_radialSegments; ++j)
        {
            int current = i * (m_radialSegments + 1) + j;
            int next = current + 1;
            int below = current + (m_radialSegments + 1);
            int belowNext = below + 1;

            m_indices.push_back(current);
            m_indices.push_back(next);
            m_indices.push_back(below);

            m_indices.push_back(next);
            m_indices.push_back(belowNext);
            m_indices.push_back(below);
        }
    }

    if (m_generateCaps)
    {
        GenerateEndCaps(splinePoints, thicknessValues, tangents, normals, binormals);
    }

    if (m_VAO == 0)
    {
        Mesh::Initialise(m_vertices.size(), m_vertices.data(), m_indices.size(), m_indices.data());
    }
    else
    {
        glBindVertexArray(m_VAO);

        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), m_vertices.data(), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_DYNAMIC_DRAW);

        m_indexCount = m_indices.size();
        m_triCount = m_indices.size() / 3;

        glBindVertexArray(0);
    }
}

void RopeMesh::GenerateEndCaps(const std::vector<glm::vec3>& splinePoints,
    const std::vector<float>& thicknessValues,
    const std::vector<glm::vec3>& tangents,
    const std::vector<glm::vec3>& normals,
    const std::vector<glm::vec3>& binormals)
{
    if (splinePoints.size() < 2) return;

    int baseVertexCount = splinePoints.size() * (m_radialSegments + 1);

    {
        size_t startIdx = 0;
        float radius = (startIdx < thicknessValues.size()) ? thicknessValues[startIdx] : 0.1f;
        glm::vec3 center = splinePoints[startIdx];
        glm::vec3 capNormal = -tangents[startIdx];

        Vertex centerVertex;
        centerVertex.position = glm::vec4(center, 1.0f);
        centerVertex.normal = glm::vec4(capNormal, 0.0f);
        centerVertex.texCoord = glm::vec2(0.5f, 0.5f);
        centerVertex.tangent = glm::vec4(normals[startIdx], 0.0f);
        centerVertex.bitangent = glm::vec4(binormals[startIdx], 0.0f);
        centerVertex.colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        m_vertices.push_back(centerVertex);

        int centerVertexIdx = m_vertices.size() - 1;

        for (int j = 0; j < m_radialSegments; ++j)
        {
            int current = startIdx * (m_radialSegments + 1) + j;
            int next = startIdx * (m_radialSegments + 1) + ((j + 1) % m_radialSegments);

            m_indices.push_back(centerVertexIdx);
            m_indices.push_back(next);
            m_indices.push_back(current);
        }
    }

    {
        size_t endIdx = splinePoints.size() - 1;
        float radius = (endIdx < thicknessValues.size()) ? thicknessValues[endIdx] : 0.1f;
        glm::vec3 center = splinePoints[endIdx];
        glm::vec3 capNormal = tangents[endIdx]; 


        Vertex centerVertex;
        centerVertex.position = glm::vec4(center, 1.0f);
        centerVertex.normal = glm::vec4(capNormal, 0.0f);
        centerVertex.texCoord = glm::vec2(0.5f, 0.5f);
        centerVertex.tangent = glm::vec4(normals[endIdx], 0.0f);
        centerVertex.bitangent = glm::vec4(binormals[endIdx], 0.0f);
        centerVertex.colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        m_vertices.push_back(centerVertex);

        int centerVertexIdx = m_vertices.size() - 1;

        for (int j = 0; j < m_radialSegments; ++j)
        {
            int current = endIdx * (m_radialSegments + 1) + j;
            int next = endIdx * (m_radialSegments + 1) + ((j + 1) % m_radialSegments);

            m_indices.push_back(centerVertexIdx);
            m_indices.push_back(current);
            m_indices.push_back(next);
        }
    }
}

void RopeMesh::CalculateFrenetFrames(const std::vector<glm::vec3>& points,
    std::vector<glm::vec3>& tangents,
    std::vector<glm::vec3>& normals,
    std::vector<glm::vec3>& binormals)
{
    tangents.clear();
    normals.clear();
    binormals.clear();

    if (points.size() < 2) return;

    for (size_t i = 0; i < points.size(); ++i)
    {
        glm::vec3 tangent;

        if (i == 0)
        {
            tangent = glm::normalize(points[i + 1] - points[i]);
        }
        else if (i == points.size() - 1)
        {
            tangent = glm::normalize(points[i] - points[i - 1]);
        }
        else
        {
            tangent = glm::normalize(points[i + 1] - points[i - 1]);
        }

        tangents.push_back(tangent);
    }

    glm::vec3 firstNormal;
    glm::vec3 t = tangents[0];
    float minComponent = std::min(std::abs(t.x), std::min(std::abs(t.y), std::abs(t.z)));

    if (std::abs(t.x) == minComponent)
        firstNormal = glm::vec3(1, 0, 0);
    else if (std::abs(t.y) == minComponent)
        firstNormal = glm::vec3(0, 1, 0);
    else
        firstNormal = glm::vec3(0, 0, 1);

    firstNormal = glm::normalize(firstNormal - glm::dot(firstNormal, t) * t);
    normals.push_back(firstNormal);
    binormals.push_back(glm::cross(tangents[0], firstNormal));

    for (size_t i = 1; i < points.size(); ++i)
    {
        glm::vec3 prevTangent = tangents[i - 1];
        glm::vec3 currTangent = tangents[i];

        glm::vec3 axis = glm::cross(prevTangent, currTangent);
        float angle = acos(glm::clamp(glm::dot(prevTangent, currTangent), -1.0f, 1.0f));

        glm::vec3 normal = normals[i - 1];
        glm::vec3 binormal = binormals[i - 1];

        if (glm::length(axis) > 0.001f && std::abs(angle) > 0.001f)
        {
            axis = glm::normalize(axis);
            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), angle, axis);
            normal = glm::vec3(rotation * glm::vec4(normal, 0.0f));
            binormal = glm::vec3(rotation * glm::vec4(binormal, 0.0f));
        }

        normal = glm::normalize(normal - glm::dot(normal, currTangent) * currTangent);
        binormal = glm::cross(currTangent, normal);

        normals.push_back(normal);
        binormals.push_back(binormal);
    }
}

void RopeMesh::SetUVTiling(float lengthTiling, float radialTiling)
{
    m_lengthTiling = lengthTiling;
    m_radialTiling = radialTiling;
}
