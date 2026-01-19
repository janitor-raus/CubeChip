/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "FrontendInterface.hpp"
#include "BasicLogger.hpp"

#include "fonts/RobotoMono.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

/*==================================================================*/

static SDL_Renderer* s_current_renderer{};

static ImFont* s_main_font{};
static ImGuiID s_main_dock_id{};

static ImGuiStyle s_default_style;

static float s_zoom_scaling = 1.0f;
static float s_text_scaling = 1.0f;

static bool  s_pending_style_changes = true;

/*==================================================================*/

bool FrontendInterface::merge_overflowing_windows() noexcept {
	std::unique_lock lock(s_hooks->windows.overflow_lock);

	auto& src_windows = s_hooks->windows.overflow.buffer;
	if (src_windows.empty()) { return false; }

	auto& dst_windows = s_hooks->windows.registry.buffer;

	blog.newEntry<BLOG::DBG>("{} overflow windows found.", src_windows.size());

	dst_windows.insert(dst_windows.end(),
		std::make_move_iterator(src_windows.begin()),
		std::make_move_iterator(src_windows.end())
	);
	src_windows.clear();

	return true;
}

void FrontendInterface::invoke_registered_windows() noexcept {
	std::unique_lock lock(s_hooks->windows.registry_lock);

	auto& windows = s_hooks->windows.registry;

	do {
		while (windows.offset < windows.buffer.size()) {
			auto& entry = windows.buffer[windows.offset];
			if (auto shared_ptr = entry.lock()) {
				(*shared_ptr)(); ++windows.offset;
			} else {
				windows.buffer.erase(windows.buffer.begin() + windows.offset);
			}
		}
	} while (merge_overflowing_windows());

	windows.offset = 0;
}

bool FrontendInterface::merge_overflowing_menus(const LabelKey& window_key) noexcept {
	std::unique_lock lock(s_hooks->menus.overflow_lock);

	auto& src_window = s_hooks->menus.overflow[window_key.get_id_or_label()];
	if (src_window.empty()) { return false; }

	unsigned migration_count = 0;
	for (auto& [menu_key, src_hooks] : src_window) {
		if (src_hooks.buffer.empty()) { continue; }
		auto& dst_hooks = s_hooks->menus.registry[window_key.get_id_or_label()][menu_key];

		blog.newEntry<BLOG::DBG>("{} overflow hooks for menu \"{}\" found.",
			src_hooks.buffer.size(), menu_key.second.c_str());

		dst_hooks.buffer.insert(dst_hooks.buffer.end(),
			std::make_move_iterator(src_hooks.buffer.begin()),
			std::make_move_iterator(src_hooks.buffer.end())
		);
		src_hooks.buffer.clear();
		++migration_count;
	}

	return !!migration_count;
}

void FrontendInterface::invoke_registered_menus(const LabelKey& window_key) noexcept {
	std::unique_lock lock(s_hooks->menus.registry_lock);

	merge_overflowing_menus(window_key); // unconditional first merge
	auto window = s_hooks->menus.registry.find(window_key.get_id_or_label());
	if (window == s_hooks->menus.registry.end()) { return; }

	do {
		for (auto& [menu_key, hooks] : window->second) {
			if (hooks.buffer.empty()) { continue; }

			if (ImGui::BeginMenu(menu_key.second.c_str())) {
				hooks.first_hit = !std::exchange(hooks.has_focus, true);
				s_active_menu = &hooks;

				while (hooks.offset < hooks.buffer.size()) {
					auto& entry = hooks.buffer[hooks.offset];
					if (auto shared_ptr = entry.lock()) {
						(*shared_ptr)(); ++hooks.offset;
					} else {
						hooks.buffer.erase(hooks.buffer.begin() + hooks.offset);
					}
				}

				ImGui::EndMenu();
			} else {
				s_active_menu = nullptr;
				hooks.has_focus = false;
				continue;
			}
		}
	} while (merge_overflowing_menus(window_key));

	for (auto& [_, hooks] : window->second)
		{ hooks.offset = 0; } // reset for next frame
}

/*==================================================================*/

SDL_Renderer* FrontendInterface::get_current_renderer()  noexcept { return s_current_renderer; }
unsigned      FrontendInterface::get_main_dockspace_id() noexcept { return s_main_dock_id; }

void FrontendInterface::set_ui_zoom_scaling(float scale) noexcept {
	s_zoom_scaling = std::clamp(scale, 1.0f, 4.0f);
	s_pending_style_changes = true;
}

float FrontendInterface::get_ui_zoom_scaling() noexcept {
	return s_zoom_scaling;
}

void FrontendInterface::set_ui_text_scaling(float scale) noexcept {
	s_text_scaling = std::clamp(scale, 0.5f, 2.0f);
	s_pending_style_changes = true;
}

float FrontendInterface::get_ui_text_scaling() noexcept {
	return s_text_scaling;
}

/*==================================================================*/

void FrontendInterface::init_context(const char* home_dir) {
	static auto ini_path = home_dir ? std::string(home_dir) + "imgui.ini" : std::string();
	static auto log_path = home_dir ? std::string(home_dir) + "imgui.log" : std::string();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto& io = ImGui::GetIO();

	io.IniFilename = home_dir ? ini_path.c_str() : nullptr;
	io.LogFilename = home_dir ? log_path.c_str() : nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	io.ConfigDpiScaleFonts = true;
	io.ConfigDpiScaleViewports = true;

	s_hooks = std::make_unique<RegistryAggregate>();

	ImGui::StyleColorsDark(&s_default_style);
	s_default_style.WindowPadding     = ImVec2(6.0f, 6.0f);
	s_default_style.FramePadding      = ImVec2(8.0f, 4.0f);
	s_default_style.ItemSpacing       = ImVec2(8.0f, 2.0f);
	s_default_style.ItemInnerSpacing  = ImVec2(8.0f, 2.0f);
	s_default_style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
	s_default_style.IndentSpacing = 38.0f;
	s_default_style.GrabMinSize   =  8.0f;

	s_default_style.WindowBorderSize = 1.0f;
	s_default_style.ChildBorderSize  = 1.0f;
	s_default_style.PopupBorderSize  = 1.0f;
	s_default_style.FrameBorderSize  = 0.0f;

	s_default_style.WindowRounding = 4.0f;
	s_default_style.ChildRounding  = 4.0f;
	s_default_style.FrameRounding  = 2.0f;
	s_default_style.PopupRounding  = 2.0f;
	s_default_style.GrabRounding   = 2.0f;

	s_default_style.ScrollbarSize     = 10.0f;
	s_default_style.ScrollbarRounding =  2.0f;
	s_default_style.ScrollbarPadding  =  0.0f;

	s_default_style.TabBorderSize      =  1.0f;
	s_default_style.TabBarBorderSize   =  2.0f;
	s_default_style.TabBarOverlineSize =  2.0f;
	s_default_style.TabMinWidthBase    = 64.0f;
	s_default_style.TabMinWidthShrink  = 64.0f;
	s_default_style.TabCloseButtonMinWidthSelected   = -1.0f;
	s_default_style.TabCloseButtonMinWidthUnselected = -1.0f;
	s_default_style.TabRounding        = 4.0f;

	s_default_style.WindowTitleAlign          = ImVec2(0.0f, 0.5f);
	s_default_style.WindowBorderHoverPadding  = 4.0f;
	s_default_style.WindowMenuButtonPosition  = ImGuiDir_Right;

	s_default_style.CellPadding                 = ImVec2(6.0f, 2.0f);
	s_default_style.TableAngledHeadersAngle     = 35.0f;
	s_default_style.TableAngledHeadersTextAlign = ImVec2(0.5f, 0.0f);

	s_default_style.ColorMarkerSize = 4.0f;
	s_default_style.ColorButtonPosition = ImGuiDir_Left;

	s_default_style.SeparatorTextBorderSize = 3.0f;
	s_default_style.SeparatorTextPadding    = ImVec2(24.0f, 4.0f);

	s_default_style.DockingNodeHasCloseButton = false;
	s_default_style.DockingSeparatorSize      = 3.0f;

	auto* colors = s_default_style.Colors;
	colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
	colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg]               = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
	colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.13f);
	colors[ImGuiCol_PopupBg]                = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
	colors[ImGuiCol_Border]                 = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
	colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg]                = ImVec4(0.17f, 0.17f, 0.21f, 1.00f);
	colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.77f, 0.50f, 1.00f, 0.25f);
	colors[ImGuiCol_FrameBgActive]          = ImVec4(0.77f, 0.50f, 1.00f, 0.36f);
	colors[ImGuiCol_TitleBg]                = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
	colors[ImGuiCol_TitleBgActive]          = ImVec4(0.23f, 0.18f, 0.29f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
	colors[ImGuiCol_MenuBarBg]              = ImVec4(0.77f, 0.50f, 1.00f, 0.06f);
	colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.17f, 0.17f, 0.21f, 1.00f);
	colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.77f, 0.50f, 1.00f, 0.38f);
	colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
	colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
	colors[ImGuiCol_CheckMark]              = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
	colors[ImGuiCol_SliderGrab]             = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
	colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
	colors[ImGuiCol_Button]                 = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
	colors[ImGuiCol_ButtonHovered]          = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
	colors[ImGuiCol_ButtonActive]           = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
	colors[ImGuiCol_Header]                 = ImVec4(0.77f, 0.50f, 1.00f, 0.38f);
	colors[ImGuiCol_HeaderHovered]          = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
	colors[ImGuiCol_HeaderActive]           = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
	colors[ImGuiCol_Separator]              = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
	colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
	colors[ImGuiCol_SeparatorActive]        = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
	colors[ImGuiCol_ResizeGrip]             = ImVec4(0.77f, 0.50f, 1.00f, 0.00f);
	colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
	colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
	colors[ImGuiCol_InputTextCursor]        = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TabHovered]             = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
	colors[ImGuiCol_Tab]                    = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
	colors[ImGuiCol_TabSelected]            = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
	colors[ImGuiCol_TabSelectedOverline]    = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
	colors[ImGuiCol_TabDimmed]              = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
	colors[ImGuiCol_TabDimmedSelected]      = ImVec4(0.77f, 0.50f, 1.00f, 0.25f);
	colors[ImGuiCol_TabDimmedSelectedOverline]  = ImVec4(0.77f, 0.50f, 1.00f, 0.38f);
	colors[ImGuiCol_DockingPreview]         = ImVec4(0.77f, 0.50f, 1.00f, 0.38f);
	colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
	colors[ImGuiCol_PlotLines]              = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
	colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 0.75f);
	colors[ImGuiCol_PlotHistogram]          = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
	colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 0.75f);
	colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.77f, 0.50f, 1.00f, 0.19f);
	colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
	colors[ImGuiCol_TableBorderLight]       = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
	colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
	colors[ImGuiCol_TextLink]               = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.77f, 0.50f, 1.00f, 0.19f);
	colors[ImGuiCol_TreeLines]              = ImVec4(0.31f, 0.31f, 0.38f, 1.00f);
	colors[ImGuiCol_DragDropTarget]         = ImVec4(0.77f, 0.50f, 1.00f, 1.00f);
	colors[ImGuiCol_DragDropTargetBg]       = ImVec4(0.77f, 0.50f, 1.00f, 0.13f);
	colors[ImGuiCol_UnsavedMarker]          = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_NavCursor]              = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
	colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	// XXX this is where we should apply theme customizations later

	s_main_font = io.Fonts->AddFontFromMemoryCompressedTTF(
		FontData::Roboto_Mono, std::size(FontData::Roboto_Mono), 18.0f);

	io.FontDefault = s_main_font;
}

void FrontendInterface::quit_context() {
	ImGui::DestroyContext();
}

void FrontendInterface::init_video(SDL_Window* window, SDL_Renderer* renderer) {
	ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer3_Init(renderer);
	s_current_renderer = renderer;
}

void FrontendInterface::quit_video() {
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
}

/*==================================================================*/

void FrontendInterface::process_event(void* event) {
	ImGui_ImplSDL3_ProcessEvent(reinterpret_cast<SDL_Event*>(event));
}

void FrontendInterface::begin_new_frame() {
	if (s_pending_style_changes) {
		ImGui::GetStyle() = s_default_style;

		ImGui::GetStyle().ScaleAllSizes(s_zoom_scaling);
		ImGui::GetStyle().FontScaleMain = s_zoom_scaling * s_text_scaling;

		s_pending_style_changes = false;
	}

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();

	ImGui::NewFrame();

	if (ImGui::BeginMainMenuBar()) {
		invoke_registered_menus("");
		ImGui::EndMainMenuBar();
	}

	const auto& viewport = *ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport.WorkPos);
	ImGui::SetNextWindowSize(viewport.WorkSize);
	ImGui::SetNextWindowViewport(viewport.ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
	ImGui::Begin("MainDockspace", nullptr, ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
	ImGui::PopStyleVar(2);

	s_main_dock_id = ImGui::GetID("MainDockspace");
	ImGui::DockSpace(s_main_dock_id);
	ImGui::End();

	invoke_registered_windows();
}

void FrontendInterface::render_frame(SDL_Renderer* renderer) {
	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(),
		renderer ? renderer : s_current_renderer);
}

void FrontendInterface::dock_next_window_to(unsigned id, bool first_time) noexcept {
	ImGui::SetNextWindowDockID(id ? id : s_main_dock_id,
		first_time ? ImGuiCond_FirstUseEver : ImGuiCond_Always);
}

void FrontendInterface::call_menubar(const char* window_name) noexcept {
	if (ImGui::BeginMenuBar()) {
		invoke_registered_menus(window_name);
		ImGui::EndMenuBar();
	}
}

void FrontendInterface::call_autohide_menubar(const char* window_name, bool& hidden) noexcept {

	if (hidden) {
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && (
			ImGui::IsKeyPressed(ImGuiKey_LeftAlt) || ImGui::IsKeyPressed(ImGuiKey_RightAlt)
		)) { hidden = false; return; }
	} else {
		if (ImGui::IsKeyPressed(ImGuiKey_LeftAlt) ||
			ImGui::IsKeyPressed(ImGuiKey_RightAlt)) { hidden = true; }
	}

	bool menu_or_popup_hovered = false;

	if (ImGui::BeginMenuBar()) {
		menu_or_popup_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)
			&& (ImGui::IsAnyItemHovered() || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId));

		invoke_registered_menus(window_name);
		ImGui::EndMenuBar();
	}

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		!menu_or_popup_hovered && !ImGui::GetIO().WantCaptureMouse) { hidden = true; }
}
