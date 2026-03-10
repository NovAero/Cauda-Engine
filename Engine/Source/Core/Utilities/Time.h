#pragma once

#include <SDL3/SDL_time.h>

namespace SysTime
{
	std::string GetDateTimeString(bool stripped);
	std::string GetTimeString(bool stripped);
	std::string GetDateString(bool stripped);

	double Delta(Uint64 ticksNow, Uint64 ticksThen);
	double DeltaNS(Uint64 ticksNow, Uint64 ticksThen);
}