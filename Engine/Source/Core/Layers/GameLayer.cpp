#include "cepch.h"
#include "GameLayer.h"
#include "Core/Character/CharacterModule.h"
#include "Core/Character/CharacterController.h"
#include "Core/Physics/PhysicsModule.h"
#include "Core/Scene/CameraModule.h"
#include "Core/Editor/EditorModule.h"
#include "Core/Utilities/ImGuiUtilities.h"
#include "Platform/Win32/Application.h"
#include "Core/Audio/AudioModule.h"
#include "Content/Grimoires/Health.h"
#include "Content/Grimoires/Dash.h"
#include "Content/Grimoires/IntroCutscene.h"

#include "Source/UI/MainMenu.h"
#include "Source/UI/GameMenu.h"
#include "Source/UI/EndGameScreen.h"

namespace Cauda
{
	GameLayer::GameLayer(flecs::world& w) : //Layer("Game Layer"),
		m_world(w), m_showGameOverlay(true)
	{
		m_characterModule = m_world.try_get_mut<CharacterModule>();
		m_physicsModule = m_world.try_get_mut<PhysicsModule>();
		m_cameraModule = m_world.try_get_mut<CameraModule>();
		m_gamemodeModule = m_world.try_get_mut<GamemodeModule>();
		if (m_characterControllers.empty())
		{
			for (int i = 0; i < MAX_GAMEPAD_SOCKETS; ++i)
			{
				m_characterControllers.push_back(new CharacterController(m_world));
			}
		}
		SetupSystems();

	}

	GameLayer::~GameLayer()
	{
		for (auto controller : m_characterControllers)
		{
			delete controller;
		}
		InputSystem::ResetInputSockets();

	}

	void GameLayer::OnAttach()
	{
#ifndef _DISTRIB
		InputSystem::ResetInputSockets();
		m_characterControllers[0]->PossessFirstAvailable();
		InputSystem::Inst()->BindFirstAvailableController(m_characterControllers[0]);
#endif // !

		if (m_inMainMenu)
		{
			if (auto audioModule = m_world.try_get_mut<AudioModule>())
			{
				if (audioModule->TryStopAudioSource(m_roundBGMHandle)) m_roundBGMHandle = -1;

				if (!audioModule->IsValidHandle(m_lobbyBGMHandle))
				{
					if (auto asset = ResourceLibrary::GetAudioAsset("SW_MainMenu"))
					{
						asset->settings.loop = true;
						m_lobbyBGMHandle = audioModule->PlaySoundStreamBackground(*reinterpret_cast<SoundStream*>(asset));
						asset->settings.loop = false;
					}
				}
			}
		}

		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_HasGamepad;

		if (auto camComp = m_cameraModule->GetMainCamera().try_get_mut<CameraComponent>())
		{
			camComp->enableMovement = false;
		}

		InputSystem::Inst()->AddKeyListener(this);
		InputSystem::Inst()->AddMouseListener(this);

		Cauda::Application::RunGame();

		// Cache player entities for UI
		CachePlayerEntities();

		OverrideImGuiStyle();
		
	}

	void GameLayer::OnDetach()
	{
		for (auto controller : m_characterControllers)
		{
			controller->DetachFromPawn();
		}
		InputSystem::ResetInputSockets();
		if (auto camComp = m_cameraModule->GetMainCamera().try_get_mut<CameraComponent>())
		{
			camComp->enableMovement = true;
		}

		Cauda::Application::StopGame();
		m_gamemodeModule->ResetGametime();

		InputSystem::Inst()->RemoveKeyListener(this);
		InputSystem::Inst()->RemoveMouseListener(this);

		RestoreImGuiStyle();
	}

	void GameLayer::OnUserEvent(SDL_UserEvent& event)
	{
		switch (event.code)
		{
		case EVENT_ON_EXIT:
		{
			DetachCharacterControllers();
		} break;
		case EVENT_BUTTON_NEWROUND:
		{
			m_gameWon = false; 
			m_inLobby = false;
			m_inMainMenu = false;
			m_endGameCalled = false;

			for (auto controller : m_characterControllers)
			{
				controller->CachePlayerIDForReload();
				controller->DisableInput();
			}

			RunOutro();

			if (Cauda::Application::IsGamePaused())
			{
				Cauda::Application::PauseGame(false);
			}

			m_startingRound = true;

			flecs::entity timerEntity = m_world.entity();
			Timer::SetDelay(timerEntity, 6.f, [&]()
				{
					SDL_PushEvent(&GameEvents::g_LoadRoundEvent);
				});

			if (auto audioModule = m_world.try_get_mut<AudioModule>())
			{
				if (audioModule->TryStopAudioSource(m_lobbyBGMHandle)) m_lobbyBGMHandle = -1;

				if (!audioModule->IsValidHandle(m_roundBGMHandle))
				{
					if (auto asset = ResourceLibrary::GetAudioAsset("SW_LevelMusic"))
					{
						asset->settings.loop = true;
						m_roundBGMHandle = audioModule->PlaySoundStreamBackground(*reinterpret_cast<SoundStream*>(asset));
						asset->settings.loop = false;
					}
				}
			}

		} break;
		case EVENT_POST_RELOAD: //Post Reload
		{
			for (auto controller : m_characterControllers)
			{
				controller->TryReposessCachedPlayerID();
			}

			if (m_inLobby)
			{
				RunIntro();
			}

			if (m_startingRound)
			{
				SDL_PushEvent(&GameEvents::g_BeginRoundEvent);
			}
		} break;
		case EVENT_LOAD_LOBBY:
		{
			m_inLobby = true;
			if (Cauda::Application::IsGamePaused())
			{
				Cauda::Application::PauseGame(false);
			}

			for (auto controller : m_characterControllers)
			{
				controller->ResetCachedID();
			}

			RunIntro();

			if (auto audioModule = m_world.try_get_mut<AudioModule>())
			{
				if (audioModule->TryStopAudioSource(m_roundBGMHandle)) m_roundBGMHandle = -1;
				
				if (!audioModule->IsValidHandle(m_lobbyBGMHandle))
				{
					if (auto asset = ResourceLibrary::GetAudioAsset("SW_MainMenu"))
					{
						asset->settings.loop = true;
						m_lobbyBGMHandle = audioModule->PlaySoundStreamBackground(*reinterpret_cast<SoundStream*>(asset));
						asset->settings.loop = false;
					}
				}
			}

			m_endGameCalled = false;
			m_gameWon = false;
			m_inMainMenu = false;
			m_startingRound = false;


		} break;
		case EVENT_PLAYER_READIED:
		{
			if (m_startingRound) break;

			for (auto controller : m_characterControllers)
			{
				if (controller->IsPossessing())
				{
					auto player = controller->GetPosessedPawn();
					auto character = player.try_get_mut<CharacterComponent>();
					if (controller->IsPlayerReadied())
					{
						m_playersReadied[character->characterID - 1] = true;
					}
					else
					{
						m_playersReadied[character->characterID - 1] = false;
					}
				}
			}

			int readiedPlayers = m_playersReadied[0] + m_playersReadied[1] + m_playersReadied[2] + m_playersReadied[3];

			if (readiedPlayers > 1 && readiedPlayers == m_possessedPlayers.size())
			{
				m_startingRound = true;
				m_lobbyTimerTime = 5.f;
			}

		} break;
		case EVENT_PLAYER_UNREADIED:
		{
			for (auto controller : m_characterControllers)
			{
				if (controller->IsPossessing())
				{
					auto player = controller->GetPosessedPawn();
					auto character = player.try_get_mut<CharacterComponent>();
					if (controller->IsPlayerReadied())
					{
						m_playersReadied[character->characterID - 1] = true;
					}
					else
					{
						m_playersReadied[character->characterID - 1] = false;
					}
				}
			}

			if (m_startingRound)
			{
				m_lobbyTimerTime = 5.f;
				m_startingRound = false;
			}

		} break;
		case EVENT_ALL_PLAYERS_READY:
		{
			for (auto controller : m_characterControllers)
			{
				controller->CachePlayerIDForReload();

				controller->DisableInput();
			}

			RunOutro();

			flecs::entity timerEntity = m_world.entity();
			Timer::SetDelay(timerEntity, 6.f, [&]()
				{
					SDL_PushEvent(&GameEvents::g_LoadRoundEvent);
				});

		} break;
		case EVENT_LOAD_ROUND:
		{
			m_playersReadied[0] = false;
			m_playersReadied[1] = false;
			m_playersReadied[2] = false;
			m_playersReadied[3] = false;

			Cauda::Application::Get().BeginSceneLoadDuringPlay(m_gameScene);
		} break;
		case EVENT_BEGIN_ROUND:
		{
			m_inLobby = false;
			m_startingRound = false;
			CleanupUnposessedPlayers();

			m_playersReadied[0] = false;
			m_playersReadied[1] = false;
			m_playersReadied[2] = false;
			m_playersReadied[3] = false;

			for (auto controller : m_characterControllers)
			{
				controller->ResetReadyState();
			}

			for (auto controller : m_characterControllers)
			{
				controller->DisableInput();
			}

			auto query = m_world.query<IntroCutscene>();
			auto lerpEntity = query.first();
			if (lerpEntity)
			{
				auto lerpScript = lerpEntity.try_get_mut<IntroCutscene>();

				lerpScript->cutsceneDuration = 5.f;

				lerpScript->TriggerIntro(lerpEntity);
			}

			if (auto audioModule = m_world.try_get_mut<AudioModule>())
			{
				if (audioModule->TryStopAudioSource(m_lobbyBGMHandle)) m_lobbyBGMHandle = -1;

				if (!audioModule->IsValidHandle(m_roundBGMHandle))
				{
					if (auto asset = ResourceLibrary::GetAudioAsset("SW_LevelMusic"))
					{
						asset->settings.loop = true;
						m_roundBGMHandle = audioModule->PlaySoundStreamBackground(*reinterpret_cast<SoundStream*>(asset));
						asset->settings.loop = false;
					}
				}
			}

			m_roundBeginDelay = 6.f;
		} break;
		case EVENT_END_ROUND:
		{
			m_gamemodeModule->EndRound();
		} break;
		case EVENT_BUTTON_APPSETTINGS:
		{
			if (m_endGameCalled || m_inMainMenu) break;
			TogglePause();
		} break;
		case EVENT_BUTTON_GAMESETTINGS:
		{
			ToggleGameSettingsMenu();
		} break;
		case EVENT_BUTTON_EXIT_D:
		{
			Application::Get().Close();
		} break;
		case EVENT_BUTTON_EXIT_M:
		{
			m_inMainMenu = true;
			m_inLobby = false;
			m_gameWon = false;
			m_endGameCalled = false;
			m_gamemodeModule->EndRound();
			Cauda::Application::Get().BeginSceneLoadDuringPlay(m_menuScene);
			
		} break;
		case EVENT_PLAYER_DEATH:
		{
			if (!m_gamemodeModule->IsRoundRunning())
			{
				m_world.query<CharacterComponent>()
					.each([&](flecs::entity entity, CharacterComponent& character)
						{
							if (auto stats = entity.try_get_mut<PlayerStatComponent>())
							{
								if (stats->numLives < 3 && stats->numLives > 0)
								{
									stats->numLives++; //Offset the decrease in lives
								}
							}
						});
				break;
			} 

			int livingPlayers = 0;

			for (auto controller : m_characterControllers)
			{
				if (controller->IsPossessing())
				{
					auto stats = controller->GetPosessedPawn().try_get_mut<PlayerStatComponent>();
					if (stats->numLives > 0)
					{
						livingPlayers++;
					}
				}
			}

			if (livingPlayers == 1)
			{
				m_gameWon = true;
				endGameScreenUI.OnGameEnd(m_characterControllers);
				SDL_PushEvent(&GameEvents::g_EndRoundEvent);
				Cauda::Application::PauseGame(true);
			}
			else
			{
				m_gameWon = false;
			}
		} break;
		}
	}

	void GameLayer::OnKeyPress(PKey key)
	{
		switch (key)
		{
#ifndef _DISTRIB
		case PKey::Space:
		{
			if (!m_characterControllers[0]->IsPossessing())
			{
				m_characterControllers[0]->PossessFirstAvailable();
			}
		} break;
		case PKey::LCtrl:
		{
			m_characterControllers[0]->DetachFromPawn();
		} break;
		case PKey::F1:
		{
			m_showDebugPanel = !m_showDebugPanel;
		} break;
		case PKey::F2:
		{
			m_showGameOverlay = !m_showGameOverlay;
		} break;
#endif // !_DISTRIB
		case PKey::Escape:
		{
			if ((m_gamemodeModule->IsRoundRunning() || m_inLobby) && !m_gameWon && !m_startingRound)
			{
				TogglePause();
			}
		} break;
		}
	}

	void GameLayer::OnButtonDown(Uint8 button)
	{
		switch (button)
		{
			case SDL_GAMEPAD_BUTTON_START:
			{
				if ((m_gamemodeModule->IsRoundRunning() || m_inLobby) && !m_gameWon && !m_startingRound)
				{
					TogglePause();
				}
			} break;
		}
	}

	void GameLayer::OverrideImGuiStyle()
	{
		using namespace Colour;

		ImGui::GetStyle().Alpha = 1.0f;

		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colours = style.Colors;

		ImGuiIO& io = ImGui::GetIO();

		style.WindowRounding = 4.0f;
		style.ChildRounding = 4.0f;
		style.FrameRounding = 4.0f;
		style.PopupRounding = 4.0f;
		style.ScrollbarRounding = 4.0f;
		style.GrabRounding = 4.0f;
		style.TabRounding = 4.0f;

		//style.AntiAliasedLines = false;
		//style.AntiAliasedFill = false;
		//style.AntiAliasedLinesUseTex = false;

		style.WindowPadding = ImVec2(4.0f, 4.0f);
		style.FramePadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(8.0f, 6.0f);
		style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
		style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
		style.IndentSpacing = 16.0f;
		style.ScrollbarSize = 12.0f;
		style.GrabMinSize = 8.0f;

		style.WindowBorderSize = 0.f;
		style.ChildBorderSize = 0.f;
		style.PopupBorderSize = 0.f;
		style.FrameBorderSize = 0.f;
		style.TabBorderSize = 1.5f;

		// Background colours
		colours[ImGuiCol_WindowBg] = LeadDark;
		colours[ImGuiCol_PopupBg] = LeadDark;
		colours[ImGuiCol_ChildBg] = ImVec4(LeadMid.x, LeadMid.y, LeadMid.z, 0.50f);

		// Frame colours
		colours[ImGuiCol_FrameBg] = ImVec4(0.1,0.1,0.1,0.1);
		colours[ImGuiCol_FrameBgHovered] = LeadLight;
		colours[ImGuiCol_FrameBgActive] = Brighten(ImVec4(HumbleGreen.x, HumbleGreen.y, HumbleGreen.z, 0.70f));

		// Header colours
		colours[ImGuiCol_Header] = LeadLight;
		colours[ImGuiCol_HeaderHovered] = Brighten(HumbleBlue);
		colours[ImGuiCol_HeaderActive] = Brighten(HumbleRed);

		// Tab colours
		colours[ImGuiCol_Tab] = LeadMid;
		colours[ImGuiCol_TabHovered] = LeadLight;
		colours[ImGuiCol_TabActive] = Brighten(ImVec4(HumbleBlue.x, HumbleBlue.y, HumbleBlue.z, 0.80f));
		colours[ImGuiCol_TabUnfocused] = LeadDark;
		colours[ImGuiCol_TabUnfocusedActive] = LeadMid;

		// Title colours
		colours[ImGuiCol_TitleBg] = LeadDarkest;
		colours[ImGuiCol_TitleBgActive] = LeadDark;
		colours[ImGuiCol_TitleBgCollapsed] = ImVec4(LeadDarkest.x, LeadDarkest.y, LeadDarkest.z, 0.60f);
		colours[ImGuiCol_MenuBarBg] = LeadDarkest;

		// Text colours
		colours[ImGuiCol_Text] = TextWhite;
		colours[ImGuiCol_TextDisabled] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
		colours[ImGuiCol_TextSelectedBg] = Brighten(ImVec4(HumbleBlue.x, HumbleBlue.y, HumbleBlue.z, 0.35f));

		// Button colours
		colours[ImGuiCol_Button] = LeadMid;
		colours[ImGuiCol_ButtonHovered] = LeadLight;
		colours[ImGuiCol_ButtonActive] = Brighten(HumbleRed);

		// Widget colours
		colours[ImGuiCol_CheckMark] = Brighten(HumbleGreen);
		colours[ImGuiCol_SliderGrab] = Brighten(HumbleBlue);
		colours[ImGuiCol_SliderGrabActive] = Brighten(HumbleRed);

		// Scrollbar colours
		colours[ImGuiCol_ScrollbarBg] = LeadDarkest;
		colours[ImGuiCol_ScrollbarGrab] = LeadLight;
		colours[ImGuiCol_ScrollbarGrabHovered] = Brighten(HumbleBlue);
		colours[ImGuiCol_ScrollbarGrabActive] = Brighten(HumbleGreen);

		// Border and separator colours
		colours[ImGuiCol_Border] = Brighten(ImVec4(HumbleBlue.x, HumbleBlue.y, HumbleBlue.z, 0.50f));
		colours[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

		colours[ImGuiCol_Separator] = LeadDarkest;
		colours[ImGuiCol_SeparatorHovered] = Brighten(HumbleBlue);
		colours[ImGuiCol_SeparatorActive] = Brighten(HumbleRed);

		// Resize grip colours
		colours[ImGuiCol_ResizeGrip] = Brighten(ImVec4(HumbleGreen.x, HumbleGreen.y, HumbleGreen.z, 0.25f));
		colours[ImGuiCol_ResizeGripHovered] = Brighten(ImVec4(HumbleBlue.x, HumbleBlue.y, HumbleBlue.z, 0.67f));
		colours[ImGuiCol_ResizeGripActive] = Brighten(HumbleRed);

		// Plot colours
		colours[ImGuiCol_PlotLines] = Brighten(HumbleGreen);
		colours[ImGuiCol_PlotLinesHovered] = Brighten(HumbleBlue);
		colours[ImGuiCol_PlotHistogram] = Brighten(HumbleRed);
		colours[ImGuiCol_PlotHistogramHovered] = Brighten(HumbleBlue);

		// Table colours
		colours[ImGuiCol_TableHeaderBg] = LeadMid;
		colours[ImGuiCol_TableBorderStrong] = ImVec4(LeadLight.x, LeadLight.y, LeadLight.z, 1.00f);
		colours[ImGuiCol_TableBorderLight] = ImVec4(LeadMid.x, LeadMid.y, LeadMid.z, 1.00f);
		colours[ImGuiCol_TableRowBg] = ImVec4(LeadDark.x, LeadDark.y, LeadDark.z, 0.00f);
		colours[ImGuiCol_TableRowBgAlt] = ImVec4(LeadLight.x, LeadLight.y, LeadLight.z, 0.12f);

		// Miscellaneous
		colours[ImGuiCol_DragDropTarget] = Brighten(HumbleGreen);
		colours[ImGuiCol_NavHighlight] = Brighten(HumbleBlue);
		colours[ImGuiCol_NavWindowingHighlight] = Brighten(ImVec4(HumbleRed.x, HumbleRed.y, HumbleRed.z, 0.70f));
		colours[ImGuiCol_NavWindowingDimBg] = ImVec4(LeadLight.x, LeadLight.y, LeadLight.z, 0.20f);

		colours[ImGuiCol_ModalWindowDimBg] = ImVec4(LeadDarkest.x, LeadDarkest.y, LeadDarkest.z, 0.60f);

	}

	void GameLayer::RestoreImGuiStyle()
	{
		using namespace Colour;

		ImGui::GetStyle().Alpha = 1.0f;

		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colours = style.Colors;

		ImGuiIO& io = ImGui::GetIO();


		io.FontGlobalScale = 1.0f;

		style.WindowRounding = 4.0f;
		style.ChildRounding = 4.0f;
		style.FrameRounding = 4.0f;
		style.PopupRounding = 4.0f;
		style.ScrollbarRounding = 4.0f;
		style.GrabRounding = 4.0f;
		style.TabRounding = 4.0f;

		//style.AntiAliasedLines = false;
		//style.AntiAliasedFill = false;
		//style.AntiAliasedLinesUseTex = false;

		style.WindowPadding = ImVec2(4.0f, 4.0f);
		style.FramePadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(8.0f, 6.0f);
		style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
		style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
		style.IndentSpacing = 16.0f;
		style.ScrollbarSize = 12.0f;
		style.GrabMinSize = 8.0f;

		style.WindowBorderSize = 1.5f;
		style.ChildBorderSize = 1.5f;
		style.PopupBorderSize = 1.5f;
		style.FrameBorderSize = 1.5f;
		style.TabBorderSize = 1.5f;


		// Background colours
		colours[ImGuiCol_WindowBg] = LeadDark;
		colours[ImGuiCol_PopupBg] = LeadDark;
		colours[ImGuiCol_ChildBg] = ImVec4(LeadMid.x, LeadMid.y, LeadMid.z, 0.50f);

		// Frame colours
		colours[ImGuiCol_FrameBg] = LeadMid;
		colours[ImGuiCol_FrameBgHovered] = LeadLight;
		colours[ImGuiCol_FrameBgActive] = Brighten(ImVec4(HumbleGreen.x, HumbleGreen.y, HumbleGreen.z, 0.70f));

		// Header colours
		colours[ImGuiCol_Header] = LeadLight;
		colours[ImGuiCol_HeaderHovered] = Brighten(HumbleBlue);
		colours[ImGuiCol_HeaderActive] = Brighten(HumbleRed);

		// Tab colours
		colours[ImGuiCol_Tab] = LeadMid;
		colours[ImGuiCol_TabHovered] = LeadLight;
		colours[ImGuiCol_TabActive] = Brighten(ImVec4(HumbleBlue.x, HumbleBlue.y, HumbleBlue.z, 0.80f));
		colours[ImGuiCol_TabUnfocused] = LeadDark;
		colours[ImGuiCol_TabUnfocusedActive] = LeadMid;

		// Title colours
		colours[ImGuiCol_TitleBg] = LeadDarkest;
		colours[ImGuiCol_TitleBgActive] = LeadDark;
		colours[ImGuiCol_TitleBgCollapsed] = ImVec4(LeadDarkest.x, LeadDarkest.y, LeadDarkest.z, 0.60f);
		colours[ImGuiCol_MenuBarBg] = LeadDarkest;

		// Text colours
		colours[ImGuiCol_Text] = TextWhite;
		colours[ImGuiCol_TextDisabled] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
		colours[ImGuiCol_TextSelectedBg] = Brighten(ImVec4(HumbleBlue.x, HumbleBlue.y, HumbleBlue.z, 0.35f));

		// Button colours
		colours[ImGuiCol_Button] = LeadMid;
		colours[ImGuiCol_ButtonHovered] = LeadLight;
		colours[ImGuiCol_ButtonActive] = Brighten(HumbleRed);

		// Widget colours
		colours[ImGuiCol_CheckMark] = Brighten(HumbleGreen);
		colours[ImGuiCol_SliderGrab] = Brighten(HumbleBlue);
		colours[ImGuiCol_SliderGrabActive] = Brighten(HumbleRed);

		// Scrollbar colours
		colours[ImGuiCol_ScrollbarBg] = LeadDarkest;
		colours[ImGuiCol_ScrollbarGrab] = LeadLight;
		colours[ImGuiCol_ScrollbarGrabHovered] = Brighten(HumbleBlue);
		colours[ImGuiCol_ScrollbarGrabActive] = Brighten(HumbleGreen);

		// Border and separator colours
		colours[ImGuiCol_Border] = Brighten(ImVec4(HumbleBlue.x, HumbleBlue.y, HumbleBlue.z, 0.50f));
		colours[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

		colours[ImGuiCol_Separator] = Brighten(ImVec4(HumbleGreen.x, HumbleGreen.y, HumbleGreen.z, 0.50f));
		colours[ImGuiCol_SeparatorHovered] = Brighten(HumbleBlue);
		colours[ImGuiCol_SeparatorActive] = Brighten(HumbleRed);

		// Resize grip colours
		colours[ImGuiCol_ResizeGrip] = Brighten(ImVec4(HumbleGreen.x, HumbleGreen.y, HumbleGreen.z, 0.25f));
		colours[ImGuiCol_ResizeGripHovered] = Brighten(ImVec4(HumbleBlue.x, HumbleBlue.y, HumbleBlue.z, 0.67f));
		colours[ImGuiCol_ResizeGripActive] = Brighten(HumbleRed);

		// Plot colours
		colours[ImGuiCol_PlotLines] = Brighten(HumbleGreen);
		colours[ImGuiCol_PlotLinesHovered] = Brighten(HumbleBlue);
		colours[ImGuiCol_PlotHistogram] = Brighten(HumbleRed);
		colours[ImGuiCol_PlotHistogramHovered] = Brighten(HumbleBlue);

		// Table colours
		colours[ImGuiCol_TableHeaderBg] = LeadMid;
		colours[ImGuiCol_TableBorderStrong] = ImVec4(LeadLight.x, LeadLight.y, LeadLight.z, 1.00f);
		colours[ImGuiCol_TableBorderLight] = ImVec4(LeadMid.x, LeadMid.y, LeadMid.z, 1.00f);
		colours[ImGuiCol_TableRowBg] = ImVec4(LeadDark.x, LeadDark.y, LeadDark.z, 0.00f);
		colours[ImGuiCol_TableRowBgAlt] = ImVec4(LeadLight.x, LeadLight.y, LeadLight.z, 0.12f);

		// Miscellaneous
		colours[ImGuiCol_DragDropTarget] = Brighten(HumbleGreen);
		colours[ImGuiCol_NavHighlight] = Brighten(HumbleBlue);
		colours[ImGuiCol_NavWindowingHighlight] = Brighten(ImVec4(HumbleRed.x, HumbleRed.y, HumbleRed.z, 0.70f));
		colours[ImGuiCol_NavWindowingDimBg] = ImVec4(LeadLight.x, LeadLight.y, LeadLight.z, 0.20f);

		colours[ImGuiCol_ModalWindowDimBg] = ImVec4(LeadDarkest.x, LeadDarkest.y, LeadDarkest.z, 0.60f);

	}

	void GameLayer::CleanupUnposessedPlayers()
	{
		if (m_possessedPlayers.size() == 4) return; //all players accounted for

		bool possessedPlayerIDs[4] = { 0 };

		for (auto controller : m_characterControllers)
		{
			if (!controller->IsPossessing()) continue;
			auto player = controller->GetPosessedPawn();
			if (auto character = player.try_get_mut<CharacterComponent>())
			{
				possessedPlayerIDs[character->characterID - 1] = true;
			}
		}

		auto lobby = m_world.query<SpawnLobby>().first();

		if (!lobby) return;

		auto lobbyTransform = lobby.try_get_mut<TransformComponent>();

		m_world.query<CharacterComponent, TransformComponent>()
			.each([&](flecs::entity entity, CharacterComponent& character, TransformComponent& transform)
				{
					//Check if player is possessed
					if (possessedPlayerIDs[character.characterID - 1]) return;

					m_characterModule->RespawnPlayerAtPosition(entity, transform, lobbyTransform->position);

					if (auto xRay = entity.try_get_mut<XrayComponent>())
					{
						xRay->enabled = false;
					}
				});

	}

	void GameLayer::DrawGameUI()
	{
		if (Cauda::Application::IsGamePaused() && !Cauda::Application::IsEditorRunning())
		{
			auto gamemode = m_world.get<GamemodeSingleton>();
			if (gamemode.gametime >= gamemode.timeLimit || m_gameWon)
			{
				if (!m_endGameCalled)
				{
					m_endGameCalled = true;
					endGameScreenUI.OnGameEnd(m_characterControllers);
				}
				endGameScreenUI.Open(m_world);
				endGameScreenUI.Draw(m_world);
				endGameScreenUI.Close();
				return;
			}
			else
			{
				if (!m_inMainMenu)
				{
					RenderPauseMenu();
				}
			}
		}

		if (m_inMainMenu) RenderMainMenu();

		if (m_showGameMenu) RenderGameMenu();

		if (m_showGameOverlay)
		{
			if (Fonts::Custom)
				ImGui::PushFont(Fonts::Custom);
			ImDrawList* fgDrawList = ImGui::GetForegroundDrawList();

			if (m_startingRound && m_lobbyTimerTime > 0)
			{
				RenderPlayersReadiedTimer();
				ImGui::PopFont();
				return;
			}

			if (!m_world.try_get_mut<EditorModule>()->IsPlayingInEditor() && !m_gameWon && !m_startingRound)
			{
				RenderPlayerUICorners();
			}

			if (m_inLobby && !m_startingRound)
			{
				RenderLobbyUI();
			}

			if (m_gamemodeModule->IsRoundRunning())
			{
				RenderGameTimer();
			}

			if (Fonts::Custom) ImGui::PopFont();

			//RenderDebugPanel();
		}
	}

	void GameLayer::CachePlayerEntities()
	{
		m_playerEntities.clear();

		// Find all entities with CharacterComponent and InputComponent
		auto query = m_world.query<CharacterComponent, InputComponent>();
		query.each([this](flecs::entity entity, CharacterComponent& character, InputComponent& input)
			{
				m_playerEntities.push_back(entity);
			});

		// Sort by character ID for consistent ordering
		std::sort(m_playerEntities.begin(), m_playerEntities.end(),
			[](flecs::entity a, flecs::entity b)
			{
				auto charA = a.try_get<CharacterComponent>();
				auto charB = b.try_get<CharacterComponent>();
				if (!charA || !charB) return false;
				return charA->characterID < charB->characterID;
			});
	}

	void GameLayer::RenderPlayerUICorners()
	{
		auto viewport = ImGui::GetMainViewport();
		ImVec2 viewportSize = viewport->Size;

		const float designWidth = 640.0f;
		const float designHeight = 360.0f;
		float uiScale = ImGui::GetIO().DisplaySize.y / designHeight;

		const float atlasWidth = 512.0f;
		const float atlasHeight = 512.0f;
		const ImVec2 baseUISize(200.0f, 96.0f);
		ImVec2 playerUISize(baseUISize.x * uiScale, baseUISize.y * uiScale);

		const float baseMargin = 2.0f;
		float margin = baseMargin * uiScale;

		UI::Anchor corners[4] =
		{
			UI::Anchor::TopLeft,
			UI::Anchor::TopRight,
			UI::Anchor::BottomLeft,
			UI::Anchor::BottomRight
		};

		ImVec2 offsets[4] =
		{
			ImVec2(0.0f, 0.0f),
			ImVec2(-margin, 0.0f),
			ImVec2(0.0f, -margin),
			ImVec2(-margin, -margin)
		};

		for (int i = 0; i < 4; ++i)
		{
			char overlayName[32];
			sprintf_s(overlayName, "Player%dOverlay", i + 1);
			flecs::entity playerEntity;
			if (i < m_playerEntities.size() && m_playerEntities[i].is_alive())
			{
				playerEntity = m_playerEntities[i];
			}
			bool isPlayerActive = playerEntity.is_alive();
			bool isPlayerConnected = IsPlayerConnected(i);

			ImGui::SetNextWindowBgAlpha(0.0f);

			if (UI::BeginOverlay(overlayName, corners[i], playerUISize, offsets[i]))
			{
				if (!isPlayerActive || !isPlayerConnected)
				{
					if (m_inLobby)
					{
						UI::SpriteRect cardSpriteRect(244, 146, 92.f, 35.0f);
						ImVec2 cardSize = ImVec2(cardSpriteRect.width * uiScale, cardSpriteRect.height * uiScale);

						static Texture* menuUIAtlas = nullptr;
						if (!menuUIAtlas)
						{
							menuUIAtlas = ResourceLibrary::GetTexture("menu_ui_512");
							if (!menuUIAtlas)
							{
								menuUIAtlas = ResourceLibrary::LoadTexture("menu_ui_512", "textures/menu_ui_512.png");
							}
						}

						ImVec2 textPos = ImGui::GetCursorScreenPos();

						textPos.x += playerUISize.x * 0.5;
						textPos.y += playerUISize.y * 0.5;

						ImVec2 cardPos = ImVec2(textPos.x - (cardSize.x * 0.5), textPos.y - (cardSize.y * 0.5));

						UI::DrawImageAtlasRect(
							(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
							cardPos,
							cardSize,
							cardSpriteRect,
							atlasWidth,
							atlasHeight,
							ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

						UI::TextCentred("Press any button to join", textPos);

						ImGui::PopStyleColor(1);
					}
				}
				else
				{
					RenderPlayerUI(i, playerEntity);
				}
			}
			UI::EndOverlay();
		}
	}

	void GameLayer::RenderPlayerUI(int playerIndex, flecs::entity playerEntity)
	{
		bool isPlayerActive = playerEntity.is_alive();
		bool isPlayerConnected = IsPlayerConnected(playerIndex);

		//if (!isPlayerActive || !isPlayerConnected) return;

		ImVec2 startPos = ImGui::GetCursorScreenPos();
		ImVec2 windowSize = ImGui::GetCurrentWindow()->Size;

		const float designHeight = 360.0f;
		float uiScale = ImGui::GetIO().DisplaySize.y / designHeight;

		bool isRightAligned = (playerIndex == 1 || playerIndex == 3);

		float currentStamina = 100.0f;
		float maxStamina = 100.0f;
		float currentHealth = 100.0f;
		float maxHealth = 100.0f;

		auto character = playerEntity.try_get<CharacterComponent>();
		if (character)
		{
			currentStamina = character->currentStamina;
			maxStamina = character->maxStamina;
		}

		auto health = playerEntity.try_get<Health>();
		if (health)
		{
			currentHealth = health->currentHealth;
			maxHealth = health->maxHealth;
		}

		ImVec4 playerOutlineColours[4] = {
			ImVec4(0.30196f, 0.52549f, 0.91373f, 1.0f),
			ImVec4(0.90980f, 0.50196f, 0.18824f, 1.0f),
			ImVec4(0.27843f, 0.82745f, 0.42745f, 1.0f),
			ImVec4(0.73725f, 0.44706f, 1.0f, 1.0f)
		};

		static Texture* pixelUIAtlas = nullptr;
		if (!pixelUIAtlas)
		{
			pixelUIAtlas = ResourceLibrary::GetTexture("pixel_ui_512");
			if (!pixelUIAtlas)
			{
				pixelUIAtlas = ResourceLibrary::LoadTexture("pixel_ui_512", "textures/pixel_ui_512.png");
			}
		}
		static Texture* menuUIAtlas = nullptr;
		if (!menuUIAtlas)
		{
			menuUIAtlas = ResourceLibrary::GetTexture("menu_ui_512");
			if (!menuUIAtlas)
			{
				menuUIAtlas = ResourceLibrary::LoadTexture("menu_ui_512", "textures/menu_ui_512.png");
			}
		}

		const int atlasColumns = 32;
		const int atlasRows = 32;

		const int playerSpriteIndices[4] = { 0, 1, 2, 3 };
		const int playerOutlineIndices[4] = { 8, 9, 10, 11 };
		const int statOutlineIndices[4] = { 16, 17, 18, 19 };
		const int heartIconIndex = 22;
		const int dashIconIndex = 54;
		const int skullIconIndex = 86;

		const int heartIconIndexRight = 23;
		const int dashIconIndexRight = 55;
		const int skullIconIndexRight = 87;

		const float rawSpriteSize = 64.0f;
		const float rawSpacing = 1.f;
		const float rawHeartSize = 16.0f;
		const float rawDashSize = 16.0f;
		const float rawSkullSize = 16.0f;

		UI::SpriteRect playerSpriteRect(304 + (playerIndex * 50), playerIndex == 0 || playerIndex == 2 ? 304 : 304 + 52, 50, 52);
		ImVec2 spriteSize(playerSpriteRect.width * uiScale, playerSpriteRect.height * uiScale);

		UI::SpriteRect statSpriteRect(1 + (playerIndex * 62), 130, 62, 44);
		ImVec2 statSize(statSpriteRect.width * uiScale, statSpriteRect.height * uiScale);

		float spacing = rawSpacing * uiScale;

		if (pixelUIAtlas && pixelUIAtlas->GetHandle() != 0 && menuUIAtlas && menuUIAtlas->GetHandle() != 0)
		{
			int spriteIndex = playerSpriteIndices[playerIndex];
			int outlineIndex = playerOutlineIndices[playerIndex];
			int statIndex = statOutlineIndices[playerIndex];

			bool isBottomRow = (playerIndex == 2 || playerIndex == 3);

			ImVec2 spritePos;
			if (isBottomRow)
			{
				if (isRightAligned)
				{
					spritePos = UI::GetAnchorPosition(UI::Anchor::BottomRight,
						ImVec2(-(spriteSize.x + spacing), -(spriteSize.y + spacing)));
				}
				else
				{
					spritePos = UI::GetAnchorPosition(UI::Anchor::BottomLeft,
						ImVec2(spacing, -(spriteSize.y + spacing)));
				}
			}
			else
			{
				if (isRightAligned)
				{
					spritePos = UI::GetAnchorPosition(UI::Anchor::TopRight,
						ImVec2(-(spriteSize.x + spacing), spacing));
				}
				else
				{
					spritePos = UI::GetAnchorPosition(UI::Anchor::TopLeft,
						ImVec2(spacing, spacing));
				}
			}

			ImVec2 statPos;
			if (isBottomRow)
			{
				if (isRightAligned)
				{
					statPos = UI::GetAnchorPosition(UI::Anchor::BottomRight,
						ImVec2(-(spriteSize.x + statSize.x + (spacing * 3)), -spriteSize.y ));
				}
				else
				{
					statPos = UI::GetAnchorPosition(UI::Anchor::BottomLeft,
						ImVec2(spriteSize.x + (spacing * 2), -(spriteSize.y + spacing)));
				}
			}
			else
			{
				if (isRightAligned)
				{
					statPos = UI::GetAnchorPosition(UI::Anchor::TopRight,
						ImVec2(-(spriteSize.x + statSize.x + (spacing * 3)), spacing * 2 ));
				}
				else
				{
					statPos = UI::GetAnchorPosition(UI::Anchor::TopLeft,
						ImVec2(spriteSize.x + (spacing * 2), spacing));
				}
			}

			UI::DrawImageAtlasRect(
				(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
				spritePos,
				spriteSize,
				playerSpriteRect,
				512,
				512,
				ImVec4(1, 1, 1, 1));

			Cauda::UI::DrawImageAtlasRect(
				(ImTextureID)(uintptr_t)pixelUIAtlas->GetHandle(),
				statPos,
				statSize,
				statSpriteRect,
				512,
				512,
				playerOutlineColours[playerIndex]
			);


			ImVec2 heartSize = ImVec2(rawHeartSize * uiScale, rawHeartSize * uiScale);
			float heartSpacing = 0.0f;// 2.0f * uiScale;
			const int heartsPerRow = 3;
			float totalHeartsWidth = heartsPerRow * heartSize.x + (heartsPerRow - 1) * heartSpacing;

			ImVec2 heartStartPos;
			if (isBottomRow)
			{
				if (isRightAligned)
				{
					heartStartPos = ImVec2(spritePos.x - totalHeartsWidth - (spacing * 6), spritePos.y - (spacing * 4) + (statSize.y - (heartSize.y * 2)));
				}
				else
				{
					heartStartPos = ImVec2(spritePos.x + spriteSize.x + (spacing * 6), spritePos.y - (spacing * 4) + (statSize.y - (heartSize.y * 2)));
				}
			}
			else
			{
				if (isRightAligned)
				{
					heartStartPos = ImVec2(spritePos.x - totalHeartsWidth - (spacing * 8), spacing * 6);
				}
				else
				{
					heartStartPos = ImVec2(spritePos.x + spriteSize.x + (spacing * 8), spacing * 6);
				}
			}

			int maxHearts = (int)(maxHealth / 10.0f);
			int fullHearts = (int)(currentHealth / 10.0f);
			float remainder = fmod(currentHealth, 10.0f);
			bool hasHalfHeart = remainder >= 5.0f;

			for (int i = 0; i < maxHearts; i++)
			{
				int row = i / heartsPerRow;
				int col = i % heartsPerRow;

				ImVec2 heartPos = ImVec2(
					heartStartPos.x + col * (heartSize.x + heartSpacing),
					heartStartPos.y + row * (heartSize.y + heartSpacing)
				);

				int heartIndex = isRightAligned ? heartIconIndexRight : heartIconIndex;
				ImVec4 heartTint;

				if (i < fullHearts)
				{
					heartTint = Colour::WithAlpha(Colour::HumbleRed, 1.0f);
				}
				else if (i == fullHearts && hasHalfHeart)
				{
					heartTint = Colour::WithAlpha(Colour::HumbleRed, 0.7f);
				}
				else
				{
					heartTint = Colour::WithAlpha(Colour::HumbleRed, 0.2f);
				}

				Cauda::UI::DrawImageAtlasIndex(
					(ImTextureID)(uintptr_t)pixelUIAtlas->GetHandle(),
					heartPos,
					heartSize,
					heartIndex,
					atlasColumns,
					atlasRows,
					heartTint
				);
			}

			float heartsHeight = ((maxHearts - 1) / heartsPerRow + 1) * (heartSize.y + heartSpacing);

			int availableDashes = 0;
			int maxDashes = 3;
			if (auto* dash = playerEntity.try_get<Dash>())
			{
				availableDashes = dash->GetAvailableCharges();
				maxDashes = dash->maxDashCount;
			}

			ImVec2 dashIconSize = ImVec2(rawDashSize * uiScale, rawDashSize * uiScale);
			float dashIconSpacing = 0.0;// 2.0f * uiScale;
			float totalDashWidth = maxDashes * dashIconSize.x + (maxDashes - 1) * dashIconSpacing;

			ImVec2 dashStartPos;
			if (isRightAligned)
			{
				dashStartPos = ImVec2(heartStartPos.x + totalHeartsWidth - totalDashWidth,
					heartStartPos.y + heartsHeight);
			}
			else
			{
				dashStartPos = ImVec2(heartStartPos.x, heartStartPos.y + heartsHeight);
			}

			for (int i = 0; i < maxDashes; i++)
			{
				ImVec2 dashPos = ImVec2(
					dashStartPos.x + i * (dashIconSize.x + dashIconSpacing),
					dashStartPos.y
				);

				int dashIndex = isRightAligned ? dashIconIndexRight : dashIconIndex;
				ImVec4 dashTint = playerOutlineColours[playerIndex];
				if (i < availableDashes)
				{
					dashTint = Colour::WithAlpha(dashTint, 1.0f);
				}
				else
				{
					dashTint = Colour::WithAlpha(dashTint, 0.2f);
				}

				Cauda::UI::DrawImageAtlasIndex(
					(ImTextureID)(uintptr_t)pixelUIAtlas->GetHandle(),
					dashPos,
					dashIconSize,
					dashIndex,
					atlasColumns,
					atlasRows,
					dashTint
				);
			}

			int currentLives = 3;
			int maxLives = 3;
			if (auto* stats = playerEntity.try_get<PlayerStatComponent>())
			{
				currentLives = stats->numLives;
			}

			ImVec2 skullIconSize = ImVec2(rawSkullSize * uiScale, rawSkullSize * uiScale);
			float skullIconSpacing = 0.0f;
			float totalSkullWidth = maxLives * skullIconSize.x + (maxLives - 1) * skullIconSpacing;

			ImVec2 skullStartPos;
			if (isBottomRow)
			{
				if (isRightAligned)
				{
					skullStartPos = ImVec2(spritePos.x - spacing, spritePos.y - skullIconSize.y - spacing);
				}
				else
				{
					skullStartPos = ImVec2(spritePos.x + spacing, spritePos.y - skullIconSize.y - spacing);
				}
			}
			else
			{
				if (isRightAligned)
				{
					skullStartPos = ImVec2(spritePos.x - spacing, spritePos.y + spriteSize.y + spacing);
				}
				else
				{
					skullStartPos = ImVec2(spritePos.x + spacing, spritePos.y + spriteSize.y + spacing);
				}
			}

			for (int i = 0; i < maxLives; i++)
			{
				ImVec2 skullPos = ImVec2(
					skullStartPos.x + i * (skullIconSize.x + skullIconSpacing),
					skullStartPos.y
				);

				int skullIndex = isRightAligned ? skullIconIndexRight : skullIconIndex;
				ImVec4 skullTint;
				if (i < currentLives)
				{
					skullTint = Colour::WithAlpha(Colour::TextWhite, 1.0f);
				}
				else
				{
					skullTint = Colour::WithAlpha(Colour::TextWhite, 0.2f);
				}

				Cauda::UI::DrawImageAtlasIndex(
					(ImTextureID)(uintptr_t)pixelUIAtlas->GetHandle(),
					skullPos,
					skullIconSize,
					skullIndex,
					atlasColumns,
					atlasRows,
					skullTint
				);
			}
		}
	}

	void GameLayer::RenderGameTimer()
	{
		auto viewport = ImGui::GetMainViewport();
		ImVec2 viewportSize = viewport->Size;

		const float designWidth = 640.0f;
		const float designHeight = 360.0f;
		float uiScale = ImGui::GetIO().DisplaySize.y / designHeight;

		const float atlasWidth = 512.0f;
		const float atlasHeight = 512.0f;

		static Texture* menuUIAtlas = nullptr;
		if (!menuUIAtlas)
		{
			menuUIAtlas = ResourceLibrary::GetTexture("menu_ui_512");
			if (!menuUIAtlas)
			{
				menuUIAtlas = ResourceLibrary::LoadTexture("menu_ui_512", "textures/menu_ui_512.png");
			}
		}

		UI::SpriteRect gameTimerBGRect(316, 195, 46, 42);

		ImVec2 timerSize(46 * 2, 42 * 2);

		ImVec2 cardSpriteSize = ImVec2(gameTimerBGRect.width * uiScale, gameTimerBGRect.height * uiScale);

		if (UI::BeginOverlay("##game_timer", UI::Anchor::TopCentre, cardSpriteSize))
		{
			ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
			ImVec2 contentAvail = ImGui::GetContentRegionAvail();

			UI::DrawImageAtlasRect(
				(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
				ImVec2(cursor_pos.x - 8.f, cursor_pos.y - 6.f),
				cardSpriteSize,
				gameTimerBGRect,
				atlasWidth,
				atlasHeight,
				ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

			ImGui::SetWindowFontScale(1.33f);

			ImVec2 minutesCentre = ImVec2(cursor_pos.x + (contentAvail.x * 0.3f), cursor_pos.y + (contentAvail.y * 0.24));
			ImVec2 secondsCentre = ImVec2(cursor_pos.x + (contentAvail.x * 0.65f), cursor_pos.y + (contentAvail.y * 0.24));

			auto gamemode = m_world.get<GamemodeSingleton>();
			int timer = (int)std::max(gamemode.timeLimit - gamemode.gametime, 0.f);
			int mins = timer / 60;
			int secs = (timer - mins * 60);

			ImGui::PushStyleColor(ImGuiCol_Text, timer < 60.f ? ImVec4(1,0,0,1) : ImVec4(0, 0, 0, 1));

			char buffer[4];

			sprintf_s(buffer, "%i%i", mins / 10, mins % 10);

			UI::TextCentred(buffer, minutesCentre);

			sprintf_s(buffer, "%i%i", secs / 10, secs % 10);

			UI::TextCentred(buffer, secondsCentre);

			ImGui::PopStyleColor(1);
		}
		UI::EndOverlay();
	}


	void GameLayer::RenderCenterHUD()
	{

	}

	void GameLayer::RenderDebugPanel()
	{
		if (!m_showDebugPanel)
			return;

		if (UI::BeginFullscreenOverlay("DebugBackground", 0.3f))
		{
		}
		UI::EndFullscreenOverlay();

		if (ImGui::Begin("Game Debug", &m_showDebugPanel))
		{
			ImGui::PushStyleColor(ImGuiCol_Text, UI::ColourToU32(Colour::HumbleGreen));
			ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
			ImGui::PopStyleColor();

			ImGui::Separator();

			ImGui::PushStyleColor(ImGuiCol_Text, UI::ColourToU32(Colour::HumblePurple));
			ImGui::Text("Active Controllers:");
			ImGui::PopStyleColor();

			for (size_t i = 0; i < m_characterControllers.size(); ++i)
			{
				bool isActive = IsPlayerConnected(i);
				ImGui::PushStyleColor(ImGuiCol_Text,
				UI::ColourToU32(isActive ? Colour::HumbleGreen : Colour::HumbleRed));
				ImGui::Text("Player %zu: %s", i + 1, isActive ? "Active" : "Inactive");
				ImGui::PopStyleColor();
			}

			ImGui::Separator();
		}
		ImGui::End();
	}

	void GameLayer::RenderPauseMenu()
	{
		// Used for window permeation, imgui's bool* p_open
		bool paused = true;

		pauseMenuUI.Open(m_world);
		pauseMenuUI.Draw(&paused);
		pauseMenuUI.Close();

		if (!paused) UnpauseGame();
		//if (!m_showPauseMenu) Cauda::Application::Get().UnpauseUpdates();
	}

	void GameLayer::RenderGameMenu()
	{
		/* auto gamemode = m_world.get<GamemodeSingleton>();
		float timer = std::max(gamemode.timeLimit - gamemode.gametime, 0.f);

		gameMenuUI.Open();
		gameMenuUI.Draw(timer, m_selectedGameScene, &m_showGameMenu);
		gameMenuUI.Close();
		*/
	}

	void GameLayer::RenderMainMenu()
	{
		mainMenuUI.Open(m_world);
		mainMenuUI.Draw(m_world);
		mainMenuUI.Close(m_world);
	}

	void GameLayer::RenderPlayersReadiedTimer()
	{
		if (!m_startingRound) return;

		auto viewport = ImGui::GetMainViewport();
		ImVec2 viewportSize = viewport->Size;

		const float designWidth = 640.0f;
		const float designHeight = 360.0f;
		float uiScale = ImGui::GetIO().DisplaySize.y / designHeight;

		const float atlasWidth = 512.0f;
		const float atlasHeight = 512.0f;

		static Texture* menuUIAtlas = nullptr;
		if (!menuUIAtlas)
		{
			menuUIAtlas = ResourceLibrary::GetTexture("menu_ui_512");
			if (!menuUIAtlas)
			{
				menuUIAtlas = ResourceLibrary::LoadTexture("menu_ui_512", "textures/menu_ui_512.png");
			}
		}

		UI::SpriteRect buttonSpriteRect(244, 195, 66.0f, 35.0f);
		ImVec2 buttonSize = ImVec2(66.0f * uiScale, 35.0f * uiScale);

		if (UI::BeginFullscreenOverlay("##players_readied_timer"))
		{
			ImVec2 cursor_pos = UI::GetViewportCentre();

			ImVec2 buttonPos = ImVec2(cursor_pos.x - (buttonSize.x * 0.5), cursor_pos.y - (buttonSize.y * 0.5));

			UI::DrawImageAtlasRect(
				(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
				buttonPos,
				buttonSize,
				buttonSpriteRect,
				atlasWidth,
				atlasHeight,
				ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

			ImGui::SetWindowFontScale(2.f);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));

			std::string timerString = std::to_string((int)m_lobbyTimerTime);

			UI::TextCentred(timerString.c_str(), cursor_pos);

			ImGui::PopStyleColor(1);
		}
		UI::EndFullscreenOverlay();
	}

	void GameLayer::RenderLobbyUI()
	{
		if (m_startingRound) return;

		auto viewport = ImGui::GetMainViewport();
		ImVec2 viewportSize = viewport->Size;

		const float designWidth = 640.0f;
		const float designHeight = 360.0f;
		float uiScale = ImGui::GetIO().DisplaySize.y / designHeight;

		static int activeSettingsTab = 0;

		const float atlasWidth = 512.0f;
		const float atlasHeight = 512.0f;
		static Texture* menuUIAtlas = nullptr;

		UI::SpriteRect readyPlayersBGRect(375, 187, 133, 61);

		ImVec2 cardSpriteSize = ImVec2(readyPlayersBGRect.width * uiScale, readyPlayersBGRect.height * uiScale);

		ImVec2 overlaySize = ImVec2(cardSpriteSize.x, cardSpriteSize.y);

		ImVec2 readyPlayersBGPos = UI::GetAnchorPosition(UI::Anchor::TopCentre, ImVec2(-(cardSpriteSize.x * 0.5f), 16.f));

		if (!menuUIAtlas)
		{
			menuUIAtlas = ResourceLibrary::GetTexture("menu_ui_512");
			if (!menuUIAtlas)
			{
				menuUIAtlas = ResourceLibrary::LoadTexture("menu_ui_512", "textures/menu_ui_512.png");
			}
		}

		if (UI::BeginFullscreenOverlay("##fullscreen_ready_overlay"))
		{
			UI::DrawImageAtlasRect(
				(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
				readyPlayersBGPos,
				cardSpriteSize,
				readyPlayersBGRect,
				atlasWidth,
				atlasHeight,
				ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

			if (UI::BeginOverlay("##ready_players_overlay", UI::Anchor::TopCentre, overlaySize, ImVec2(0, 16)))
			{
				ImGui::SetWindowFontScale(1.6f);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

				ImVec2 cursorPos = ImGui::GetCursorScreenPos();
				ImVec2 contentAvail = ImGui::GetContentRegionAvail();

				int readyPlayers = m_playersReadied[0] + m_playersReadied[1] + m_playersReadied[2] + m_playersReadied[3];

				cursorPos.x += contentAvail.x * 0.45f + 8.f;
				cursorPos.y += contentAvail.y * 0.6f;

				char buffer[16];

				sprintf_s(buffer, "%i   %i",
					readyPlayers,
					m_possessedPlayers.size() >= 2 ? m_possessedPlayers.size() : 2);

				UI::TextCentred(buffer, cursorPos);
				ImGui::PopStyleColor();
			}
			UI::EndOverlay();
		}
		UI::EndFullscreenOverlay();
	}

	void GameLayer::RenderCursor()
	{
	}

	void GameLayer::RunOutro()
	{
		auto query = m_world.query<IntroCutscene>();
		auto lerpEntity = query.first();

		auto followQuery = m_world.query<CameraTarget>();
		auto followTarget = followQuery.first();

		if (followTarget)
		{
			auto script = followTarget.try_get_mut<CameraTarget>();
			script->onlyCountActivePlayers = false;
		}

		if (lerpEntity)
		{
			auto lerpScript = lerpEntity.try_get_mut<IntroCutscene>();

			lerpScript->cutsceneDuration = 5.f;

			lerpScript->TriggerOutro(lerpEntity);
		}
	}

	void GameLayer::RunIntro()
	{
		auto query = m_world.query<IntroCutscene>();
		auto lerpEntity = query.first();

		auto followQuery = m_world.query<CameraTarget>();
		auto followTarget = followQuery.first();

		if (followTarget)
		{
			auto script = followTarget.try_get_mut<CameraTarget>();
			script->onlyCountActivePlayers = true;
		}

		if (lerpEntity)
		{
			auto lerpScript = lerpEntity.try_get_mut<IntroCutscene>();

			lerpScript->cutsceneDuration = 5.f;

			lerpScript->TriggerIntro(lerpEntity);
		}
	}

	bool GameLayer::IsPlayerConnected(int playerIndex) const
	{
		for (auto controller : m_characterControllers)
		{
			if (controller->GetGamepadSocket() == playerIndex)
			{
				if(controller->IsPossessing()) return true;
			}
		}
		return false;
	}


	void GameLayer::SetupSystems()
	{
		flecs::entity GameTick = Cauda::Application::GetGameTickSource();

		m_controllerUpdateSystem = m_world.system("CharacterControllerUpdate")
			.kind(flecs::PreUpdate)
			.tick_source(GameTick)
			.run([this](flecs::iter& it)
				{
					float deltaTime = it.delta_time();

					for (auto* controller : m_characterControllers)
					{
						controller->Update(deltaTime);
					}

					if (m_lobbyTimerTime > 0.f && m_startingRound)
					{
						m_lobbyTimerTime -= deltaTime;

						if (m_lobbyTimerTime <= 0)
						{
							SDL_PushEvent(&GameEvents::g_AllPlayersReadyEvent);
						}
					}

					if (m_roundBeginDelay > 0)
					{
						m_roundBeginDelay -= deltaTime;
						if (m_roundBeginDelay <= 0.f)
						{
							for (auto controller : m_characterControllers)
							{
								controller->EnableInput();
							}
							m_gamemodeModule->StartRound();
						}
					}

				});
	}

	void GameLayer::PauseGame(bool pause)
	{
		Cauda::Application::PauseGame(pause);
	}

	void GameLayer::TogglePause()
	{
		Cauda::Application::TogglePauseGame();
	}

	bool GameLayer::IsGameRunning()
	{
		return Cauda::Application::IsGameRunning();
	}

	bool GameLayer::IsInLobby()
	{
		return m_inLobby;
	}

	bool GameLayer::IsInMainMenu()
	{
		return m_inMainMenu;
	}

	bool GameLayer::IsStartingRound()
	{
		return m_startingRound;
	}

	void GameLayer::OnPlayerPossessed(flecs::entity player, CharacterController& which)
	{
		if (!player) return;
		if (std::find(m_possessedPlayers.begin(), m_possessedPlayers.end(), player.id()) == m_possessedPlayers.end())
		{
			m_possessedPlayers.push_back(player.id()); 
		}
	}

	void GameLayer::OnPlayerDetach(flecs::entity player, CharacterController& which)
	{
		if (!player) return;
		auto it = std::find(m_possessedPlayers.begin(), m_possessedPlayers.end(), player.id());
		if (it != m_possessedPlayers.end())
		{
			m_possessedPlayers.erase(it);

			for (auto controller : m_characterControllers)
			{
				if (controller->IsPossessing())
				{
					auto player = controller->GetPosessedPawn();
					auto character = player.try_get_mut<CharacterComponent>();
					if (controller->IsPlayerReadied())
					{
						m_playersReadied[character->characterID - 1] = true;
					}
					else
					{
						m_playersReadied[character->characterID - 1] = false;
					}
				}
			}
		}
	}

	bool GameLayer::ToggleGameSettingsMenu()
	{
		m_showGameMenu = !m_showGameMenu;
		return m_showGameMenu;
	}

	void GameLayer::DetachCharacterControllers()
	{
		for (auto controller : m_characterControllers)
		{
			controller->DetachFromPawn();
		}
	}

	const CharacterController* GameLayer::GetCharacterController(flecs::entity possessed)
	{
		for (auto* controller : m_characterControllers)
		{
			if (!controller->IsPossessing()) continue;
			if (controller->GetPosessedPawn().id() == possessed.id())
			{
				return controller;
			}
		}
		return nullptr;
	}

	const CharacterController* GameLayer::GetCharacterController(int which)
	{
		if (which > 3 || which < 0) return nullptr; //Out of bounds, there's only 4!
		return m_characterControllers[which];
	}

	const std::vector<CharacterController*>& GameLayer::GetCharacterControllers()
	{
		return m_characterControllers;
	}

}