/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <chrono>

/*==================================================================*/

class FrameLimiter final {
	using chrono = std::chrono::steady_clock::time_point;
	using sint64 = long long;

	bool doneFirstRunSetup{}; // forces initial setup of previousFrameTime
	bool forceInitialFrame{}; // forces initial frame to pass unconditionally
	bool allowMissedFrames{}; // allows missed frames when elapsedOverTarget > targetFramePeriod
	bool previousFrameSkip{}; // indicates if frameskip occurred on last check (not how many)

	float targetFramePeriod{}; // stores the target frame period (1s / framerate) in millis
	float elapsedOverTarget{}; // stores time exceeded over last target frame period
	float elapsedTimePeriod{}; // stores time elapsed since last valid frame check

	chrono previousFrameTime{}; // stores timestamp of last valid frame check
	sint64 validFrameCounter{}; // counts total amount of valid frame checks

private:
	bool hasTargetPeriodElapsed() noexcept;

/*==================================================================*/

public:
	FrameLimiter(
		float framerate = 60.0f,
		bool  force_initial_frame = true,
		bool  allow_missed_frames = true
	) noexcept {
		setLimiterProperties(framerate, force_initial_frame, allow_missed_frames);
	}

	FrameLimiter(const FrameLimiter&)            = delete;
	FrameLimiter& operator=(const FrameLimiter&) = delete;
	FrameLimiter& operator=(FrameLimiter&&)      = delete;

	void setLimiterProperties(float framerate) noexcept;
	void setLimiterProperties(float framerate, bool force_initial_frame, bool allow_missed_frames) noexcept;

/*==================================================================*/

public:
	// Check if a new frame is ready, yield/sleep as needed otherwise.
	// If 'lazy' is true, does not yield at all, only sleeps.
	bool isFrameReady(bool lazy = false) noexcept;
	// Check if a new frame is ready, without blocking the thread.
	bool isFrameReadyNoBlock() noexcept;

	auto getElapsedMillisSince() const noexcept;
	auto getElapsedMicrosSince() const noexcept;

/*==================================================================*/

public:
	auto getValidFrameCounter() const noexcept { return validFrameCounter; }
	auto getElapsedMillisLast() const noexcept { return elapsedTimePeriod; }
	auto getTargetFramePeriod() const noexcept { return targetFramePeriod; }
	auto getElapsedOverTarget() const noexcept { return elapsedOverTarget; }
	auto getRemainderToTarget() const noexcept { return targetFramePeriod - elapsedTimePeriod; }
	auto getFractionForTarget() const noexcept { return elapsedTimePeriod / targetFramePeriod; }

	bool isKeepingPace() const noexcept { return !previousFrameSkip; }
};
