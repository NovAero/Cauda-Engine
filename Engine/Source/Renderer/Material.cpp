#include "cepch.h"
#include "Material.h"
#include "ShaderProgram.h"
#include "Core/Editor/ResourceLibrary.h"
#include <iostream>
#include <fstream>
#include <sstream>

Material::Material(ShaderProgram* shader) :
    m_shader(shader)
   /* m_albedoMap(ResourceLibrary::GetTexture("white_texture")),  
    m_normalMap(ResourceLibrary::GetTexture("normal_texture")),
    m_roughMap(ResourceLibrary::GetTexture("white_texture")),
    m_metalMap(ResourceLibrary::GetTexture("black_texture")),
    m_emissiveMap(ResourceLibrary::GetTexture("black_texture"))*/
{
    this->SetTexture("albedoMap", ResourceLibrary::GetTexture("white_texture"));
    this->SetTexture("normalMap", ResourceLibrary::GetTexture("normal_texture"));
    this->SetTexture("roughMap", ResourceLibrary::GetTexture("white_texture"));
    this->SetTexture("metalMap", ResourceLibrary::GetTexture("black_texture"));
    this->SetTexture("emissiveMap", ResourceLibrary::GetTexture("black_texture"));
}

//void Material::LoadFromFile(const char* fileName)
//{
//    std::ifstream file(g_ContentPath + fileName);
//    if (!file.is_open())
//    {
//        std::cout << "Error: Failed to read file \"" << fileName << "\"\n";
//        return;
//    }
//
//    std::string line;
//    while (std::getline(file, line))
//    {
//        std::istringstream ss(line);
//        std::string identifier;
//        ss >> identifier;
//
//        std::string directory(fileName);
//        int index = directory.rfind('/');
//        if (index != -1)
//            directory = directory.substr(0, index + 1);
//
//        if (identifier == "map_Kd")
//        {
//            std::string mapFileName;
//            ss >> mapFileName;
//            m_albedoMap->LoadFromFile((directory + mapFileName));
//        }
//        else if (identifier == "map_Bump")
//        {
//            std::string mapFileName;
//            ss >> mapFileName;
//            m_normalMap->LoadFromFile((directory + mapFileName));
//        }
//        else if (identifier == "map_Ns")
//        {
//            std::string mapFileName;
//            ss >> mapFileName;
//            m_roughMap->LoadFromFile((directory + mapFileName));
//        }
//        else if (identifier == "map_refl")
//        {
//            std::string mapFileName;
//            ss >> mapFileName;
//            m_metalMap->LoadFromFile((directory + mapFileName));
//        }
//    }
//}

const std::vector<UniformInfo>& Material::GetExposedUniforms() const
{
    if (m_shader) {
        return m_shader->GetExposedUniforms();
    }
    static const std::vector<UniformInfo> empty;
    return empty;
}

void Material::GetUniformsFromShader()
{
    if (!m_shader) return;

    m_shader->IntrospectUniforms();
    const auto& uniforms = m_shader->GetExposedUniforms();

    for (const auto& uniformInfo : uniforms) {
        SetDefaultUniformValue(uniformInfo);
    }
}

void Material::SetDefaultUniformValue(const UniformInfo& uniformInfo)
{
    switch (uniformInfo.type) {
    case UniformType::Float:
        m_floatUniforms.try_emplace(uniformInfo.name, 1.0f);
        break;
    case UniformType::Int:
        m_intUniforms.try_emplace(uniformInfo.name, 0);
        break;
    case UniformType::Bool:
        m_boolUniforms.try_emplace(uniformInfo.name, false);
        break;
    case UniformType::Vec2:
        m_vec2Uniforms.try_emplace(uniformInfo.name, glm::vec2(1.0f));
        break;
    case UniformType::Vec3:
        m_vec3Uniforms.try_emplace(uniformInfo.name, glm::vec3(1.0f));
        break;
    case UniformType::Vec4:
        m_vec4Uniforms.try_emplace(uniformInfo.name, glm::vec4(1.0f));
        break;
    case UniformType::Sampler2D:
        m_textureHandles.try_emplace(uniformInfo.name, "");
        break;
    }
}

void Material::ApplyUniforms()
{
    if (!m_shader) return;

    int textureSlot = 0;

    if (m_albedoMap) {
        m_albedoMap->Bind(textureSlot);
        m_shader->SetIntUniform("albedoMap", textureSlot);
        textureSlot++;
    }

    if (m_normalMap) {
        m_normalMap->Bind(textureSlot);
        m_shader->SetIntUniform("normalMap", textureSlot);
        textureSlot++;
    }

    if (m_roughMap) {
        m_roughMap->Bind(textureSlot);
        m_shader->SetIntUniform("roughMap", textureSlot);
        textureSlot++;
    }

    if (m_metalMap) {
        m_metalMap->Bind(textureSlot);
        m_shader->SetIntUniform("metalMap", textureSlot);
        textureSlot++;
    }

    if (m_emissiveMap) {
        m_emissiveMap->Bind(textureSlot);
        m_shader->SetIntUniform("emissiveMap", textureSlot);
        textureSlot++;
    }

    for (const auto& [name, textureHandle] : m_textureHandles) 
    {
        if (name == "albedoMap" || name == "normalMap" ||
            name == "roughMap" || name == "metalMap" || name == "emissiveMap") {
            continue;
        }

        if (!textureHandle.empty()) {
            Texture* texture = ResourceLibrary::GetTexture(textureHandle.c_str());
            if (texture) {
                texture->Bind(textureSlot);
                m_shader->SetIntUniform(name, textureSlot);
                textureSlot++;
            }
        }
    }


    for (const auto& [name, value] : m_floatUniforms) {
        m_shader->SetFloatUniform(name, value);
    }

    for (const auto& [name, value] : m_intUniforms) {
        m_shader->SetIntUniform(name, value);
    }

    for (const auto& [name, value] : m_boolUniforms) {
        m_shader->SetBoolUniform(name, value);
    }

    for (const auto& [name, value] : m_vec2Uniforms) {
        m_shader->SetVec2Uniform(name, value);
    }

    for (const auto& [name, value] : m_vec3Uniforms) {
        m_shader->SetVec3Uniform(name, value);
    }

    for (const auto& [name, value] : m_vec4Uniforms) {
        m_shader->SetVec4Uniform(name, value);
    }
}

// Uniform getters and setters
void Material::SetFloat(const std::string& name, float value)
{
    m_floatUniforms[name] = value;
}

void Material::SetInt(const std::string& name, int value)
{
    m_intUniforms[name] = value;
}

void Material::SetBool(const std::string& name, bool value)
{
    m_boolUniforms[name] = value;
}

void Material::SetVec2(const std::string& name, const glm::vec2& value)
{
    m_vec2Uniforms[name] = value;
}

void Material::SetVec3(const std::string& name, const glm::vec3& value)
{
    m_vec3Uniforms[name] = value;
}

void Material::SetVec4(const std::string& name, const glm::vec4& value)
{
    m_vec4Uniforms[name] = value;
}

void Material::SetTexture(const std::string& name, Texture* texture)
{
    if (name == "albedoMap") 
    {
        m_albedoMap = texture;
    }
    else if (name == "normalMap") 
    {
        m_normalMap = texture;
    }
    else if (name == "roughMap")
    {
        m_roughMap = texture;
    }
    else if (name == "metalMap")
    {
        m_metalMap = texture;
    }
    else if (name == "emissiveMap")
    {

        m_emissiveMap = texture;
    }
    else if (texture)
    {
        std::string handle = ResourceLibrary::GetTextureHandle(texture);
        m_textureHandles[name] = handle;
    }
    else 
    {
        m_textureHandles[name] = "";
    }
}

float Material::GetFloat(const std::string& name) const
{
    auto it = m_floatUniforms.find(name);
    return (it != m_floatUniforms.end()) ? it->second : 0.0f;
}

int Material::GetInt(const std::string& name) const
{
    auto it = m_intUniforms.find(name);
    return (it != m_intUniforms.end()) ? it->second : 0;
}

bool Material::GetBool(const std::string& name) const
{
    auto it = m_boolUniforms.find(name);
    return (it != m_boolUniforms.end()) ? it->second : false;
}

glm::vec2 Material::GetVec2(const std::string& name) const
{
    auto it = m_vec2Uniforms.find(name);
    return (it != m_vec2Uniforms.end()) ? it->second : glm::vec2(0.0f);
}

glm::vec3 Material::GetVec3(const std::string& name) const
{
    auto it = m_vec3Uniforms.find(name);
    return (it != m_vec3Uniforms.end()) ? it->second : glm::vec3(0.0f);
}

glm::vec4 Material::GetVec4(const std::string& name) const
{
    auto it = m_vec4Uniforms.find(name);
    return (it != m_vec4Uniforms.end()) ? it->second : glm::vec4(0.0f);
}

Texture* Material::GetTexture(const std::string& name) const
{
    auto it = m_textureHandles.find(name);
    if (it != m_textureHandles.end() && !it->second.empty()) {
        return ResourceLibrary::GetTexture(it->second.c_str());
    }
    return nullptr;
}