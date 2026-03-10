#pragma once

#include <SDL3/SDL_events.h>
#include <string>

namespace Cauda {

	class Layer
	{
	public:
		Layer(const std::string& name = "Layer");
		virtual ~Layer();

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(float delta) {}
		virtual void OnImGuiRender() {}
		virtual void OnEvent(SDL_Event& event) {}

#ifdef _DEBUG
		inline const std::string& GetName() const { return m_debugName; }

	protected:
		std::string m_debugName;
#endif // _DEBUG

	};

}