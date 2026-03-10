#pragma once
#ifndef JPH_DEBUG_RENDERER
#define JPH_DEBUG_RENDERER 1
#endif

#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRendererSimple.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class ShaderProgram;

class PhysicsDebugRenderer : public JPH::DebugRendererSimple
{
public:
    PhysicsDebugRenderer();
    ~PhysicsDebugRenderer();

    void Initialise();
    void Cleanup();
    void Clear();

    void Render(glm::mat4 viewMatrix, glm::mat4 projMatrix);

    virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;
    virtual void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) override;
    virtual void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight) override;

private:
    struct LineVertex
    {
        glm::vec3 position;
        glm::vec3 colour;
    };

    struct TriangleVertex 
    {
        glm::vec3 position;
        glm::vec3 colour;
        glm::vec3 normal;
    };

    struct TextEntry 
    {
        glm::vec3 position;
        std::string text;
        glm::vec3 colour;
        float height;
    };

    unsigned int m_lineVAO = 0, m_lineVBO = 0;
    unsigned int m_triangleVAO = 0, m_triangleVBO = 0;
    std::unique_ptr<ShaderProgram> m_lineShader;
    std::unique_ptr<ShaderProgram> m_triangleShader;

    std::vector<LineVertex> m_lines;
    std::vector<TriangleVertex> m_triangles;
    std::vector<TextEntry> m_textEntries;

    bool m_initialised = false;

    glm::vec3 JoltToGLM(JPH::RVec3Arg vec) const;
    glm::vec3 ColourToGLM(JPH::ColorArg color) const;

    void InitialiseLineRendering();
    void InitialiseTriangleRendering();
    void RenderLines(glm::mat4 viewMatrix, glm::mat4 projMatrix);
    void RenderTriangles(glm::mat4 viewMatrix, glm::mat4 projMatrix);
    void RenderText(glm::mat4 viewMatrix, glm::mat4 projMatrix);
};