#pragma once
#include "Core/Layers/Layer.h"
#include "ThirdParty/Flecs.h"
#include "glm/glm.hpp"
#include "imgui.h"
#include "imgui_tex_inspect.h"
#include <vector>
#include "Core/Input/Input.h"

class EditorModule;
class RendererModule;
class PhysicsModule;
class SensorModule;
class CameraModule;
class GrimoriumModule;
class EditorConsole;
class GrimoriumModule;
class ConstraintModule;
class AudioModule;
class GamemodeModule;

struct ImGuiWindow;

namespace Cauda
{
	class GameLayer;
	class RenderLayer;

	class EditorLayer : IKeyListener, IMouseListener//, public Layer
	{
	public:
		EditorLayer(flecs::world& w);
		virtual ~EditorLayer();

		void OnAttach();
		void OnDetach();
		void OnImGuiRender();

		//void OnEvent(SDL_Event& event) {}
		void OnUserEvent(SDL_UserEvent& event);

		//Input listening
		virtual void OnKeyPress(PKey key) override;
		virtual void OnKeyRelease(PKey key) override {}

		virtual void OnMouseClick(MouseButton button, int clicks) override {}
		virtual void OnMouseRelease(MouseButton mouseButton) override {}
		virtual void OnMouseScroll(float amount) override;

		//Make sure this is called only AFTER all modules have been processed
		void RegisterSceneEntityNames();
		void RegisterScene();

		ImGuiWindow* GetViewportWindow() { return m_viewportWindow; }
		glm::vec2 GetViewportSize() { return { m_viewportSize.x, m_viewportSize.y }; }
		bool HasViewportResized() { return m_viewportSize != m_viewportSizePrev; }

		void TogglePlayInEditor();

	private:
		void DrawMenuBar();
		void DrawGameViewport();
		void DrawInspectorControlsWindow();
		void DrawPlayControls(const ImVec2& renderPos, const ImVec2& renderSize);
		void DrawSceneManagementPopups();

	private:
		flecs::world& m_world;
		EditorModule* m_editorModule;
		RendererModule* m_rendererModule;
		PhysicsModule* m_physicsModule;
		SensorModule* m_sensorModule;
		CameraModule* m_cameraModule;
		GrimoriumModule* m_grimoriumModule;
		ConstraintModule* m_constraintModule;
		AudioModule* m_audioModule;
		GamemodeModule* m_gamemodeModule;

		GameLayer* m_gameLayer;
		RenderLayer* m_renderLayer;

		ImGuiWindow* m_viewportWindow;
		glm::vec2 m_viewportSize = { 1280, 720 };
		glm::vec2 m_viewportSizePrev = { 0, 0 };

		bool m_showHierarchy = true;
		bool m_showInspector = true;
		bool m_showEditorConsole = true;
		bool m_showAssetLibrary = true;
		bool m_showGamemodeInfo = true;
		bool m_showRenderStats = true;

		//std::string m_currentSceneName = "NewScene.json";
		std::string m_saveAsSceneName;
		bool m_showSaveConfirmation = false;
		bool m_showSaveAsDialog = false;
		bool m_showEngineSettings = false;

		std::string m_pieSessionName = "pieSession.json";

		bool m_delayedEvent = false;
		bool m_inspectorMode = false;
		bool m_showDebugControls = true;

		glm::vec2 m_viewportMousePos = { 0.0f, 0.0f };
		glm::ivec2 m_viewportPixelPos = { 0, 0 };

		EditorConsole* m_console;
	};
}