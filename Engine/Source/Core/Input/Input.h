#pragma once

#include <SDL3/SDL_Events.h>
#include "Core/Input/KeyCodes.h"
#include <unordered_map>
#include <glm/glm.hpp>

constexpr int MIN_TICKS_FOR_HOLD = 60;

enum class MouseButton : Uint8
{
	LMB = 1,
	MMB = 2,
	RMB = 3,
	Mouse4 = 4,
	Mouse5 = 5,
	UNKNOWN
};

struct MouseButtonState
{
	MouseButton button;
	bool down;
	Uint8 clicks;
};

struct GamepadState
{
	SDL_JoystickID GamepadID;

	glm::vec2 Left;
	glm::vec2 Right;

	Uint16 pad;

	int LTrigger = 0;
	int RTrigger = 0;

	std::unordered_map<Uint8, int> buttonStates; //Button and ticks held
};

class IKeyListener
{
public:
	virtual ~IKeyListener() = default;

	virtual void OnKeyPress(PKey key) = 0;
	virtual void OnKeyRelease(PKey key) = 0;
};

class IMouseListener {
public:
	virtual ~IMouseListener() = default;

	virtual void OnMouseClick(MouseButton button, int clicks) = 0;
	virtual void OnMouseRelease(MouseButton mouseButton) = 0;
	virtual void OnMouseScroll(float amount) = 0;
};

class IGamepadListener
{
public:
	virtual ~IGamepadListener() = default;

	virtual void OnButtonDown(Uint8 button) = 0;
	virtual void OnButtonHeld(Uint8 button, int heldTicks) {}
	virtual void OnButtonCanceled(Uint8 button) {}
	virtual void OnButtonUp(Uint8 button) = 0;

	virtual void OnLeftAxisInput(float xValue, float yValue) = 0;
	virtual void OnRightAxisInput(float xValue, float yValue) = 0;

	virtual void OnTriggerDown(bool left) = 0;
	virtual void OnTriggerHeld(bool left, int heldTicks) {}
	virtual void OnTriggerCanceled(bool left) {}
	virtual void OnTriggerUp(bool left) = 0;

	//Returns true if new binding was added, false if the binding already existed
	inline bool AddBinding(SDL_JoystickID gamepad) {
		if (SDL_GetJoystickFromID(gamepad) && std::find(m_boundJoysticks.begin(), m_boundJoysticks.end(), gamepad) == m_boundJoysticks.end()) {
			m_boundJoysticks.push_back(gamepad);
			return true;
		}
		return false;
	}
	inline bool RemoveBinding(SDL_JoystickID gamepad) {
		auto it = std::find(m_boundJoysticks.begin(), m_boundJoysticks.end(), gamepad);
		if (it != m_boundJoysticks.end()) {
			m_boundJoysticks.erase(it);
		}
		if (m_boundJoysticks.size() == 0) return true;
		return false;
	}

	const std::vector<SDL_JoystickID>& Bindings() const { return m_boundJoysticks; }

protected:

	std::vector<SDL_JoystickID> m_boundJoysticks;
};