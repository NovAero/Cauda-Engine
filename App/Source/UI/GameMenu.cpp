#include "GameMenu.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"

#include "Core/Utilities/ImGuiUtilities.h"

#include <Core/Modules.h>

#include "Config/EngineDefaults.h"

using namespace Cauda;

GameMenu gameMenuUI;

void GameMenu::Open()
{
}

void GameMenu::Draw(float& gameTimer, std::string& levelName, bool* p_open)
{
	if (UI::BeginFullscreenOverlay("BackgrondBlur", 0.3f))
	{
		ImGui::SetNextWindowFocus();
		
		std::vector<const char*> scenes = ResourceLibrary::GetAllSceneHandles();
		static int selected_scene = 0;
		if (UI::BeginOverlay("##menu_items", UI::Anchor::Centre, ImVec2{ 500, 170 }))
		{
			if (ImGui::Button("<"))
			{
				selected_scene = selected_scene == 0 ? scenes.size() - 1 : selected_scene - 1;
				levelName = scenes[selected_scene];
			}
			ImGui::SameLine();
			ImGui::Text(scenes[selected_scene]);
			ImGui::SameLine();
			if (ImGui::Button(">"))
			{
				selected_scene = selected_scene == scenes.size() - 1 ? 0 : selected_scene + 1;
				levelName = scenes[selected_scene];
			}

			ImGui::Separator();

			int timerInt = gameTimer;

			if(ImGui::SliderInt("Round Timer", &timerInt, 15, 6000))
			{
				gameTimer = timerInt;
			}

			if (ImGui::Button("Exit"))
			{
				//Exit to previous game state (menu or closed)
				if (p_open)*p_open = false;
				//return;
			}
		}
		UI::EndOverlay();
	}
	UI::EndFullscreenOverlay();
}

void GameMenu::Close()
{

}