#pragma once
#include <string>
#include "imgui.h"

class Logger {
private:
	static Logger* s_LogInst;

public:
	static Logger* Instance() { return s_LogInst; }

public:
	Logger();
	~Logger();

	static void PrintLog(const char* fmt, ...);

	static std::string LogDirectory();
	static std::string LogFile();

	static void PrintDebugSeperator();
	
};

class EditorConsole
{
	static EditorConsole* s_instance;
	friend class Logger;
public:
	EditorConsole();
	~EditorConsole();

	// Passing 'bool* p_open' displays a Close button on the upper-right corner of the window,
	// the pointed value will be set to false when the button is pressed.
	void OnImGuiRender(bool* p_open = nullptr) { DrawConsoleWindow(p_open); }

	void AddConsoleMessage(const std::string& message, bool log = true);
	void AddConsoleError(const std::string& message, bool log = false);
	void AddConsoleWarning(const std::string& message, bool log = false);
	void AddConsoleInfo(const std::string& message, bool log = false);

	static EditorConsole* Inst() { if (!s_instance) s_instance = new EditorConsole(); return s_instance; }

private:
	//bool m_showConsole = true;
	std::vector<std::string> m_consoleLines;
	char m_consoleInput[256];

	// Passing 'bool* p_open' displays a Close button on the upper-right corner of the window,
	// the pointed value will be set to false when the button is pressed.
	void DrawConsoleWindow(bool* p_open = nullptr);
	static ImVec4 GetMessageColour(const std::string& message);

};
