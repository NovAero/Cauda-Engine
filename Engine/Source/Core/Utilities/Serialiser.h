#pragma once

#include "core/CoreDefinitions.h"
#include <ThirdParty/json.hpp>

using json = nlohmann::json;

/**
*  File I/O, Serialisation, and Parsing
*  Files are located in the App.vcxproj working directory : CaudaEngine\App\ ($(ProjectDir))
**/
namespace CerealBox
{
	/**
	* Clobber over a file, overwriting data stored inside
	* If the file does not exist in the given directory, it will create the directory and file
	* 3rd param "path" is for creating/seeking a folder from the working directory
	**/
	void ClobberFile(std::string& data, std::string& fileName, const char* path = "");

	/**
	* Append the contents of a file,
	* If the file does not exist in the given directory, it will create the directory and file
	* 3rd param "path" is for creating/seeking a folder from the working directory 
	**/
	void AppendFile(std::string& data, std::string& fileName, const char* path = "");

	/*
		Returns a string with the contents of a file
		If the file is already open or cannot be found, returns and empty string
	*/
	std::string LoadFileAsString(std::string filename);

	json DiffJson(json current, json target);
}