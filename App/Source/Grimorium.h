#pragma once
#include <Core/Modules.h>
#include <ThirdParty/Flecs.h>

namespace Grimorium
{
	void InitialiseGrimoires(flecs::world& world);
	void BurnTheLibrary();
}