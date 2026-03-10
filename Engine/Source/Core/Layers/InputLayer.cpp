#include "cepch.h"
#include "InputLayer.h"
#include "Core/Input/InputSystem.h"
#include "Platform/Win32/Application.h"
#include "Core/Layers/RenderLayer.h" 
#include <imgui_impl_sdl3.h>

float Saturate(float v) { return v < 0.0f ? 0.0f : v  > 1.0f ? 1.0f : v; }

namespace Cauda {

	InputLayer::InputLayer() //: Layer("Input Layer")
	{
		if (!InputSystem::Inst()) {
			Logger::PrintDebugSeperator();
			Logger::PrintLog("Input System is not initialised");
		}
		m_inputDispatch = InputSystem::Inst();
	}

	InputLayer::~InputLayer()
	{
		m_inputDispatch = nullptr;
	}


	void InputLayer::OnAttach()
	{
		SDL_Init(SDL_INIT_GAMEPAD);

		int gpCount;
		auto gamepads = SDL_GetGamepads(&gpCount);

		for (int i = 0; i < gpCount; i++) {
			m_inputDispatch->ActivateGamepad(gamepads[i]);
		}

		SDL_free(gamepads);
	}

	void InputLayer::OnDetach()
	{
		for (auto gamepad : m_inputDispatch->m_activeGamepads)
		{
			m_inputDispatch->DeactivateGamepad(gamepad);
		}
	}

	bool InputLayer::OnUpdate(float deltaTime)
	{
		m_inputDispatch->Update();
		bool hideCursor = false;
		if (Cauda::Application::IsGameRunning())
		{
			if (m_timeSinceMouseActivity >= m_hideMouseTime)
			{
				hideCursor = true;
			}
			m_timeSinceMouseActivity += deltaTime;
		}
		return hideCursor;
	}

	void InputLayer::OnEvent(SDL_Event& event)
	{
		switch (event.type)
		{
			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
			{
				m_inputDispatch->UpdateKeyState((PKey)event.key.scancode, event.key.down);
			} break;
			case SDL_EVENT_MOUSE_MOTION:
			{
				m_timeSinceMouseActivity = 0.f;
			}break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				m_timeSinceMouseActivity = 0.f;

				MouseButtonState state;
				state.button = (MouseButton)event.button.button;
				state.clicks = event.button.clicks;
				state.down = event.button.down;
				m_inputDispatch->UpdateMouseButton(state.button, state);
			} break;
			case SDL_EVENT_MOUSE_WHEEL: {

				m_timeSinceMouseActivity = 0.f;

				m_inputDispatch->UpdateMouseScrollDelta(event.wheel.y);
			} break;
		
			case SDL_EVENT_GAMEPAD_ADDED:
			{
				m_inputDispatch->ActivateGamepad(event.gdevice.which);
			} break;
			case SDL_EVENT_GAMEPAD_REMOVED:
			{
				m_inputDispatch->DeactivateGamepad(event.gdevice.which);
			}break;
			case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			case SDL_EVENT_GAMEPAD_BUTTON_UP:
			{
				m_inputDispatch->UpdateGamepadButton(event.gdevice.which, event.gbutton.button, event.gbutton.down);
				//ImGui::GetIO().AddKeyAnalogEvent(GamepadToImguiKey(event.gbutton.which), event.gbutton.down, event.gbutton.down);
			} break;
			case SDL_EVENT_GAMEPAD_AXIS_MOTION:
			{
				m_inputDispatch->UpdateGamepadAxis(event.gdevice.which, (SDL_GamepadAxis)event.gaxis.axis, event.gaxis.value);
				/*
				float merged_value = 0.f;
				float v0 = 0.f;
				float v1 = 0.f;
				switch (event.gaxis.axis) {
				case SDL_GAMEPAD_AXIS_LEFTX:	v0 = event.gaxis.value > 0 ? 8000 : -8000; v1 = event.gaxis.value > 0 ? 32767 : -32768; break;
				case SDL_GAMEPAD_AXIS_LEFTY:	v0 = event.gaxis.value > 0 ? 8000 : -8000; v1 = event.gaxis.value > 0 ? 32767 : -32768;  break;
				case SDL_GAMEPAD_AXIS_RIGHTX:	v0 = event.gaxis.value > 0 ? 8000 : -8000; v1 = event.gaxis.value > 0 ? 32767 : -32768; break;
				case SDL_GAMEPAD_AXIS_RIGHTY:	v0 = event.gaxis.value > 0 ? 8000 : -8000; v1 = event.gaxis.value > 0 ? 32767 : -32768; break;
				case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: v0 = 0.f; v1 = 32767; break;
				case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: v0 = 0.f; v1 = 32767; break;
				}
				
				float vn = Saturate((float)(event.gaxis.value - v0) / (float)(v1 - v0));
				if (merged_value < vn)
					merged_value = vn;

				ImGui::GetIO().AddKeyAnalogEvent(GamepadAxisToImguiKey(event.gaxis.axis, event.gaxis.value), merged_value > 0, merged_value);*/

			}break;
		}
	}
}
