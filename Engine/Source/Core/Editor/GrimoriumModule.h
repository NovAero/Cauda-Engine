#pragma once
#include "ThirdParty/flecs.h"
#include <functional>
#include <unordered_set>
#include <string>
#include <vector>
#include <iostream>
#include "Platform/Win32/Application.h"

struct OnEnableGrimoire {};
struct OnDisableGrimoire {};
struct OnStartGrimoire {};
struct OnLoadGrimoire {};
struct OnPostLoadGrimoire {};
struct OnPreUpdateGrimoire {};
struct OnUpdateGrimoire {};
struct OnValidateGrimoire {};
struct OnPostUpdateGrimoire {};
struct OnPreStoreGrimoire {};
struct OnStoreGrimoire {};

struct GrimoireComponent
{
    bool enabled = true;
    bool wasEnabled = false;
};

struct Grimoire
{
public:
    bool m_initialised = false;

    virtual void OnStart(flecs::entity entity, float deltaTime) {}

    virtual void OnLoad(flecs::entity entity, float deltaTime) {}
    virtual void OnPostLoad(flecs::entity entity, float deltaTime) {}
    virtual void OnPreUpdate(flecs::entity entity, float deltaTime) {}
    virtual void OnUpdate(flecs::entity entity, float deltaTime) {}
    virtual void OnValidate(flecs::entity entity, float deltaTime) {}
    virtual void OnPostUpdate(flecs::entity entity, float deltaTime) {}
    virtual void OnPreStore(flecs::entity entity, float deltaTime) {}
    virtual void OnStore(flecs::entity entity, float deltaTime) {}

    virtual void OnAdd(flecs::entity entity) {}
    virtual void OnRemove(flecs::entity entity) {}
    virtual void OnDestroy(flecs::entity entity) {}

    virtual void OnEnable(flecs::entity entity) {}
    virtual void OnDisable(flecs::entity entity) {}

    virtual void OnCollisionEnter(flecs::entity entity, flecs::entity other) {}
    virtual void OnCollisionStay(flecs::entity entity, flecs::entity other) {}
    virtual void OnCollisionExit(flecs::entity entity, flecs::entity other) {}

    virtual void OnDrawInspector(flecs::entity entity) {}
    virtual void OnDrawDebug(flecs::entity entity) {}
    virtual const char* GetScriptName() const = 0;

    template<typename T>
    T* GetComponent(flecs::entity entity)
    {
        return entity.is_valid() ? entity.try_get_mut<T>() : nullptr;
    }

    template<typename T>
    const T* GetComponent(flecs::entity entity) const
    {
        return entity.is_valid() ? entity.try_get<T>() : nullptr;
    }

    template<typename T>
    bool HasComponent(flecs::entity entity) const
    {
        return entity.is_valid() ? entity.has<T>() : false;
    }

    flecs::world GetWorld(flecs::entity entity) const { return entity.world(); }
    //float DeltaTime(flecs::entity entity) const { return entity.world().delta_time(); }

    virtual ~Grimoire() = default;
};

struct ScriptRegistration
{
    std::string name;
    std::function<void(flecs::entity)> add;
    std::function<void(flecs::entity)> remove;
    std::function<bool(flecs::entity)> has;
    std::function<void(flecs::entity)> draw_inspector;
    std::function<Grimoire* (flecs::entity)> get_grimoire;
};

#define GRIMOIRE_SCRIPT_SYSTEM(Phase, TickSource, MethodName, ScriptType) \
    world.system<ScriptType, GrimoireComponent>((std::string(name) + "_" #MethodName).c_str()) \
        .immediate() \
        .kind(Phase) \
        .tick_source(TickSource) \
        .each([name](flecs::iter& it, size_t row, ScriptType& script, GrimoireComponent& grimoire){ \
            if(!Cauda::Application::IsGameRunning()) return; \
            float deltaTime = it.delta_system_time(); \
            flecs::entity e = it.entity(row); \
            if (GrimoriumModule::IsLoading() || !e.is_valid() || !grimoire.enabled) { \
                return; \
            } \
            script.MethodName(e, deltaTime); \
        });

class GrimoriumModule
{
public:
    GrimoriumModule(flecs::world& world);
    ~GrimoriumModule() = default;
    GrimoriumModule(const GrimoriumModule&) = delete;
    GrimoriumModule& operator=(const GrimoriumModule&) = delete;

    void OnImGuiRender();

    void CallGrimoireDebugDraw();

    static void RegisterScript(ScriptRegistration&& registration);

    template<typename ScriptType>
    static void RegisterScript(flecs::world& world, const char* name)
    {
        auto component = world.component<ScriptType>(name);
        RegisterScriptMembers<ScriptType>(component);

        SetupLifecycleObservers<ScriptType>(world, name);

        flecs::entity tick_source = Cauda::Application::GetGameTickSource();
        flecs::entity game_start = Cauda::Application::GetOnGameStart();
        GRIMOIRE_SCRIPT_SYSTEM(flecs::OnLoad, game_start, OnStart, ScriptType);
        GRIMOIRE_SCRIPT_SYSTEM(flecs::OnLoad, tick_source, OnLoad, ScriptType);
        GRIMOIRE_SCRIPT_SYSTEM(flecs::PostLoad, tick_source, OnPostLoad, ScriptType);
        GRIMOIRE_SCRIPT_SYSTEM(flecs::PreUpdate, tick_source, OnPreUpdate, ScriptType);
        GRIMOIRE_SCRIPT_SYSTEM(flecs::OnUpdate, tick_source, OnUpdate, ScriptType);
        GRIMOIRE_SCRIPT_SYSTEM(flecs::OnValidate, tick_source, OnValidate, ScriptType);
        GRIMOIRE_SCRIPT_SYSTEM(flecs::PostUpdate, tick_source, OnPostUpdate, ScriptType);
        GRIMOIRE_SCRIPT_SYSTEM(flecs::PreStore, tick_source, OnPreStore, ScriptType);
        GRIMOIRE_SCRIPT_SYSTEM(flecs::OnStore, tick_source, OnStore, ScriptType);

        world.system<ScriptType, GrimoireComponent>((std::string(name) + "_EnableDisableMonitor").c_str())
            .kind(flecs::OnLoad)
            .each([](flecs::entity e, ScriptType& script, GrimoireComponent& grimoire)
                {
                    if (GrimoriumModule::IsLoading() || !e.is_valid()) return;

                    if (grimoire.enabled != script.m_initialised)
                    {
                        if (grimoire.enabled) 
                        {
                            script.OnEnable(e);
                        }
                        else 
                        {
                            script.OnDisable(e);
                        }

                        script.m_initialised = grimoire.enabled;
                    }
                });

        RegisterScript({
            .name = std::string(name),
            .add = [](flecs::entity e)
            {
                e.add<GrimoireComponent>();
                e.add<ScriptType>();
               /* if (auto* script = e.try_get_mut<ScriptType>())
                {
                    script->OnAdd(e);
                }*/
            },
            .remove = [](flecs::entity e)
            {
                e.remove<ScriptType>();
               /* if (auto* script = e.try_get_mut<ScriptType>())
                {
                    script->OnRemove(e);
                }*/
            },
            .has = [](flecs::entity e)
            {
                return e.has<ScriptType>();
            },
            .draw_inspector = [](flecs::entity e)
            {
                if (auto* script = e.try_get_mut<ScriptType>())
                {
                    script->OnDrawInspector(e);
                }
            },
            .get_grimoire = [](flecs::entity e) -> Grimoire*
            {
                return e.try_get_mut<ScriptType>();
            }
            });
    }

    const std::vector<ScriptRegistration>& GetRegisteredScripts();

private:
    void RegisterWithEditor();
    void SetupComponents();
    void SetupObservers();
    void DrawScriptInspector(flecs::entity entity);

    template<typename ScriptType>
    static void SetupLifecycleObservers(flecs::world& world, const char* name)
    {
        world.observer<ScriptType>()
            .event(flecs::OnAdd)
            .each([](flecs::entity entity, ScriptType& script)
                {
                    if (!Cauda::Application::IsGameRunning()) return;
                    if (GrimoriumModule::IsLoading() || !entity.is_valid()) return;
                   
                    script.OnAdd(entity);
                   
                });

        world.observer<ScriptType>()
            .event(flecs::OnRemove)
            .each([name](flecs::entity entity, ScriptType& script)
                {
                    if (!Cauda::Application::IsGameRunning()) return;
                    if (GrimoriumModule::IsLoading() || !entity.is_valid()) return;
                    
                    script.OnRemove(entity);
                    
                });

        world.observer<ScriptType>()
            .event(flecs::OnDelete)
            .each([name](flecs::entity entity, ScriptType& script)
                {
                    if (!Cauda::Application::IsGameRunning()) return;
                    if (GrimoriumModule::IsLoading() || !entity.is_valid()) return;

                    script.OnDestroy(entity);
                });
    }

    flecs::world& m_world;
    static inline std::vector<ScriptRegistration> s_scriptRegistrations;
    static inline bool s_isLoading = false;

public:
    static void SetLoading(bool loading) { s_isLoading = loading; }
    static bool IsLoading() { return s_isLoading; }

    template<typename ScriptType>
    static void RegisterScriptMembers(flecs::component<ScriptType> component)
    {
    }
};

#define OPEN_GRIMOIRE(ClassName) \
    struct ClassName : public Grimoire 

#define WRITE_GRIMOIRE(ClassName, DisplayName) \
        const char* GetScriptName() const override { return DisplayName; } \
        static void Register(flecs::world& world) \
        { \
            GrimoriumModule::RegisterScript<ClassName>(world, DisplayName); \
        } 

#define CLOSE_GRIMOIRE // Doesn't do anything but feels complete

#define SERIALISE_GRIMOIRE(ScriptType) \
template<> \
inline void GrimoriumModule::RegisterScriptMembers<ScriptType>(flecs::component<ScriptType> component)

#define SERIALISE_FIELD(name) \
    s->member(#name); \
    s->value(data->name);

#define ENSURE_FIELD(name) \
    if (strcmp(member, #name) == 0) return &dst->name;

template<typename ScriptType>
void RegisterGrimoireScript(flecs::world& world)
{
    GrimoriumModule::RegisterScript<ScriptType>(world, ScriptType{}.GetScriptName());
}