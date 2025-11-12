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

#include "FrontendHost.hpp"
#include "fonts/RobotoMono.hpp"
#include "SystemInterface.hpp"
#include "CoreRegistry.hpp"

/*==================================================================*/

FrontendHost::FrontendHost(const Path& gamePath) noexcept {
	SystemInterface::assignComponents(HDM, BVS);
	HDM->setValidator(CoreRegistry::validateProgram);
	CoreRegistry::loadProgramDB();


	if (!gamePath.empty()) { loadGameFile(gamePath); }
	if (!mSystemCore) { BVS->setMainWindowTitle(AppName, "Waiting for file..."); }
}

void FrontendHost::StopSystemThread::operator()(SystemInterface* ptr) noexcept {
	if (ptr) {
		ptr->stopWorker();
		ptr->~SystemInterface();
		::operator delete(ptr, std::align_val_t(HDIS));
	}
}

/*==================================================================*/

void FrontendHost::discardCore() {
	mSystemCore.reset();

	BVS->setMainWindowTitle(AppName, "Waiting for file...");
	BVS->resetMainWindow();

	CoreRegistry::clearEligibleCores();

	HDM->clearCachedFileData();
}

void FrontendHost::replaceCore() {
	mSystemCore.reset(); // ensures previous thread quits first!
	mSystemCore.reset(CoreRegistry::constructCore());
	if (mSystemCore) {
		BVS->setMainWindowTitle(AppName, HDM->getFileStem());
		BVS->displayBuffer.resize(mSystemCore->getDisplaySize());
		toggleSystemLimiter(); toggleSystemOSD();
		mSystemCore->startWorker();
	}
}

/*==================================================================*/

void FrontendHost::loadGameFile(const Path& gameFile) {
	BVS->raiseMainWindow();
	blog.newEntry<BLOG::INF>("Attempting to load: \"{}\"", gameFile.string());
	if (HDM->validateGameFile(gameFile)) {
		blog.newEntry<BLOG::INF>("File has been accepted!");
		replaceCore();
	} else {
		blog.newEntry<BLOG::INF>("Path has been rejected!");
	}
}

void FrontendHost::hideMainWindow(bool state) noexcept {
	if (state) {
		if (mSystemCore) { mSystemCore->addSystemState(EmuState::HIDDEN); }
	} else {
		if (mSystemCore) { mSystemCore->subSystemState(EmuState::HIDDEN); }
	}
}

void FrontendHost::quitApplication() noexcept {
	mSystemCore.reset();

	HDM->writeMainAppConfig(
		GAB->exportSettings().map(),
		BVS->exportSettings().map()
	);
}

bool FrontendHost::initApplication(StrV overrideHome, StrV configName, bool forcePortable) noexcept {
	HDM = HomeDirManager::initialize(
		overrideHome, configName, forcePortable, OrgName, AppName);
	if (!HDM) { return false; }

	FrontendInterface::InitializeContext(HomeDirManager::getHomePath());

	GlobalAudioBase::Settings GAB_settings;
	BasicVideoSpec::Settings BVS_settings;

	HDM->parseMainAppConfig(
		GAB_settings.map(),
		BVS_settings.map()
	);

	GAB = GlobalAudioBase::initialize(GAB_settings);
	if (GAB->getStatus() == GlobalAudioBase::STATUS::NO_AUDIO)
		{ blog.newEntry<BLOG::WRN>("Audio Subsystem is not available!"); }

	BVS = BasicVideoSpec::initialize(BVS_settings);
	if (!BVS) { return false; }

	return true;
}

/*==================================================================*/

s32  FrontendHost::processEvents(void* event) noexcept {
	FrontendInterface::ProcessEvent(event);

	auto sdl_event{ reinterpret_cast<SDL_Event*>(event) };
	if (BVS->isMainWindowID(sdl_event->window.windowID)) {
		switch (sdl_event->type) {
			case SDL_EVENT_QUIT:
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				quitApplication();
				return SDL_APP_SUCCESS;

			case SDL_EVENT_DROP_FILE:
				loadGameFile(sdl_event->drop.data);
				break;

			case SDL_EVENT_WINDOW_MINIMIZED:
				hideMainWindow(true);
				break;

			case SDL_EVENT_WINDOW_RESTORED:
				hideMainWindow(false);
				break;

			case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
			case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
				FrontendInterface::UpdateFontScale(AppFontData_Roboto_Mono,
					SDL_GetWindowDisplayScale(BVS->getMainWindow()));
				break;
		}
	}

	return SDL_APP_CONTINUE;
}

/*==================================================================*/

s32  FrontendHost::processFrame() {
	initializeInterface();
	handleHotkeyActions();

	const auto dialogResult{ HDM->getProbableFile() };
	if (dialogResult) { loadGameFile(*dialogResult); }

	return BVS->renderPresent(!!mSystemCore, (mSystemCore && mToggleOSD)
		? mSystemCore->copyOverlayData().c_str() : nullptr
	) ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

void FrontendHost::handleHotkeyActions() {
	static BasicKeyboard Input;
	Input.updateStates();

	if (Input.isPressed(KEY(UP)))
		{ GAB->addGlobalGain(+0.0625f); }
	if (Input.isPressed(KEY(DOWN)))
		{ GAB->addGlobalGain(-0.0625f); }
	if (Input.isPressed(KEY(RIGHT)))
		{ BVS->rotateViewport(+1); }
	if (Input.isPressed(KEY(LEFT)))
		{ BVS->rotateViewport(-1); }
	if (Input.isPressed(KEY(F9)))
		{ CoreRegistry::loadProgramDB(); }
	if (Input.isPressed(KEY(F1)))
		{ BVS->toggleUsingScanlines(); }
	if (Input.isPressed(KEY(F2)))
		{ BVS->toggleIntegerScaling(); }
	if (Input.isPressed(KEY(F3)))
		{ BVS->cycleViewportScaleMode(); }

	if (mSystemCore) {
		if (Input.isPressed(KEY(ESCAPE))) {
			discardCore();
			blog.newEntry<BLOG::INF>(
				"Emulator core exited successfully.");
			return;
		}
		if (Input.isPressed(KEY(BACKSPACE))) {
			replaceCore();
			blog.newEntry<BLOG::INF>(
				"Emulator core restarted successfully.");
			return;
		}
		if (Input.isPressed(KEY(F4))) {
			if (auto paused{ mSystemCore->tryPauseSystem() }) {
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

/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
