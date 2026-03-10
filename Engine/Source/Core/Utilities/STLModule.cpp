#include "cepch.h"
#include "STLModule.h"

STLModule::STLModule(flecs::world& world) :
	m_world(world)
{
	SetupComponents();
}

STLModule::~STLModule()
{
}

void STLModule::SetupComponents()
{
    m_world.component<std::string>()
        .opaque(flecs::String) // Opaque type that maps to string
        .serialize([](const flecs::serializer* s, const std::string* data) {
        const char* str = data->c_str();
        return s->value(flecs::String, &str); // Forward to serializer
            })
        .assign_string([](std::string* data, const char* value) {
        *data = value; // Assign new value to std::string
            });

    //Ints
    m_world.component<std::vector<int>>()
        .opaque(std_vector_ser<int>);

    //Floats
    m_world.component<std::vector<float>>()
        .opaque(std_vector_ser<float>);

    //Doubles
    m_world.component<std::vector<double>>()
        .opaque(std_vector_ser<double>);

    //std::strings
    m_world.component<std::vector<std::string>>()
        .opaque(std_vector_ser<std::string>);
}