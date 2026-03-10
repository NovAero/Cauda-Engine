#include "cepch.h"
#include "PhysicsDebugRenderer.h"

#include "ShaderProgram.h"
#include "Graphics.h"
#include <iostream>

PhysicsDebugRenderer::PhysicsDebugRenderer()
{
}

PhysicsDebugRenderer::~PhysicsDebugRenderer()
{
    Cleanup();
}

void PhysicsDebugRenderer::Initialise()
{
    if (m_initialised) return;

    m_lineShader = std::make_unique<ShaderProgram>(
        "shaders/debug_line.vert",
        "shaders/debug_line.frag"
    );

    m_triangleShader = std::make_unique<ShaderProgram>(
        "shaders/debug_triangle.vert",
        "shaders/debug_triangle.frag"
    );


    InitialiseLineRendering();
    InitialiseTriangleRendering();

    m_initialised = true;
}

void PhysicsDebugRenderer::Cleanup()
{
    if (m_lineVAO != 0) {
        glDeleteVertexArrays(1, &m_lineVAO);
        glDeleteBuffers(1, &m_lineVBO);
        m_lineVAO = m_lineVBO = 0;
    }

    if (m_triangleVAO != 0) {
        glDeleteVertexArrays(1, &m_triangleVAO);
        glDeleteBuffers(1, &m_triangleVBO);
        m_triangleVAO = m_triangleVBO = 0;
    }

    m_lineShader.reset();
    m_triangleShader.reset();
    m_initialised = false;
}

void PhysicsDebugRenderer::InitialiseLineRendering()
{
    glGenVertexArrays(1, &m_lineVAO);
    glGenBuffers(1, &m_lineVBO);

    glBindVertexArray(m_lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, colour));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void PhysicsDebugRenderer::InitialiseTriangleRendering()
{
    glGenVertexArrays(1, &m_triangleVAO);
    glGenBuffers(1, &m_triangleVBO);

    glBindVertexArray(m_triangleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_triangleVBO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, colour));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(TriangleVertex), (void*)offsetof(TriangleVertex, normal));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void PhysicsDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor)
{
    glm::vec3 colour = ColourToGLM(inColor);

    m_lines.push_back({ JoltToGLM(inFrom), colour });
    m_lines.push_back({ JoltToGLM(inTo), colour });
}


void PhysicsDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow)
{
    glm::vec3 colour = ColourToGLM(inColor);
    glm::vec3 v1 = JoltToGLM(inV1);
    glm::vec3 v2 = JoltToGLM(inV2);
    glm::vec3 v3 = JoltToGLM(inV3);

    glm::vec3 normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));

    m_triangles.push_back({ v1, colour, normal });
    m_triangles.push_back({ v2, colour, normal });
    m_triangles.push_back({ v3, colour, normal });
}

void PhysicsDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight)
{
    m_textEntries.push_back({
        JoltToGLM(inPosition),
        std::string(inString),
        ColourToGLM(inColor),
        inHeight
        });
}

void PhysicsDebugRenderer::Render(glm::mat4 viewMatrix, glm::mat4 projMatrix)
{
    if (!m_initialised) return;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    RenderLines(viewMatrix, projMatrix);
    RenderTriangles(viewMatrix, projMatrix);
    RenderText(viewMatrix, projMatrix);

    glDisable(GL_BLEND);
}

void PhysicsDebugRenderer::RenderLines(glm::mat4 viewMatrix, glm::mat4 projMatrix)
{
    if (m_lines.empty() || !m_lineShader) return;

    m_lineShader->Use();

    glm::mat4 projection = projMatrix;
    glm::mat4 view = viewMatrix;
    m_lineShader->SetMat4Uniform("projection", projection);
    m_lineShader->SetMat4Uniform("view", view);

    glBindVertexArray(m_lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);
    glBufferData(GL_ARRAY_BUFFER, m_lines.size() * sizeof(LineVertex), m_lines.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_lines.size()));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void PhysicsDebugRenderer::RenderTriangles(glm::mat4 viewMatrix, glm::mat4 projMatrix)
{
    if (m_triangles.empty() || !m_triangleShader) return;

    m_triangleShader->Use();

    glm::mat4 projection = projMatrix;
    glm::mat4 view = viewMatrix;
    m_triangleShader->SetMat4Uniform("projection", projection);
    m_triangleShader->SetMat4Uniform("view", view);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);

    glBindVertexArray(m_triangleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_triangleVBO);
    glBufferData(GL_ARRAY_BUFFER, m_triangles.size() * sizeof(TriangleVertex), m_triangles.data(), GL_DYNAMIC_DRAW);

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_triangles.size()));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glDisable(GL_POLYGON_OFFSET_FILL);
}

void PhysicsDebugRenderer::RenderText(glm::mat4 viewMatrix, glm::mat4 projMatrix)
{
    for (const auto& entry : m_textEntries)
    {
        // TODO: Implement text rendering 
    }
}

void PhysicsDebugRenderer::Clear()
{
    m_lines.clear();
    m_triangles.clear();
    m_textEntries.clear();
}

glm::vec3 PhysicsDebugRenderer::JoltToGLM(JPH::RVec3Arg vec) const
{
    return glm::vec3(vec.GetX(), vec.GetY(), vec.GetZ());
}

glm::vec3 PhysicsDebugRenderer::ColourToGLM(JPH::ColorArg colour) const
{
    return glm::vec3(
        colour.r / 255.0f,
        colour.g / 255.0f,
        colour.b / 255.0f
    );
}