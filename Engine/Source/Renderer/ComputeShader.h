#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_set>


class ComputeShader {
private:
    std::string m_filename;
    GLuint m_computeShader;
    GLuint m_shaderProgram;
    bool m_loadedSuccessfully;

public:
    ComputeShader(std::string computeFilename);
    ~ComputeShader();

    ComputeShader(const ComputeShader&) = delete;
    ComputeShader& operator=(const ComputeShader&) = delete;

    ComputeShader(ComputeShader&& other) noexcept;
    ComputeShader& operator=(ComputeShader&& other) noexcept;

    bool CompileAndLinkShader();
    void Use() const;
    bool Reload();

    void Dispatch(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ) const;
    void DispatchWithDimensions(GLuint width, GLuint height, GLuint depth,
        GLuint localSizeX, GLuint localSizeY, GLuint localSizeZ) const;
    void Synchronize(GLbitfield barriers = GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT) const;

    void SetBoolUniform(const std::string& varName, bool value) const;
    void SetIntUniform(const std::string& varName, int value) const;
    void SetUintUniform(const std::string& varName, unsigned int value) const;
    void SetFloatUniform(const std::string& varName, float value) const;
    void SetVec2Uniform(const std::string& varName, const glm::vec2& value) const;
    void SetVec3Uniform(const std::string& varName, const glm::vec3& value) const;
    void SetVec4Uniform(const std::string& varName, const glm::vec4& value) const;
    void SetIVec3Uniform(const std::string& varName, const glm::ivec3& value) const;
    void SetMat4Uniform(const std::string& varName, const glm::mat4& value) const;

    void BindImage(GLuint textureId, GLuint unit, GLint level, GLboolean layered,
        GLint layer, GLenum access, GLenum format) const;
    void BindImage3D(GLuint textureId, GLuint unit, GLint level, GLenum access, GLenum format) const;

    GLuint GetProgramId() const;
    bool IsLoaded() const;

private:
    static std::string ProcessIncludes(const std::string& source, const std::string& baseDir,
        std::unordered_set<std::string>& includedFiles, int depth = 0);
};