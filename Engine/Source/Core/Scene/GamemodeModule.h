#pragma once

class EditorModule;
class PhysicsModule;
class RendererModule;

namespace EGamemodeStage
{
    enum Type : int
    {
        Grace, // Grace period.
        Growth,
        Sucking,
        Spinning,
        Beyblade,
        Chaos,
        END
    };

    const static std::string Labels[END + 1] =
    {
        "Grace",
        "Growth",
        "Sucking",
        "Spinning",
        "Beyblade",
        "Chaos",
        "Game Not Playing",
    };
}

struct GamemodeSingleton
{
    // Serialised
    int maxPlayers = 4;
    float timeLimit = 300.f;
    std::vector<int> stageDurations = { 10, 10, 10, 1, 30, 30 };

    // Non-Serialised
    float gametime = 0.f;

    EGamemodeStage::Type stage = EGamemodeStage::END;
    float stageTimer = 0.f;
};

class GamemodeModule
{
public:
    GamemodeModule(flecs::world& ecs);
    ~GamemodeModule();

    GamemodeModule(const GamemodeModule&) = delete;
    GamemodeModule& operator=(const GamemodeModule&) = delete;
    GamemodeModule(GamemodeModule&&) = delete;
    GamemodeModule& operator=(GamemodeModule&&) = delete;

    void ResetGametime();

    // Passing 'bool* p_open' displays a Close button on the upper-right corner of the window,
    // the pointed value will be set to false when the button is pressed.
    void DrawWindow(bool* p_open = nullptr);

    void StartRound();
    void EndRound();

    bool IsRoundRunning() { return m_isRoundRunning; }

private:
    flecs::world& m_world;
    EditorModule* m_editorModule = nullptr;
    PhysicsModule* m_physicsModule = nullptr;
    RendererModule* m_rendererModule = nullptr;

    flecs::system m_updateSystem;
    //flecs::observer m_onSetObserver;
    bool m_isRoundRunning = false;

private:
    void SetupComponents();
    void SetupSystems();
    void SetupObservers();
    void SetupQueries();

    void OnStageEntered(GamemodeSingleton& gamemode);
    void OnUpdate(float deltaTime, GamemodeSingleton& gamemode);

    template<typename ComponentType>
    flecs::entity GetFirstEntityWith() {
        flecs::entity entity = flecs::entity::null();

        flecs::query q = m_world.query<ComponentType>();
        q.run([&](flecs::iter& it) { if (it.next()) entity = it.entity(0); });

        return entity;
    }
};