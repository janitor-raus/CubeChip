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

static ImGuiID sMainDockID{};

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

void FrontendInterface::set_ui_scale_factor(float scale) noexcept {
	ImGui::GetStyle().FontScaleMain = scale;
}
float FrontendInterface::get_ui_scale_factor() noexcept {
	return ImGui::GetStyle().FontScaleMain;
}

/*==================================================================*/

static ImFont* sMainFont{};

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

	s_hooks = std::make_unique<RegistryAggregate>();

	sMainFont = io.Fonts->AddFontFromMemoryCompressedTTF(
		FontData::Roboto_Mono, std::size(FontData::Roboto_Mono), 16.0f);

	io.FontDefault = sMainFont;
}

void FrontendInterface::quit_context() {
	ImGui::DestroyContext();
}

void FrontendInterface::init_video(SDL_Window* window, SDL_Renderer* renderer) {
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

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
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();

	ImGui::NewFrame();

	if (ImGui::BeginMainMenuBar()) {
		invoke_registered_menus("");
		ImGui::EndMainMenuBar();
	}

	const auto& viewport = *ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport.Pos.x, viewport.Pos.y + ImGui::GetFrameHeight()));
	ImGui::SetNextWindowSize(ImVec2(viewport.Size.x, viewport.Size.y - ImGui::GetFrameHeight()));
	ImGui::SetNextWindowViewport(viewport.ID);

	ImGui::Begin("MainDockspace", nullptr, ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

	sMainDockID = ImGui::GetID("MainDockspace");
	ImGui::DockSpace(sMainDockID);
	ImGui::End();

	invoke_registered_windows();
}

void FrontendInterface::render_frame(SDL_Renderer* renderer) {
	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void FrontendInterface::dock_next_window_to(unsigned id, bool first_time) noexcept {
	ImGui::SetNextWindowDockID(id ? id : sMainDockID,
		first_time ? ImGuiCond_FirstUseEver : ImGuiCond_Always);
}
