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

#include "FrontendHost.hpp"
#include "SystemInterface.hpp"
#include "CoreRegistry.hpp"

/*==================================================================*/

static std::vector<Str>
	s_pending_file_drops{};

static void push_back_pending_file_drops(const Str& filepath) noexcept {
	if (!filepath.empty()) { s_pending_file_drops.push_back(filepath); }
}

/*==================================================================*/

FrontendHost::FrontendHost(const Path& filepath) noexcept {
	SystemInterface::assignComponents(HDM, BVS);
	BVS->setMainWindowTitle(AppName);
	HDM->set_validator_callable(CoreRegistry::validate_game_file);
	CoreRegistry::load_game_database();

	::push_back_pending_file_drops(filepath.string());
}

void FrontendHost::StopSystemThread::operator()(SystemInterface* ptr) noexcept {
	if (ptr) {
		ptr->stopWorker();
		ptr->~SystemInterface();
		::operator delete(ptr, std::align_val_t(HDIS));
	}
}

SettingsMap FrontendHost::Settings::map() noexcept {
	return {
		::make_setting_link("Frontend.Interface.Scale", &ui_scale),
		::make_setting_link("Frontend.Interface.FileMRU", file_mru_cache.data(), s_mru_limit),
	};
}

auto FrontendHost::exportSettings() const noexcept -> Settings {
	Settings out;

	out.ui_scale = FrontendInterface::get_ui_scale_factor();
	FrontendHost::export_mru(out.file_mru_cache.data());

	return out;
}

/*==================================================================*/

void FrontendHost::discard_active_core() {
	mSystemCore.reset();
	CoreRegistry::clear_eligible_cores();
	HDM->clear_cached_file_data();
}

void FrontendHost::replace_active_core() {
	mSystemCore.reset(); // ensures previous thread quits first!
	mSystemCore.reset(CoreRegistry::construct_new_core()); // need a gui here to list and select cores!

	if (mSystemCore) {
		BVS->raiseMainWindow(); // bring to front when we get a core!
		toggleSystemLimiter(); toggleSystemOSD();
		mSystemCore->startWorker();
	}
}

/*==================================================================*/

void FrontendHost::loadGameFile(const Path& gameFile) {
	blog.newEntry<BLOG::INF>("Attempting to load: \"{}\"", gameFile.string());
	if (HDM->load_and_validate_file(gameFile)) {
		blog.newEntry<BLOG::INF>("File has been accepted!");
		s_file_mru.insert(gameFile);
		replace_active_core();
	} else {
		blog.newEntry<BLOG::INF>("Path has been rejected!");
	}
}

bool FrontendHost::initApplication(StrV overrideHome, StrV configName, bool forcePortable) noexcept {
	HDM = HomeDirManager::initialize(
		overrideHome, configName, forcePortable, OrgName, AppName);
	if (!HDM) { return false; }


	blog.create_log(std::to_string(thread_affinity::get_process_id()),
		(Path(HDM->get_home_path()) / "logs").string());

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
	if (GAB->getStatus() == GlobalAudioBase::STATUS::NO_AUDIO)
		{ blog.newEntry<BLOG::WRN>("Audio Subsystem is not available!"); }

	BVS = BasicVideoSpec::initialize(BVS_settings);
	if (!BVS) { return false; }

	FrontendInterface::set_ui_scale_factor(FEH_settings.ui_scale);
	FrontendHost::import_mru(FEH_settings.file_mru_cache.data());

	return true;
}

void FrontendHost::quitApplication() noexcept {
	mSystemCore.reset();

	HDM->write_app_config_file(
		GAB->exportSettings().map(),
		BVS->exportSettings().map(),
		this->exportSettings().map()
	);
}

/*==================================================================*/

s32  FrontendHost::processEvents(void* event) noexcept {
	FrontendInterface::process_event(event);

	auto sdl_event = reinterpret_cast<SDL_Event*>(event);

	if (BVS->isMainWindowID(sdl_event->window.windowID)) {
		switch (sdl_event->type) {
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				return SDL_APP_SUCCESS;

			case SDL_EVENT_DROP_FILE:
				::push_back_pending_file_drops(sdl_event->drop.data);
				break;

			case SDL_EVENT_WINDOW_MINIMIZED:
				toggleSystemHidden(true);
				break;

			case SDL_EVENT_WINDOW_RESTORED:
				toggleSystemHidden(false);
				break;

			case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
			case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
				// Check if ImGui updates DPI scaling automatically
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

s32  FrontendHost::processFrame() {
	initializeInterface();
	handleHotkeyActions();

	const auto dialogResult = get_open_file_dialog_result();
	if (dialogResult) { loadGameFile(*dialogResult); }

	else if (s_pending_file_drops.size() > 0) {
		// we only allow a single file load a time (for now?)
		loadGameFile(s_pending_file_drops[0]);
		s_pending_file_drops.clear();
	}

	return BVS->renderPresent() ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

void FrontendHost::handleHotkeyActions() {
	static BasicKeyboard Input;
	Input.updateStates();

	if (Input.isPressed(KEY(F9)))
		{ CoreRegistry::load_game_database(); }

	if (mSystemCore) {
		if (Input.isPressed(KEY(ESCAPE))) {
			discard_active_core();
			blog.newEntry<BLOG::INF>(
				"Emulator core exited successfully.");
			return;
		}
		if (Input.isPressed(KEY(BACKSPACE))) {
			replace_active_core();
			blog.newEntry<BLOG::INF>(
				"Emulator core restarted successfully.");
			return;
		}
		if (Input.isPressed(KEY(F9))) {
			if (auto paused = mSystemCore->tryPauseSystem()) {
				blog.newEntry<BLOG::INF>("System has been {} by hotkey!",
					*paused ? "paused" : "unpaused");
			}
		}
		if (Input.isPressed(KEY(F11)))
			{ mToggleOSD = !mToggleOSD; toggleSystemOSD(); }
		if (Input.isPressed(KEY(F10)))
			{ mUnlimited = !mUnlimited; toggleSystemLimiter(); }
	}
}

void FrontendHost::toggleSystemLimiter() noexcept {
	if (mUnlimited) {
		if (mSystemCore) { mSystemCore->addSystemState(EmuState::BENCH); }
	} else {
		if (mSystemCore) { mSystemCore->subSystemState(EmuState::BENCH); }
	}
}

void FrontendHost::toggleSystemOSD() noexcept {
	if (mToggleOSD) {
		if (mSystemCore) { mSystemCore->addSystemState(EmuState::STATS); }
	} else {
		if (mSystemCore) { mSystemCore->subSystemState(EmuState::STATS); }
	}
}

void FrontendHost::toggleSystemHidden(bool state) noexcept {
	if (state) {
		if (mSystemCore) { mSystemCore->addSystemState(EmuState::HIDDEN); }
	} else {
		if (mSystemCore) { mSystemCore->subSystemState(EmuState::HIDDEN); }
	}
}

/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
