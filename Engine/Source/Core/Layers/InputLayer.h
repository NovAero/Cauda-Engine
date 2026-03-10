#pragma once
#include "Layer.h"
#include <unordered_map>
#include <queue>

class InputSystem;

namespace Cauda {

	class InputLayer //: public Layer
	{
	public:
		InputLayer();
		~InputLayer();

		void OnAttach();
		void OnDetach();

		bool OnUpdate(float deltaTime);
		void OnEvent(SDL_Event& event);

	private:
		InputSystem* m_inputDispatch;
		float m_timeSinceMouseActivity = 0.f;
		float m_hideMouseTime = 3.f;
	};
}