#include "cepch.h"
#include "SkeletalMesh.h"
#include <ozz/base/io/stream.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/maths/vec_float.h>
#include <ozz/base/span.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <iostream>

SkeletalMesh::SkeletalMesh()
{
    m_boneOffsets.resize(MAX_BONES, glm::mat4(1.0f));
}

SkeletalMesh::~SkeletalMesh()
{
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_IBO) glDeleteBuffers(1, &m_IBO);
}

glm::mat4 AssimpToGlm(const aiMatrix4x4& ai_mat)
{
    return glm::transpose(glm::make_mat4(&ai_mat.a1));
}

bool SkeletalMesh::LoadSkeleton(const char* skeletonFile)
{
    std::string fullPath = g_ContentPath + skeletonFile;
    std::cout << "Loading skeleton from: " << fullPath << std::endl;

    ozz::io::File file(fullPath.c_str(), "rb");
    if (!file.opened())
    {
        std::cerr << "Failed to open skeleton file: " << fullPath << std::endl;
        return false;
    }

    ozz::io::IArchive archive(&file);
    m_skeleton = std::make_unique<ozz::animation::Skeleton>();
    archive >> *m_skeleton;

    if (!m_skeleton || m_skeleton->num_joints() == 0)
    {
        std::cerr << "Invalid skeleton loaded" << std::endl;
        return false;
    }

    return true;
}

bool SkeletalMesh::LoadMesh(const char* meshFile, float meshScale)
{
    if (!m_skeleton)
    {
        std::cerr << "ERROR: Must load skeleton before loading mesh!" << std::endl;
        return false;
    }

    std::string fullPath = g_ContentPath + meshFile;
    std::cout << "Loading mesh from: " << fullPath << std::endl;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(fullPath,
        aiProcess_Triangulate |
        aiProcess_CalcTangentSpace |
        aiProcess_GenSmoothNormals |
        aiProcess_LimitBoneWeights);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return false;
    }

    std::vector<SkeletalVertex> vertices;
    std::vector<unsigned int> indices;

    std::unordered_map<std::string, int> jointNameToIndex;
    auto joint_names = m_skeleton->joint_names();
    for (int i = 0; i < m_skeleton->num_joints(); ++i)
    {
        jointNameToIndex[joint_names[i]] = i;
    }

    for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx)
    {
        aiMesh* mesh = scene->mMeshes[meshIdx];
        size_t vertexOffset = vertices.size();

        vertices.reserve(vertices.size() + mesh->mNumVertices);

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            SkeletalVertex vertex;

            vertex.position = glm::vec4(
                mesh->mVertices[i].x * meshScale,
                mesh->mVertices[i].y * meshScale,
                mesh->mVertices[i].z * meshScale,
                1.0f);

            if (mesh->HasNormals())
            {
                vertex.normal = glm::vec4(
                    mesh->mNormals[i].x,
                    mesh->mNormals[i].y,
                    mesh->mNormals[i].z,
                    0.0f);
            }

            if (mesh->mTextureCoords[0])
            {
                vertex.texCoord = glm::vec2(
                    mesh->mTextureCoords[0][i].x,
                    1.0f - mesh->mTextureCoords[0][i].y);
            }

            if (mesh->HasTangentsAndBitangents())
            {
                vertex.tangent = glm::vec4(
                    mesh->mTangents[i].x,
                    mesh->mTangents[i].y,
                    mesh->mTangents[i].z,
                    0.0f);

                vertex.bitangent = glm::vec4(
                    mesh->mBitangents[i].x,
                    mesh->mBitangents[i].y,
                    mesh->mBitangents[i].z,
                    0.0f);
            }

            if (mesh->HasVertexColors(0))
            {
                vertex.colour = glm::vec4(
                    mesh->mColors[0][i].r,
                    mesh->mColors[0][i].g,
                    mesh->mColors[0][i].b,
                    mesh->mColors[0][i].a);
            }
            else
            {
                vertex.colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            }

            vertices.push_back(vertex);
        }

        std::cout << "Processing " << mesh->mNumBones << " bones for mesh " << meshIdx << std::endl;

        for (unsigned int i = 0; i < mesh->mNumBones; ++i)
        {
            aiBone* bone = mesh->mBones[i];
            std::string boneName(bone->mName.C_Str());

            auto it = jointNameToIndex.find(boneName);
            if (it == jointNameToIndex.end())
            {
                std::cerr << "WARNING: Bone '" << boneName << "' not found in skeleton!" << std::endl;
                continue;
            }

            int jointIndex = it->second;
            std::cout << "  Mapping bone '" << boneName << "' to joint " << jointIndex << std::endl;

            if (jointIndex < MAX_BONES)
            {
                glm::mat4 offsetMatrix = AssimpToGlm(bone->mOffsetMatrix);

                offsetMatrix[3][0] *= meshScale;
                offsetMatrix[3][1] *= meshScale;
                offsetMatrix[3][2] *= meshScale;

                m_boneOffsets[jointIndex] = offsetMatrix;
            }

            for (unsigned int j = 0; j < bone->mNumWeights; ++j)
            {
                unsigned int vertexId = vertexOffset + bone->mWeights[j].mVertexId;
                float weight = bone->mWeights[j].mWeight;

                if (vertexId >= vertices.size())
                {
                    std::cerr << "ERROR: Invalid vertex ID in bone weights" << std::endl;
                    continue;
                }

                bool added = false;
                for (int k = 0; k < 4; ++k)
                {
                    if (vertices[vertexId].boneWeights[k] == 0.0f)
                    {
                        vertices[vertexId].boneIds[k] = jointIndex;
                        vertices[vertexId].boneWeights[k] = weight;
                        added = true;
                        break;
                    }
                }

                if (!added)
                {
                    std::cerr << "WARNING: Vertex " << vertexId << " has more than 4 bone influences" << std::endl;
                }
            }
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
        {
            aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j)
            {
                indices.push_back(vertexOffset + face.mIndices[j]);
            }
        }
    }

    for (auto& vertex : vertices)
    {
        float totalWeight = vertex.boneWeights[0] + vertex.boneWeights[1] +
            vertex.boneWeights[2] + vertex.boneWeights[3];

        if (totalWeight > 0.0f)
        {
            vertex.boneWeights[0] /= totalWeight;
            vertex.boneWeights[1] /= totalWeight;
            vertex.boneWeights[2] /= totalWeight;
            vertex.boneWeights[3] /= totalWeight;
        }
    }

    std::cout << "Loaded mesh: " << vertices.size() << " vertices, "
        << indices.size() / 3 << " triangles" << std::endl;

    CreateGPUBuffers(vertices, indices);
    return true;
}

bool SkeletalMesh::LoadAnimation(const std::string& name, const char* animationFile)
{
    std::string fullPath = g_ContentPath + animationFile;
    std::cout << "Loading animation '" << name << "' from: " << fullPath << std::endl;

    ozz::io::File file(fullPath.c_str(), "rb");
    if (!file.opened())
    {
        std::cerr << "Failed to open animation file: " << fullPath << std::endl;
        return false;
    }

    ozz::io::IArchive archive(&file);
    auto animation = std::make_unique<ozz::animation::Animation>();
    archive >> *animation;

    if (!animation || animation->duration() <= 0.0f)
    {
        std::cerr << "Invalid animation loaded" << std::endl;
        return false;
    }

    if (animation->num_tracks() != m_skeleton->num_joints())
    {
        std::cerr << "ERROR: Animation has " << animation->num_tracks()
            << " tracks but skeleton has " << m_skeleton->num_joints()
            << " joints!" << std::endl;
        return false;
    }

    m_animations[name] = std::move(animation);

    std::cout << "Loaded animation '" << name << "' - duration: "
        << m_animations[name]->duration() << "s, "
        << m_animations[name]->num_tracks() << " tracks" << std::endl;

    return true;
}


void SkeletalMesh::Render()
{
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void SkeletalMesh::CreateGPUBuffers(const std::vector<SkeletalVertex>& vertices,
    const std::vector<unsigned int>& indices)
{
    m_indexCount = indices.size();
    m_triCount = m_indexCount / 3;

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_IBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SkeletalVertex),
        vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(SkeletalVertex),
        (void*)offsetof(SkeletalVertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(SkeletalVertex),
        (void*)offsetof(SkeletalVertex, normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SkeletalVertex),
        (void*)offsetof(SkeletalVertex, texCoord));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(SkeletalVertex),
        (void*)offsetof(SkeletalVertex, tangent));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(SkeletalVertex),
        (void*)offsetof(SkeletalVertex, bitangent));

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(SkeletalVertex),
        (void*)offsetof(SkeletalVertex, colour));

    glEnableVertexAttribArray(6);
    glVertexAttribIPointer(6, 4, GL_INT, sizeof(SkeletalVertex),
        (void*)offsetof(SkeletalVertex, boneIds));

    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(SkeletalVertex),
        (void*)offsetof(SkeletalVertex, boneWeights));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
        indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

const ozz::animation::Animation* SkeletalMesh::GetAnimation(const std::string& name) const
{
    auto it = m_animations.find(name);
    return (it != m_animations.end()) ? it->second.get() : nullptr;
}

bool SkeletalMesh::HasAnimation(const std::string& name) const
{
    return m_animations.find(name) != m_animations.end();
}

std::vector<std::string> SkeletalMesh::GetAnimationNames() const
{
    std::vector<std::string> names;
    names.reserve(m_animations.size());
    for (const auto& [name, anim] : m_animations)
    {
        names.push_back(name);
    }
    return names;
}

int SkeletalMesh::GetJointIndex(const std::string& jointName) const
{
    if (!m_skeleton) return -1;

    auto joint_names = m_skeleton->joint_names();
    for (int i = 0; i < m_skeleton->num_joints(); ++i)
    {
        if (std::string(joint_names[i]) == jointName)
            return i;
    }
    return -1;
}
