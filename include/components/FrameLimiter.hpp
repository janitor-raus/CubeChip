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
	using uint64 = unsigned long long;

	bool   initTimeCheck{}; // forces timestamp update on first check only
	bool   skipFirstPass{}; // forces valid frame return on first check only
	bool   skipLostFrame{}; // forces frameskip if timeOvershoot > timeFrequency
	bool   lastFrameLost{}; // missed frame indicator when frameskip is enabled

	float  timeFrequency{}; // holds time (ms) per unit Hertz
	float  timeOvershoot{}; // holds time remainder (ms) from last successful check
	float  timeVariation{}; // holds time difference between last check and now
	chrono timePastFrame{}; // holds timestamp of the last frame's check
	uint64 validFrameCnt{}; // counter of successful frame checks performed

	inline bool isValidFrame() noexcept;
	inline auto getElapsedTime() const noexcept
		{ return std::chrono::steady_clock::now() - timePastFrame; }

/*==================================================================*/

public:
	FrameLimiter(
		float framerate = 60.0f, // 0.5 ... 1000 range
		bool  firstpass = true,  // skipFirstPass flag
		bool  lostframe = true   // skipLostFrame flag
	) noexcept {
		setLimiter(framerate, firstpass, lostframe);
	}

	FrameLimiter(const FrameLimiter& other) noexcept
		: skipFirstPass{ other.skipFirstPass }
		, skipLostFrame{ other.skipLostFrame }
		, timeFrequency{ other.timeFrequency }
	{}

	void setLimiter(float framerate) noexcept;
	void setLimiter(float framerate, bool firstpass, bool lostframe) noexcept;

/*==================================================================*/

	bool checkTime() noexcept;

	auto getElapsedMillisSince() const noexcept {
		using namespace std::chrono;
		return duration_cast<milliseconds>(getElapsedTime()).count();
	}

	auto getElapsedMicrosSince() const noexcept {
		using namespace std::chrono;
		return duration_cast<microseconds>(getElapsedTime()).count();
	}

/*==================================================================*/

	auto getValidFrameCounter() const noexcept { return validFrameCnt; }
	auto getElapsedMillisLast() const noexcept { return timeVariation; }
	auto getFramespan()         const noexcept { return timeFrequency; }
	auto getOvershoot()         const noexcept { return timeOvershoot; }
	auto getRemainder()         const noexcept { return timeFrequency - timeVariation; }
	auto getPercentage()        const noexcept { return timeVariation / timeFrequency; }
	bool isKeepingPace()        const noexcept { return timeOvershoot < timeFrequency && !lastFrameLost; }
};
