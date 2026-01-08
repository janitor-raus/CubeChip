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

static ImFont* s_main_font{};
static ImGuiID s_main_dock_id{};

static ImGuiStyle s_default_style;

static float s_zoom_scaling = 1.0f;
static float s_text_scaling = 1.0f;

static bool  s_pending_style_changes = false;

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

bool FrontendInterface::merge_overflowing_menus(const PlainKey& tag) noexcept {
	std::unique_lock lock(s_hooks->menus.overflow_lock);

	auto& src_window = s_hooks->menus.overflow[tag];
	if (src_window.empty()) {
		 return false; }

	unsigned migration_count = 0;
	for (auto& [menu_title, src_hooks] : src_window) {
		if (src_hooks.buffer.empty()) { continue; }
		auto& dst_hooks = s_hooks->menus.registry[tag][menu_title];

		blog.newEntry<BLOG::DBG>("{} overflow hooks for menu \"{}\" found.",
			src_hooks.buffer.size(), menu_title.second.c_str());

		dst_hooks.buffer.insert(dst_hooks.buffer.end(),
			std::make_move_iterator(src_hooks.buffer.begin()),
			std::make_move_iterator(src_hooks.buffer.end())
		);
		src_hooks.buffer.clear();
		++migration_count;
	}

	return !!migration_count;
}

void FrontendInterface::invoke_registered_menus(const PlainKey& tag) noexcept {
	std::unique_lock lock(s_hooks->menus.registry_lock);

	auto window = s_hooks->menus.registry.find(tag);
	if (window == s_hooks->menus.registry.end()) { return; }

	do {
		for (auto& [menu_title, hooks] : window->second) {
			if (hooks.buffer.empty()) { continue; }

			if (ImGui::BeginMenu(menu_title.second.c_str())) {
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
	} while (merge_overflowing_menus(tag));

	for (auto& [_, hooks] : window->second)
		{ hooks.offset = 0; } // reset for next frame
}

/*==================================================================*/

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

	s_default_style = ImGui::GetStyle();

	ImGui::StyleColorsDark();

	// XXX this is where we should apply theme customizations later

	s_main_font = io.Fonts->AddFontFromMemoryCompressedTTF(
		FontData::Roboto_Mono, std::size(FontData::Roboto_Mono), 16.0f);

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
		auto& current_style = ImGui::GetStyle();

		auto monitor_dpi = current_style.FontScaleDpi;
		current_style = s_default_style;

		current_style.ScaleAllSizes(s_zoom_scaling);
		current_style.FontScaleMain = s_zoom_scaling * s_text_scaling;
		current_style.FontScaleDpi = monitor_dpi;

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
