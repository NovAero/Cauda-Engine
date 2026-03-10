#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct SelectedTag {};

struct TransformComponent
{
    // Serialised
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 cachedEuler = glm::vec3(0.f);
    glm::vec3 scale = glm::vec3(1.0f);

    // Non-Serialised
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};

struct SpawnPoint 
{
    int spawnPointID = -1;
};

struct SpawnLobby {};

/*
    All transient entities are unserialised, flows on to their children
    mainly intended for things such as diegetic spawned UI
* */
struct TransientEntity {};