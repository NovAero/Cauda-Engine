#pragma once
#include "ThirdParty/Flecs.h"

class GlmModule
{
public:
	GlmModule(flecs::world& world);
	~GlmModule() {}

private:
	flecs::world& m_world;
};

