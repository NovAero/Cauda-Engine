#pragma once
#include <SDL3/SDL_events.h>

//Scene Events

#define EVENT_REGISTER_SCENE 100
#define EVENT_LOAD_SCENE 101

//Play state events

#define EVENT_ON_PLAY 200
#define EVENT_ON_EXIT 201
#define EVENT_POST_RELOAD 203

#define EVENT_BEGIN_ROUND 204
#define EVENT_END_ROUND 205
#define EVENT_LOAD_LOBBY 206
#define EVENT_ALL_PLAYERS_READY 207
#define EVENT_PLAYER_READIED 208
#define EVENT_PLAYER_UNREADIED 209
#define EVENT_LOAD_ROUND 210

//Diegetic button events
#define EVENT_BUTTON_PLAY 300
#define EVENT_BUTTON_GAMESETTINGS 301
#define EVENT_BUTTON_APPSETTINGS 302
#define EVENT_BUTTON_BACK 303
#define EVENT_BUTTON_EXIT_D 304
#define EVENT_BUTTON_EXIT_M 305
#define EVENT_BUTTON_NEWROUND 306

//Player events

#define EVENT_PLAYER_EVENT 400
#define EVENT_PLAYER_DEATH 401

//Debug events
#define EVENT_PLAY_IN_EDITOR 600
#define EVENT_TICK_ONCE 601

#define EVENT_SWITCH_TO_EDITOR 602
#define EVENT_SWITCH_TO_GAME 603


namespace AppEvents
{
	void InitialiseAppEvents();

	extern SDL_Event g_SwitchToEditorModeEvent;
	extern SDL_Event g_SwitchToGameModeEvent;
}

namespace EditorEvents
{
	void InitialiseEditorEvents();

	extern SDL_Event g_PIEvent;
	extern SDL_Event g_RegisterSceneEvent;
	extern SDL_Event g_LoadSceneEvent;
	extern SDL_Event g_PostReloadEvent;

}

namespace GameEvents
{
	void InitialiseGameEvents();

	extern SDL_Event g_PlayEvent;
	extern SDL_Event g_ExitEvent;
	extern SDL_Event g_PlayerEvent;
	extern SDL_Event g_TickOnceEvent;

	extern SDL_Event g_BeginRoundEvent;
	extern SDL_Event g_EndRoundEvent;
	extern SDL_Event g_LoadRoundEvent;
	extern SDL_Event g_LoadLobbyEvent;
	extern SDL_Event g_AllPlayersReadyEvent;
	extern SDL_Event g_PlayerReadiedEvent;
	extern SDL_Event g_PlayerUnreadiedEvent;

	extern SDL_Event g_PlayerDeathEvent;

	extern SDL_Event g_ButtonEvent_Play;
	extern SDL_Event g_ButtonEvent_GameSettings;
	extern SDL_Event g_ButtonEvent_AppSettings;
	extern SDL_Event g_ButtonEvent_Back;
	extern SDL_Event g_ButtonEvent_ExitToDesktop;
	extern SDL_Event g_ButtonEvent_ExitToMenu;
	extern SDL_Event g_ButtonEvent_NewRound;
}