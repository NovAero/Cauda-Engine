#include "cepch.h"
#include "CustomEvents.h"



namespace AppEvents
{
	SDL_Event g_SwitchToEditorModeEvent = {};
	SDL_Event g_SwitchToGameModeEvent = {};

	void InitialiseAppEvents()
	{
		g_SwitchToEditorModeEvent.type = SDL_EVENT_USER;
		g_SwitchToEditorModeEvent.user.code = EVENT_SWITCH_TO_EDITOR;

		g_SwitchToGameModeEvent.type = SDL_EVENT_USER;
		g_SwitchToGameModeEvent.user.code = EVENT_SWITCH_TO_GAME;
	}
}


namespace EditorEvents
{
	SDL_Event g_PIEvent = {};
	SDL_Event g_RegisterSceneEvent = {};
	SDL_Event g_LoadSceneEvent = {};

	SDL_Event g_PostReloadEvent = {};

	void InitialiseEditorEvents()
	{
		g_PIEvent.type = SDL_EVENT_USER;
		g_PIEvent.user.code = EVENT_PLAY_IN_EDITOR;

		g_RegisterSceneEvent.type = SDL_EVENT_USER;
		g_RegisterSceneEvent.user.code = EVENT_REGISTER_SCENE;

		g_LoadSceneEvent.type = SDL_EVENT_USER;
		g_LoadSceneEvent.user.code = EVENT_LOAD_SCENE;

		g_PostReloadEvent.type = SDL_EVENT_USER;
		g_PostReloadEvent.user.code = EVENT_POST_RELOAD;

	}

}

namespace GameEvents
{
	SDL_Event g_PlayEvent = {};
	SDL_Event g_ExitEvent = {};
	SDL_Event g_PlayerEvent = {};
	SDL_Event g_TickOnceEvent = {};

	SDL_Event g_BeginRoundEvent = {};
	SDL_Event g_LoadRoundEvent = {};
	SDL_Event g_EndRoundEvent = {};
	SDL_Event g_LoadLobbyEvent = {};
	SDL_Event g_AllPlayersReadyEvent = {};
	SDL_Event g_PlayerReadiedEvent = {};
	SDL_Event g_PlayerUnreadiedEvent = {};

	SDL_Event g_ButtonEvent_Play = {};
	SDL_Event g_ButtonEvent_GameSettings = {};

	SDL_Event g_ButtonEvent_NewRound = {};
	SDL_Event g_ButtonEvent_AppSettings = {};
	SDL_Event g_ButtonEvent_Back = {};
	SDL_Event g_ButtonEvent_ExitToDesktop = {};
	SDL_Event g_ButtonEvent_ExitToMenu = {};

	SDL_Event g_PlayerDeathEvent = {};

	void InitialiseGameEvents()
	{
		g_PlayEvent.type = SDL_EVENT_USER;
		g_PlayEvent.user.code = EVENT_ON_PLAY;

		g_ExitEvent.type = SDL_EVENT_USER;
		g_ExitEvent.user.code = EVENT_ON_EXIT;

		g_PlayerEvent.type = SDL_EVENT_USER;
		g_PlayerEvent.user.code = EVENT_PLAYER_EVENT;

		g_TickOnceEvent.type = SDL_EVENT_USER;
		g_TickOnceEvent.user.code = EVENT_TICK_ONCE;

		g_PlayerDeathEvent.type = SDL_EVENT_USER;
		g_PlayerDeathEvent.user.code = EVENT_PLAYER_DEATH;

		//Round events
		g_BeginRoundEvent.type = SDL_EVENT_USER;
		g_BeginRoundEvent.user.code = EVENT_BEGIN_ROUND;

		g_EndRoundEvent.type = SDL_EVENT_USER;
		g_EndRoundEvent.user.code = EVENT_END_ROUND;

		g_LoadRoundEvent.type = SDL_EVENT_USER;
		g_LoadRoundEvent.user.code = EVENT_LOAD_ROUND;

		g_LoadLobbyEvent.type = SDL_EVENT_USER;
		g_LoadLobbyEvent.user.code = EVENT_LOAD_LOBBY;

		g_AllPlayersReadyEvent.type = SDL_EVENT_USER;
		g_AllPlayersReadyEvent.user.code = EVENT_ALL_PLAYERS_READY;

		g_PlayerReadiedEvent.type = SDL_EVENT_USER;
		g_PlayerReadiedEvent.user.code = EVENT_PLAYER_READIED;

		g_PlayerUnreadiedEvent.type = SDL_EVENT_USER;
		g_PlayerUnreadiedEvent.user.code = EVENT_PLAYER_UNREADIED;

		//Button Events

		g_ButtonEvent_Play.type = SDL_EVENT_USER;
		g_ButtonEvent_Play.user.code = EVENT_BUTTON_PLAY;

		g_ButtonEvent_GameSettings.type = SDL_EVENT_USER;
		g_ButtonEvent_GameSettings.user.code = EVENT_BUTTON_GAMESETTINGS;

		g_ButtonEvent_AppSettings.type = SDL_EVENT_USER;
		g_ButtonEvent_AppSettings.user.code = EVENT_BUTTON_APPSETTINGS;

		g_ButtonEvent_Back.type = SDL_EVENT_USER;
		g_ButtonEvent_Back.user.code = EVENT_BUTTON_BACK;

		g_ButtonEvent_ExitToDesktop.type = SDL_EVENT_USER;
		g_ButtonEvent_ExitToDesktop.user.code = EVENT_BUTTON_EXIT_D;

		g_ButtonEvent_ExitToMenu.type = SDL_EVENT_USER;
		g_ButtonEvent_ExitToMenu.user.code = EVENT_BUTTON_EXIT_M;

		g_ButtonEvent_NewRound.type = SDL_EVENT_USER;
		g_ButtonEvent_NewRound.user.code = EVENT_BUTTON_NEWROUND;
	}
}