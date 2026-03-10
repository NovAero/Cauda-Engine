#include "cepch.h"
#include "SDFRenderer.h"
#include "Core/Math/TransformModule.h"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>
#include <cstring>

SDFRenderer::SDFRenderer(flecs::world& ecs)
    : m_world(ecs)
{
    ecs.component<SDFRenderableComponent>();
    ecs.component<SDFMaterialComponent>();
    ecs.component<SDFPrimitiveComponent>();

    ecs.system<const TransformComponent, const SDFPrimitiveComponent,
        const SDFMaterialComponent, SDFRenderableComponent>("UpdateSDFPrimitives")
        .kind(flecs::OnUpdate)
        .each([this](const TransformComponent& transform,
            const SDFPrimitiveComponent& primitive,
            const SDFMaterialComponent& material,
            SDFRenderableComponent& renderable) 
            {
                if (!renderable.visible) return;

                if (renderable.primitiveIndex < 0)
                {
                    renderable.primitiveIndex = static_cast<int>(m_primitives.size());
                    m_primitives.push_back({});
                }

                if (renderable.primitiveIndex >= static_cast<int>(m_primitives.size())) 
                {
                    std::cerr << "Invalid primitive index: " << renderable.primitiveIndex << std::endl;
                    return;
                }

                GPUPrimitive& gpuPrim = m_primitives[renderable.primitiveIndex];

                std::memset(&gpuPrim, 0, sizeof(GPUPrimitive));

                gpuPrim.rotation = glm::vec4(transform.rotation.x, transform.rotation.y,
                    transform.rotation.z, transform.rotation.w);
                gpuPrim.parameters = primitive.parameters;
                gpuPrim.position = transform.position;
                gpuPrim.blendFactor = primitive.blendFactor;
                gpuPrim.type = static_cast<int>(primitive.type);
                gpuPrim.operation = static_cast<int>(primitive.operation);
                gpuPrim.materialIndex = material.materialIndex;


                m_isDirty = true;
            });

    ecs.system<>("UpdateSDFBuffers")
        .kind(flecs::PostUpdate)
        .each([this]()
            {
                if (m_isDirty) 
                {
                    UpdateBuffers();
                    m_isDirty = false;
                }
            });

    ecs.observer<SDFRenderableComponent>()
        .event(flecs::OnRemove)
        .each([this](SDFRenderableComponent& renderable)
            {
                RemovePrimitive(renderable.primitiveIndex);
            });

    InitialiseShaders();

    //DebugMemoryLayout();

    glGenBuffers(1, &m_primitivesBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_primitivesBuffer);

    size_t headerSize = sizeof(GPUPrimitiveBuffer);
    size_t initialCapacity = headerSize + 10 * sizeof(GPUPrimitive);
    glBufferData(GL_SHADER_STORAGE_BUFFER, initialCapacity, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

SDFRenderer::~SDFRenderer() 
{
    if (m_primitivesBuffer != 0) 
    {
        GLint currentBinding;
        glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &currentBinding);
        if (currentBinding == m_primitivesBuffer) 
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }

        glDeleteBuffers(1, &m_primitivesBuffer);
        m_primitivesBuffer = 0;
    }
}

void SDFRenderer::InitialiseShaders() 
{
    m_sdfShader = std::make_unique<ShaderProgram>("shaders/g_buffer.vert", "shaders/sdf_raymarch.frag");

    if (m_sdfShader)
    {
        m_sdfShader->Use();
    }
}

void SDFRenderer::RemovePrimitive(int index) 
{
    if (index < 0 || index >= static_cast<int>(m_primitives.size())) 
    {
        return;  
    }

    m_primitives.erase(m_primitives.begin() + index);

    m_world.each([index, this](flecs::entity e, SDFRenderableComponent& renderable)
        {
            if (renderable.primitiveIndex > index) {
                renderable.primitiveIndex--;
            }
        });

    m_isDirty = true;
}

void SDFRenderer::UpdateBuffers() 
{
    int primitiveCount = static_cast<int>(m_primitives.size());

    size_t headerSize = sizeof(GPUPrimitiveBuffer);
    size_t primitivesSize = primitiveCount * sizeof(GPUPrimitive);
    size_t bufferSize = headerSize + primitivesSize;

    //std::cout << "Updating buffer with " << primitiveCount << " primitives" << std::endl;
    //std::cout << "Buffer size: " << bufferSize << " bytes" << std::endl;
    //std::cout << "GPUPrimitive size: " << sizeof(GPUPrimitive) << " bytes" << std::endl;
    //std::cout << "Header size: " << headerSize << " bytes" << std::endl;

    std::vector<uint8_t> buffer(bufferSize, 0);

    GPUPrimitiveBuffer header;
    header.primitiveCount = primitiveCount;

    std::memcpy(buffer.data(), &header, headerSize);

    if (primitiveCount > 0) 
    {
        std::memcpy(buffer.data() + headerSize, m_primitives.data(), primitivesSize);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_primitivesBuffer);

    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, buffer.data(), GL_DYNAMIC_DRAW);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void SDFRenderer::Render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) 
{
    if (m_isDirty) 
    {
        UpdateBuffers();
        m_isDirty = false;
    }

    m_sdfShader->Use();

    m_sdfShader->SetMat4Uniform("view", viewMatrix);
    m_sdfShader->SetMat4Uniform("projection", projectionMatrix);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, PRIMITIVE_BUFFER_BINDING, m_primitivesBuffer);
}

void SDFRenderer::UpdatePrimitive(flecs::entity entity,
    SDFPrimitiveType type,
    const glm::vec4& parameters,
    SDFOperation operation,
    float blendFactor) {
    if (entity.has<SDFPrimitiveComponent>()) 
    {
        auto* primitive = entity.try_get_mut<SDFPrimitiveComponent>();
        primitive->type = type;
        primitive->operation = operation;
        primitive->blendFactor = blendFactor;
        primitive->parameters = parameters;
    }

    m_isDirty = true;
}

void SDFRenderer::SetSmoothBlending(flecs::entity e, float blendFactor) 
{
    if (e.has<SDFPrimitiveComponent>())
    {
        auto* primitive = e.try_get_mut<SDFPrimitiveComponent>();
        primitive->blendFactor = blendFactor;
        m_isDirty = true;
    }
}

void SDFRenderer::SetOperation(flecs::entity e, SDFOperation operation) 
{
    if (e.has<SDFPrimitiveComponent>()) 
    {
        auto* primitive = e.try_get_mut<SDFPrimitiveComponent>();
        primitive->operation = operation;
        m_isDirty = true;
    }
}

void SDFRenderer::SetMaterial(flecs::entity e, uint32_t materialIndex) 
{
    if (e.has<SDFMaterialComponent>()) 
    {
        auto* material = e.try_get_mut<SDFMaterialComponent>();
        material->materialIndex = materialIndex;
        m_isDirty = true;
    }
}

void SDFRenderer::SetVisibility(flecs::entity e, bool visible) 
{
    if (e.has<SDFRenderableComponent>())
    {
        auto* renderable = e.try_get_mut<SDFRenderableComponent>();
        renderable->visible = visible;
        m_isDirty = true;
    }
}

void SDFRenderer::Clear() 
{
    m_primitives.clear();

    m_isDirty = true;
    UpdateBuffers();
}

void SDFRenderer::DebugMemoryLayout() const 
{
    std::cout << "===== MEMORY LAYOUT DEBUG =====" << std::endl;
    std::cout << "GPUPrimitive total size: " << sizeof(GPUPrimitive) << " bytes" << std::endl;

    size_t rotationOffset = offsetof(GPUPrimitive, rotation);
    size_t parametersOffset = offsetof(GPUPrimitive, parameters);
    size_t positionOffset = offsetof(GPUPrimitive, position);
    size_t blendFactorOffset = offsetof(GPUPrimitive, blendFactor);
    size_t typeOffset = offsetof(GPUPrimitive, type);
    size_t operationOffset = offsetof(GPUPrimitive, operation);
    size_t materialIndexOffset = offsetof(GPUPrimitive, materialIndex);

    std::cout << "Field offsets:" << std::endl;
    std::cout << "  rotation:       " << rotationOffset << " bytes" << std::endl;
    std::cout << "  parameters:     " << parametersOffset << " bytes" << std::endl;
    std::cout << "  position:       " << positionOffset << " bytes" << std::endl;
    std::cout << "  blendFactor:    " << blendFactorOffset << " bytes" << std::endl;
    std::cout << "  type:           " << typeOffset << " bytes" << std::endl;
    std::cout << "  operation:      " << operationOffset << " bytes" << std::endl;
    std::cout << "  materialIndex:  " << materialIndexOffset << " bytes" << std::endl;

    std::cout << "GPUPrimitiveBuffer header size: " << sizeof(GPUPrimitiveBuffer) << " bytes" << std::endl;
    std::cout << "  primitiveCount offset: " << offsetof(GPUPrimitiveBuffer, primitiveCount) << " bytes" << std::endl;
    std::cout << "  padding offset:        " << offsetof(GPUPrimitiveBuffer, padding) << " bytes" << std::endl;

    std::cout << "=============================" << std::endl;
}
