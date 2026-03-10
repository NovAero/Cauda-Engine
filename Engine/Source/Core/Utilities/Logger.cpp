#include "cepch.h"
#include <fstream>
#include <ShlObj.h>
#include <cstdio>
#include <TlHelp32.h>
#include "Core/Utilities/Time.h"
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

Logger* Logger::s_LogInst;
EditorConsole* EditorConsole::s_instance;

static std::string s_LogFile;
const fs::path LogPath = "Logs\\";

Logger::Logger()
{
	s_LogInst = this;
	PrintLog("Logger Initialised");
}

Logger::~Logger()
{

}

void Logger::PrintLog(const char* fmt, ...)
{
#ifndef _DISTRIB

	char buff[4096];
	va_list args;

	va_start(args, fmt);
	vsprintf_s(buff, fmt, args);
	va_end(args);

#ifdef _CONSOLE
	std::cout << "[Logger] " << buff << '\n';
#endif // 

	std::fstream outFile;

	outFile.open(std::string(LogDirectory() + "\\" + LogFile()), std::ios_base::app);

	if (outFile.is_open()) {
		std::string s = buff;
		outFile << SysTime::GetTimeString(false).c_str() << " " << s << '\n';
		outFile.close();
	}
	else {
		MessageBox(NULL, L"Unable to open log file. &PrintLog", L"Log Error", MB_OK);
	}
#endif // !_DISTRIBUTION
}

std::string Logger::LogDirectory()
{
#ifndef _DISTRIB

	if (!fs::exists(LogPath)) {
		fs::create_directory(LogPath);
	}

	return LogPath.string();
#endif // !_DISTRIB
}

std::string Logger::LogFile()
{
#ifndef _DISTRIB
	if (s_LogFile.size() == 0)
	{
		std::stringstream ss;
		ss << "CaudaEngine" /*PerGameSettings::GameName()*/;
		ss << SysTime::GetDateTimeString(true);
		ss << ".log";
		ss >> s_LogFile;
	}
	return s_LogFile.c_str();
#endif // !_DISTRIB
}

VOID Logger::PrintDebugSeperator()
{
	std::string s = "--------------------------------------------------------------\n";

#ifdef _DEBUG
	std::fstream outFile;

	std::string filePath = LogDirectory() + "\\" + LogFile();
	outFile.open(filePath, std::ios_base::app);

	if (outFile.is_open()) {
		outFile << s;
		outFile.close();
	}
	else {
		MessageBox(NULL, L"Unable to open log file. &PrintDebugSeparator", L"Log Error", MB_OK);
	}
#endif
}


EditorConsole::EditorConsole()
{
    if (!s_instance)
        s_instance = this;
}

EditorConsole::~EditorConsole()
{
}

void EditorConsole::DrawConsoleWindow(bool* p_open)
{
    if (p_open && !(*p_open)) return;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Console", p_open))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear")) m_consoleLines.clear();

    ImGui::SameLine();
    if (ImGui::Button("Test"))
    {
        AddConsoleMessage("Test message " + std::to_string(m_consoleLines.size()), false);
    }
    ImGui::SameLine();
    if (ImGui::Button("Warning"))
    {
        AddConsoleWarning("This is a warning");
    }
    ImGui::SameLine();
    if (ImGui::Button("Error"))
    {
        AddConsoleError("This is an error");
    }

    ImGui::Separator();

    const float footer_height = ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("Output", ImVec2(0, -footer_height), ImGuiChildFlags_Borders))
    {
        for (const auto& line : m_consoleLines)
        {
            ImVec4 colour = GetMessageColour(line);

            if (colour.w > 0)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, colour);
                ImGui::TextUnformatted(line.c_str());
                ImGui::PopStyleColor();
            }
            else ImGui::TextUnformatted(line.c_str());
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();


    if (ImGui::InputText("##Input", m_consoleInput, sizeof(m_consoleInput), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if (strlen(m_consoleInput) > 0)
        {
            AddConsoleMessage("> " + std::string(m_consoleInput), false);
            memset(m_consoleInput, 0, sizeof(m_consoleInput));
        }
    }

    ImGui::End();
}

//void EditorConsole::DrawConsole()
//{
//    if (!m_showConsole) return;
//
//    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
//    if (!ImGui::Begin("Console", &m_showConsole))
//    {
//        ImGui::End();
//        return;
//    }
//
//    if (ImGui::Button("Clear"))
//    {
//        m_consoleLines.clear();
//    }
//    ImGui::SameLine();
//    if (ImGui::Button("Test"))
//    {
//        AddConsoleMessage("Test message " + std::to_string(m_consoleLines.size()), false);
//    }
//    ImGui::SameLine();
//    if (ImGui::Button("Warning"))
//    {
//        AddConsoleWarning("This is a warning");
//    }
//    ImGui::SameLine();
//    if (ImGui::Button("Error"))
//    {
//        AddConsoleError("This is an error");
//    }
//
//    ImGui::Separator();
//
//    const float footer_height = ImGui::GetFrameHeightWithSpacing();
//    if (ImGui::BeginChild("Output", ImVec2(0, -footer_height), ImGuiChildFlags_Borders))
//    {
//        for (const auto& line : m_consoleLines)
//        {
//            ImVec4 colour = GetMessageColour(line);
//
//            if (colour.w > 0)
//            {
//                ImGui::PushStyleColor(ImGuiCol_Text, colour);
//                ImGui::TextUnformatted(line.c_str());
//                ImGui::PopStyleColor();
//            }
//            else
//            {
//                ImGui::TextUnformatted(line.c_str());
//            }
//        }
//
//        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
//            ImGui::SetScrollHereY(1.0f);
//    }
//    ImGui::EndChild();
//
//
//    if (ImGui::InputText("##Input", m_consoleInput, sizeof(m_consoleInput), ImGuiInputTextFlags_EnterReturnsTrue))
//    {
//        if (strlen(m_consoleInput) > 0)
//        {
//            AddConsoleMessage("> " + std::string(m_consoleInput), false);
//            memset(m_consoleInput, 0, sizeof(m_consoleInput));
//        }
//    }
//
//    ImGui::End();
//}

ImVec4 EditorConsole::GetMessageColour(const std::string& message)
{
    if (message.find("[ERROR]") != std::string::npos)
        return ImVec4(0.95f, 0.65f, 0.65f, 1.0f);

    if (message.find("[WARNING]") != std::string::npos)
        return ImVec4(0.95f, 0.75f, 0.0f, 1.0f);

    if (message.find("> ") == 0)
        return ImVec4(0.65f, 0.95f, 0.65f, 1.0f);

    return ImVec4(1, 1, 1, 0);
}


void EditorConsole::AddConsoleMessage(const std::string& message, bool log)
{
    m_consoleLines.push_back(message);

    if(log) Logger::PrintLog(message.c_str());

    if (m_consoleLines.size() > 100)
        m_consoleLines.erase(m_consoleLines.begin());
}

void EditorConsole::AddConsoleError(const std::string& message, bool log)
{
    AddConsoleMessage("[ERROR] " + message, log);
}

void EditorConsole::AddConsoleWarning(const std::string& message, bool log)
{
    AddConsoleMessage("[WARNING] " + message, log);
}

void EditorConsole::AddConsoleInfo(const std::string& message, bool log)
{
    AddConsoleMessage("[INFO] " + message, log);
}
