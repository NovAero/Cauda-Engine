#include "cepch.h"
#include "GlmModule.h"
#include "Core/Utilities/STLModule.h"

GlmModule::GlmModule(flecs::world& world) : m_world(world)
{
    m_world.component<glm::vec2>("glm::vec2")
        .member<float>("x")
        .member<float>("y");

    m_world.component<glm::vec3>("glm::vec3")
        .member<float>("x")
        .member<float>("y")
        .member<float>("z");

    m_world.component<glm::vec4>("glm::vec4")
        .member<float>("x")
        .member<float>("y")
        .member<float>("z")
        .member<float>("w");

    m_world.component<glm::quat>("glm::quat")
        .member<float>("x")
        .member<float>("y")
        .member<float>("z")
        .member<float>("w");

    m_world.component<std::vector<glm::vec2>>()
        .opaque(std_vector_ser<glm::vec2>);

    m_world.component<std::vector<glm::vec3>>()
        .opaque(std_vector_ser<glm::vec3>);

    m_world.component<std::vector<glm::vec4>>()
        .opaque(std_vector_ser<glm::vec4>);
}