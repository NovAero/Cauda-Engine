#include "cepch.h"
#include "ImGuiLayer.h"
#include "Platform/Win32/Application.h"

#include "Core/Utilities/ImGuiUtilities.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"

#include "imgui_tex_inspect.h"
#include "tex_inspect_opengl.h"

namespace Cauda
{
	ImGuiLayer::ImGuiLayer() //: Layer("ImGui Layer")
	{
	}

	void ImGuiLayer::OnAttach()
	{
		Logger::PrintLog("Attached ImGui layer, setting up context");

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		io.ConfigViewportsNoAutoMerge = false;
		io.ConfigViewportsNoTaskBarIcon = false;
		io.ConfigViewportsNoDefaultParent = false;

		io.ConfigWindowsResizeFromEdges = true;
		io.ConfigWindowsMoveFromTitleBarOnly = true;

		Application& App = Application::Get();

		ImGui_ImplSDL3_InitForOpenGL(App.GetSDLWindow(), App.GetGLContext());
		ImGui_ImplOpenGL3_Init("#version 150");

		ImGuiTexInspect::ImplOpenGL3_Init();
		ImGuiTexInspect::Init();
		ImGuiTexInspect::CreateContext();

		Fonts::LoadFonts();
		SetupImGuiStyle();

		Logger::PrintLog("ImGui layer setup complete");
	}

	void ImGuiLayer::OnDetach()
	{
		Logger::PrintLog("Detaching ImGui layer");

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::OnEvent(SDL_Event& event)
	{
		ImGui_ImplSDL3_ProcessEvent(&event);
	}

	void ImGuiLayer::BeginDraw()
	{
		ImGuiIO& io = ImGui::GetIO();
		Application& app = Application::Get();
		io.DisplaySize = ImVec2(app.GetWindowSize().x, app.GetWindowSize().y);

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			SDL_Window* backup_window = app.GetSDLWindow();
			SDL_GLContext backup_context = app.GetGLContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			SDL_GL_MakeCurrent(backup_window, backup_context);
		}

		ImGui::NewFrame();

		ImGuizmo::BeginFrame();
		ImGuizmo::Enable(true);
		ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());

		ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
	}

	void ImGuiLayer::EndDraw()
	{
		ImGui::Render();
	}

	void ImGuiLayer::RenderDrawData()
	{
		// Rendering
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	void ImGuiLayer::SetupImGuiStyle()
	{
		using namespace Colour;

		ImGui::GetStyle().Alpha = 1.0f;

		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colours = style.Colors;

		ImGuiIO& io = ImGui::GetIO();


		io.FontGlobalScale = 1.0f;

		style.WindowRounding = 4.0f;
		style.ChildRounding = 4.0f;
		style.FrameRounding = 4.0f;
		style.PopupRounding = 4.0f;
		style.ScrollbarRounding = 4.0f;
		style.GrabRounding = 4.0f;
		style.TabRounding = 4.0f;

		//style.AntiAliasedLines = false;
		//style.AntiAliasedFill = false;
		//style.AntiAliasedLinesUseTex = false;

		style.WindowPadding = ImVec2(4.0f, 4.0f);
		style.FramePadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(8.0f, 6.0f);
		style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
		style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
		style.IndentSpacing = 16.0f;
		style.ScrollbarSize = 12.0f;
		style.GrabMinSize = 8.0f;

		style.WindowBorderSize = 1.5f;
		style.ChildBorderSize = 1.5f;
		style.PopupBorderSize = 1.5f;
		style.FrameBorderSize = 1.5f;
		style.TabBorderSize = 1.5f;


		// Background colours
		colours[ImGuiCol_WindowBg] = LeadDark;
		colours[ImGuiCol_PopupBg] = LeadDark;
		colours[ImGuiCol_ChildBg] = ImVec4(LeadMid.x, LeadMid.y, LeadMid.z, 0.50f);

		// Frame colours
		colours[ImGuiCol_FrameBg] = LeadMid;
		colours[ImGuiCol_FrameBgHovered] = LeadLight;
		colours[ImGuiCol_FrameBgActive] = Brighten(ImVec4(HumbleGreen.x, HumbleGreen.y, HumbleGreen.z, 0.70f));

		// Header colours
		colours[ImGuiCol_Header] = LeadLight;
		colours[ImGuiCol_HeaderHovered] = Brighten(HumbleBlue);
		colours[ImGuiCol_HeaderActive] = Brighten(HumbleRed);

		// Tab colours
		colours[ImGuiCol_Tab] = LeadMid;
		colours[ImGuiCol_TabHovered] = LeadLight;
		colours[ImGuiCol_TabActive] = Brighten(ImVec4(HumbleBlue.x, HumbleBlue.y, HumbleBlue.z, 0.80f));
		colours[ImGuiCol_TabUnfocused] = LeadDark;
		colours[ImGuiCol_TabUnfocusedActive] = LeadMid;

		// Title colours
		colours[ImGuiCol_TitleBg] = LeadDarkest;
		colours[ImGuiCol_TitleBgActive] = LeadDark;
		colours[ImGuiCol_TitleBgCollapsed] = ImVec4(LeadDarkest.x, LeadDarkest.y, LeadDarkest.z, 0.60f);
		colours[ImGuiCol_MenuBarBg] = LeadDarkest;

		// Text colours
		colours[ImGuiCol_Text] = TextWhite;
		colours[ImGuiCol_TextDisabled] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
		colours[ImGuiCol_TextSelectedBg] = Brighten(ImVec4(HumbleBlue.x, HumbleBlue.y, HumbleBlue.z, 0.35f));

		// Button colours
		colours[ImGuiCol_Button] = LeadMid;
		colours[ImGuiCol_ButtonHovered] = LeadLight;
		colours[ImGuiCol_ButtonActive] = Brighten(HumbleRed);

		// Widget colours
		colours[ImGuiCol_CheckMark] = Brighten(HumbleGreen);
		colours[ImGuiCol_SliderGrab] = Brighten(HumbleBlue);
		colours[ImGuiCol_SliderGrabActive] = Brighten(HumbleRed);

		// Scrollbar colours
		colours[ImGuiCol_ScrollbarBg] = LeadDarkest;
		colours[ImGuiCol_ScrollbarGrab] = LeadLight;
		colours[ImGuiCol_ScrollbarGrabHovered] = Brighten(HumbleBlue);
		colours[ImGuiCol_ScrollbarGrabActive] = Brighten(HumbleGreen);

		// Border and separator colours
		colours[ImGuiCol_Border] = Brighten(ImVec4(HumbleBlue.x, HumbleBlue.y, HumbleBlue.z, 0.50f));
		colours[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

		colours[ImGuiCol_Separator] = Brighten(ImVec4(HumbleGreen.x, HumbleGreen.y, HumbleGreen.z, 0.50f));
		colours[ImGuiCol_SeparatorHovered] = Brighten(HumbleBlue);
		colours[ImGuiCol_SeparatorActive] = Brighten(HumbleRed);

		// Resize grip colours
		colours[ImGuiCol_ResizeGrip] = Brighten(ImVec4(HumbleGreen.x, HumbleGreen.y, HumbleGreen.z, 0.25f));
		colours[ImGuiCol_ResizeGripHovered] = Brighten(ImVec4(HumbleBlue.x, HumbleBlue.y, HumbleBlue.z, 0.67f));
		colours[ImGuiCol_ResizeGripActive] = Brighten(HumbleRed);

		// Plot colours
		colours[ImGuiCol_PlotLines] = Brighten(HumbleGreen);
		colours[ImGuiCol_PlotLinesHovered] = Brighten(HumbleBlue);
		colours[ImGuiCol_PlotHistogram] = Brighten(HumbleRed);
		colours[ImGuiCol_PlotHistogramHovered] = Brighten(HumbleBlue);

		// Table colours
		colours[ImGuiCol_TableHeaderBg] = LeadMid;
		colours[ImGuiCol_TableBorderStrong] = ImVec4(LeadLight.x, LeadLight.y, LeadLight.z, 1.00f);
		colours[ImGuiCol_TableBorderLight] = ImVec4(LeadMid.x, LeadMid.y, LeadMid.z, 1.00f);
		colours[ImGuiCol_TableRowBg] = ImVec4(LeadDark.x, LeadDark.y, LeadDark.z, 0.00f);
		colours[ImGuiCol_TableRowBgAlt] = ImVec4(LeadLight.x, LeadLight.y, LeadLight.z, 0.12f);

		// Miscellaneous
		colours[ImGuiCol_DragDropTarget] = Brighten(HumbleGreen);
		colours[ImGuiCol_NavHighlight] = Brighten(HumbleBlue);
		colours[ImGuiCol_NavWindowingHighlight] = Brighten(ImVec4(HumbleRed.x, HumbleRed.y, HumbleRed.z, 0.70f));
		colours[ImGuiCol_NavWindowingDimBg] = ImVec4(LeadLight.x, LeadLight.y, LeadLight.z, 0.20f);

		colours[ImGuiCol_ModalWindowDimBg] = ImVec4(LeadDarkest.x, LeadDarkest.y, LeadDarkest.z, 0.60f);
	}
}