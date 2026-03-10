#pragma once
#include <ThirdParty/Flecs.h>
#include <string>

namespace SceneIO
{
	void InitialiseDefaultScene(flecs::world& world);
}

struct AudioMixerSettings
{
	float masterVolume = 1.0f;
	float bgmVolume = 1.0f;
	float sfxVolume = 1.0f;
	float ambientVolume = 1.0f;
};

struct AssetBrowserSettings
{
	float iconSize = 55.0f;
	int iconSpacing = 8;
	bool stretchSpacing = true;
};

struct GizmoSettings
{
	//Gizmos
	float translationSnap = 1.f;
	float rotationSnap = 15.f;
	float scaleSnap = 0.5f;
	bool translationSnapEnabled = true;
	bool rotationSnapEnabled = true;
	bool scaleSnapEnabled = true;
	bool worldSpace = true;
};

struct DebugDrawSettings
{
	bool drawPhysicsDebug = false;
	bool drawWireframe = true;
};

struct EditorSettings
{
	//Debug drawing
	bool vSync = true;

	//Scene
	std::string loadScenePath = "";

	bool orthographic = true;
};

extern flecs::entity g_EngineSettingsEntity;

void SetupEngineComponents(flecs::world& world);

void SaveEngineSettings();
void LoadEngineSettings(flecs::world& world);

// Passing 'bool* p_open' displays a Close button on the upper-right corner of the window,
// the pointed value will be set to false when the button is pressed.
void DrawEngineSettingsWindow(bool* p_open = nullptr);
