#include "cepch.h"
#include "Time.h"

#include <sstream>

namespace SysTime
{
    std::string GetDateTimeString(bool stripped)
    {
        SDL_DateTime dt;
        SDL_Time ticks;

        SDL_GetCurrentTime(&ticks);
        SDL_TimeToDateTime(ticks, &dt, true);

        std::stringstream ss;

        if (stripped)
        {
            ss << '[' << std::to_string(dt.day) << '_' << std::to_string(dt.month) << '_' << std::to_string(dt.year);
            ss << '-' << std::to_string(dt.hour) << '_' << std::to_string(dt.minute) << '_' << std::to_string(dt.second) << "]";

        }
        else
        {
            ss << '[' << std::to_string(dt.day) << ", " << std::to_string(dt.month) << ", " << std::to_string(dt.year);
            ss << " | " << std::to_string(dt.hour) << ':' << std::to_string(dt.minute) << ':' << std::to_string(dt.second) << " ]";
        }

        return ss.str();
    }

    std::string GetTimeString(bool stripped)
    {
        SDL_DateTime dt;
        SDL_Time ticks;
        SDL_GetCurrentTime(&ticks);
        SDL_TimeToDateTime(ticks, &dt, true);

        std::stringstream ss;
        if (stripped)
        {
            ss << std::to_string(dt.hour) << '_' << std::to_string(dt.minute) << '_' << std::to_string(dt.second);
        }
        else
        {
            ss << '[' << std::to_string(dt.hour) << ':' << std::to_string(dt.minute) << ':' << std::to_string(dt.second) << ']';
        }
        return ss.str();
    }

    std::string GetDateString(bool stripped)
    {
        SDL_DateTime dt;
        SDL_Time ticks;

        SDL_GetCurrentTime(&ticks);
        SDL_TimeToDateTime(ticks, &dt, true);

        std::stringstream ss;

        if (stripped)
        {
            ss << '[' << std::to_string(dt.day) << '_' << std::to_string(dt.month) << '_' << std::to_string(dt.year) << ']';
        }
        else
        {
            ss << '[' << std::to_string(dt.day) << ", " << std::to_string(dt.month) << ", " << std::to_string(dt.year) << ']';
        }

        return ss.str();
    }

    double Delta(Uint64 ticksNow, Uint64 ticksThen)
    {
        return (double)(ticksNow - ticksThen) / 1000;
    }

    double DeltaNS(Uint64 ticksNow, Uint64 ticksThen)
    {
        return (double)(ticksNow - ticksThen);
    }

}