#pragma once
#include "Texture.h" 
#include "ShaderProgram.h"
#include <unordered_map>
#include <glm/glm.hpp>

class Material
{
public:
    Material(ShaderProgram* shader);
    ~Material() = default;

    void SetShader(ShaderProgram* shader) { m_shader = shader; }
    void SetAlbedoMap(Texture* texture) { m_albedoMap = texture; }
    void SetNormalMap(Texture* texture) { m_normalMap = texture; }
    void SetRoughMap(Texture* texture) { m_roughMap = texture; }
    void SetMetalMap(Texture* texture) { m_metalMap = texture; }
    void SetEmissiveMap(Texture* texture) { m_emissiveMap = texture; }

    void LoadAlbedoMap(const char* fileName) { m_albedoMap->LoadFromFile(fileName); }
    void LoadNormalMap(const char* fileName) { m_normalMap->LoadFromFile(fileName); }
    void LoadRoughMap(const char* fileName) { m_roughMap->LoadFromFile(fileName); }
    void LoadMetalMap(const char* fileName) { m_metalMap->LoadFromFile(fileName); }
    void LoadEmissiveMap(const char* fileName) { m_emissiveMap->LoadFromFile(fileName); }

    ShaderProgram* GetShader() const { return m_shader; }
    const Texture* GetAlbedoMap() const { return m_albedoMap; }
    const Texture* GetNormalMap() const { return m_normalMap; }
    const Texture* GetRoughMap() const { return m_roughMap; }
    const Texture* GetMetalMap() const { return m_metalMap; }
    const Texture* GetEmissiveMap() const { return m_emissiveMap; }

    void SetFloat(const std::string& name, float value);
    void SetInt(const std::string& name, int value);
    void SetBool(const std::string& name, bool value);
    void SetVec2(const std::string& name, const glm::vec2& value);
    void SetVec3(const std::string& name, const glm::vec3& value);
    void SetVec4(const std::string& name, const glm::vec4& value);
    void SetTexture(const std::string& name, Texture* texture);

    float GetFloat(const std::string& name) const;
    int GetInt(const std::string& name) const;
    bool GetBool(const std::string& name) const;
    glm::vec2 GetVec2(const std::string& name) const;
    glm::vec3 GetVec3(const std::string& name) const;
    glm::vec4 GetVec4(const std::string& name) const;
    Texture* GetTexture(const std::string& name) const;

    const std::vector<UniformInfo>& GetExposedUniforms() const;
    void GetUniformsFromShader(); 
    void ApplyUniforms(); 

    //void LoadFromFile(const char* fileName);

    const std::unordered_map<std::string, float>& GetFloatUniforms() const { return m_floatUniforms; }
    const std::unordered_map<std::string, int>& GetIntUniforms() const { return m_intUniforms; }
    const std::unordered_map<std::string, bool>& GetBoolUniforms() const { return m_boolUniforms; }
    const std::unordered_map<std::string, glm::vec2>& GetVec2Uniforms() const { return m_vec2Uniforms; }
    const std::unordered_map<std::string, glm::vec3>& GetVec3Uniforms() const { return m_vec3Uniforms; }
    const std::unordered_map<std::string, glm::vec4>& GetVec4Uniforms() const { return m_vec4Uniforms; }
    const std::unordered_map<std::string, std::string>& GetTextureHandles() const { return m_textureHandles; }

    void SetFloatUniforms(const std::unordered_map<std::string, float>& uniforms) { m_floatUniforms = uniforms; }
    void SetIntUniforms(const std::unordered_map<std::string, int>& uniforms) { m_intUniforms = uniforms; }
    void SetBoolUniforms(const std::unordered_map<std::string, bool>& uniforms) { m_boolUniforms = uniforms; }
    void SetVec2Uniforms(const std::unordered_map<std::string, glm::vec2>& uniforms) { m_vec2Uniforms = uniforms; }
    void SetVec3Uniforms(const std::unordered_map<std::string, glm::vec3>& uniforms) { m_vec3Uniforms = uniforms; }
    void SetVec4Uniforms(const std::unordered_map<std::string, glm::vec4>& uniforms) { m_vec4Uniforms = uniforms; }
    void SetTextureHandles(const std::unordered_map<std::string, std::string>& handles) { m_textureHandles = handles; }

private:
    ShaderProgram* m_shader;
    Texture* m_albedoMap;   
    Texture* m_normalMap;
    Texture* m_roughMap;
    Texture* m_metalMap;
    Texture* m_emissiveMap;

    std::unordered_map<std::string, float> m_floatUniforms;
    std::unordered_map<std::string, int> m_intUniforms;
    std::unordered_map<std::string, bool> m_boolUniforms;
    std::unordered_map<std::string, glm::vec2> m_vec2Uniforms;
    std::unordered_map<std::string, glm::vec3> m_vec3Uniforms;
    std::unordered_map<std::string, glm::vec4> m_vec4Uniforms;
    std::unordered_map<std::string, std::string> m_textureHandles; 

    void SetDefaultUniformValue(const UniformInfo& uniformInfo);
};