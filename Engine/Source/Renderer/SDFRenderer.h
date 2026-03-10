#pragma once
#include "ShaderProgram.h"
#include "Graphics.h"
#include "Core/Components/Components.h"
#include <ThirdParty/flecs.h>
#include <vector>
#include <memory>


enum class SDFPrimitiveType : int {
    SPHERE = 0,
    BOX = 1,
    ROUND_BOX = 2,
    TORUS = 3,
    PLANE = 4
};

enum class SDFOperation : int {
    UNION = 0,
    SUBTRACTION = 1,
    INTERSECTION = 2,
    SMOOTH_UNION = 3
};

struct SDFRenderableComponent {
    int primitiveIndex = -1;
    bool visible = true;
};

struct SDFMaterialComponent {
    uint32_t materialIndex = 3;  
};

struct SDFPrimitiveComponent {
    SDFPrimitiveType type = SDFPrimitiveType::SPHERE;
    SDFOperation operation = SDFOperation::UNION;
    float blendFactor = 0.0f;
    glm::vec4 parameters = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); 
};

struct GPUPrimitive {
    glm::vec4 rotation;          
    glm::vec4 parameters;        
    glm::vec3 position;          
    float blendFactor;           
    int32_t type;                
    int32_t operation;           
    uint32_t materialIndex;      
    float padding4;         
};

struct GPUPrimitiveBuffer {
    int32_t primitiveCount;  
    int32_t padding[3] = { 0 };
};


class SDFRenderer {
private:
    flecs::world& m_world;
    std::vector<GPUPrimitive> m_primitives;
    bool m_isDirty = true;

    static constexpr int PRIMITIVE_BUFFER_BINDING = 0;

    void UpdateBuffers();
    void InitialiseShaders();
    void RemovePrimitive(int index);

    void DebugMemoryLayout() const;

public:
    SDFRenderer(flecs::world& ecs);
    ~SDFRenderer();

    SDFRenderer(const SDFRenderer&) = delete;
    SDFRenderer& operator=(const SDFRenderer&) = delete;
    SDFRenderer(SDFRenderer&&) = delete;
    SDFRenderer& operator=(SDFRenderer&&) = delete;

    GLuint m_primitivesBuffer = 0;
    std::unique_ptr<ShaderProgram> m_sdfShader;

    void Render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void UpdatePrimitive(flecs::entity entity, SDFPrimitiveType type,
        const glm::vec4& parameters, SDFOperation operation,
        float blendFactor);
    void SetSmoothBlending(flecs::entity e, float blendFactor);
    void SetOperation(flecs::entity e, SDFOperation operation);
    void SetMaterial(flecs::entity e, uint32_t materialIndex);
    void SetVisibility(flecs::entity e, bool visible);
    void Clear();

    int GetPrimitiveCount() const { return static_cast<int>(m_primitives.size()); }
};