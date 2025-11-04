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

	float targetFramePeriod{}; // stores the desired frame period
	float elapsedOverTarget{}; // stores time exceeded over target frame period
	float previousTimeDelta{}; // stores delta between last two frame checks

	chrono previousFrameTime{}; // stores timestamp of last valid frame check
	sint64 validFrameCounter{}; // counts total amount of valid frame checks

private:
	bool hasPeriodElapsed() noexcept;

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
	bool checkTime() noexcept;
	bool checkTimeNoBlock() noexcept;

	auto getElapsedMillisSince() const noexcept;
	auto getElapsedMicrosSince() const noexcept;

/*==================================================================*/

public:
	auto getValidFrameCounter() const noexcept { return validFrameCounter; }
	auto getElapsedMillisLast() const noexcept { return previousTimeDelta; }
	auto getFramespan()         const noexcept { return targetFramePeriod; }
	auto getOvershoot()         const noexcept { return elapsedOverTarget; }
	auto getRemainder()         const noexcept { return targetFramePeriod - previousTimeDelta; }
	auto getPercentage()        const noexcept { return previousTimeDelta / targetFramePeriod; }
	bool isKeepingPace()        const noexcept { return elapsedOverTarget < targetFramePeriod && !previousFrameSkip; }
};
