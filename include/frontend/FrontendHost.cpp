/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_dialog.h>

#include "HomeDirManager.hpp"
#include "BasicLogger.hpp"
#include "BasicInput.hpp"

#include "FrontendInterface.hpp"
#include "BasicVideoSpec.hpp"
#include "GlobalAudioBase.hpp"
#include "DefaultConfig.hpp"
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

	FrontendInterface::FnHook_OpenFile
		.store(openFileDialog, mo::relaxed);

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
	mSystemCore.reset();
	mSystemCore.reset(CoreRegistry::constructCore());
	if (mSystemCore) {
		BVS->setMainWindowTitle(AppName, HDM->getFileStem());
		BVS->displayBuffer.resize(mSystemCore->getDisplaySize());
		toggleSystemLimiter();
		mSystemCore->startWorker();
	}
}

/*==================================================================*/

void FrontendHost::loadGameFile(const Path& gameFile) {
	BVS->raiseMainWindow();
	blog.newEntry<BLOG::INFO>("Attempting to load: \"{}\"", gameFile.string());
	if (HDM->validateGameFile(gameFile)) {
		blog.newEntry<BLOG::INFO>("File has been accepted!");
		replaceCore();
	} else {
		blog.newEntry<BLOG::INFO>("Path has been rejected!");
	}
}

void FrontendHost::hideMainWindow(bool state) noexcept {
	if (!mSystemCore) { return; }

	if (state) {
		mSystemCore->addSystemState(EmuState::HIDDEN);
	} else {
		mSystemCore->subSystemState(EmuState::HIDDEN);
	}
}

void FrontendHost::pauseSystem(bool state) noexcept {
	if (!mSystemCore) { return; }

	if (state) {
		mSystemCore->addSystemState(EmuState::PAUSED);
	} else {
		mSystemCore->subSystemState(EmuState::PAUSED);
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

	GlobalAudioBase::Settings GAB_settings;
	BasicVideoSpec::Settings BVS_settings;

	HDM->parseMainAppConfig(
		GAB_settings.map(),
		BVS_settings.map()
	);

	GAB = GlobalAudioBase::initialize(GAB_settings);
	if (GAB->getStatus() == GlobalAudioBase::STATUS::NO_AUDIO)
		{ blog.newEntry<BLOG::WARN>("Audio Subsystem is not available!"); }

	BVS = BasicVideoSpec::initialize(BVS_settings);
	if (!BVS) { return false; }

	return true;
}

s32  FrontendHost::processEvents(void* event) noexcept {
	FrontendInterface::ProcessEvent(event);

	auto sdl_event{ reinterpret_cast<SDL_Event*>(event) };
	if (BVS->isMainWindowID(sdl_event->window.windowID)) {
		switch (sdl_event->type) {
			case SDL_EVENT_QUIT:
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
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

void FrontendHost::processFrame() {
	checkForHotkeys();

	const auto dialogResult{ HDM->getProbableFile() };
	if (dialogResult) { loadGameFile(*dialogResult); }

	if (!BVS->isSuccessful())
		[[unlikely]] { return; }

	BVS->renderPresent(!!mSystemCore, mSystemCore && mShowOverlay
		? mSystemCore->copyOverlayData().c_str() : nullptr);
}

void FrontendHost::openFileDialog() noexcept {
	SDL_ShowOpenFileDialog(HomeDirManager::probableFileCallback,
		nullptr, BVS->getMainWindow(), nullptr, 0, nullptr, false);
}

void FrontendHost::checkForHotkeys() {
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
		if (Input.isPressed(KEY(ESCAPE)))
			{ discardCore(); return; }
		if (Input.isPressed(KEY(BACKSPACE)))
			{ replaceCore(); return; }

		if (Input.isPressed(KEY(F11)))
			{ mShowOverlay = !mShowOverlay; }
		if (Input.isPressed(KEY(F10)))
			{ mUnlimited = !mUnlimited; toggleSystemLimiter(); }
	}
}

void FrontendHost::toggleSystemLimiter() noexcept {
	if (!mSystemCore) { return; }

	if (mUnlimited) {
		mSystemCore->addSystemState(EmuState::BENCH);
	} else {
		mSystemCore->subSystemState(EmuState::BENCH);
	}
}

/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
