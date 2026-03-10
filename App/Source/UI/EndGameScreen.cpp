#include "EndGameScreen.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"

#include "Core/Utilities/ImGuiUtilities.h"

#include <Core/Modules.h>

#include <Core/Character/CharacterController.h>
#include <Core/Layers/GameLayer.h>

using namespace Cauda;

EndGameScreen endGameScreenUI;

void EndGameScreen::Open(flecs::world& world)
{

}

void EndGameScreen::Draw(flecs::world& world, bool* p_open)
{
	if (UI::BeginFullscreenOverlay("##end_game_overlay"))
	{
		auto viewport = ImGui::GetMainViewport();
		ImVec2 viewportSize = viewport->Size;

		const float designWidth = 640.0f;
		const float designHeight = 360.0f;
		float uiScale = ImGui::GetIO().DisplaySize.y / designHeight;


		ImVec4 playerColours[4] = {
			Colour::HumbleBlue,
			Colour::HumbleOrange,
			Colour::HumbleGreen,
			Colour::HumblePurple
		};

		ImVec4 inactivePlayerColours[4] = {
			ImVec4(playerColours[0].x * 0.5f, playerColours[0].y * 0.5f, playerColours[0].z * 0.5f, 1.f),
			ImVec4(playerColours[1].x * 0.5f, playerColours[1].y * 0.5f, playerColours[1].z * 0.5f, 1.f),
			ImVec4(playerColours[2].x * 0.5f, playerColours[2].y * 0.5f, playerColours[2].z * 0.5f, 1.f),
			ImVec4(playerColours[3].x * 0.5f, playerColours[3].y * 0.5f, playerColours[3].z * 0.5f, 1.f),
		};

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

		UI::SpriteRect playerCardRect(16.0f, 320.0f, 97.0f, 177.0f);
		UI::SpriteRect playerCardOutlineRect(134.0f, 314.0f, 113.0f, 193.0f);
		const float atlasWidth = 512.0f;
		const float atlasHeight = 512.0f;

		ImVec2 cardSpriteSize = ImVec2(playerCardRect.width * uiScale, playerCardRect.height * uiScale);
		ImVec2 outlineSpriteSize = ImVec2(playerCardOutlineRect.width * uiScale, playerCardOutlineRect.height * uiScale);

		float cardGap = 32.0f * uiScale;
		float totalCardsWidth = (cardSpriteSize.x * 4) + (cardGap * 3);

		float startX = floorf((viewportSize.x - totalCardsWidth) * 0.5f);

		float startY = floorf((viewportSize.y - cardSpriteSize.y) * 0.5f);


		UI::SpriteRect winnerCardRect(339, 145, 134, 35);
		ImVec2 winnerCardSpriteSize = ImVec2(winnerCardRect.width * uiScale, winnerCardRect.height * uiScale);
		ImVec2 centreCardPos = UI::GetAnchorPosition(UI::Anchor::TopCentre, ImVec2(0, (winnerCardSpriteSize.y * 0.5) + (32.f * uiScale)));

		UI::DrawImageAtlasRect(
			(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
			ImVec2(centreCardPos.x - (winnerCardSpriteSize.x * 0.5), centreCardPos.y - (winnerCardSpriteSize.y * 0.5)),
			winnerCardSpriteSize,
			winnerCardRect,
			atlasWidth,
			atlasHeight,
			Colour::Darken(playerColours[winnerID - 1]));

		std::string winnerString;
		switch (winnerID)
		{
		case 1:
			winnerString = "Blue Wins!";
			break;
		case 2:
			winnerString = "Orange Wins!";
			break;
		case 3:
			winnerString = "Green Wins!";
			break;
		case 4:
			winnerString = "Purple Wins!";
			break;
		}

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
		ImGui::SetWindowFontScale(1.66f);

		UI::TextCentred(winnerString.c_str(), centreCardPos);

		ImGui::SetWindowFontScale(1.0f);
		ImGui::PopStyleColor(1);

		if (UI::BeginOverlay("##player_grid", UI::Anchor::TopLeft, ImVec2(viewportSize.x, viewportSize.y), ImVec2(0, 0)))
		{
			int i = 0;

			for (auto entity : m_activePlayers)
			{
				std::string overlayName = "##playerui_";
				overlayName.append(entity.name().c_str());

				auto character = entity.try_get_mut<CharacterComponent>();

				UI::SpriteRect playerSpriteRectStart(304 + (i * 50), i < 2 ? 304 : 304 + 52, 50, 52);
				ImVec2 playerSpriteSize(playerSpriteRectStart.width * uiScale, playerSpriteRectStart.height * uiScale);

				ImVec2 childOffset = ImVec2(startX + i * (cardSpriteSize.x + cardGap), startY);

				if (character->characterID == winnerID)
				{
					childOffset.y -= 16 * uiScale;
				}

				ImVec2 cardOffset = ImVec2(
					childOffset.x + floorf((outlineSpriteSize.x - cardSpriteSize.x) * 0.5f),
					childOffset.y + floorf((outlineSpriteSize.y - cardSpriteSize.y) * 0.5f)
				);

				float edgeGap = (cardSpriteSize.x - playerSpriteSize.x) * 0.5f;

				ImVec2 playerSpritePos = ImVec2(cardOffset.x + edgeGap, cardOffset.y + edgeGap * 0.5f);

				ImVec2 outlineOffset = ImVec2(childOffset.x - (2.0f * uiScale), childOffset.y + (2.0f * uiScale));

				ImVec2 overlaySize = ImVec2(outlineSpriteSize.x, std::max(outlineSpriteSize.y, 400.0f));
				if (UI::BeginOverlay(overlayName.c_str(), UI::Anchor::TopLeft, overlaySize, childOffset))
				{
					UI::DrawImageAtlasRect(
						(ImTextureID)(uintptr_t)pixelUIAtlas->GetHandle(),
						cardOffset,
						cardSpriteSize,
						playerCardRect,
						atlasWidth,
						atlasHeight,
						ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

					UI::DrawImageAtlasRect(
						(ImTextureID)(uintptr_t)pixelUIAtlas->GetHandle(),
						outlineOffset,
						outlineSpriteSize,
						playerCardOutlineRect,
						atlasWidth,
						atlasHeight,
						playerColours[i]);

					UI::DrawImageAtlasRect(
						(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
						playerSpritePos,
						playerSpriteSize,
						playerSpriteRectStart,
						atlasWidth,
						atlasHeight,
						ImVec4(1, 1, 1, 1));

					if (auto stats = entity.try_get_mut<PlayerStatComponent>())
					{
						ImVec2 textPos = ImVec2(24 * uiScale + childOffset.x, 80 * uiScale + childOffset.y);
						float lineSpacing = 24;

						ImGui::PushFont(Fonts::Custom);

						ImGui::SetCursorScreenPos(textPos);

						ImGui::TextColored(ImVec4(0, 0, 0, 1), "Lives Used     : %i", 3 - stats->numLives);

						textPos.y += lineSpacing;
						ImGui::SetCursorScreenPos(textPos);
						ImGui::TextColored(ImVec4(0, 0, 0, 1), "Damage Dealt   : %.0f", stats->damageDealt);

						textPos.y += lineSpacing;
						ImGui::SetCursorScreenPos(textPos);
						ImGui::TextColored(ImVec4(0, 0, 0, 1), "Damage Taken   : %.0f", stats->damageTaken);

						textPos.y += lineSpacing;
						ImGui::SetCursorScreenPos(textPos);
						ImGui::TextColored(ImVec4(0, 0, 0, 1), "Shots Fired    : %i", stats->shotsFired);

						textPos.y += lineSpacing;
						ImGui::SetCursorScreenPos(textPos);
						ImGui::TextColored(ImVec4(0, 0, 0, 1), "Shots Hit      : %i", stats->shotsHit);

						textPos.y += lineSpacing;
						ImGui::SetCursorScreenPos(textPos);
						ImGui::TextColored(ImVec4(0, 0, 0, 1), "Accuracy %%     : %.2f",
							stats->shotsFired == 0 ? 0 : (100.f / stats->shotsFired) * stats->shotsHit);

						int mins = stats->timeAlive / 60;
						int secs = (stats->timeAlive - mins * 60);

						textPos.y += lineSpacing;
						ImGui::SetCursorScreenPos(textPos);
						ImGui::TextColored(ImVec4(0, 0, 0, 1), "Time Alive     : %i:%i%i", mins % 10, secs / 10, secs % 10);
						textPos.y += lineSpacing;
						ImGui::SetCursorScreenPos(textPos);
						ImGui::TextColored(ImVec4(0, 0, 0, 1), "Shield Bashes  : %i", stats->shieldBashes);

						textPos.y += lineSpacing;
						ImGui::SetCursorScreenPos(textPos);
						ImGui::TextColored(ImVec4(0, 0, 0, 1), "Arrows Parried : %i", stats->arrowsParried);

						textPos.y += lineSpacing;
						ImGui::SetCursorScreenPos(textPos);
						ImGui::TextColored(ImVec4(0, 0, 0, 1), "Ammo Collected : %i", stats->ammoPickedUp);

						textPos.y += lineSpacing;
						ImGui::SetCursorScreenPos(textPos);
						ImGui::TextColored(ImVec4(0, 0, 0, 1), "Ammo Hoarded   : %i", stats->ammoHoarded);

						ImGui::PopFont();
					}
				}
				UI::EndOverlay();
				++i;
			}
			for (i; i < 4; ++i)
			{

				std::string overlayName = "##playerui_" + std::to_string(i);

				UI::SpriteRect playerSpriteRectStart(304 + (i * 50), i < 2 ? 304 : 304 + 50, 50, 53);
				ImVec2 playerSpriteSize(playerSpriteRectStart.width* uiScale, playerSpriteRectStart.height* uiScale);

				ImVec2 childOffset = ImVec2(startX + i * (cardSpriteSize.x + cardGap), startY);
				
				ImVec2 cardOffset = ImVec2(
					childOffset.x + floorf((outlineSpriteSize.x - cardSpriteSize.x) * 0.5f),
					childOffset.y + floorf((outlineSpriteSize.y - cardSpriteSize.y) * 0.5f)
				);

				float edgeGap = (cardSpriteSize.x - playerSpriteSize.x) * 0.5f;

				ImVec2 playerSpritePos = ImVec2(cardOffset.x + edgeGap, cardOffset.y + edgeGap * 0.5f);

				ImVec2 outlineOffset = ImVec2(childOffset.x - (2.0f * uiScale), childOffset.y + (2.0f * uiScale));

				ImVec2 overlaySize = ImVec2(outlineSpriteSize.x, std::max(outlineSpriteSize.y, 400.0f));
				if (UI::BeginOverlay(overlayName.c_str(), UI::Anchor::TopLeft, overlaySize, childOffset))
				{
					UI::DrawImageAtlasRect(
						(ImTextureID)(uintptr_t)pixelUIAtlas->GetHandle(),
						cardOffset,
						cardSpriteSize,
						playerCardRect,
						atlasWidth,
						atlasHeight,
						ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

					UI::DrawImageAtlasRect(
						(ImTextureID)(uintptr_t)pixelUIAtlas->GetHandle(),
						outlineOffset,
						outlineSpriteSize,
						playerCardOutlineRect,
						atlasWidth,
						atlasHeight,
						inactivePlayerColours[i]);

					UI::DrawImageAtlasRect(
						(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
						playerSpritePos,
						playerSpriteSize,
						playerSpriteRectStart,
						atlasWidth,
						atlasHeight,
						ImVec4(0.5f, 0.5f, 0.5f, 0.5f));

					ImGui::PushFont(Fonts::Custom);

					ImVec2 textPos = ImVec2(114 + childOffset.x, 300 + childOffset.y);
					ImGui::SetCursorScreenPos(textPos);
					ImGui::TextColored(ImVec4(0, 0, 0, 1), "Inactive Player");
					ImGui::PopFont();

				}
				UI::EndOverlay();
			}
		}
		UI::EndOverlay();

		if (UI::BeginFullscreenOverlay("##button_overlay", 0.0f))
		{
			UI::SpriteRect buttonSpriteRect(244, 195, 66.0f, 35.0f);

			ImVec2 buttonSize = ImVec2(66.0f * uiScale, 35.0f * uiScale);
			float buttonGap = 16.0f * uiScale;

			float totalButtonsWidth = (buttonSize.x * 2) + buttonGap;

			float buttonStartX = (viewportSize.x - totalButtonsWidth) * 0.5f;
			float buttonY = startY + outlineSpriteSize.y + (16.0f * uiScale);

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

			static bool lost_focus = false;

			if (lost_focus)
			{
				ImGui::SetKeyboardFocusHere();
				lost_focus = false;
			}

			int item_focused = -1;

			ImVec2 buttonPos = ImVec2(buttonStartX, buttonY);
			UI::DrawImageAtlasRect(
				(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
				buttonPos,
				buttonSize,
				buttonSpriteRect,
				atlasWidth,
				atlasHeight,
				ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

			ImGui::PushFont(Fonts::Custom);

			ImGui::SetCursorScreenPos(buttonPos);
			if (ImGui::Button("Exit to Menu", buttonSize))
			{
				SDL_PushEvent(&GameEvents::g_ButtonEvent_ExitToMenu);
			}
			if (ImGui::IsItemFocused()) item_focused = 0;

			buttonPos = ImVec2(buttonStartX + buttonSize.x + buttonGap, buttonY);
			UI::DrawImageAtlasRect(
				(ImTextureID)(uintptr_t)menuUIAtlas->GetHandle(),
				buttonPos,
				buttonSize,
				buttonSpriteRect,
				atlasWidth,
				atlasHeight,
				ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

			ImGui::SetCursorScreenPos(buttonPos);
			if (ImGui::Button("New Round", buttonSize))
			{
				SDL_PushEvent(&GameEvents::g_ButtonEvent_NewRound);
			}
			if (ImGui::IsItemFocused()) item_focused = 1;

			if (item_focused == -1)
			{
				lost_focus = true;
			}

			ImGui::PopFont();
			ImGui::PopStyleColor(4);
		}
		UI::EndFullscreenOverlay();
	}
	UI::EndFullscreenOverlay();

}

void EndGameScreen::Close()
{

}

void EndGameScreen::OnGameEnd(const std::vector<CharacterController*>& controllers)
{
	SDL_PushEvent(&GameEvents::g_EndRoundEvent);

	m_activePlayers.clear();

	int playersAlive = 0;


	for (auto controller : controllers)
	{
		if (controller->IsPossessing())
		{
			m_activePlayers.push_back(controller->GetPosessedPawn());
		}
	}

	std::sort(m_activePlayers.begin(), m_activePlayers.end(), [](flecs::entity entityA, flecs::entity entityB)
		{
			auto characterA = entityA.try_get<CharacterComponent>();
			auto characterB = entityB.try_get<CharacterComponent>();

			return characterA->characterID < characterB->characterID;
		});

	float highestScore = -FLT_MAX;

	for (auto player : m_activePlayers)
	{
		auto character = player.try_get<CharacterComponent>();
		auto stats = player.try_get<PlayerStatComponent>();

		float score = CalculateCombatScore(player);

		if (score > highestScore)
		{
			winnerID = character->characterID;
			highestScore = score;
		}
	}
}

float EndGameScreen::CalculateCombatScore(flecs::entity player)
{
	if (auto stats = player.try_get_mut<PlayerStatComponent>())
	{
		int deaths = 3 - stats->numLives;
		if(stats->numLives == 0) return -FLT_MAX;

		float combatScore =	((stats->damageDealt - stats->damageTaken) / deaths == 0 ? 1 : deaths)
			+ (stats->shotsFired == 0 ? 0 : (100.f / stats->shotsFired) * stats->shotsHit) +
			((stats->shieldBashes - stats->timesShieldBashed) * 2)
			+ (stats->arrowsParried * 15) + (stats->harpoonsParried * 50)
			+ (stats->ammoHoarded * 0.33f);
		combatScore *= (stats->timeAlive / 60.f);
		return combatScore;
	}
	return -FLT_MAX;
}
