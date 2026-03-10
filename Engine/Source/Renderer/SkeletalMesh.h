#pragma once
#include "Graphics.h"
#include <glm/glm.hpp>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct SkeletalVertex
{
    glm::vec4 position;
    glm::vec4 normal;
    glm::vec2 texCoord;
    glm::vec4 tangent;
    glm::vec4 bitangent;
    glm::vec4 colour;
    glm::ivec4 boneIds = glm::ivec4(-1);
    glm::vec4 boneWeights = glm::vec4(0.0f);
};

class SkeletalMesh
{
public:
    SkeletalMesh();
    ~SkeletalMesh();

    bool LoadMesh(const char* meshFile, float meshScale = 1.0f);
    bool LoadSkeleton(const char* skeletonFile);
    bool LoadAnimation(const std::string& name, const char* animationFile);

    void Render();
    void RenderInstanced(int instanceCount);
    void SetupInstancedAttributes(GLuint instanceVBO);

    void CreateGPUBuffers(const std::vector<SkeletalVertex>& vertices,
        const std::vector<unsigned int>& indices);

    unsigned int GetVAO() const { return m_VAO; }
    unsigned int GetTriCount() const { return m_triCount; }

    const ozz::animation::Skeleton* GetSkeleton() const { return m_skeleton.get(); }
    const ozz::animation::Animation* GetAnimation(const std::string& name) const;
    bool HasAnimation(const std::string& name) const;
    std::vector<std::string> GetAnimationNames() const;

    int GetJointIndex(const std::string& jointName) const;
    const std::vector<glm::mat4>& GetBoneOffsets() const { return m_boneOffsets; }

    static constexpr int MAX_BONES = 128;

private:
    unsigned int m_triCount = 0;
    unsigned int m_VAO = 0, m_VBO = 0, m_IBO = 0;
    unsigned int m_indexCount = 0;
    bool m_hasInstancedAttributes = false;

    std::unique_ptr<ozz::animation::Skeleton> m_skeleton;
    std::unordered_map<std::string, std::unique_ptr<ozz::animation::Animation>> m_animations;

    std::vector<glm::mat4> m_boneOffsets;
    std::unordered_map<std::string, int> m_boneNameToIndex;
};