/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <fmt/format.h>

#include "ThreadAffinity.hpp"
#include "BasicVideoSpec.hpp"
#include "FrameLimiter.hpp"

#include "SystemInterface.hpp"

/*==================================================================*/

void SystemInterface::startWorker() noexcept {
	if (!mSystemThread.joinable()) {
		initializeSystem();
		mSystemThread = Thread([this](StopToken token) { systemThreadEntry(token); });
	}
	if (!mTimingThread.joinable()) {
		mTimingThread = Thread([this](StopToken token) { timingThreadEntry(token); });
	}
}

void SystemInterface::stopWorker() noexcept {
	if (mSystemThread.joinable()) {
		mSystemThread.request_stop();
		declareNextFrame(true);
		mSystemThread.join();
	}
	if (mTimingThread.joinable()) {
		mTimingThread.request_stop();
		mTimingThread.join();
	}
}

void SystemInterface::systemThreadEntry(StopToken token) {
	SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);
	thread_affinity::set_affinity(~0b11ull);

	do {
		standbyNextFrame(false);

		mElapsedFrames += 1;
		mBenchedFrames = hasSystemState(EmuState::BENCH)
			? mBenchedFrames + 1 : 0;

		mTimer.start();
		mainSystemLoop();
		mFrameBusyTime.store(mTimer.get_elapsed_millis(), mo::release);
	} while (!token.stop_requested());
}

void SystemInterface::timingThreadEntry(StopToken token) {
	SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);
	thread_affinity::set_affinity(0b11ull);

	FrameLimiter Pacer(getRealSystemFramerate());

	do {
		if (Pacer.isFrameReady(!hasSystemState(EmuState::BENCH))) {
			Pacer.setLimiterProperties(getRealSystemFramerate());

			if (!canSystemWork()) [[unlikely]] { continue; }

			setStopFrame(true);
			declareNextFrame(true);
		}
	} while (!token.stop_requested());
}

SystemInterface::SystemInterface() noexcept
	: mOverlayData{ std::make_shared<Str>() }
{
	static thread_local auto pRNG  { std::make_unique<Well512>()       };
	static thread_local auto pInput{ std::make_unique<BasicKeyboard>() };
	RNG   = pRNG.get();
	Input = pInput.get();
}

/*==================================================================*/

void SystemInterface::setViewportSizes(bool cond, u32 W, u32 H, u32 mult, u32 ppad) noexcept {
	// should go through VideoDevice later instead
	if (cond) { BVS->setViewportSizes(s32(W), s32(H), s32(mult), s32(ppad)); }
}

void SystemInterface::setDisplayBorderColor(u32 color) noexcept {
	// should go through VideoDevice later instead
	BVS->setBorderColor(color);
}

f32 SystemInterface::getBaseSystemFramerate() const noexcept
	{ return mBaseSystemFramerate.load(mo::relaxed); }

f32 SystemInterface::getFramerateMultiplier() const noexcept
	{ return mFramerateMultiplier.load(mo::relaxed); }

f32 SystemInterface::getRealSystemFramerate() const noexcept
	{ return getBaseSystemFramerate() * getFramerateMultiplier(); }

void SystemInterface::setBaseSystemFramerate(f32 value) noexcept
	{ mBaseSystemFramerate.store(std::clamp(value, 24.0f, 100.0f), mo::relaxed); }

void SystemInterface::setFramerateMultiplier(f32 value) noexcept
	{ mFramerateMultiplier.store(std::clamp(value, 0.10f, 10.00f), mo::relaxed); }

void SystemInterface::saveOverlayData(const Str* data) {
	mOverlayData.store(std::make_shared<Str>(*data), mo::release);
}

Str* SystemInterface::makeOverlayData() {
	const auto framerate{ getRealSystemFramerate() };
	const auto frametime{ mTimer.get_elapsed_millis() };
	const auto framespan{ 1000.0f / framerate };

	*getOverlayDataBuffer() = fmt::format(
		"Framerate:{:9.3f} fps |{:9.3f}ms\n"
		"Frametime:{:9.3f} ms ({:>6.2f}%)\n",
		framerate, framespan, frametime,
		frametime / framespan * 100
	);

	return getOverlayDataBuffer();
}

void SystemInterface::pushOverlayData() {
	saveOverlayData(SystemInterface::makeOverlayData());
}

Str SystemInterface::copyOverlayData() const noexcept {
	return *mOverlayData.load(mo::acquire);
}
