#include "cepch.h"
#include "Mesh.h"
#include "Graphics.h"
#include "Core/Utilities/Serialiser.h"
#include "Material.h"

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>

Mesh::~Mesh()
{
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteBuffers(1, &m_IBO);
}

void Mesh::Initialise(unsigned int vertexCount, const Vertex* vertices, unsigned int indexCount, unsigned int* indices)
{
    assert(m_VAO == 0);

    StorePhysicsData(vertexCount, vertices, indexCount, indices);
    CalculateBoundingBox(vertexCount, vertices);

    glGenBuffers(1, &m_VBO);
    glGenVertexArrays(1, &m_VAO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(Vertex), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bitangent));

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, colour));

    m_indexCount = indexCount;
    if (indexCount != 0)
    {
        glGenBuffers(1, &m_IBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned int), indices, GL_STATIC_DRAW);

        m_triCount = indexCount / 3;
    }
    else
    {
        m_triCount = vertexCount / 3;
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::LoadFromFile(const char* filename)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(g_ContentPath + filename, aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return;
    }

    aiMesh* mesh = scene->mMeshes[0];

    int numFaces = mesh->mNumFaces;
    std::vector<unsigned int> indices;
    for (int i = 0; i < numFaces; i++)
    {
        indices.push_back(mesh->mFaces[i].mIndices[0]);
        indices.push_back(mesh->mFaces[i].mIndices[1]);
        indices.push_back(mesh->mFaces[i].mIndices[2]);

        if (mesh->mFaces[i].mNumIndices == 4)
        {
            indices.push_back(mesh->mFaces[i].mIndices[0]);
            indices.push_back(mesh->mFaces[i].mIndices[2]);
            indices.push_back(mesh->mFaces[i].mIndices[3]);
        }
    }

    int numV = mesh->mNumVertices;
    Vertex* vertices = new Vertex[numV];
    for (int i = 0; i < numV; i++)
    {
        vertices[i].position = glm::vec4(
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z,
            1.0f);

        vertices[i].normal = glm::vec4(
            mesh->mNormals[i].x,
            mesh->mNormals[i].y,
            mesh->mNormals[i].z,
            0.0f);

        if (mesh->mTextureCoords[0])
            vertices[i].texCoord = glm::vec2(
                mesh->mTextureCoords[0][i].x,
                1.0f - mesh->mTextureCoords[0][i].y);
        else
            vertices[i].texCoord = glm::vec2(0);

        if (mesh->mTangents)
            vertices[i].tangent = glm::vec4(
                mesh->mTangents[i].x,
                mesh->mTangents[i].y,
                mesh->mTangents[i].z,
                0.0f);
        else
            vertices[i].tangent = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

        if (mesh->mBitangents)
            vertices[i].bitangent = glm::vec4(
                mesh->mBitangents[i].x,
                mesh->mBitangents[i].y,
                mesh->mBitangents[i].z,
                0.0f);
        else
            vertices[i].bitangent = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);

        if (mesh->HasVertexColors(0))
            vertices[i].colour = glm::vec4(
                mesh->mColors[0][i].r,
                mesh->mColors[0][i].g,
                mesh->mColors[0][i].b,
                mesh->mColors[0][i].a);
        else
            vertices[i].colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    Initialise(numV, vertices, (unsigned int)indices.size(), indices.data());
    delete[] vertices;
}

void Mesh::CreateQuad()
{
    glm::vec4 tangent(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 bitangent(0.0f, 1.0f, 0.0f, 0.0f);
    glm::vec4 normal(0.0f, 0.0f, 1.0f, 0.0f);

    glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    Vertex vertices[] = {
        {glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f), normal, glm::vec2(0.0f, 0.0f), tangent, bitangent, white},
        {glm::vec4(1.0f, -1.0f, 0.0f, 1.0f), normal, glm::vec2(1.0f, 0.0f), tangent, bitangent, white},
        {glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), normal, glm::vec2(1.0f, 1.0f), tangent, bitangent, white},
        {glm::vec4(-1.0f, 1.0f, 0.0f, 1.0f), normal, glm::vec2(0.0f, 1.0f), tangent, bitangent, white}
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    this->Initialise(4, vertices, 6, indices);
}

void Mesh::CreatePlane(float width, float depth, unsigned int segmentsW, unsigned int segmentsD)
{
    unsigned int vertexCount = (segmentsW + 1) * (segmentsD + 1);
    std::vector<Vertex> vertices(vertexCount);

    float halfW = width / 2.0f;
    float halfD = depth / 2.0f;

    glm::vec4 normal(0.0f, 1.0f, 0.0f, 0.0f);
    glm::vec4 tangent(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 bitangent(0.0f, 0.0f, 1.0f, 0.0f);

    for (unsigned int d = 0; d <= segmentsD; ++d)
    {
        for (unsigned int w = 0; w <= segmentsW; ++w)
        {
            unsigned int index = d * (segmentsW + 1) + w;

            float x = (float)w / segmentsW * width - halfW;
            float z = (float)d / segmentsD * depth - halfD;

            vertices[index].position = glm::vec4(x, 0.0f, z, 1.0f);
            vertices[index].normal = normal;
            vertices[index].texCoord = glm::vec2((float)w / segmentsW, (float)d / segmentsD);
            vertices[index].tangent = tangent;
            vertices[index].bitangent = bitangent;
            vertices[index].colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    unsigned int indexCount = segmentsW * segmentsD * 6;
    std::vector<unsigned int> indices(indexCount);
    unsigned int i = 0;
    for (unsigned int d = 0; d < segmentsD; ++d)
    {
        for (unsigned int w = 0; w < segmentsW; ++w)
        {
            unsigned int a = d * (segmentsW + 1) + w;
            unsigned int b = a + 1;
            unsigned int c = (d + 1) * (segmentsW + 1) + w;
            unsigned int e = c + 1;

            indices[i++] = a; indices[i++] = c; indices[i++] = b;
            indices[i++] = b; indices[i++] = c; indices[i++] = e;
        }
    }

    Initialise(vertexCount, vertices.data(), indexCount, indices.data());
}

void Mesh::CreateBox(float width, float height, float depth)
{
    float w = width / 2.f;
    float h = height / 2.f;
    float d = depth / 2.f;

    glm::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 defaultTangent(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 defaultBitangent(0.0f, 1.0f, 0.0f, 0.0f);
    Vertex vertices[] = {
        {glm::vec4(-w, -h,  d, 1), glm::vec4(0, 0, 1, 0), glm::vec2(0, 0), defaultTangent, defaultBitangent, white},
        {glm::vec4(w, -h,  d, 1), glm::vec4(0, 0, 1, 0), glm::vec2(1, 0), defaultTangent, defaultBitangent, white},
        {glm::vec4(w,  h,  d, 1), glm::vec4(0, 0, 1, 0), glm::vec2(1, 1), defaultTangent, defaultBitangent, white},
        {glm::vec4(-w,  h,  d, 1), glm::vec4(0, 0, 1, 0), glm::vec2(0, 1), defaultTangent, defaultBitangent, white},
        {glm::vec4(w, -h, -d, 1), glm::vec4(0, 0, -1, 0), glm::vec2(0, 0), defaultTangent, defaultBitangent, white},
        {glm::vec4(-w, -h, -d, 1), glm::vec4(0, 0, -1, 0), glm::vec2(1, 0), defaultTangent, defaultBitangent, white},
        {glm::vec4(-w,  h, -d, 1), glm::vec4(0, 0, -1, 0), glm::vec2(1, 1), defaultTangent, defaultBitangent, white},
        {glm::vec4(w,  h, -d, 1), glm::vec4(0, 0, -1, 0), glm::vec2(0, 1), defaultTangent, defaultBitangent, white},
        {glm::vec4(-w, -h, -d, 1), glm::vec4(-1, 0, 0, 0), glm::vec2(0, 0), defaultTangent, defaultBitangent, white},
        {glm::vec4(-w, -h,  d, 1), glm::vec4(-1, 0, 0, 0), glm::vec2(1, 0), defaultTangent, defaultBitangent, white},
        {glm::vec4(-w,  h,  d, 1), glm::vec4(-1, 0, 0, 0), glm::vec2(1, 1), defaultTangent, defaultBitangent, white},
        {glm::vec4(-w,  h, -d, 1), glm::vec4(-1, 0, 0, 0), glm::vec2(0, 1), defaultTangent, defaultBitangent, white},
        {glm::vec4(w, -h,  d, 1), glm::vec4(1, 0, 0, 0), glm::vec2(0, 0), defaultTangent, defaultBitangent, white},
        {glm::vec4(w, -h, -d, 1), glm::vec4(1, 0, 0, 0), glm::vec2(1, 0), defaultTangent, defaultBitangent, white},
        {glm::vec4(w,  h, -d, 1), glm::vec4(1, 0, 0, 0), glm::vec2(1, 1), defaultTangent, defaultBitangent, white},
        {glm::vec4(w,  h,  d, 1), glm::vec4(1, 0, 0, 0), glm::vec2(0, 1), defaultTangent, defaultBitangent, white},
        {glm::vec4(-w,  h,  d, 1), glm::vec4(0, 1, 0, 0), glm::vec2(0, 0), defaultTangent, defaultBitangent, white},
        {glm::vec4(w,  h,  d, 1), glm::vec4(0, 1, 0, 0), glm::vec2(1, 0), defaultTangent, defaultBitangent, white},
        {glm::vec4(w,  h, -d, 1), glm::vec4(0, 1, 0, 0), glm::vec2(1, 1), defaultTangent, defaultBitangent, white},
        {glm::vec4(-w,  h, -d, 1), glm::vec4(0, 1, 0, 0), glm::vec2(0, 1), defaultTangent, defaultBitangent, white},
        {glm::vec4(-w, -h, -d, 1), glm::vec4(0, -1, 0, 0), glm::vec2(0, 0), defaultTangent, defaultBitangent, white},
        {glm::vec4(w, -h, -d, 1), glm::vec4(0, -1, 0, 0), glm::vec2(1, 0), defaultTangent, defaultBitangent, white},
        {glm::vec4(w, -h,  d, 1), glm::vec4(0, -1, 0, 0), glm::vec2(1, 1), defaultTangent, defaultBitangent, white},
        {glm::vec4(-w, -h,  d, 1), glm::vec4(0, -1, 0, 0), glm::vec2(0, 1), defaultTangent, defaultBitangent, white},
    };

    unsigned int indices[] = {
         0,  1,  2,  2,  3,  0,
         4,  5,  6,  6,  7,  4,
         8,  9, 10, 10, 11,  8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    for (int i = 0; i < 24; ++i)
    {
        glm::vec3 n = vertices[i].normal;
        glm::vec3 t = glm::cross(n, glm::vec3(0, 1, 0));
        if (glm::length(t) < 0.01f)
            t = glm::cross(n, glm::vec3(1, 0, 0));
        vertices[i].tangent = glm::vec4(glm::normalize(t), 0);
        vertices[i].bitangent = glm::vec4(glm::normalize(glm::cross(n, t)), 0);
    }

    Initialise(24, vertices, 36, indices);
}

void Mesh::CreateSphere(float radius, unsigned int rings, unsigned int sectors)
{
    unsigned int vertexCount = (rings + 1) * (sectors + 1);
    std::vector<Vertex> vertices(vertexCount);

    const float PI = 3.1415927f;
    float ringStep = PI / rings;
    float sectorStep = 2.0f * PI / sectors;

    for (unsigned int r = 0; r <= rings; ++r)
    {
        for (unsigned int s = 0; s <= sectors; ++s)
        {
            unsigned int index = r * (sectors + 1) + s;

            float phi = r * ringStep;
            float theta = s * sectorStep;

            float y = radius * cos(phi);
            float x = radius * sin(phi) * cos(theta);
            float z = radius * sin(phi) * sin(theta);

            vertices[index].position = glm::vec4(x, y, z, 1.0f);
            vertices[index].normal = glm::vec4(glm::normalize(glm::vec3(x, y, z)), 0.0f);
            vertices[index].texCoord = glm::vec2((float)s / sectors, (float)r / rings);

            float tangentX = -radius * sin(phi) * sin(theta);
            float tangentZ = radius * sin(phi) * cos(theta);
            vertices[index].tangent = glm::vec4(glm::normalize(glm::vec3(tangentX, 0, tangentZ)), 0.0f);

            glm::vec3 bitan = glm::cross(glm::vec3(vertices[index].normal), glm::vec3(vertices[index].tangent));
            vertices[index].bitangent = glm::vec4(glm::normalize(bitan), 0.0f);
            vertices[index].colour = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        }
    }

    unsigned int indexCount = rings * sectors * 6;
    std::vector<unsigned int> indices(indexCount);
    unsigned int i = 0;
    for (unsigned int r = 0; r < rings; ++r)
    {
        for (unsigned int s = 0; s < sectors; ++s)
        {
            unsigned int a = r * (sectors + 1) + s;
            unsigned int b = a + 1;
            unsigned int c = (r + 1) * (sectors + 1) + s;
            unsigned int e = c + 1;

            indices[i++] = a; indices[i++] = b; indices[i++] = c;
            indices[i++] = b; indices[i++] = e; indices[i++] = c;
        }
    }

    Initialise(vertexCount, vertices.data(), indexCount, indices.data());
}

void Mesh::CreateCapsule(float radius, float height, unsigned int segments)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    const float PI = 3.1415927f;

    float halfHeight = height / 2.0f;
    for (unsigned int i = 0; i <= segments; ++i)
    {
        float angle = (float)i / segments * 2.0f * PI;
        float x = cos(angle) * radius;
        float z = sin(angle) * radius;

        vertices.push_back({ glm::vec4(x, halfHeight, z, 1), glm::vec4(cos(angle), 0, sin(angle), 0), glm::vec2((float)i / segments, 1.0f), glm::vec4(0), glm::vec4(0), glm::vec4(1) });
        vertices.push_back({ glm::vec4(x, -halfHeight, z, 1), glm::vec4(cos(angle), 0, sin(angle), 0), glm::vec2((float)i / segments, 0.0f), glm::vec4(0), glm::vec4(0), glm::vec4(1) });
    }

    for (unsigned int i = 0; i < segments; ++i)
    {
        unsigned int topCurrent = i * 2;
        unsigned int bottomCurrent = i * 2 + 1;
        unsigned int topNext = (i + 1) * 2;
        unsigned int bottomNext = (i + 1) * 2 + 1;

        indices.push_back(topCurrent); indices.push_back(topNext); indices.push_back(bottomCurrent);
        indices.push_back(bottomCurrent); indices.push_back(topNext); indices.push_back(bottomNext);
    }

    unsigned int rings = segments / 2;

    for (int j = 0; j < 2; j++)
    {
        float ySign = (j == 0) ? 1.0f : -1.0f;
        unsigned int baseIndex = vertices.size();

        vertices.push_back({ glm::vec4(0, halfHeight * ySign + radius * ySign, 0, 1), glm::vec4(0, ySign, 0, 0), glm::vec2(0.5f, ySign > 0 ? 1.0f : 0.0f), glm::vec4(0), glm::vec4(0), glm::vec4(1) });

        for (unsigned int r = 1; r <= rings; ++r)
        {
            float phi = (float)r / rings * PI / 2.0f;
            float y = cos(phi) * radius * ySign;
            float ringRadius = sin(phi) * radius;

            for (unsigned int s = 0; s <= segments; ++s)
            {
                float theta = (float)s / segments * 2.0f * PI;
                float x = cos(theta) * ringRadius;
                float z = sin(theta) * ringRadius;

                glm::vec3 pos(x, y + halfHeight * ySign, z);
                glm::vec3 normal = glm::normalize(pos - glm::vec3(0, halfHeight * ySign, 0));
                vertices.push_back({ glm::vec4(pos, 1), glm::vec4(normal, 0), glm::vec2((float)s / segments, ySign > 0 ? 1.0f - (float)r / rings : (float)r / rings), glm::vec4(0), glm::vec4(0), glm::vec4(1) });
            }
        }

        unsigned int poleIndex = baseIndex;
        for (unsigned int s = 0; s < segments; ++s)
        {
            unsigned int current = baseIndex + 1 + s;
            unsigned int next = baseIndex + 1 + s + 1;

            if (ySign > 0)
            {
                indices.push_back(poleIndex); indices.push_back(next); indices.push_back(current);
            }
            else
            {
                indices.push_back(poleIndex); indices.push_back(current); indices.push_back(next);
            }
        }

        for (unsigned int r = 1; r < rings; ++r)
        {
            unsigned int ringStartIndex = baseIndex + 1 + (r - 1) * (segments + 1);
            for (unsigned int s = 0; s < segments; ++s)
            {
                unsigned int a = ringStartIndex + s;
                unsigned int b = ringStartIndex + s + 1;
                unsigned int c = ringStartIndex + s + segments + 1;
                unsigned int d = ringStartIndex + s + segments + 2;

                if (ySign > 0)
                {
                    indices.push_back(a); indices.push_back(b); indices.push_back(c);
                    indices.push_back(b); indices.push_back(d); indices.push_back(c);
                }
                else
                {
                    indices.push_back(a); indices.push_back(c); indices.push_back(b);
                    indices.push_back(b); indices.push_back(c); indices.push_back(d);
                }
            }
        }
    }

    for (auto& v : vertices)
    {
        glm::vec3 n(v.normal);
        glm::vec3 t = glm::cross(n, glm::vec3(0, 1, 0));
        if (glm::dot(t, t) < 0.01f) t = glm::cross(n, glm::vec3(1, 0, 0));
        v.tangent = glm::vec4(glm::normalize(t), 0);
        v.bitangent = glm::vec4(glm::normalize(glm::cross(n, t)), 0);
    }

    Initialise(vertices.size(), vertices.data(), indices.size(), indices.data());
}

void Mesh::CreateCylinder(float radius, float height, unsigned int segments)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    const float PI = 3.1415927f;

    float halfHeight = height / 2.0f;

    for (unsigned int i = 0; i <= segments; ++i)
    {
        float angle = (float)i / segments * 2.0f * PI;
        float x = cos(angle) * radius;
        float z = sin(angle) * radius;

        vertices.push_back({
            glm::vec4(x, halfHeight, z, 1.0f),
            glm::vec4(cos(angle), 0.0f, sin(angle), 0.0f),
            glm::vec2((float)i / segments, 1.0f),
            glm::vec4(-sin(angle), 0.0f, cos(angle), 0.0f),
            glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
            glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
            });

        vertices.push_back({
            glm::vec4(x, -halfHeight, z, 1.0f),
            glm::vec4(cos(angle), 0.0f, sin(angle), 0.0f),
            glm::vec2((float)i / segments, 0.0f),
            glm::vec4(-sin(angle), 0.0f, cos(angle), 0.0f),
            glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
            glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
            });
    }

    for (unsigned int i = 0; i < segments; ++i)
    {
        unsigned int topCurrent = i * 2;
        unsigned int bottomCurrent = i * 2 + 1;
        unsigned int topNext = (i + 1) * 2;
        unsigned int bottomNext = (i + 1) * 2 + 1;

        indices.push_back(topCurrent);
        indices.push_back(topNext);
        indices.push_back(bottomCurrent);

        indices.push_back(bottomCurrent);
        indices.push_back(topNext);
        indices.push_back(bottomNext);
    }

    unsigned int topCenterIndex = vertices.size();
    vertices.push_back({
        glm::vec4(0.0f, halfHeight, 0.0f, 1.0f),
        glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
        glm::vec2(0.5f, 0.5f),
        glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
        });

    for (unsigned int i = 0; i <= segments; ++i)
    {
        float angle = (float)i / segments * 2.0f * PI;
        float x = cos(angle) * radius;
        float z = sin(angle) * radius;

        vertices.push_back({
            glm::vec4(x, halfHeight, z, 1.0f),
            glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
            glm::vec2((cos(angle) + 1.0f) * 0.5f, (sin(angle) + 1.0f) * 0.5f),
            glm::vec4(-sin(angle), 0.0f, cos(angle), 0.0f),
            glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
            glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
            });
    }

    unsigned int bottomCenterIndex = vertices.size();
    vertices.push_back({
        glm::vec4(0.0f, -halfHeight, 0.0f, 1.0f),
        glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),
        glm::vec2(0.5f, 0.5f),
        glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec4(0.0f, 0.0f, -1.0f, 0.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
        });

    for (unsigned int i = 0; i <= segments; ++i)
    {
        float angle = (float)i / segments * 2.0f * PI;
        float x = cos(angle) * radius;
        float z = sin(angle) * radius;

        vertices.push_back({
            glm::vec4(x, -halfHeight, z, 1.0f),
            glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),
            glm::vec2((cos(angle) + 1.0f) * 0.5f, (sin(angle) + 1.0f) * 0.5f),
            glm::vec4(-sin(angle), 0.0f, cos(angle), 0.0f),
            glm::vec4(0.0f, 0.0f, -1.0f, 0.0f),
            glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
            });
    }

    for (unsigned int i = 0; i < segments; ++i)
    {
        indices.push_back(topCenterIndex);
        indices.push_back(topCenterIndex + 1 + ((i + 1) % (segments + 1)));
        indices.push_back(topCenterIndex + 1 + i);
    }

    for (unsigned int i = 0; i < segments; ++i)
    {
        indices.push_back(bottomCenterIndex);
        indices.push_back(bottomCenterIndex + 1 + i);
        indices.push_back(bottomCenterIndex + 1 + ((i + 1) % (segments + 1)));
    }

    Initialise(vertices.size(), vertices.data(), indices.size(), indices.data());
}

void Mesh::CreateCone(float radius, float height, int segments)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    const float PI = 3.1415927f;

    vertices.push_back({
        glm::vec4(0.0f, height, 0.0f, 1.0f),
        glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
        glm::vec2(0.5f, 1.0f),
        glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
        });

    vertices.push_back({
        glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
        glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),
        glm::vec2(0.5f, 0.5f),
        glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
        glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
        });

    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0f * PI * i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);

        vertices.push_back({
            glm::vec4(x, 0.0f, z, 1.0f),
            glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),
            glm::vec2((cos(angle) + 1.0f) * 0.5f, (sin(angle) + 1.0f) * 0.5f),
            glm::vec4(-sin(angle), 0.0f, cos(angle), 0.0f),
            glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
            glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
            });
    }

    for (int i = 0; i < segments; ++i)
    {
        indices.insert(indices.end(),
            {
                0,
                (unsigned int)(2 + ((i + 1) % segments)),
                (unsigned int)(2 + i)
            });
    }

    for (int i = 0; i < segments; ++i)
    {
        indices.insert(indices.end(),
            {
                1,
                (unsigned int)(2 + i),
                (unsigned int)(2 + ((i + 1) % segments))
            });
    }

    vertices[0].normal = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    vertices[1].normal = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);

    for (int i = 0; i < segments; ++i)
    {
        int baseIdx = 2 + i;
        glm::vec3 basePos = glm::vec3(vertices[baseIdx].position);
        glm::vec3 tipPos = glm::vec3(vertices[0].position);

        glm::vec3 toTip = glm::normalize(tipPos - basePos);
        glm::vec3 radialDir = glm::normalize(basePos);
        glm::vec3 sideNormal = glm::normalize(glm::cross(radialDir, toTip));

        vertices[baseIdx].normal = glm::vec4(sideNormal, 0.0f);
    }

    Initialise(vertices.size(), vertices.data(), indices.size(), indices.data());
}

void Mesh::Render()
{
    glBindVertexArray(m_VAO);
    if (m_IBO != 0)
        glDrawElements(GL_TRIANGLES, 3 * m_triCount, GL_UNSIGNED_INT, 0);
    else
        glDrawArrays(GL_TRIANGLES, 0, 3 * m_triCount);

    glBindVertexArray(0);
}

void Mesh::SetupInstancedAttributes(GLuint instanceVBO)
{
    if (m_hasInstancedAttributes) return;

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(6 + i);
        glVertexAttribPointer(6 + i, 4, GL_FLOAT, GL_FALSE,
            sizeof(glm::mat4),
            (void*)(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(6 + i, 1);
    }

    m_hasInstancedAttributes = true;
}

void Mesh::RenderInstanced(int instanceCount)
{
    if (!m_hasInstancedAttributes) return;
    glBindVertexArray(m_VAO);
    if (m_IBO != 0)
        glDrawElementsInstanced(GL_TRIANGLES, 3 * m_triCount, GL_UNSIGNED_INT, 0, instanceCount);
    else
        glDrawArraysInstanced(GL_TRIANGLES, 0, 3 * m_triCount, instanceCount);

    glBindVertexArray(0);
}

void Mesh::StorePhysicsData(unsigned int vertexCount, const Vertex* vertices, unsigned int indexCount, const unsigned int* indices)
{
    m_vertices.clear();
    m_vertices.reserve(vertexCount);

    for (unsigned int i = 0; i < vertexCount; ++i)
    {
        m_vertices.emplace_back(vertices[i].position.x, vertices[i].position.y, vertices[i].position.z);
    }

    m_indices.clear();
    if (indexCount > 0 && indices)
    {
        m_indices.reserve(indexCount);
        for (unsigned int i = 0; i < indexCount; ++i)
        {
            m_indices.push_back(static_cast<uint32_t>(indices[i]));
        }
    }
    else
    {
        m_indices.reserve(vertexCount);
        for (unsigned int i = 0; i < vertexCount; ++i)
        {
            m_indices.push_back(i);
        }
    }
}

void Mesh::CalculateBoundingBox(unsigned int vertexCount, const Vertex* vertices)
{
    m_boundingBox = AABB();

    for (unsigned int i = 0; i < vertexCount; ++i)
    {
        glm::vec3 pos(vertices[i].position.x, vertices[i].position.y, vertices[i].position.z);
        m_boundingBox.Expand(pos);
    }
}
