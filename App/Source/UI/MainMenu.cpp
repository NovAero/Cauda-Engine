#include "MainMenu.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"

#include "Core/Utilities/ImGuiUtilities.h"

#include <Core/Modules.h>

#include "Config/EngineDefaults.h"
#include <Source/CaudaApp.h>

using namespace Cauda;

MainMenu mainMenuUI;
PauseMenu pauseMenuUI;

void MainMenu::Open(flecs::world& world)
{

}

void MainMenu::Draw(flecs::world& world)
{
	auto viewport = ImGui::GetMainViewport();
	ImVec2 viewportSize = viewport->Size;

	const float designWidth = 640.0f;
	const float designHeight = 360.0f;
	float uiScale = ImGui::GetIO().DisplaySize.y / designHeight;

	UI::SpriteRect playerCardOutlineRect(134.0f, 314.0f, 113.0f, 193.0f);
	ImVec2 outlineSpriteSize = ImVec2(playerCardOutlineRect.width * uiScale, playerCardOutlineRect.height * uiScale);
	
	const float atlasWidth = 512.0f;
	const float atlasHeight = 512.0f;

	static Texture* pixelUIAtlas = nullptr;
	if (!pixelUIAtlas)
	{
		pixelUIAtlas = ResourceLibrary::GetTexture("pixel_ui_512");
		if (!pixelUIAtlas)
		{
			pixelUIAtlas = ResourceLibrary::LoadTexture("pixel_ui_512", "textures/pixel_ui_512.png");
		}
	}

	static Texture* menuUIAtlas = nullptr;
	if (!menuUIAtlas)
	{
		menuUIAtlas = ResourceLibrary::GetTexture("menu_ui_512");
		if (!menuUIAtlas)
		{
			menuUIAtlas = ResourceLibrary::LoadTexture("menu_ui_512", "textures/menu_ui_512.png");
		}
	}
	
	static Texture* titleCardTexture = nullptr;
	if (!titleCardTexture)
	{
		titleCardTexture = ResourceLibrary::GetTexture("titlecard");
		if (!titleCardTexture)
		{
			titleCardTexture = ResourceLibrary::LoadTexture("titlecard", "textures/tumulttitlecard.png");
		}
	}

	ImGui::SetNextItemAllowOverlap();
	UI::BeginFullscreenOverlay("##main_menu_overlay");

	ImVec2 titleCardPos = UI::GetAnchorPosition(UI::Anchor::TopCentre, ImVec2(-620, 56));
	ImVec2 titleCardSize = ImVec2(1240, 874);
	
	UI::DrawImage(
		(ImTextureID)(uintptr_t)titleCardTexture->GetHandle(),
		titleCardPos,
		titleCardSize);


		if (m_showingSettings)
		{
			RenderSettingsInMenu((ImTextureID)(uintptr_t)pixelUIAtlas->GetHandle());
		}
		else if (m_showingCredits)
		{
			RenderCredits();
		}
		else
		{
			ImGui::SetNextWindowFocus();
			ImGui::SetNextItemAllowOverlap();
			if (UI::BeginOverlay("##menu_buttons", UI::Anchor::BottomCentre, ImVec2(836, 300), ImVec2(0, 0)))
			{
				float startY = ImGui::GetCursorScreenPos().y;
				UI::SpriteRect buttonSpriteRect(244, 195, 66.0f, 35.0f);

				ImVec2 buttonSize = ImVec2(66.0f * uiScale, 35.0f * uiScale);
				float buttonGap = 16.0f * uiScale;

				float totalButtonsWidth = (buttonSize.x * 3) + (buttonGap * 2);

				float buttonStartX = (viewportSize.x - totalButtonsWidth) * 0.5f;
				float buttonY = startY + outlineSpriteSize.y + (32.0f * uiScale);

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.05f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));


				static bool lost_focus = false;

				if (lost_focus)
				{
					ImGui::SetKeyboardFocusHere();
					lost_focus = false;
				}

				int item_focused = -1;

				ImVec2 buttonPos = ImGui::GetCursorScreenPos();
				UI::DrawImageAtlasRect(
					(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
					buttonPos,
					buttonSize,
					buttonSpriteRect,
					atlasWidth,
					atlasHeight,
					ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

				ImGui::PushFont(Fonts::Custom);

				if (ImGui::Button("Play", buttonSize))
				{
					SDL_PushEvent(&GameEvents::g_LoadLobbyEvent);
				}
				if (ImGui::IsItemFocused())
				{
					item_focused = 0;
				}
				ImGui::SameLine();

				buttonPos = ImGui::GetCursorScreenPos();
				UI::DrawImageAtlasRect(
					(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
					buttonPos,
					buttonSize,
					buttonSpriteRect,
					atlasWidth,
					atlasHeight,
					ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

				if (ImGui::Button("Settings", buttonSize))
				{
					m_showingSettings = true;
				}
				if (ImGui::IsItemFocused())
				{
					item_focused = 1;
				}
				ImGui::SameLine();

				buttonPos = ImGui::GetCursorScreenPos();
				UI::DrawImageAtlasRect(
					(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
					buttonPos,
					buttonSize,
					buttonSpriteRect,
					atlasWidth,
					atlasHeight,
					ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

				if (ImGui::Button("Credits", buttonSize))
				{
					m_showingCredits = true;
				}
				if (ImGui::IsItemFocused())
				{
					item_focused = 2;
				}

				ImGui::SameLine();

				buttonPos = ImGui::GetCursorScreenPos();
				UI::DrawImageAtlasRect(
					(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
					buttonPos,
					buttonSize,
					buttonSpriteRect,
					atlasWidth,
					atlasHeight,
					ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
				if (ImGui::Button("Exit To Desktop", buttonSize))
				{
					SDL_PushEvent(&GameEvents::g_ButtonEvent_ExitToDesktop);
				}
				if (ImGui::IsItemFocused())
				{
					item_focused = 3;
				}

				ImGui::PopFont();
				ImGui::PopStyleColor(4);
			}
			UI::EndOverlay();
		}

	UI::EndFullscreenOverlay();
}

void MainMenu::Close(flecs::world& world)
{
}

void MainMenu::RenderSettingsInMenu(ImTextureID atlasTexture)
{

	auto viewport = ImGui::GetMainViewport();
	ImVec2 viewportSize = viewport->Size;

	const float designWidth = 640.0f;
	const float designHeight = 360.0f;
	float uiScale = ImGui::GetIO().DisplaySize.y / designHeight;

	UI::SpriteRect menuBoxRect(2, 213, 200, 116);

	static int activeSettingsTab = 0;

	const float atlasWidth = 512.0f;
	const float atlasHeight = 512.0f;

	ImVec2 cardSpriteSize = ImVec2(menuBoxRect.width * uiScale, menuBoxRect.height * uiScale);

	ImVec2 menuBGPos = UI::GetAnchorPosition(UI::Anchor::BottomCentre, ImVec2(-(cardSpriteSize.x / 2), -(cardSpriteSize.y + 32)));

	ImVec2 settingsSize = ImVec2(cardSpriteSize.x - 80.f, cardSpriteSize.y - 32.f);

	static Texture* menuUIAtlas = nullptr;
	if (!menuUIAtlas)
	{
		menuUIAtlas = ResourceLibrary::GetTexture("menu_ui_512");
		if (!menuUIAtlas)
		{
			menuUIAtlas = ResourceLibrary::LoadTexture("menu_ui_512", "textures/menu_ui_512.png");
		}
	}

	if (InputSystem::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) && !m_handlingButtonEvent)
	{
		m_handlingButtonEvent = true;
		m_currentSettingsTab = m_currentSettingsTab == 0 ? 2 : m_currentSettingsTab - 1;
	}
	if (InputSystem::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) && !m_handlingButtonEvent)
	{
		m_handlingButtonEvent = true;
		m_currentSettingsTab = m_currentSettingsTab == 2 ? 0 : m_currentSettingsTab + 1;
	}

	if (!InputSystem::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) && !InputSystem::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
	{
		m_handlingButtonEvent = false;
	}

	UI::DrawImageAtlasRect(
		(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
		menuBGPos,
		cardSpriteSize,
		menuBoxRect,
		atlasWidth,
		atlasHeight,
		ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

	if (UI::BeginOverlay("##menu_items", UI::Anchor::BottomCentre, settingsSize, ImVec2(0, -32)))
	{
		UI::SpriteRect LBSpriteRect(399, 55, 30, 14);
		UI::SpriteRect RBSpriteRect(433, 55, 30, 14);
		UI::SpriteRect backButtonSpriteRect(245, 242, 31, 15);

		ImVec2 tabButtonSize = ImVec2(60, 28);
		ImVec2 backButtonsize = ImVec2(62, 30);

		ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
		ImVec2 contentAvailable = ImGui::GetContentRegionAvail();

		float paddingX = 12.f;
		float paddingY = 12.f;

		ImVec2 rbButtonPos = ImVec2(cursor_pos.x + contentAvailable.x - tabButtonSize.x - paddingX, cursor_pos.y + paddingY);
		ImVec2 lbButtonPos = ImVec2(cursor_pos.x + paddingX, cursor_pos.y + paddingY);

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.05f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

		contentAvailable.x -= tabButtonSize.x * 2;

		ImVec2 tabHeaderCentreGap = ImVec2((contentAvailable.x * 0.25f), cursor_pos.y + paddingY + tabButtonSize.y * 0.5f);

		UI::DrawImageAtlasRect((ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
			lbButtonPos,
			tabButtonSize,
			LBSpriteRect,
			atlasWidth,
			atlasHeight);

		ImGui::PushFont(Fonts::Custom);

		ImGui::SetCursorScreenPos(lbButtonPos);

		if (ImGui::Button("##LB", tabButtonSize))
		{
			m_currentSettingsTab = m_currentSettingsTab == 0 ? 2 : m_currentSettingsTab - 1;
		}

		cursor_pos = ImGui::GetCursorScreenPos();

		UI::DrawImageAtlasRect((ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
			rbButtonPos,
			tabButtonSize,
			RBSpriteRect,
			atlasWidth,
			atlasHeight);

		ImGui::SetCursorScreenPos(rbButtonPos);

		if (ImGui::Button("##RB", tabButtonSize))
		{
			m_currentSettingsTab = m_currentSettingsTab == 2 ? 0 : m_currentSettingsTab + 1;
		}

		cursor_pos.x += tabHeaderCentreGap.x + tabButtonSize.x ;
		cursor_pos.y = tabHeaderCentreGap.y;

		ImGui::PopStyleColor(1);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 0.5, 0.5, 1.f));

		if (m_currentSettingsTab == 0)
		{
			ImGui::PopStyleColor(1);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1.f));
		}

		UI::TextCentred("General", cursor_pos);

		ImGui::PopStyleColor(1);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 0.5, 0.5, 1.f));

		if (m_currentSettingsTab == 1)
		{
			ImGui::PopStyleColor(1);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1.f));
		}

		cursor_pos.x += tabHeaderCentreGap.x;

		UI::TextCentred("Audio", cursor_pos);

		ImGui::PopStyleColor(1);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 0.5, 0.5, 1.f));

		cursor_pos.x += tabHeaderCentreGap.x;

		if (m_currentSettingsTab == 2)
		{
			ImGui::PopStyleColor(1);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1.f));
		}

		UI::TextCentred("Graphics", cursor_pos);

		ImGui::PopStyleColor(1);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1.f));

		cursor_pos = ImGui::GetCursorScreenPos();
		cursor_pos.y += 6.f;
		ImGui::SetCursorScreenPos(cursor_pos);
		switch (m_currentSettingsTab)
		{
		case 0:
			DrawGeneralSettings();
			break;
		case 1:
			DrawAudioSettings();
			break;
		case 2:
			DrawGraphicsSettings();
			break;
		}

		cursor_pos = ImGui::GetCursorScreenPos();

		UI::DrawImageAtlasRect((ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
			cursor_pos,
			backButtonsize,
			backButtonSpriteRect,
			atlasWidth,
			atlasHeight);

		if (ImGui::Button("##Back", backButtonsize) || InputSystem::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_EAST))
		{
			m_showingSettings = false;
		}

		ImGui::PopStyleColor(4);
		ImGui::PopFont();

	}
	UI::EndOverlay();
}

void MainMenu::RenderCredits()
{
	auto viewport = ImGui::GetMainViewport();
	ImVec2 viewportSize = viewport->Size;

	const float designWidth = 640.0f;
	const float designHeight = 360.0f;
	float uiScale = ImGui::GetIO().DisplaySize.y / designHeight;

	const float atlasWidth = 512.0f;
	const float atlasHeight = 512.0f;

	static Texture* menuUIAtlas = nullptr;
	if (!menuUIAtlas)
	{
		menuUIAtlas = ResourceLibrary::GetTexture("menu_ui_512");
		if (!menuUIAtlas)
		{
			menuUIAtlas = ResourceLibrary::LoadTexture("menu_ui_512", "textures/menu_ui_512.png");
		}
	}

	UI::SpriteRect creditsBGSpriteRect(1, 337, 261, 109);
	UI::SpriteRect backButtonSpriteRect(245, 242, 31, 15);
	ImVec2 backButtonsize = ImVec2(62, 30);

	ImVec2 creditsSize = ImVec2(724, 256);

	ImVec2 cardSpriteSize = ImVec2(creditsBGSpriteRect.width * uiScale, creditsBGSpriteRect.height * uiScale);

	ImVec2 creditsBGPos = UI::GetAnchorPosition(UI::Anchor::BottomCentre, ImVec2(-(cardSpriteSize.x / 2), -(cardSpriteSize.y + 116)));

	UI::DrawImageAtlasRect(
		(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
		creditsBGPos,
		cardSpriteSize,
		creditsBGSpriteRect,
		atlasWidth,
		atlasHeight,
		ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

	if (UI::BeginOverlay("##credits_page", UI::Anchor::BottomCentre, creditsSize, ImVec2(0, -128)))
	{
		ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
		ImVec2 contentAvail = ImGui::GetContentRegionAvail();

		ImVec2 centrePos = ImVec2(cursor_pos.x + (contentAvail.x * 0.5), cursor_pos.y + 10.f);

		if (Fonts::Custom)
			ImGui::PushFont(Fonts::Custom);

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.05f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

		UI::TextCentred("TUMULT - A game by Perfect Parry", centrePos);

		centrePos.y += 45.f;
		UI::TextCentred("Keamu Hill - Programming, Techincal Art, UI/UX", centrePos);

		centrePos.y += 45.f;
		centrePos.x -= contentAvail.x * 0.23;
		UI::TextCentred("Luke Jones - Producer, Design, Modelling", centrePos);

		centrePos.x += contentAvail.x * 0.5;
		UI::TextCentred("Kyran Donato - Programming, UI/UX", centrePos);

		centrePos.y += 45.f;
		centrePos.x -= contentAvail.x * 0.5;
		UI::TextCentred("Douglas Mathrick - Design, Sounds", centrePos);

		centrePos.x += contentAvail.x * 0.5;
		UI::TextCentred("Jevan Soh - Additional Programming", centrePos);

		cursor_pos = ImGui::GetCursorScreenPos();
		cursor_pos.x += (contentAvail.x * 0.5) - (backButtonsize.x * 0.5);
		cursor_pos.y += 6.f;
		ImGui::SetCursorScreenPos(cursor_pos);

		UI::DrawImageAtlasRect((ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
			cursor_pos,
			backButtonsize,
			backButtonSpriteRect,
			atlasWidth,
			atlasHeight);

		if (ImGui::Button("##Back", backButtonsize) || InputSystem::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_EAST))
		{
			m_showingCredits = false;
		}

		ImGui::PopStyleColor(4);
		ImGui::PopFont();
	}
	UI::EndOverlay();
}

void MainMenu::DrawGeneralSettings()
{
	ImGui::Separator();
	if (ImGui::Button("Toggle Gamepad Rumble :"))
	{
		Cauda::Application::Get().SetRumbleEnabled(!Cauda::Application::Get().IsRumbleEnabled());
	}
	ImGui::SameLine();
	ImGui::Text(Cauda::Application::Get().IsRumbleEnabled() ? "Enabled" : "Disabled");
}

void MainMenu::DrawAudioSettings()
{
	auto world = Cauda::Application::GetWorld();

	ImGui::Separator();
	if (auto audioModule = world.try_get_mut<AudioModule>())
	{
		audioModule->DrawAudioMixerInWindow();
	}
}

void MainMenu::DrawGraphicsSettings()
{
	ImGui::Separator();
	auto cameraModule = Cauda::Application::GetWorld().try_get_mut<CameraModule>();
	auto camera = cameraModule->GetMainCamera();
	static bool lowRes = false;
	if (ImGui::Button("Toggle VSync :"))
	{
		Cauda::Application::Get().ToggleVsync();
	}
	ImGui::SameLine();
	ImGui::TextColored(ImVec4{ 0,0,0,1 }, Cauda::Application::Get().GetVsync() ? "On" : "Off");

	if (ImGui::Button("Low Res/High Res :"))
	{
		lowRes = !lowRes;
		if (camera)
		{
			if (auto camComp = camera.try_get_mut<CameraComponent>())
			{
				camComp->pixelScale = lowRes ? 2 : 1;
			}
			if (auto postProcess = camera.try_get_mut<PostProcessComponent>())
			{
				postProcess->edgeWidth = lowRes ? 1 : 2;
			}
		}
	}
	ImGui::SameLine();
	ImGui::TextColored(ImVec4{ 0,0,0,1 }, lowRes ? "Low" : "High");

}











void PauseMenu::Draw(bool* p_open)
{
	auto viewport = ImGui::GetMainViewport();
	ImVec2 viewportSize = viewport->Size;

	const float designWidth = 640.0f;
	const float designHeight = 360.0f;
	float uiScale = ImGui::GetIO().DisplaySize.y / designHeight;

	static Texture* menuUIAtlas = nullptr;
	if (!menuUIAtlas)
	{
		menuUIAtlas = ResourceLibrary::GetTexture("menu_ui_512");
		if (!menuUIAtlas)
		{
			menuUIAtlas = ResourceLibrary::LoadTexture("menu_ui_512", "textures/menu_ui_512.png");
		}
	}

	UI::SpriteRect menuBoxRect(2, 213, 200, 116);

	ImGuiTableFlags tableFlags =
		ImGuiTableFlags_NoSavedSettings;

	static int activeSettingsTab = 0;

	const float atlasWidth = 512.0f;
	const float atlasHeight = 512.0f;

	ImVec2 cardSpriteSize = ImVec2(menuBoxRect.width * uiScale, menuBoxRect.height * uiScale);

	float startY = ImGui::GetCursorScreenPos().y;
	UI::SpriteRect buttonSpriteRect(244, 195, 66.0f, 35.0f);

	ImVec2 buttonSize = ImVec2(66.0f * uiScale, 35.0f * uiScale);
	float buttonGap = 16.0f * uiScale;

	ImVec2 menuSize = ImVec2(buttonSize.x + 16.f, (buttonSize.y + buttonGap) * 3);

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.05f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

	ImGui::PushFont(Fonts::Custom);

	if (UI::BeginFullscreenOverlay("##pause_overlay"))
	{
		if (m_showingSettings)
		{
			RenderSettingsInMenu();
		}
		else
		{
			if (UI::BeginOverlay("##menu_items", UI::Anchor::Centre, menuSize, ImVec2(0, 64)))
			{
				static bool lost_focus = false;

				if (lost_focus)
				{
					ImGui::SetKeyboardFocusHere();
					lost_focus = false;
				}

				int item_focused = -1;

				ImVec2 buttonPos = ImGui::GetCursorScreenPos();
				UI::DrawImageAtlasRect(
					(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
					buttonPos,
					buttonSize,
					buttonSpriteRect,
					atlasWidth,
					atlasHeight,
					ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

				ImVec2 overlaySize = ImGui::GetWindowSize();

				if (ImGui::Button("Continue", buttonSize))
				{
					//Unpause
					if (p_open)*p_open = false;

					UI::EndOverlay();
					UI::EndFullscreenOverlay();

					ImGui::PopFont();
					ImGui::PopStyleColor(4);

					return;
				}
				if (ImGui::IsItemFocused())
				{
					item_focused = 0;
				}

				buttonPos = ImGui::GetCursorScreenPos();
				UI::DrawImageAtlasRect(
					(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
					buttonPos,
					buttonSize,
					buttonSpriteRect,
					atlasWidth,
					atlasHeight,
					ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

				if (ImGui::Button("Settings", buttonSize))
				{
					m_showingSettings = true;

					//auto audioModule = m_world->try_get_mut<AudioModule>();
					//audioModule->TryPlaySound2D("SW_MenuSelect", AudioChannel::EFFECTS);

				}
				if (ImGui::IsItemFocused())
				{
					item_focused = 1;
				}

				buttonPos = ImGui::GetCursorScreenPos();

				UI::DrawImageAtlasRect(
					(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
					buttonPos,
					buttonSize,
					buttonSpriteRect,
					atlasWidth,
					atlasHeight,
					ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

				if (ImGui::Button("Exit To Menu", buttonSize))
				{
					//Exit to previous game state (menu or closed)
					if (p_open)*p_open = false;

					SDL_PushEvent(&GameEvents::g_ButtonEvent_ExitToMenu);

					UI::EndOverlay();
					UI::EndFullscreenOverlay();

					ImGui::PopFont();
					ImGui::PopStyleColor(4);

					return;
				}
				if (ImGui::IsItemFocused())
				{
					item_focused = 2;
				}
			}
			UI::EndOverlay();
		}

		ImGui::PopFont();
		ImGui::PopStyleColor(4);

	}
	UI::EndFullscreenOverlay();
}

void PauseMenu::Open(flecs::world& world)
{
	m_world = &world;
}

void PauseMenu::Close()
{
	m_world = nullptr;
}

PauseMenu::~PauseMenu()
{
	m_world = nullptr;
}

void PauseMenu::DrawGeneralSettings()
{
	ImGui::Separator();
	if (ImGui::Button("Gamepad Rumble :"))
	{
		Cauda::Application::Get().SetRumbleEnabled(!Cauda::Application::Get().IsRumbleEnabled());
	}
	ImGui::SameLine();
	ImGui::Text(Cauda::Application::Get().IsRumbleEnabled() ? "Enabled" : "Disabled");
}

void PauseMenu::DrawAudioSettings()
{
	auto world = Cauda::Application::GetWorld();

	ImGui::Separator();
	if (auto audioModule = world.try_get_mut<AudioModule>())
	{
		audioModule->DrawAudioMixerInWindow();
	}
}

void PauseMenu::DrawGraphicsSettings()
{
	ImGui::Separator();
	auto cameraModule = Cauda::Application::GetWorld().try_get_mut<CameraModule>();
	auto camera = cameraModule->GetMainCamera();
	static bool lowRes = false;
	if (ImGui::Button("Toggle VSync :"))
	{
		Cauda::Application::Get().ToggleVsync();
	}
	ImGui::SameLine();
	ImGui::TextColored(ImVec4{ 0,0,0,1 }, Cauda::Application::Get().GetVsync() ? "On" : "Off");

	if (ImGui::Button("Low Res/High Res :"))
	{
		lowRes = !lowRes;
		if (camera)
		{
			if (auto camComp = camera.try_get_mut<CameraComponent>())
			{
				camComp->pixelScale = lowRes ? 2 : 1;
			}
			if (auto postProcess = camera.try_get_mut<PostProcessComponent>())
			{
				postProcess->edgeWidth = lowRes ? 1 : 2;
			}
		}
	}
	ImGui::SameLine();
	ImGui::TextColored(ImVec4{ 0,0,0,1 }, lowRes ? "Low" : "High");
}

void PauseMenu::RenderSettingsInMenu()
{

	auto viewport = ImGui::GetMainViewport();
	ImVec2 viewportSize = viewport->Size;

	const float designWidth = 640.0f;
	const float designHeight = 360.0f;
	float uiScale = ImGui::GetIO().DisplaySize.y / designHeight;

	UI::SpriteRect menuBoxRect(2, 213, 200, 116);

	ImGuiTableFlags tableFlags =
		ImGuiTableFlags_NoSavedSettings;

	static int activeSettingsTab = 0;

	const float atlasWidth = 512.0f;
	const float atlasHeight = 512.0f;

	ImVec2 cardSpriteSize = ImVec2(menuBoxRect.width * uiScale, menuBoxRect.height * uiScale);

	ImVec2 menuBGPos = UI::GetAnchorPosition(UI::Anchor::Centre, ImVec2(-(cardSpriteSize.x * 0.5f), -(cardSpriteSize.y * 0.5f)));

	ImVec2 settingsSize = ImVec2(cardSpriteSize.x - 80.f, cardSpriteSize.y - 32.f);

	static Texture* menuUIAtlas = nullptr;
	if (!menuUIAtlas)
	{
		menuUIAtlas = ResourceLibrary::GetTexture("menu_ui_512");
		if (!menuUIAtlas)
		{
			menuUIAtlas = ResourceLibrary::LoadTexture("menu_ui_512", "textures/menu_ui_512.png");
		}
	}

	if (InputSystem::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) && !m_handlingButtonEvent)
	{
		m_handlingButtonEvent = true;
		m_currentSettingsTab = m_currentSettingsTab == 0 ? 2 : m_currentSettingsTab - 1;
	}
	if (InputSystem::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) && !m_handlingButtonEvent)
	{
		m_handlingButtonEvent = true;
		m_currentSettingsTab = m_currentSettingsTab == 2 ? 0 : m_currentSettingsTab + 1;
	}

	if (!InputSystem::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) && !InputSystem::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
	{
		m_handlingButtonEvent = false;
	}

	UI::DrawImageAtlasRect(
		(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
		menuBGPos,
		cardSpriteSize,
		menuBoxRect,
		atlasWidth,
		atlasHeight,
		ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

	if (UI::BeginOverlay("##menu_items", UI::Anchor::Centre, settingsSize, ImVec2(0, 16)))
	{
		UI::SpriteRect LBSpriteRect(399, 55, 30, 14);
		UI::SpriteRect RBSpriteRect(433, 55, 30, 14);
		UI::SpriteRect backButtonSpriteRect(245, 242, 31, 15);

		ImVec2 tabButtonSize = ImVec2(60, 28);
		ImVec2 backButtonsize = ImVec2(62, 30);

		ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
		ImVec2 contentAvailable = ImGui::GetContentRegionAvail();

		float paddingX = 12.f;
		float paddingY = 12.f;

		ImVec2 rbButtonPos = ImVec2(cursor_pos.x + contentAvailable.x - tabButtonSize.x - paddingX, cursor_pos.y + paddingY);
		ImVec2 lbButtonPos = ImVec2(cursor_pos.x + paddingX, cursor_pos.y + paddingY);

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.05f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

		contentAvailable.x -= tabButtonSize.x * 2;

		ImVec2 tabHeaderCentreGap = ImVec2((contentAvailable.x * 0.25f), cursor_pos.y + paddingY + tabButtonSize.y * 0.5f);

		UI::DrawImageAtlasRect((ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
			lbButtonPos,
			tabButtonSize,
			LBSpriteRect,
			atlasWidth,
			atlasHeight);

		ImGui::PushFont(Fonts::Custom);

		ImGui::SetCursorScreenPos(lbButtonPos);

		if (ImGui::Button("##LB", tabButtonSize))
		{
			m_currentSettingsTab = m_currentSettingsTab == 0 ? 2 : m_currentSettingsTab - 1;
		}

		cursor_pos = ImGui::GetCursorScreenPos();

		UI::DrawImageAtlasRect((ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
			rbButtonPos,
			tabButtonSize,
			RBSpriteRect,
			atlasWidth,
			atlasHeight);

		ImGui::SetCursorScreenPos(rbButtonPos);

		if (ImGui::Button("##RB", tabButtonSize))
		{
			m_currentSettingsTab = m_currentSettingsTab == 2 ? 0 : m_currentSettingsTab + 1;
		}

		cursor_pos.x += tabHeaderCentreGap.x + tabButtonSize.x;
		cursor_pos.y = tabHeaderCentreGap.y;

		ImGui::PopStyleColor(1);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 0.5, 0.5, 1.f));

		if (m_currentSettingsTab == 0)
		{
			ImGui::PopStyleColor(1);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1.f));
		}

		UI::TextCentred("General", cursor_pos);

		ImGui::PopStyleColor(1);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 0.5, 0.5, 1.f));

		if (m_currentSettingsTab == 1)
		{
			ImGui::PopStyleColor(1);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1.f));
		}

		cursor_pos.x += tabHeaderCentreGap.x;

		UI::TextCentred("Audio", cursor_pos);

		ImGui::PopStyleColor(1);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5, 0.5, 0.5, 1.f));

		cursor_pos.x += tabHeaderCentreGap.x;

		if (m_currentSettingsTab == 2)
		{
			ImGui::PopStyleColor(1);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1.f));
		}

		UI::TextCentred("Graphics", cursor_pos);

		ImGui::PopStyleColor(1);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1.f));

		cursor_pos = ImGui::GetCursorScreenPos();
		cursor_pos.y += 6.f;
		ImGui::SetCursorScreenPos(cursor_pos);
		switch (m_currentSettingsTab)
		{
		case 0:
			DrawGeneralSettings();
			break;
		case 1:
			DrawAudioSettings();
			break;
		case 2:
			DrawGraphicsSettings();
			break;
		}

		cursor_pos = ImGui::GetCursorScreenPos();

		UI::DrawImageAtlasRect((ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
			cursor_pos,
			backButtonsize,
			backButtonSpriteRect,
			atlasWidth,
			atlasHeight);

		if (ImGui::Button("##Back", backButtonsize) || InputSystem::IsGamepadButtonDown(0, SDL_GAMEPAD_BUTTON_EAST))
		{
			m_showingSettings = false;
		}

		ImGui::PopStyleColor(4);
		ImGui::PopFont();

	}
	UI::EndOverlay();
}
