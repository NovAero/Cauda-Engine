#pragma once
#include <ThirdParty/Flecs.h>
#include <vector>

class STLModule
{
public:
	STLModule(flecs::world& world);
	~STLModule();

	STLModule(const STLModule&) = delete;
	STLModule& operator=(const STLModule&) = delete;
	STLModule(STLModule&&) = delete;
	STLModule& operator=(STLModule&&) = delete;

	void SetupComponents();

private:

	flecs::world& m_world;

};

//Template type to convert to serialisable vectors
template <typename Elem, typename Vector = std::vector<Elem>>
flecs::opaque<Vector, Elem> std_vector_ser(flecs::world& world) {
    return flecs::opaque<Vector, Elem>()
        .as_type(world.vector<Elem>())

        // Forward elements of std::vector value to serializer
        .serialize([](const flecs::serializer* s, const Vector* data) {
        for (const auto& el : *data) {
            s->value(el);
        }
        return 0;
            })

        // Return vector count
        .count([](const Vector* data) {
        return data->size();
            })

        // Resize contents of vector
        .resize([](Vector* data, size_t size) {
        data->resize(size);
            })

        // Ensure element exists, return pointer
        .ensure_element([](Vector* data, size_t elem) {
        if (data->size() <= elem) {
            data->resize(elem + 1);
        }

        return &data->data()[elem];
            });
}

