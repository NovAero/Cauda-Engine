#pragma once

#include "Core/Input/Input.h"
#include <unordered_map>
#include <queue>

namespace Cauda
{
	class InputLayer;
}

class InputSystem
{
	friend class Cauda::InputLayer;
public:
	InputSystem();
	~InputSystem();

	void Update();

	inline static bool IsKeyDown(PKey key) {
		return s_instance->m_currentKeyStates[key];
	}
	inline static bool IsGamepadButtonDown(SDL_JoystickID gamepad, Uint8 button) {

		if (gamepad == 0)
		{
			for (auto [joystick, states] : s_instance->m_currentGamepadStates)
			{
				if (states.buttonStates[button] > 0)
				{
					return true;
				}
			}
			return false;
		}
		else
		{
			return s_instance->m_currentGamepadStates[gamepad].buttonStates[button] > 0;
		}
	}
	inline static bool IsGamepadButtonHeld(SDL_JoystickID gamepad, Uint8 button, int ticks)
	{
		return s_instance->m_currentGamepadStates[gamepad].buttonStates[button] >= ticks;
	}
	inline static bool IsMouseButtonDown(MouseButton button) {
		return s_instance->m_currentMouseStates[button].down;
	}
	inline static glm::vec2 GetMousePosition()
	{
		return s_instance->m_currentMousePosition;
	}
	inline static float GetMouseScrollDelta()
	{
		return s_instance->m_scrollDelta;
	}
	inline static glm::vec2 GetMouseDelta()
	{
		return s_instance->m_mouseDelta;
	}
	static glm::vec2 GetViewportMousePosition();
	static void ResetInputSockets();

	void SetViewMode(bool isGameView, bool isFullscreen);
	void SetGameWindowInfo(float x, float y, int width, int height, int fbWidth, int fbHeight);

	//void AddUniversalListener(IGamepadListener* listener);
	//void RemoveUniversalListener(IGamepadListener* listener);

	void AddKeyListener(IKeyListener* listener);
	void AddMouseListener(IMouseListener* listener);
	void AddGamepadListener(IGamepadListener* listener, SDL_JoystickID gamepadID = 0);
	void SetUniversalListener(IGamepadListener* listener);

	void RemoveKeyListener(IKeyListener* listener);
	void RemoveMouseListener(IMouseListener* listener);
	void RemoveGamepadListener(IGamepadListener* listener);

	static InputSystem* Inst() { return s_instance; }

	void UpdateKeyState(PKey key, bool state);
	void UpdateGamepadButton(SDL_JoystickID pad, Uint8 button, bool down);
	void UpdateGamepadAxis(SDL_JoystickID pad, SDL_GamepadAxis which, Sint16 value);
	void UpdateMouseButton(MouseButton button, MouseButtonState state);
	void UpdateMouseScrollDelta(float amount);

	int RequestSocketForPossess();
	void ReleaseSocket(int socket);
	void BindFirstAvailableController(IGamepadListener* listener);

	bool m_isGameView = false;
	bool m_isFullscreen = true;
private:

	bool m_hasGameWindowInfo = false;

	std::unordered_map<PKey, bool>						m_currentKeyStates;
	std::unordered_map<PKey, bool>	  					m_previousKeyStates;
	std::unordered_map<SDL_JoystickID, GamepadState>	m_currentGamepadStates;
	std::unordered_map<SDL_JoystickID, GamepadState>    m_previousGamepadStates;
	std::unordered_map<MouseButton, MouseButtonState>	m_currentMouseStates;
	std::unordered_map<MouseButton, MouseButtonState>	m_previousMouseStates;

	std::vector<SDL_JoystickID> m_activeGamepads;
	std::vector<SDL_JoystickID> m_boundGamepads;
	int m_backupPadIndex = 0;

	glm::vec2 m_currentMousePosition;
	glm::vec2 m_previousMousePosition;
	glm::vec2 m_mouseDelta;

	float m_scrollDelta = 0.f;
	float m_gamepadDeadzone = 0.2f;

	int m_viewportWidth = 0;
	int m_viewportHeight = 0;

	float m_gameWindowX = 0;
	float m_gameWindowY = 0;
	int m_gameWindowWidth = 0;
	int m_gameWindowHeight = 0;

	std::vector<IKeyListener*>					m_keyListeners;
	std::vector<IMouseListener*>				m_mouseListeners;
	std::vector<IGamepadListener*>				m_gamepadListeners; //First listener is universal (typically the application)
	// Allow for multiple universalGamepadListeners instead - NO MUTLTIPLE UNIVERSAL LISTENERS, ONLY ONE
	//std::vector<IGamepadListener*>				m_universalGamepadListeners;

	std::deque<IGamepadListener*>				m_orphanedListeners;
	std::vector<int>							m_availableSockets;

	void ActivateGamepad(SDL_JoystickID gamepad);
	void DeactivateGamepad(SDL_JoystickID gamepad);
	SDL_JoystickID FindFirstUnboundGamepad();

	inline void UpdateActiveGamepads(std::vector<SDL_JoystickID> activePads)
	{
		m_activeGamepads = activePads;
	}
	inline static float AXIS_NORMALISE(Sint16 value)
	{
		return (float)value / (float)INT16_MAX;
	}

	void TransmitKeyStates();
	void TransmitMouseButtons();
	void TransmitScrollDelta();
	void TransmitGamepadStates();
	void TransmitGamepadStatesToListener(IGamepadListener* listener);

	static InputSystem* s_instance;
};

static inline ImGuiKey GamepadToImguiKey(uint8_t button)
{
	ImGuiKey key;
	switch (button)
	{
	case SDL_GAMEPAD_BUTTON_START: key = ImGuiKey_GamepadStart; break;
	case SDL_GAMEPAD_BUTTON_BACK: key = ImGuiKey_GamepadBack; break;
	case SDL_GAMEPAD_BUTTON_WEST: key = ImGuiKey_GamepadFaceLeft; break;
	case SDL_GAMEPAD_BUTTON_EAST: key = ImGuiKey_GamepadFaceRight; break;
	case SDL_GAMEPAD_BUTTON_NORTH: key = ImGuiKey_GamepadFaceUp; break;
	case SDL_GAMEPAD_BUTTON_SOUTH: key = ImGuiKey_GamepadFaceDown; break;
	case SDL_GAMEPAD_BUTTON_DPAD_LEFT: key = ImGuiKey_GamepadDpadLeft; break;
	case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: key = ImGuiKey_GamepadDpadRight; break;
	case SDL_GAMEPAD_BUTTON_DPAD_UP: key = ImGuiKey_GamepadDpadUp; break;
	case SDL_GAMEPAD_BUTTON_DPAD_DOWN: key = ImGuiKey_GamepadDpadDown; break;
	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: key = ImGuiKey_GamepadL1; break;
	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: key = ImGuiKey_GamepadR1; break;
	case SDL_GAMEPAD_BUTTON_LEFT_STICK: key = ImGuiKey_GamepadL3; break;
	case SDL_GAMEPAD_BUTTON_RIGHT_STICK: key = ImGuiKey_GamepadR3; break;
	}
	return key;
}

static inline ImGuiKey GamepadAxisToImguiKey(uint8_t axis, Sint16 value)
{
	ImGuiKey key;
	switch (axis)
	{
	case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: key = ImGuiKey_GamepadL2; break;
	case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: key = ImGuiKey_GamepadR2; break;
	case SDL_GAMEPAD_AXIS_LEFTX: key = value < 0 ? ImGuiKey_GamepadLStickLeft : ImGuiKey_GamepadLStickRight; break;
	case SDL_GAMEPAD_AXIS_LEFTY: key = value < 0 ? ImGuiKey_GamepadLStickUp : ImGuiKey_GamepadLStickDown; break;
	case SDL_GAMEPAD_AXIS_RIGHTX: key = value < 0 ? ImGuiKey_GamepadRStickLeft : ImGuiKey_GamepadRStickRight; break;
	case SDL_GAMEPAD_AXIS_RIGHTY: key = value < 0 ? ImGuiKey_GamepadRStickUp : ImGuiKey_GamepadRStickDown; break;
	}
	return key;
}