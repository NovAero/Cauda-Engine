#pragma once

#include <string>

#define MAX_NAME_STRING 256
#define MAX_SHADER_INCLUDE_DEPTH 32
#define WORLD_KILL_Y -100

constexpr int MAX_GAMEPAD_SOCKETS = 4;
constexpr float GAMEPAD_DEADZONE = 0.2f;

const std::string g_ConfigPath = "Config/";
const std::string g_ContentPath = "Content/";

namespace Cauda
{
	struct Command
	{
		virtual void Execute() = 0;
		virtual void Undo() = 0;
		virtual ~Command() {}
	};
}