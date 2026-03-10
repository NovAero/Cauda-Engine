#include "cepch.h"
#include "GamemodeModule.h"
#include "Core/Editor/ResourceLibrary.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Physics/PhysicsModule.h"
#include "Core/Character/CharacterModule.h"
#include "Renderer/RendererModule.h"
#include <Core/Editor/GrimoriumModule.h>
#include "Content/Grimoires/GravityWell.h"

namespace EGamemodeStage
{
	// prefix (++enumType)
	Type& operator++(Type& orig)
	{
		if (orig == END)
			orig = static_cast<Type>(0);
		else
			orig = static_cast<Type>(orig + 1);
		return orig;
	}

	// postfix (enumType++)
	Type operator++(Type& orig, int)
	{
		Type out = orig;
		++orig;
		return out;
	}
}


GamemodeModule::GamemodeModule(flecs::world& ecs)
	: m_world(ecs),
	m_editorModule(m_world.try_get_mut<EditorModule>()),
	m_physicsModule(m_world.try_get_mut<PhysicsModule>()),
	m_rendererModule(m_world.try_get_mut<RendererModule>())
{
	SetupComponents();
	SetupSystems();
	SetupObservers();
	SetupQueries();

	EditorConsole::Inst()->AddConsoleInfo("GamemodeModule initialised successfully", true);
}

GamemodeModule::~GamemodeModule()
{
	Logger::PrintLog("GamemodeModule shutdown starting...");

	if (m_updateSystem && m_updateSystem.is_alive())
		m_updateSystem.destruct();
	//if (m_onSetObserver && m_onSetObserver.is_alive())
	//	m_onSetObserver.destruct();

	Logger::PrintLog("GamemodeModule shutdown completed.");
}

void GamemodeModule::OnStageEntered(GamemodeSingleton& gamemode)
{
	flecs::entity gravityWell = GetFirstEntityWith<GravityWell>();
	if (!gravityWell.is_alive()) return;
	auto gravComp = gravityWell.try_get_mut<GravityWell>();
	auto matComp = gravityWell.try_get_mut<MaterialComponent>();
	auto rendererModule = m_world.try_get_mut<RendererModule>();
	auto material = rendererModule->GetMaterial(*matComp);

	if (!gravComp->enabled) return;

	switch (gamemode.stage)
	{
	case EGamemodeStage::Grace:
	{
		material->SetVec3("colourTint", glm::vec3(0.6f, 0.6f, 0.6f));
		//gravComp->SetPhase(EGravityWellPhase::DoNothing);
		//gravComp->bulletMode = EGravityWellBulletMode::Disabled;
	}break;
	case EGamemodeStage::Growth:
	{
		material->SetVec3("colourTint", glm::vec3(1.f, 1.f, 1.f));
		gravComp->SetPhase(EGravityWellPhase::GrowSuck);
	}break;
	case EGamemodeStage::Sucking:
	{
		material->SetVec3("colourTint", glm::vec3(0.2f, 0.2f, 0.2f));
		gravComp->SetPhase(EGravityWellPhase::AntiGravSuck);
	}break;
	case EGamemodeStage::Spinning:
	{
		material->SetVec3("colourTint", glm::vec3(0.6f, 0.0f, 0.0f));
		gravComp->SetPhase(EGravityWellPhase::Vortex);
	}break;
	case EGamemodeStage::Beyblade:
	{
		material->SetVec3("colourTint", glm::vec3(0.8f, 0.1f, 0.1f));
		gravComp->SetPhase(EGravityWellPhase::Push);
		gravComp->bulletMode = EGravityWellBulletMode::Spiral;
	}break;
	case EGamemodeStage::Chaos:
	{
		material->SetVec3("colourTint", glm::vec3(1.0f, 0.1f, 0.1f));
		gravComp->SetPhase(EGravityWellPhase::DoNothing);
		gravComp->bulletMode = EGravityWellBulletMode::Random;
	}break;
	}
}

void GamemodeModule::OnUpdate(float deltaTime, GamemodeSingleton& gamemode)
{
	if (!m_isRoundRunning) return;

	gamemode.stageTimer -= deltaTime;
	while (gamemode.stageTimer < 0.f)
	{
		// Advance to next stage
		gamemode.stage++;

		// Loop back around for now.
		if (gamemode.stage == EGamemodeStage::END)
			gamemode.stage = EGamemodeStage::Sucking;

		gamemode.stageTimer += gamemode.stageDurations[gamemode.stage];

		OnStageEntered(gamemode);
	}

	// yeah it's hardcoded i know... 
	////// GAMEMODE SCRIPT LOGIC //////

	// This is for changing stuff during stages.
	switch (gamemode.stage)
	{
		// Add some actual rng later on.
		case EGamemodeStage::Grace:
		{
		}break;
		case EGamemodeStage::Growth:
		{
		}break;
		case EGamemodeStage::Beyblade:
		{
		}break;
		case EGamemodeStage::Chaos:
		{
		}break;
	}
}

void GamemodeModule::DrawWindow(bool* p_open)
{
	if (p_open && !(*p_open)) return;

    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Gamemode", p_open)) { ImGui::End(); return; }

	auto gamemode = m_world.try_get_mut<GamemodeSingleton>();
	bool isRunning = Cauda::Application::IsGameRunning();

	ImGui::TextUnformatted("Info");
	ImGui::Separator();
	if (ImGui::BeginTable("##gamemode_info", 2, ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		// Display Running State
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Running");
		ImGui::TableNextColumn();
		ImGui::BeginDisabled();
		ImGui::Checkbox("##GameRunning", &isRunning);
		ImGui::EndDisabled();

		// Display Elapsed Game Time
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Game Time");
		ImGui::TableNextColumn();
		ImGui::Text("%.2f", gamemode->gametime);

		// Display Current Stage
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Current Stage");
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(EGamemodeStage::Labels[gamemode->stage].c_str());

		// Display Time left for Current Stage
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Stage Time left");
		ImGui::TableNextColumn();
		ImGui::Text("%.2f", gamemode->stageTimer);

		ImGui::EndTable();
	}
	ImGui::Spacing();
	ImGui::Spacing();

	bool changed = false;

	ImGui::TextUnformatted("Settings");
	ImGui::Separator();
	if (ImGui::BeginTable("##gamemode_settings", 2, ImGuiTableFlags_Resizable))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 150.f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		// Disable editing of settings during gameplay.
		if (isRunning) ImGui::BeginDisabled();

		// Set Max Players
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Max Players");
		ImGui::TableNextColumn();
		ImGui::InputInt("##MaxPlayerCount", &gamemode->maxPlayers, 1, 1);
		gamemode->maxPlayers = std::clamp(gamemode->maxPlayers, 0, 4);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;

		// Set Time Limit per Round
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Time Limit");
		ImGui::TableNextColumn();
		ImGui::DragFloat("##TimeLimit", &gamemode->timeLimit, 1.f, 0.f, 0.f, "%.1f");
		gamemode->timeLimit = std::max(gamemode->timeLimit, 0.f);
		if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;


		// Stage durations section
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Stage Durations");
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::Separator();
		ImGui::TableNextColumn();
		ImGui::Separator();

		// Set Stage durations
		for (int stage = 0; stage < EGamemodeStage::END; stage++)
		{
			std::string stageName = EGamemodeStage::Labels[stage];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(stageName.c_str());
			ImGui::TableNextColumn();
			ImGui::DragInt(("##" + stageName).c_str(), &gamemode->stageDurations[stage], 1.f, 0);
			gamemode->stageDurations[stage] = std::max(gamemode->stageDurations[stage], 0);
			if (ImGui::IsItemDeactivatedAfterEdit()) changed = true;
		}

		if (isRunning) ImGui::EndDisabled();

		ImGui::EndTable();
	}

	if (changed) m_world.modified<GamemodeSingleton>();

	ImGui::End();
}

void GamemodeModule::StartRound()
{
	m_isRoundRunning = true;
}

void GamemodeModule::EndRound()
{
	m_isRoundRunning = false;
}

void GamemodeModule::ResetGametime()
{
	auto gamemode = m_world.get<GamemodeSingleton>();
	gamemode.gametime = 0.f;
	gamemode.stageTimer = 0.f;
	gamemode.stage = EGamemodeStage::END;
	m_world.set<GamemodeSingleton>(gamemode);
}

void GamemodeModule::SetupComponents()
{
	m_world.component<GamemodeSingleton>("Gamemode")
		.member<int>("maxPlayers")
		.member<float>("timeLimit")
		.member<std::vector<int>>("stageDurations")
		.add(flecs::Singleton);

	m_world.set<GamemodeSingleton>({});

}

void GamemodeModule::SetupSystems()
{
	flecs::entity GameTick = Cauda::Application::GetGameTickSource();
	//flecs::entity GamePhase = Cauda::Application::GetGamePhase();


	m_updateSystem = m_world.system<GamemodeSingleton>("GamemodeUpdate")
		.kind(flecs::PreUpdate)
		.tick_source(GameTick)
		.each([this](flecs::iter& it, size_t row, GamemodeSingleton& gamemode)
		{
			if (!m_isRoundRunning) return;
			float deltaTime = it.delta_system_time();
			//std::cout << "GamePhase: " << deltaTime << std::endl;
			gamemode.gametime += deltaTime;

			if (gamemode.gametime > gamemode.timeLimit)
			{
				// Pause the game.
				SDL_PushEvent(&GameEvents::g_EndRoundEvent);
				Cauda::Application::PauseGame(true);
				return;
			}

			OnUpdate(deltaTime, gamemode);
		});
}

void GamemodeModule::SetupObservers()
{
	//m_onSetObserver = m_world.observer<GamemodeSingleton>()
	//	.event(flecs::OnSet)
	//	.each([this](flecs::entity entity, GamemodeSingleton& gamemode)
	//	{
	//	});
}

void GamemodeModule::SetupQueries()
{

}
