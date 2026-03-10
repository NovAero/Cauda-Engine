#pragma once
#include "ThirdParty/flecs.h"
#include <functional>
#include <unordered_map>
#include <string>
#include "Platform/Win32/Application.h" 


namespace Timer
{
    using Callback = std::function<void()>;

    struct CallbackTimer
    {
        Callback callback;
        float duration;
        float elapsed = 0.0f;
        bool repeat = false;
        bool active = true;
    };

    struct ScriptTimers
    {
        std::unordered_map<std::string, CallbackTimer> namedTimers;
        std::vector<CallbackTimer> delays;
    };

    inline void Initialise(flecs::world& world)
    {
        world.component<ScriptTimers>();

        flecs::entity GameTick = Cauda::Application::GetGameTickSource();
        
        world.system<ScriptTimers>()
			.kind(flecs::PostLoad)
            .immediate()
            .tick_source(GameTick)
            .each([](flecs::iter& iter, size_t row, ScriptTimers& timers)
            //.each([](flecs::entity e, ScriptTimers& timers)
                {
                    flecs::entity e = iter.entity(row);
                    float dt = iter.delta_system_time();
                    //if (!Cauda::Application::IsGameRunning())
                    //{
                    //    return;
                    //}

                    if (!e.is_alive()) return;

                    for (auto it = timers.namedTimers.begin(); it != timers.namedTimers.end();)
                    {
                        auto& [name, timer] = *it;
                        if (!timer.active)
                        {
                            ++it;
                            continue;
                        }

                        timer.elapsed += dt;
                        if (timer.elapsed >= timer.duration)
                        {
                            if (timer.callback && e.is_alive())
                            {
                                try
                                {
                                    timer.callback();
                                }
                                catch (...)
                                {
                                    it = timers.namedTimers.erase(it);
                                    continue;
                                }
                            }

                            if (timer.repeat && e.is_alive())
                            {
                                timer.elapsed = 0.0f;
                                ++it;
                            }
                            else
                            {
                                it = timers.namedTimers.erase(it);
                            }
                        }
                        else
                        {
                            ++it;
                        }
                    }

                    for (auto it = timers.delays.begin(); it != timers.delays.end();)
                    {
                        if (!it->active)
                        {
                            ++it;
                            continue;
                        }

                        it->elapsed += dt;
                        if (it->elapsed >= it->duration)
                        {
                            if (it->callback && e.is_alive())
                            {
                                try
                                {
                                    it->callback();
                                }
                                catch (...)
                                {
                                }
                            }
                            it = timers.delays.erase(it);
                        }
                        else
                        {
                            ++it;
                        }
                    }
                });

        world.observer<ScriptTimers>()
            .event(flecs::OnRemove)
            .each([](flecs::entity entity, ScriptTimers& timers)
                {
                    std::cout << entity.name() << " removed timer" << std::endl;
                    timers.namedTimers.clear();
                    timers.delays.clear();
                });


        world.system()
            .kind(flecs::OnValidate)
            .tick_source(GameTick)
            .run([](flecs::iter& it)
                {
                    static bool wasRunning = false;
                    bool isRunning = Cauda::Application::IsGameRunning();

                    if (!isRunning && wasRunning)
                    {
                        auto timerQuery = it.world().query<ScriptTimers>();
                        timerQuery.each([](flecs::entity e, ScriptTimers& timers)
                            {
                                timers.namedTimers.clear();
                                timers.delays.clear();
                            });
                    }

                    wasRunning = isRunning;
                });

    }

    inline void SetDelay(flecs::entity entity, float delay, Callback callback)
    {
        if (!entity.is_alive()) return;

        if (!entity.has<ScriptTimers>())
        {
            entity.set<ScriptTimers>({});
        }

        auto timers = entity.try_get_mut<ScriptTimers>();
        if (!timers)
        {
            std::cout << "timer not found" << std::endl;
            return;
        }

        timers->delays.emplace_back(CallbackTimer{
            .callback = callback,
            .duration = delay,
            .elapsed = 0.0f,
            .repeat = false,
            .active = true
            });
    }

    inline void AddScriptTimerComponent(flecs::entity entity)
    {
        entity.add<ScriptTimers>();
    }

    inline void SetTimer(flecs::entity entity, const std::string& name,
        float interval, Callback callback, bool repeat = true)
    {
        if (!entity.is_alive()) return;

        if (!entity.has<ScriptTimers>())
        {
            entity.set<ScriptTimers>({});
        }

        auto* timers = entity.try_get_mut<ScriptTimers>();
        if (!timers) return;

        timers->namedTimers[name] = CallbackTimer{
            .callback = callback,
            .duration = interval,
            .elapsed = 0.0f,
            .repeat = repeat,
            .active = true
        };
    }

    inline void StopTimer(flecs::entity entity, const std::string& name)
    {
        if (!entity.is_alive()) return;

        if (auto* timers = entity.try_get_mut<ScriptTimers>())
        {
            timers->namedTimers.erase(name);
        }
    }

    inline void PauseTimer(flecs::entity entity, const std::string& name)
    {
        if (!entity.is_alive()) return;

        if (auto* timers = entity.try_get_mut<ScriptTimers>())
        {
            if (auto it = timers->namedTimers.find(name); it != timers->namedTimers.end())
            {
                it->second.active = false;
            }
        }
    }

    inline void ResumeTimer(flecs::entity entity, const std::string& name)
    {
        if (!entity.is_alive()) return;

        if (auto* timers = entity.try_get_mut<ScriptTimers>())
        {
            if (auto it = timers->namedTimers.find(name); it != timers->namedTimers.end())
            {
                it->second.active = true;
            }
        }
    }

    inline void ResetTimer(flecs::entity entity, const std::string& name)
    {
        if (!entity.is_alive()) return;

        if (auto* timers = entity.try_get_mut<ScriptTimers>())
        {
            if (auto it = timers->namedTimers.find(name); it != timers->namedTimers.end())
            {
                it->second.elapsed = 0.0f;
            }
        }
    }

    inline bool HasTimer(flecs::entity entity, const std::string& name)
    {
        if (!entity.is_alive()) return false;

        if (auto* timers = entity.try_get<ScriptTimers>())
        {
            return timers->namedTimers.contains(name);
        }
        return false;
    }

    inline float GetTimerProgress(flecs::entity entity, const std::string& name)
    {
        if (!entity.is_alive()) return 0.0f;

        if (auto* timers = entity.try_get<ScriptTimers>())
        {
            if (auto it = timers->namedTimers.find(name); it != timers->namedTimers.end())
            {
                return std::min(it->second.elapsed / it->second.duration, 1.0f);
            }
        }
        return 0.0f;
    }

    inline float GetTimerRemaining(flecs::entity entity, const std::string& name)
    {
        if (!entity.is_alive()) return 0.0f;

        if (auto* timers = entity.try_get<ScriptTimers>())
        {
            if (auto it = timers->namedTimers.find(name); it != timers->namedTimers.end())
            {
                return std::max(0.0f, it->second.duration - it->second.elapsed);
            }
        }
        return 0.0f;
    }

    inline void ClearAllTimers(flecs::entity entity)
    {
        if (!entity.is_alive()) return;

        if (auto* timers = entity.try_get_mut<ScriptTimers>())
        {
            timers->namedTimers.clear();
            timers->delays.clear();
        }
    }
}