/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_events.h>

#include "HomeDir.hpp"
#include "SettingWrapper.hpp"
#include "BasicLogger.hpp"
#include "BasicInput.hpp"
#include "SHA1.hpp"

#include "GlobalAudioBase.hpp"
#include "HDIS_HCIS.hpp"
#include "ThreadAffinity.hpp"
#include "AtomSharedPtr.hpp"
#include "SystemDescriptor.hpp"
#include "SystemStaging.hpp"

#include "ApplicationHost.hpp"
#include "ISystemEmu.hpp"
#include "CoreRegistry.hpp"
#include "GuiSession.hpp"

/*==================================================================*/

static std::vector<std::string>
	s_pending_file_drops{};

static void append_pending_file_drops(std::string_view filepath) noexcept {
	if (!filepath.empty()) { s_pending_file_drops.push_back(filepath.data()); }
}

/*==================================================================*/

static AtomSharedPtr<std::string>
	s_open_file_result{};

[[nodiscard]]
static auto get_open_file_dialog_result() noexcept {
	return s_open_file_result.exchange(nullptr, mo::relaxed);
}

void ApplicationHost::set_open_file_dialog_result(std::string_view file) noexcept {
	s_open_file_result.store(std::make_shared<std::string>(file), mo::relaxed);
}

/*==================================================================*/

#ifndef WIN32
static constexpr u8 c_app_logo_data[] = {
	#include "app_logo.data"
};
#endif

/*==================================================================*/

ApplicationHost::Settings ApplicationHost::s_settings;
static SettingsMap s_settings_map;

static bool s_application_minimized = false;
static bool s_application_headless  = false;

ApplicationHost::ApplicationHost() noexcept {
	CoreRegistry::load_game_database();
	setup_gui_callables();
}

void ApplicationHost::SystemInstance::StopSystemThread::operator()(ISystemEmu* ptr) noexcept {
	if (ptr) {
		ptr->stop_worker();
		ptr->~ISystemEmu();
		::operator delete(ptr,std::align_val_t(::HDIS));
	}
}

/*==================================================================*/

void ApplicationHost::prune_terminated_systems() noexcept {
	auto it = m_systems.begin();
	while (it != m_systems.end()) {
		const auto& [id, system] = *it;

		if (!system || !system->is_viewport_visible()) {
			blog.info("System instance {} terminated, unloading...", id);
			m_focus_mru.erase(id);
			it = m_systems.erase(it);
		} else { ++it; }
	}
}

void ApplicationHost::find_last_focused_system() noexcept {
	SystemID found_focused_system = 0;
	for (const auto& id : *m_focus_mru) {
		const bool is_focused = m_systems[id]->force_viewport_focused(false);

		if (found_focused_system != 0) { continue; }
		if (is_focused && m_focus_mru.front() != id) {
			blog.debug("Focused system instance is now {}.", id);
			m_focus_mru.insert(id);
			found_focused_system = id;
		}
	}

	const bool allow_screensaver = found_focused_system != 0
		&& !s_application_minimized && !s_application_headless;

	if (allow_screensaver) {
		SDL_DisableScreenSaver();
	} else {
		SDL_EnableScreenSaver();
	}
}

void ApplicationHost::unload_system_instance(SystemID system_id) noexcept {
	const auto target_system_id = system_id ? system_id
		: (m_focus_mru.empty() ? 0 : m_focus_mru.front());
	m_systems.erase(target_system_id);
	m_focus_mru.erase(target_system_id);
}

void ApplicationHost::insert_system_instance(ISystemEmu* ptr) noexcept {
	if (!ptr) { return; }

	blog.info("Starting up '{}' ({}) system instance.",
		ptr->get_descriptor().system_pretty_name, ptr->instance_id);

	m_focus_mru.insert(ptr->instance_id);
	blog.debug("Forcing focus to newly inserted system instance with ID {}.", ptr->instance_id);
	auto& system = m_systems[m_focus_mru.front()];

	system.core.reset(ptr);
	system->start_worker();
}

/*==================================================================*/

void ApplicationHost::load_file_from_disk(std::string_view file_path) noexcept {
	if (SystemStaging::file_image.load(std::string(file_path))) {
		if (SystemStaging::file_image.size() == 0) {
			SystemStaging::file_image.clear();
			blog.info("File is empty: '{}'", file_path);
			return;
		}
		blog.info("File received: '{}'", file_path);
		return;
	}
	blog.info("File rejected: '{}'", file_path);
	return;
}

ApplicationHost* ApplicationHost::init_application(
	std::string_view config_name,
	std::string_view game_file_path, bool headless
) noexcept {
	static ApplicationHost* self = nullptr;
	if (self) { return self; }

	SDL_SetHint(SDL_HINT_APP_NAME, c_app_name);
	SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, headless ? "waitevent" : nullptr);
	SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
	SDL_SetAppMetadata(c_app_name, c_app_ver.with_hash, nullptr);

	s_application_headless = headless;
	s_config_path = HomeDir::path() + config_name;

	GuiSession::set_file_path(HomeDir::path().data());

	if (auto error = SettingsMap::parse_config_file(s_config_path.data())) {
		blog.warn("[TOML] Failed to parse App Config: {}", error);
	} else {
		blog.info("[TOML] App Config found and successfully loaded!");
	}

	s_settings_map
		.add_setting("Frontend.Interface.Scale.Zoom",
			&s_settings.ui_zoom_scale)
		.add_setting("Frontend.Interface.Scale.Text",
			&s_settings.ui_text_scale)
		.add_setting("Frontend.Interface.FileMRU",
			s_settings.file_mru_cache, s_mru_limit)
		.add_setting("Frontend.Interface.Display.BorderlessView",
			&s_settings.borderless_view_mode)
		.pull_from(SettingsMap::main_table);

	UserInterface::set_ui_zoom_scaling(s_settings.ui_zoom_scale);
	UserInterface::set_ui_text_scaling(s_settings.ui_text_scale);
	UserInterface::set_borderless_view_mode(s_settings.borderless_view_mode);
	ApplicationHost::import_mru(s_settings.file_mru_cache);

	if (!s_application_headless) {
		if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
			blog.fatal("SDL Video subsystem is not available!");
			return nullptr;
		} else {
			auto& app_window = PlatformWindow::create("main", nullptr, 960, 780,
				SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

			if (!app_window.is_ready()) {
				blog.fatal("Failed to prepare main application window, aborting!");
				return nullptr;
			} else {
#ifndef WIN32
				app_window.set_png_icon(c_app_logo_data,
					std::size(c_app_logo_data));
#endif
				PlatformWindow::set_main_handle(app_window);
				app_window.set_min_size(960, 780);
				app_window.set_title(c_app_name);
				app_window.show();
				app_window.raise();
			}

			auto& gui_window = GuiSession::attach(app_window);

			if (!gui_window.is_ready()) {
				blog.fatal("Failed to attach ImGui to main application window, aborting!");
				return nullptr;
			}
		}

		if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
			blog.warn("SDL Audio subsystem is not available!");
		} else {
			// XXX - nothing here, maybe important down the line
		}
		GlobalAudioBase::import_settings();
	}

	blog.info("SHA1 hardware acceleration: {}",
		SHA1::has_hardware_support() ? "ON" : "OFF");

	thread_affinity::set_affinity(0b11ull);
	::append_pending_file_drops(game_file_path);

	static ApplicationHost instance;
	return self = &instance;
}

void ApplicationHost::quit_application() noexcept {
	m_systems.clear(); // terminate all systems before quitting

	GlobalAudioBase::export_settings();
	ApplicationHost::export_mru(s_settings.file_mru_cache);
	s_settings.ui_zoom_scale        = UserInterface::get_ui_zoom_scaling();
	s_settings.ui_text_scale        = UserInterface::get_ui_text_scaling();
	s_settings.borderless_view_mode = UserInterface::get_borderless_view_mode();
	s_settings_map.push_into(SettingsMap::main_table);

	PlatformWindow::clear_registry();

	if (SettingsMap::write_config_file(s_config_path.c_str())) {
		blog.error("[TOML] Failed to write App Config! Expected to"
			"write file at the following location: '{}'", s_config_path);
	} else {
		blog.info("[TOML] App Config written to file successfully!");
	}

	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/*==================================================================*/

static int handle_main_window_events(const SDL_Event& event) noexcept {
	if (s_application_headless) { return SDL_APP_CONTINUE; }
	switch (event.type) {
		case SDL_EVENT_DROP_FILE:
			::append_pending_file_drops(event.drop.data);
			PlatformWindow::has_exec_context()->raise(); // bring main window to front!
			break;

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			GlobalAudioBase::toggle_background_volume(false);
			break;

		case SDL_EVENT_WINDOW_FOCUS_LOST:
			GlobalAudioBase::toggle_background_volume(true);
			break;

		case SDL_EVENT_WINDOW_MINIMIZED:
			s_application_minimized = true;
			break;

		case SDL_EVENT_WINDOW_RESTORED:
			s_application_minimized = false;
			break;
	}
	return SDL_APP_CONTINUE;
}

int ApplicationHost::handle_client_events(const SDL_Event& event) noexcept {
	if (event.type == SDL_EVENT_QUIT) { return SDL_APP_SUCCESS; }

	auto* matched_window = PlatformWindow::find_by_id(event.window.windowID);

	if (matched_window == nullptr) {
		//ScopedLogSource guard("events");
		//std::string description(512, '\0');
		//SDL_GetEventDescription(&event, description.data(), 512);
		//blog.debug("Window-less event caught: {}", description);
		return SDL_APP_CONTINUE;
	}
	const bool is_main_window = matched_window
		== PlatformWindow::get_main_handle();

	if (!is_main_window) {
		(void) (*matched_window)->on_event(event); return SDL_APP_CONTINUE;
	} else {
		return (*matched_window)->on_event(event, handle_main_window_events);
	}
}

/*==================================================================*/

int ApplicationHost::process_client_frame() {
	handle_main_hotkeys();

	for (auto& [id, system] : m_systems) {
		if (id == m_focus_mru.front() ? s_application_minimized : true) {
			if (system) { system->add_system_state(EmuState::HIDDEN); }
		} else {
			if (system) { system->sub_system_state(EmuState::HIDDEN); }
		}
	}

	const auto dialog_result = ::get_open_file_dialog_result();
	if (dialog_result) { load_file_from_disk(*dialog_result); }

	else if (s_pending_file_drops.size() > 0) {
		// XXX - we only allow a single file load a time (for now?)
		load_file_from_disk(s_pending_file_drops.front());
		s_pending_file_drops.clear();
	}

	if (!s_application_headless) {
		PlatformWindow::render_present();
	}

	prune_terminated_systems();
	find_last_focused_system();

	return SDL_APP_CONTINUE;
}

void ApplicationHost::handle_main_hotkeys() noexcept {
	static BasicKeyboard s_input;
	s_input.advance_state();

	if (s_application_headless || PlatformWindow::get_live_handle()
		!= PlatformWindow::get_main_handle()) { return; }

	if (s_input.is_pressed(KEY(F8))) {
		CoreRegistry::load_game_database();
	}

	if (s_input.is_pressed(KEY(F1))) {
		PlatformWindow::get_live_handle()->toggle_fullscreen();
	}

	if (!m_focus_mru.empty()) {
		auto& system = m_systems[m_focus_mru.front()];
		const auto& descriptor = system->get_descriptor();

		if ((s_input.is_held(KEY(LSHIFT)) || s_input.is_held(KEY(RSHIFT)))
			&& s_input.is_pressed(KEY(ESCAPE))
		) {
			blog.info("System '{}' ({}) terminated by hotkey.",
				descriptor.system_pretty_name, m_focus_mru.front());
			unload_system_instance(); return;
		}
		if (s_input.is_pressed(KEY(F8))) {
			system->request_instance_reset();
			blog.info("System '{}' ({}) restarted by hotkey.",
				descriptor.system_pretty_name, m_focus_mru.front());
			return;
		}
		if (s_input.is_pressed(KEY(F9))) {
			if (auto paused = system->try_pause_system()) {
				blog.info("System '{}' ({}) {} by hotkey!",
					descriptor.system_pretty_name, m_focus_mru.front(),
					*paused ? "paused" : "unpaused");
			}
		}
		if (s_input.is_pressed(KEY(F11))) {
			system->xor_system_state(EmuState::STATS);
		}
		if (s_input.is_pressed(KEY(F10))) {
			system->xor_system_state(EmuState::BENCH);
		}
	}
}
