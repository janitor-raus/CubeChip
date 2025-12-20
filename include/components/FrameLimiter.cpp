/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <cmath>
#include <thread>
#include <algorithm>

#include "FrameLimiter.hpp"

/*==================================================================*/

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
	#include <immintrin.h>
	#define cpu_relax() _mm_pause()
#elif defined(__aarch64__) || defined(__arm__)
	#define cpu_relax() asm volatile("yield")
#elif defined(__riscv) && defined(__riscv_zihintpause)
	#define cpu_relax() asm volatile("pause")
#else
	#define cpu_relax() ((void)0)
#endif

/*==================================================================*/

static constexpr auto millis = std::chrono::milliseconds(1);

// 99.5% of 1ms sleeps measure below this value
static constexpr auto spin_threshold = 2.3f;

static auto current_time() noexcept { return std::chrono::steady_clock::now(); }

/*==================================================================*/

void FrameLimiter::setLimiterProperties(float framerate) noexcept
	{ targetFramePeriod = 1000.0f / std::clamp(framerate, 0.5f, 10000.0f); }

void FrameLimiter::setLimiterProperties(float framerate, bool force_initial_frame, bool allow_missed_frames) noexcept {
	setLimiterProperties(framerate);
	forceInitialFrame = force_initial_frame;
	allowMissedFrames = allow_missed_frames;
	previousFrameSkip = false;
}

/*==================================================================*/

bool FrameLimiter::isFrameReady(bool lazy) noexcept {
	if (hasTargetPeriodElapsed()) { return true; }

	if ((lazy && targetFramePeriod >= spin_threshold) \
		|| (getRemainderToTarget() >= spin_threshold))
	{
		std::this_thread::sleep_for(millis);
		return false;
	} else {
		for (auto i = 0; ++i <= 128;) { cpu_relax(); }
		std::this_thread::yield();
		return false;
	}
}

bool FrameLimiter::isFrameReadyNoBlock() noexcept {
	return hasTargetPeriodElapsed();
}

auto FrameLimiter::getElapsedMillisSince() const noexcept {
	return std::chrono::duration_cast<std::chrono::microseconds>
		(current_time() - previousFrameTime).count() / 1e3f;
}

auto FrameLimiter::getElapsedMicrosSince() const noexcept {
	return std::chrono::duration_cast<std::chrono::nanoseconds>
		(current_time() - previousFrameTime).count() / 1e3f;
}

/*==================================================================*/

bool FrameLimiter::hasTargetPeriodElapsed() noexcept {
	const auto currentTimePoint = current_time();

	if (!doneFirstRunSetup) [[unlikely]] {
		previousFrameTime = currentTimePoint;
		doneFirstRunSetup = true;
	}

	if (forceInitialFrame) [[unlikely]] {
		forceInitialFrame = false;
		++validFrameCounter;
		return true;
	}

	elapsedTimePeriod = elapsedOverTarget + std::chrono::duration
		<float, std::milli>(currentTimePoint - previousFrameTime).count();

	if (elapsedTimePeriod < targetFramePeriod)
		[[likely]] { return false; }

	if (allowMissedFrames) {
		previousFrameSkip = elapsedTimePeriod >= targetFramePeriod + 0.050f;
		elapsedOverTarget = std::fmod(elapsedTimePeriod, targetFramePeriod);
	} else {
		// without frameskip, we carry over frame debt until caught up
		elapsedOverTarget = elapsedTimePeriod - targetFramePeriod;
	}

	previousFrameTime = currentTimePoint;
	++validFrameCounter;
	return true;
}
