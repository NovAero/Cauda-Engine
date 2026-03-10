#pragma once

#include <ThirdParty/Flecs.h>
#include <string>

struct GameMenu
{
	void Open();
	void Draw(float& gameTimer, std::string& levelName, bool* p_open = nullptr);
	void Close();

private:

};

extern GameMenu gameMenuUI;