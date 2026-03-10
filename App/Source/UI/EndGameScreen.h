#pragma once

#include <ThirdParty/Flecs.h>
#include <vector>
#include <string>

class CharacterController;

struct EndGameScreen
{
public:
	void Open(flecs::world& world);
	void Draw(flecs::world& world, bool* p_open = nullptr);
	void Close();

	void OnGameEnd(const std::vector<CharacterController*>& controllers);

protected:

	std::string winner = "None";
	std::vector<flecs::entity> m_activePlayers;

	int winnerID = 0;


	float CalculateCombatScore(flecs::entity player);
};

extern EndGameScreen endGameScreenUI;