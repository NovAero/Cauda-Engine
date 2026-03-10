#include "cepch.h"
#include "Core/Utilities/Serialiser.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace CerealBox
{
	void ClobberFile(std::string& data, std::string& fileName, const char* path)
	{
		std::fstream file;

		fs::path dirPath(path);
		fs::path fullPath = dirPath / fileName;  

		if (!fs::exists(dirPath))
		{
			fs::create_directories(dirPath);
		}

		file.open(fullPath, std::ios::out);
		if (!file.is_open()) return;
		file << data;
		file.close();
	}

	void AppendFile(std::string& data, std::string& fileName, const char* path)
	{
		std::fstream file;
		std::string fullPath = path + fileName;

		//Create directory if it's nonexistent
		if (!fs::exists(fullPath)) {
			fs::create_directory(path);
		}

		file.open(fullPath, std::ios::out | std::ios::app);
		if (!file.is_open()) return;

		file << data;

		file.close();
	}

	std::string LoadFileAsString(std::string filename)
	{
		std::ifstream fileStream(filename);

		if (fileStream.is_open())
		{
			std::stringstream read;
			read << fileStream.rdbuf();
			fileStream.close();

			return read.str();
		}

		std::stringstream ss;
		ss << "Error, failed to read file \"" << filename.c_str() << "\"";
		Logger::PrintLog(ss.str().c_str());
#ifdef _CONSOLE
		std::cout << "Error, failed to read file \"" + filename + "\"\n";
#endif
		return std::string();
	}

	json DiffJson(json current, json target)
	{
		auto diff = json::diff(current, target);

		return diff;
	}
}