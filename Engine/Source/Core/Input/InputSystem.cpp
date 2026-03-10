#include "cepch.h"
#include "InputSystem.h"
#include "Core/Layers/InputLayer.h"
#include <algorithm>

InputSystem* InputSystem::s_instance;

InputSystem::InputSystem() :
	m_currentMousePosition(0.0f, 0.0f),
	m_previousMousePosition(0.0f, 0.0f),
	m_scrollDelta(0.0f)
{
	if (!s_instance)
	{
		s_instance = this;
	}

	m_availableSockets.reserve(sizeof(int) * MAX_GAMEPAD_SOCKETS);
	for (int i = 0; i < MAX_GAMEPAD_SOCKETS; ++i) {
		m_availableSockets.emplace(m_availableSockets.end(), i);
	}
}

InputSystem::~InputSystem()
{
	for (auto gamepad : m_activeGamepads) {
		SDL_CloseGamepad(SDL_GetGamepadFromID(gamepad));
	}	
}

void InputSystem::Update()
{
	SDL_GetMouseState(&m_currentMousePosition.x, &m_currentMousePosition.y);

    TransmitKeyStates();
	TransmitGamepadStates();
    TransmitMouseButtons();
    TransmitScrollDelta();

	m_previousKeyStates = m_currentKeyStates;
	m_previousGamepadStates = m_currentGamepadStates;
	m_mouseDelta = m_previousMousePosition - m_currentMousePosition;
	m_previousMousePosition = m_currentMousePosition;
}

glm::vec2 InputSystem::GetViewportMousePosition()
{
	if (s_instance->m_isGameView)
	{
		float normalizedX = (float)(s_instance->m_currentMousePosition.x / s_instance->m_gameWindowWidth);
		float normalizedY = (float)(s_instance->m_currentMousePosition.y / s_instance->m_gameWindowHeight);

		return glm::vec2(
			normalizedX * s_instance->m_viewportWidth,
			normalizedY * s_instance->m_viewportHeight
		);
	}

	glm::vec2 mousePos = GetMousePosition();

	if (mousePos.x >= s_instance->m_gameWindowX &&
		mousePos.x <= s_instance->m_gameWindowX + s_instance->m_gameWindowWidth &&
		mousePos.y >= s_instance->m_gameWindowY &&
		mousePos.y <= s_instance->m_gameWindowY + s_instance->m_gameWindowHeight)
	{
		float normalizedX = (mousePos.x - s_instance->m_gameWindowX) / s_instance->m_gameWindowWidth;
		float normalizedY = (mousePos.y - s_instance->m_gameWindowY) / s_instance->m_gameWindowHeight;

		return glm::vec2(
			normalizedX * s_instance->m_viewportWidth,
			normalizedY * s_instance->m_viewportHeight
		);
	}

	return glm::vec2(-1.0f, -1.0f);
}

void InputSystem::ResetInputSockets()
{
	s_instance->m_availableSockets.clear();
	for (int i = 0; i < MAX_GAMEPAD_SOCKETS; ++i)
	{
		s_instance->m_availableSockets.push_back(i);
	}
}

void InputSystem::SetGameWindowInfo(float x, float y, int width, int height, int fbWidth, int fbHeight)
{
	m_gameWindowX = x;
	m_gameWindowY = y;
	m_gameWindowWidth = width;
	m_gameWindowHeight = height;
	m_viewportWidth = fbWidth;
	m_viewportHeight = fbHeight;
	m_hasGameWindowInfo = true;
}

void InputSystem::SetViewMode(bool isGameView, bool isFullscreen)
{
	m_isGameView = isGameView;
	m_isFullscreen = isFullscreen;
}


//void InputSystem::AddUniversalListener(IGamepadListener* listener)
//{
//	if (listener && std::find(m_universalGamepadListeners.begin(), m_universalGamepadListeners.end(), listener) == m_universalGamepadListeners.end()) {
//		m_universalGamepadListeners.push_back(listener);
//		for (auto pad : m_activeGamepads) {
//			listener->AddBinding(pad);
//		}
//	}
//}
//
//void InputSystem::RemoveUniversalListener(IGamepadListener* listener)
//{
//	auto it = std::find(s_instance->m_universalGamepadListeners.begin(), s_instance->m_universalGamepadListeners.end(), listener);
//	if (it != m_universalGamepadListeners.end()) {
//		m_universalGamepadListeners.erase(it);
//	}
//}

void InputSystem::AddKeyListener(IKeyListener* listener)
{
	if (listener && std::find(m_keyListeners.begin(), m_keyListeners.end(), listener) == m_keyListeners.end()) {
		m_keyListeners.push_back(listener);
	}
}

void InputSystem::AddMouseListener(IMouseListener* listener)
{
	if (listener && std::find(m_mouseListeners.begin(), m_mouseListeners.end(), listener) == m_mouseListeners.end()) {
		m_mouseListeners.push_back(listener);
	}
}

void InputSystem::AddGamepadListener(IGamepadListener* listener, SDL_JoystickID gamepadID)
{
	if (m_activeGamepads.size() == 0)
	{
		m_orphanedListeners.push_back(listener);
		return;
	}

	if (listener && std::find(m_gamepadListeners.begin(), m_gamepadListeners.end(), listener) == m_gamepadListeners.end()) {
		m_gamepadListeners.push_back(listener);
		if (gamepadID == 0) //Find first gamepad
		{
			gamepadID = FindFirstUnboundGamepad();
			if (gamepadID == 0) //No gamepad found, orphan the listener to avoid overlaps
			{
				m_orphanedListeners.push_front(listener);
			}
			else
			{
				listener->AddBinding(gamepadID);
				m_boundGamepads.push_back(gamepadID);
			}
		}
		else {
			if (std::find(m_activeGamepads.begin(), m_activeGamepads.end(), gamepadID) != m_activeGamepads.end())
			{
				listener->AddBinding(gamepadID);
			}
		}
	}
}

void InputSystem::SetUniversalListener(IGamepadListener* listener)
{
	std::vector<IGamepadListener*> listeners;
	listeners.push_back(listener);

	for (auto l : m_gamepadListeners) {
		if (l == m_gamepadListeners.front()) continue;
		listeners.push_back(l);
	}
	m_gamepadListeners = listeners;

	for (auto pad : m_activeGamepads) {
		m_gamepadListeners[0]->AddBinding(pad);
	}
}

void InputSystem::RemoveKeyListener(IKeyListener* listener)
{
	auto it = std::find(s_instance->m_keyListeners.begin(), s_instance->m_keyListeners.end(), listener);
	if (it != m_keyListeners.end()) {
		m_keyListeners.erase(it);
	}
}

void InputSystem::RemoveMouseListener(IMouseListener* listener)
{
	auto it = std::find(m_mouseListeners.begin(), m_mouseListeners.end(), listener);
	if (it != m_mouseListeners.end()) {
		m_mouseListeners.erase(it);
	}
}

void InputSystem::RemoveGamepadListener(IGamepadListener* listener)
{
	auto it = std::find(m_gamepadListeners.begin(), m_gamepadListeners.end(), listener);
	if (it != m_gamepadListeners.end()) {

		m_gamepadListeners.erase(it);
	}
}

void InputSystem::UpdateKeyState(PKey key, bool state)
{
	s_instance->m_currentKeyStates[key] = state;
}

void InputSystem::UpdateGamepadButton(SDL_JoystickID pad, Uint8 button, bool down)
{
	s_instance->m_currentGamepadStates[pad].buttonStates[button] = down;
}

void InputSystem::UpdateGamepadAxis(SDL_JoystickID pad, SDL_GamepadAxis which, Sint16 value)
{
	switch (which) {
	case SDL_GAMEPAD_AXIS_LEFTX: { s_instance->m_currentGamepadStates[pad].Left.x = AXIS_NORMALISE(value); } break;
	case SDL_GAMEPAD_AXIS_LEFTY: { s_instance->m_currentGamepadStates[pad].Left.y = AXIS_NORMALISE(value); } break;
	case SDL_GAMEPAD_AXIS_RIGHTX: { s_instance->m_currentGamepadStates[pad].Right.x = AXIS_NORMALISE(value); } break;
	case SDL_GAMEPAD_AXIS_RIGHTY: { s_instance->m_currentGamepadStates[pad].Right.y = AXIS_NORMALISE(value); } break;
	case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: { s_instance->m_currentGamepadStates[pad].LTrigger = value > 0 ? s_instance->m_currentGamepadStates[pad].LTrigger + 1 : 0; } break;
	case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: { s_instance->m_currentGamepadStates[pad].RTrigger = value > 0 ? s_instance->m_currentGamepadStates[pad].RTrigger + 1 : 0; } break;
	}
}

void InputSystem::UpdateMouseButton(MouseButton button, MouseButtonState state)
{
	s_instance->m_currentMouseStates[button] = state;
}

void InputSystem::UpdateMouseScrollDelta(float amount)
{
	s_instance->m_scrollDelta = amount;
}

int InputSystem::RequestSocketForPossess()
{
	if (m_availableSockets.size() == 0) return -1;
	int ret = m_availableSockets.front();

	m_availableSockets.erase(m_availableSockets.begin());
	std::sort(m_availableSockets.begin(), m_availableSockets.end());

	return ret;
}

void InputSystem::ReleaseSocket(int socket)
{
	if (std::find(m_availableSockets.begin(), m_availableSockets.end(), socket) != m_availableSockets.end()) return;
	
	m_availableSockets.push_back(socket);
	std::sort(m_availableSockets.begin(), m_availableSockets.end());
}

void InputSystem::BindFirstAvailableController(IGamepadListener* listener)
{
	if (!listener) return;
	auto gamepad = FindFirstUnboundGamepad();
	if (gamepad == 0) return;

	listener->AddBinding(gamepad);
	m_boundGamepads.push_back(gamepad);

	auto it = std::find(m_orphanedListeners.begin(), m_orphanedListeners.end(), listener);
	if (it != m_orphanedListeners.end())
	{
		m_orphanedListeners.erase(it);
	}
}

void InputSystem::ActivateGamepad(SDL_JoystickID gamepad)
{
	if (SDL_OpenGamepad(gamepad)) {
		if (std::find(m_activeGamepads.begin(), m_activeGamepads.end(), gamepad) != m_activeGamepads.end()) return;
		m_activeGamepads.push_back(gamepad);
		Logger::PrintLog("Activated gamepad");

		//Universal listener always gets the binding
		if (m_gamepadListeners.size() > 0) m_gamepadListeners[0]->AddBinding(gamepad);
		if (m_orphanedListeners.size() > 0)
		{
			m_orphanedListeners.front()->AddBinding(gamepad);
			m_gamepadListeners.push_back(m_orphanedListeners.front());
			m_boundGamepads.push_back(gamepad);
			m_orphanedListeners.pop_front();
		}
	}
}

void InputSystem::DeactivateGamepad(SDL_JoystickID gamepad)
{
	SDL_CloseGamepad(SDL_GetGamepadFromID(gamepad));
	auto it = std::find(m_activeGamepads.begin(), m_activeGamepads.end(), gamepad);
	if (it != m_activeGamepads.end()) m_activeGamepads.erase(it);

	it = std::find(m_boundGamepads.begin(), m_boundGamepads.end(), gamepad);
	if (it != m_boundGamepads.end()) m_boundGamepads.erase(it);

	for (auto listener : m_gamepadListeners) {
		if (std::find(listener->Bindings().begin(), listener->Bindings().end(), gamepad) != listener->Bindings().end())
		{
			//If the listener no longer has any bindings, save it and wait for a new gamepad
			if (listener->RemoveBinding(gamepad)) {
				m_orphanedListeners.push_front(listener);
			}
		}
	}
}

SDL_JoystickID InputSystem::FindFirstUnboundGamepad()
{
	if (m_activeGamepads.size() > m_boundGamepads.size())
	{
		for (auto pad : m_activeGamepads)
		{
			if (std::find(m_boundGamepads.begin(), m_boundGamepads.end(), pad) == m_boundGamepads.end()) {
				return pad;
			}
		}
	}

	return 0;
}

void InputSystem::TransmitKeyStates()
{
	for (auto key : m_currentKeyStates) {
		if (key.second && !m_previousKeyStates[key.first]) {
			for (auto listener : m_keyListeners) {
				listener->OnKeyPress(key.first);
			}
		}
		if (!key.second && m_previousKeyStates[key.first]) {
			for (auto listener : m_keyListeners) {
				listener->OnKeyRelease(key.first);
			}
		}
	}
}

void InputSystem::TransmitMouseButtons()
{
	for (auto button : m_currentMouseStates) {
		if (button.second.down && !m_previousMouseStates[button.first].down)
		{
			for (auto listener : m_mouseListeners) {
				listener->OnMouseClick(button.second.button, button.second.clicks);
			}
		}
		if (!button.second.down && m_previousMouseStates[button.first].down)
		{
			for (auto listener : m_mouseListeners) {
				listener->OnMouseRelease(button.first);
			}
		}
	}
}

void InputSystem::TransmitScrollDelta()
{
	if (m_scrollDelta != 0.0f) {
		for (auto listener : m_mouseListeners) {
			listener->OnMouseScroll(m_scrollDelta);
		}
		m_scrollDelta = 0.0f;
	}
}

void InputSystem::TransmitGamepadStates()
{
	for (auto listener : m_gamepadListeners) {
		TransmitGamepadStatesToListener(listener);
	}
}

void InputSystem::TransmitGamepadStatesToListener(IGamepadListener* listener)
{
	for (int i = 0; i < listener->Bindings().size(); ++i) {
		GamepadState& gamepad = m_currentGamepadStates[listener->Bindings()[i]];
		GamepadState previous = m_previousGamepadStates[listener->Bindings()[i]];
		//Face buttons and bumpers
		for (auto button : gamepad.buttonStates) {
			//Press
			if (button.second == 1 && previous.buttonStates[button.first] == 0) {
				listener->OnButtonDown(button.first);
				gamepad.buttonStates[button.first] = previous.buttonStates[button.first] + 1;
			} //Hold
			else if (button.second >= 1 && previous.buttonStates[button.first] >= 1)
			{
				if (button.second >= MIN_TICKS_FOR_HOLD)
				{
					listener->OnButtonHeld(button.first, button.second);
				}
				gamepad.buttonStates[button.first] = previous.buttonStates[button.first] + 1;
			}
			//Release
			if (button.second == 0 && previous.buttonStates[button.first] > 0) {
				//Canceled
				if (previous.buttonStates[button.first] < MIN_TICKS_FOR_HOLD)
				{
					listener->OnButtonCanceled(button.first);
				}
				listener->OnButtonUp(button.first);
			}
		}
		//Left stick
		if (abs(gamepad.Left.x) > GAMEPAD_DEADZONE || abs(gamepad.Left.y) > GAMEPAD_DEADZONE) {
			listener->OnLeftAxisInput(gamepad.Left.x, gamepad.Left.y);
		}
		else if (abs(previous.Left.x) > GAMEPAD_DEADZONE || abs(previous.Left.y) > GAMEPAD_DEADZONE) {
			listener->OnLeftAxisInput(0.0f, 0.0f);
		}
		//Right stick
		if (abs(gamepad.Right.x) > GAMEPAD_DEADZONE || abs(gamepad.Right.y) > GAMEPAD_DEADZONE) {
			listener->OnRightAxisInput(gamepad.Right.x, gamepad.Right.y);
		}
		else if (abs(previous.Right.x) > GAMEPAD_DEADZONE || abs(previous.Right.y) > GAMEPAD_DEADZONE) {
			listener->OnRightAxisInput(0.0f, 0.0f);
		}
		//Left trigger
		if (gamepad.LTrigger >= 1 && previous.LTrigger == 0) {
			listener->OnTriggerDown(true);
		}
		else if (gamepad.LTrigger >= 1 && previous.LTrigger >= 1)
		{
			if (gamepad.LTrigger >= MIN_TICKS_FOR_HOLD)
			{
				listener->OnTriggerHeld(true, gamepad.LTrigger);
			}
		}
		if (gamepad.LTrigger == 0 && previous.LTrigger > 0) {
			if (gamepad.LTrigger < MIN_TICKS_FOR_HOLD)
			{
				listener->OnTriggerCanceled(true);
			}
			listener->OnTriggerUp(true);
		}
		//Right trigger
		if (gamepad.RTrigger >= 1 && previous.RTrigger == 0) {
			listener->OnTriggerDown(false);
		}
		else if (gamepad.RTrigger >= 1 && previous.RTrigger >= 1)
		{
			if (gamepad.RTrigger >= MIN_TICKS_FOR_HOLD)
			{
				listener->OnTriggerHeld(false, gamepad.RTrigger);
			}
		}
		if (gamepad.RTrigger == 0 && previous.RTrigger > 0) {
			if (gamepad.RTrigger < MIN_TICKS_FOR_HOLD)
			{
				listener->OnTriggerCanceled(false);
			}
			listener->OnTriggerUp(false);
		}
	}
}
