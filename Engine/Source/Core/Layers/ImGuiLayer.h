#pragma once
#include "Core/Layers/Layer.h"
#include <ThirdParty/Flecs.h>

namespace Cauda
{
	class Application;

	class ImGuiLayer //: public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer() = default;

		void OnAttach();
		void OnDetach();
		void OnEvent(SDL_Event& event);
		
		void BeginDraw();
		void EndDraw();
		void RenderDrawData();

	private:
		void SetupImGuiStyle();
	};
}