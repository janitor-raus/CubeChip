/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <cmath>
#include <chrono>
#include <thread>
#include <algorithm>

#include "FrameLimiter.hpp"

/*==================================================================*/

void FrameLimiter::setLimiter(float framerate) noexcept
	{ timeFrequency = 1000.0f / std::clamp(framerate, 0.5f, 1000.0f); }

void FrameLimiter::setLimiter(float framerate, bool firstpass, bool lostframe) noexcept {
	setLimiter(framerate);
	skipFirstPass = firstpass;
	skipLostFrame = lostframe;
}

/*==================================================================*/

bool FrameLimiter::checkTime() noexcept {
	using millis = std::chrono::milliseconds;
	if (isValidFrame()) { return true; }

	if (getRemainder() >= 2.3f)
		{ std::this_thread::sleep_for(millis(1)); }
	else
		{ std::this_thread::yield(); }

	return false;
}

/*==================================================================*/

inline bool FrameLimiter::isValidFrame() noexcept {
	using namespace std::chrono;
	const auto timeAtCurrent{ steady_clock::now() };

	if (!initTimeCheck) [[unlikely]] {
		timePastFrame = timeAtCurrent;
		initTimeCheck = true;
	}

	if (skipFirstPass) [[unlikely]] {
		skipFirstPass = false;
		++validFrameCnt;
		return true;
	}

	timeVariation = {
		timeOvershoot + duration<float, std::milli>
		(timeAtCurrent - timePastFrame).count()
	};

	if (timeVariation < timeFrequency)
		[[likely]] { return false; }

	if (skipLostFrame) {
		lastFrameLost = timeVariation >= timeFrequency + 0.050f;
		timeOvershoot = std::fmod(timeVariation, timeFrequency);
	} else {
		timeOvershoot = timeVariation - timeFrequency;
	}

	timePastFrame = timeAtCurrent;
	++validFrameCnt;
	return true;
}
