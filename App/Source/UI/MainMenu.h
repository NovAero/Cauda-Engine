#pragma once

#include <ThirdParty/Flecs.h>
#include <imgui.h>

class MainMenu
{
public:

	void Draw(flecs::world& world);

	void Open(flecs::world& world);

	void Close(flecs::world& world);

private:

	void RenderSettingsInMenu(ImTextureID atlasTexture);
	void RenderCredits();

	void DrawGeneralSettings();
	void DrawAudioSettings();
	void DrawGraphicsSettings();


	bool m_showingSettings = false;

	int m_currentSettingsTab = 0;

	bool m_handlingButtonEvent = false;

	bool m_vSync = false;

	bool m_showingCredits = false;
};

class PauseMenu
{
public:

	void Draw(bool* p_open = nullptr);
	
	void Open(flecs::world& world);

	void Close();

	~PauseMenu();

private:

	flecs::world* m_world = nullptr;

	bool m_showingSettings = false;

	int m_currentSettingsTab = 0;

	bool m_handlingButtonEvent = false;

	bool m_vSync = false;

	void DrawGeneralSettings();
	void DrawAudioSettings();
	void DrawGraphicsSettings();
	void RenderSettingsInMenu();

};

extern MainMenu mainMenuUI;
extern PauseMenu pauseMenuUI;

