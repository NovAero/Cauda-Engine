#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_set>
#include <unordered_map>


enum class UniformType
{
	Float,
	Int,
	Bool,
	Vec2,
	Vec3, 
	Vec4, 
	Sampler2D
};

struct UniformInfo
{
	std::string name;
	UniformType type;
	GLint location;
};

class ShaderProgram
{
public:
	ShaderProgram(std::string vertexFilename, std::string fragmentFilename);

	~ShaderProgram();

	ShaderProgram(const ShaderProgram& other) = delete;
	ShaderProgram& operator=(const ShaderProgram& other) = delete;

	ShaderProgram(ShaderProgram&& other) noexcept;
	ShaderProgram& operator= (ShaderProgram&& other) noexcept;

	void Use() const;
	bool Reload();

	void IntrospectUniforms();
	const std::vector<UniformInfo>& GetExposedUniforms() const { return m_exposedUniforms; }

	void SetBoolUniform(const std::string& varName, bool value) const;
	void SetIntUniform(const std::string& varName, int value) const;
	void SetUintUniform(const std::string& varName, unsigned int value) const;
	void SetFloatUniform(const std::string varName, float value) const;
	void SetVec2Uniform(const std::string& varName, const glm::vec2& value) const;
	void SetVec3Uniform(const std::string& varName, const glm::vec3& value) const;
	void SetVec4Uniform(const std::string& varName, const glm::vec4& value) const;
	void SetIVec3Uniform(const std::string& varName, const glm::ivec3& value) const;
	void SetMat4Uniform(const std::string varName, glm::mat4 value) const;

	GLuint GetHandle() const { return m_shaderProgram; }

	std::string m_vertexFilename;
	std::string m_fragmentFilename;
private:
	bool CompileAndLinkShaders();

	static std::string ProcessIncludes(const std::string& source, const std::string& baseDir,
		std::unordered_set<std::string>& includedFiles, int depth = 0);

	std::vector<UniformInfo> m_exposedUniforms;

	UniformType GLTypeToUniformType(GLenum glType) const;
	bool IsPrivateUniform(const std::string& name) const;

	mutable std::unordered_map<std::string, GLint> m_uniformLocationCache;

	GLint GetUniformLocation(const std::string& name) const;

	GLuint m_shaderProgram = 0;
	GLuint m_fragmentShader = 0;
	GLuint m_vertexShader = 0;

	bool m_loadedSuccessfully = false;
};