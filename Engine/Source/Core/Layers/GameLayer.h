#pragma once
#include "Core/Layers/Layer.h"
#include "ThirdParty/Flecs.h"
#include "imgui.h"

class CharacterModule;
class CharacterController;
class PhysicsModule;
class CameraModule;
class GamemodeModule;
class Scene;

namespace Cauda
{
	class GameLayer : IKeyListener, IMouseListener, IGamepadListener//, public Layer
	{
	public:
		GameLayer(flecs::world& w);
		~GameLayer();
		void OnAttach();
		void OnDetach();

		void OnUserEvent(SDL_UserEvent& event);

	protected:
		// Input Processing
		virtual void OnKeyPress(PKey key) override;
		virtual void OnKeyRelease(PKey key) override {}

		virtual void OnMouseClick(MouseButton button, int clicks) override {}
		virtual void OnMouseRelease(MouseButton mouseButton) override {}
		virtual void OnMouseScroll(float amount) override {}

		virtual void OnButtonDown(Uint8 button) override;
		virtual void OnButtonUp(Uint8 button) override {}

		virtual void OnLeftAxisInput(float xValue, float yValue) override {}
		virtual void OnRightAxisInput(float xValue, float yValue) override {}

		virtual void OnTriggerDown(bool left) override {}
		virtual void OnTriggerUp(bool left) override {}

	public:
		void OverrideImGuiStyle();
		void RestoreImGuiStyle();

		void CleanupUnposessedPlayers();

		void DrawGameUI();
		void RenderCursor();
		void SetupSystems();

		void PauseGame(bool pause = true);
		void UnpauseGame() { PauseGame(false); }
		void TogglePause();
		bool IsGameRunning();
		bool IsInLobby();
		bool IsInMainMenu();
		bool IsStartingRound();

		void OnPlayerPossessed(flecs::entity player, CharacterController& which);
		void OnPlayerDetach(flecs::entity player, CharacterController& which);
		//bool TogglePauseMenu();
		bool ToggleGameSettingsMenu();

		void DetachCharacterControllers();
		const CharacterController* GetCharacterController(flecs::entity possessed);
		const CharacterController* GetCharacterController(int which);
		const std::vector<CharacterController*>& GetCharacterControllers();
	private:
		flecs::world& m_world;
		flecs::system m_controllerUpdateSystem;

		std::vector<CharacterController*> m_characterControllers;
		std::vector<flecs::entity> m_playerEntities;
		CharacterModule* m_characterModule;
		PhysicsModule* m_physicsModule;
		CameraModule* m_cameraModule;
		GamemodeModule* m_gamemodeModule;

		bool m_gameWon = false;
		bool m_endGameCalled = false;
		bool m_inMainMenu = true;
		bool m_inLobby = false;

		std::string m_selectedGameScene;

		std::string m_gameScene = "GravityWellV64.json";
		std::string m_menuScene = "GravityWellV64.json";
		std::string m_lobbyScene = "GravityWellV64.json";

		float m_lobbyTimerTime = -1.f;
		float m_roundBeginDelay = -1.f;

		bool m_startingRound = false;
		bool m_playersReadied[4] = { 0 };
		std::vector<flecs::entity_t> m_possessedPlayers;

		bool m_showGameOverlay = true;
		bool m_showDebugPanel = false;
		bool m_showGameMenu = false;
		bool m_shouldTickTimer = false;

		int m_lobbyBGMHandle = -1;
		int m_roundBGMHandle = -1;

		void CachePlayerEntities();

		void RenderPlayerUICorners();
		void RenderPlayerUI(int playerIndex, flecs::entity playerEntity);
		void RenderGameTimer();
		void RenderCenterHUD();
		void RenderDebugPanel();
		void RenderPauseMenu();
		void RenderGameMenu();
		void RenderMainMenu();
		void RenderPlayersReadiedTimer();
		void RenderLobbyUI();

		void RunOutro();
		void RunIntro();

		bool IsPlayerConnected(int playerIndex) const;

	};
}