/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_events.h>

#include "HomeDirManager.hpp"
#include "BasicLogger.hpp"
#include "BasicInput.hpp"

#include "FrontendInterface.hpp"
#include "BasicVideoSpec.hpp"
#include "GlobalAudioBase.hpp"
#include "HDIS_HCIS.hpp"
#include "ThreadAffinity.hpp"
#include "AtomSharedPtr.hpp"
#include "SystemDescriptor.hpp"
#include "SystemStaging.hpp"

#include "FrontendHost.hpp"
#include "SystemInterface.hpp"
#include "CoreRegistry.hpp"

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

void FrontendHost::set_open_file_dialog_result(std::string_view file) noexcept {
	s_open_file_result.store(std::make_shared<std::string>(file), mo::relaxed);
}

/*==================================================================*/

FrontendHost::FrontendHost() noexcept {
	BVS->set_window_title(AppName);
	CoreRegistry::load_game_database();

	setup_gui_callables();
}

void FrontendHost::SystemInstance::StopSystemThread::operator()(SystemInterface* ptr) noexcept {
	if (ptr) {
		ptr->stop_workers();
		ptr->~SystemInterface();
		::operator delete(ptr, std::align_val_t(HDIS));
	}
}

SettingsMap FrontendHost::Settings::map() noexcept {
	return {
		::make_setting_link("Frontend.Interface.Scale.Zoom", &ui_zoom_scale),
		::make_setting_link("Frontend.Interface.Scale.Text", &ui_text_scale),
		::make_setting_link("Frontend.Interface.FileMRU", file_mru_cache, s_mru_limit),
	};
}

auto FrontendHost::export_settings() const noexcept -> Settings {
	Settings out;

	out.ui_zoom_scale = FrontendInterface::get_ui_zoom_scaling();
	out.ui_text_scale = FrontendInterface::get_ui_text_scaling();
	FrontendHost::export_mru(out.file_mru_cache);

	return out;
}

/*==================================================================*/

void FrontendHost::prune_terminated_systems() noexcept {
	auto it = m_systems.begin();
	while (it != m_systems.end()) {
		const auto& system = it->second.core;

		if (!system || system->is_awaiting_shutdown()) {
			m_focus_mru.erase(it->first);
			it = m_systems.erase(it);
		} else { ++it; }
	}
}

void FrontendHost::find_last_focused_system() noexcept {
	for (const auto& [id, system] : m_systems) {
		if (system && system.core->is_currently_focused()) {
			m_focus_mru.insert(id); return;
		}
	}
}

void FrontendHost::unload_system_instance(SystemID system_id) noexcept {
	const auto target_system_id = system_id ? system_id
		: (m_focus_mru.empty() ? 0 : m_focus_mru[0]);
	m_systems.erase(target_system_id);
	m_focus_mru.erase(target_system_id);
}

void FrontendHost::insert_system_instance(SystemInterface* ptr) noexcept {
	if (!ptr) { return; }
	BVS->raise_window(); // bring main window to front!

	blog.info("Starting up '{}' ({}) system instance.",
		ptr->get_descriptor().system_pretty_name, ptr->instance_id);

	m_focus_mru.insert(ptr->instance_id);
	auto& system = m_systems[m_focus_mru[0]];

	system.core.reset(ptr);
	toggle_system_delimiters(system);
	toggle_system_statistics(system);
	system.core->start_workers();
}

/*==================================================================*/

void FrontendHost::load_file_from_disk(std::string_view file_path) {
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

FrontendHost* FrontendHost::init_application(
	std::string_view home_override, std::string_view config_name,
	std::string_view game_file_path, bool force_portable
) noexcept {
	static FrontendHost* self = nullptr;
	if (self) { return self; }

	HDM = HomeDirManager::initialize(
		home_override, config_name, force_portable, OrgName, AppName);
	if (!HDM) { return nullptr; }

	blog.create_log(std::to_string(thread_affinity::get_process_id()),
		(fs::Path(HDM->get_home_path()) / "logs").string());

	FrontendInterface::init_context(HomeDirManager::get_home_path().c_str());

	GlobalAudioBase::Settings GAB_settings;
	BasicVideoSpec ::Settings BVS_settings;
	FrontendHost   ::Settings FEH_settings;

	HDM->parse_app_config_file(
		GAB_settings.map(),
		BVS_settings.map(),
		FEH_settings.map()
	);

	GAB = GlobalAudioBase::initialize(GAB_settings);
	if (!GAB->has_audio_output()) {
		blog.warn("Audio Subsystem is not available!");
	}

	BVS = BasicVideoSpec::initialize(BVS_settings);
	if (!BVS) { return nullptr; }

	FrontendInterface::init_video(BVS->get_main_window(), BVS->get_main_renderer());
	FrontendInterface::set_ui_zoom_scaling(FEH_settings.ui_zoom_scale);
	FrontendInterface::set_ui_text_scaling(FEH_settings.ui_text_scale);

	FrontendHost::import_mru(FEH_settings.file_mru_cache);

	::append_pending_file_drops(game_file_path);
	thread_affinity::set_affinity(0b11ull);

	static FrontendHost instance;
	return self = &instance;
}

void FrontendHost::quit_application() noexcept {
	FrontendInterface::quit_video();
	FrontendInterface::quit_context();

	for (auto& [id, system] : m_systems) { system.core.reset(); }

	HDM->write_app_config_file(
		GAB->export_settings().map(),
		BVS->export_settings().map(),
		/***/export_settings().map()
	);
}

/*==================================================================*/

int FrontendHost::handle_client_events(void* event) noexcept {
	FrontendInterface::process_event(event);

	auto sdl_event = reinterpret_cast<SDL_Event*>(event);

	if (BVS->is_main_window_id(sdl_event->window.windowID)) {
		switch (sdl_event->type) {
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				return SDL_APP_SUCCESS;

			case SDL_EVENT_DROP_FILE:
				::append_pending_file_drops(sdl_event->drop.data);
				break;

			case SDL_EVENT_WINDOW_MINIMIZED:
				m_application_minimized = true;
				break;

			case SDL_EVENT_WINDOW_RESTORED:
				m_application_minimized = false;
				break;
		}
	} else {
		switch (sdl_event->type) {
			case SDL_EVENT_QUIT:
				return SDL_APP_SUCCESS;
		}
	}

	return SDL_APP_CONTINUE;
}

/*==================================================================*/

int FrontendHost::process_client_frame() {
	handle_main_hotkeys();

	prune_terminated_systems();
	find_last_focused_system();

	for (auto& [id, system] : m_systems) {
		set_system_hidden_status(system, id == m_focus_mru[0]
			? m_application_minimized : true);
	}

	const auto dialog_result = ::get_open_file_dialog_result();
	if (dialog_result) { load_file_from_disk(*dialog_result); }

	else if (s_pending_file_drops.size() > 0) {
		// XXX - we only allow a single file load a time (for now?)
		load_file_from_disk(s_pending_file_drops[0]);
		s_pending_file_drops.clear();
	}

	return BVS->render_present([]() {
		FrontendInterface::begin_new_frame();
		FrontendInterface::render_frame();
	}) ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

void FrontendHost::handle_main_hotkeys() {
	static BasicKeyboard Input;
	Input.update_states();

	if (Input.is_pressed(KEY(F8))) {
		CoreRegistry::load_game_database();
	}

	if (!m_focus_mru.empty()) {
		auto& system = m_systems[m_focus_mru[0]];
		const auto& descriptor = system.core->get_descriptor();

		if (Input.are_any_held(KEY(LSHIFT), KEY(RSHIFT))
			&& Input.is_pressed(KEY(ESCAPE))
		) {
			blog.info("System '{}' ({}) terminated by hotkey.",
				descriptor.system_name, m_focus_mru[0]);
			unload_system_instance(); return;
		}
		//if (Input.is_pressed(KEY(BACKSPACE))) {
		//	// XXX - need to perform manual system reset now
		//	blog.info("Emulator core restarted manually.");
		//	return;
		//}
		if (Input.is_pressed(KEY(F9))) {
			if (auto paused = system.core->try_pause_system()) {
				blog.info("System '{}' ({}) {} by hotkey!",
					descriptor.system_name, m_focus_mru[0],
					*paused ? "paused" : "unpaused");
			}
		}
		if (Input.is_pressed(KEY(F11))) {
			system.statistics = !system.statistics;
			toggle_system_statistics(system);
		}
		if (Input.is_pressed(KEY(F10))) {
			system.delimiters = !system.delimiters;
			toggle_system_delimiters(system);
		}
	}
}

void FrontendHost::toggle_system_delimiters(SystemInstance& system) noexcept {
	if (system.delimiters) {
		if (system.core) { system.core->add_system_state(EmuState::BENCH); }
	} else {
		if (system.core) { system.core->sub_system_state(EmuState::BENCH); }
	}
}

void FrontendHost::toggle_system_statistics(SystemInstance& system) noexcept {
	if (system.statistics) {
		if (system.core) { system.core->add_system_state(EmuState::STATS); }
	} else {
		if (system.core) { system.core->sub_system_state(EmuState::STATS); }
	}
}

void FrontendHost::set_system_hidden_status(SystemInstance& system, bool state) noexcept {
	if (state) {
		if (system.core) { system.core->add_system_state(EmuState::HIDDEN); }
	} else {
		if (system.core) { system.core->sub_system_state(EmuState::HIDDEN); }
	}
}
