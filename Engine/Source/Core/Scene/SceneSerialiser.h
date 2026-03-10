#pragma once

#include "Core/Utilities/Serialiser.h"
#include "ThirdParty/Flecs.h"

namespace SceneIO
{
	extern void InitialiseDefaultScene(flecs::world& world);

	bool LoadSceneFromFile(std::string& filePath, flecs::world& world, flecs::entity sceneRoot);
	bool SaveSceneToFile(std::string& filePath, flecs::world& world);

	void SerialiseSceneGamemode(flecs::world& world, flecs::entity_to_json_desc_t& desc, std::string& content);
	void SerialiseEntityBranch(flecs::entity entity, flecs::entity_to_json_desc_t& desc, std::string& content);
}