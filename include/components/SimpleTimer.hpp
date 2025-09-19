/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <chrono>

/*==================================================================*/

class SimpleTimer {
	using clock      = std::chrono::steady_clock;
	using time_point = clock::time_point;
	using duration   = clock::duration;

	using self = SimpleTimer;

	duration   mPausedTime{};
	time_point mTimerStart{};
	time_point mTimerStop{};

	bool       mIsActive{};
	bool       mIsPaused{};

public:
	constexpr bool is_active() const noexcept { return mIsActive; }
	constexpr bool is_paused() const noexcept { return mIsPaused; }

public:
	auto start()  noexcept -> self&;
	auto resume() noexcept -> self&;
	auto pause()  noexcept -> self&;
	auto reset()  noexcept -> self&;

private:
	auto mark_lap() noexcept;

public:
	[[nodiscard]] float lap_millis() noexcept;
	[[nodiscard]] float lap_micros() noexcept;

private:
	auto get_elapsed() const noexcept;

public:
	bool has_millis_elapsed(float time) const noexcept;
	bool has_micros_elapsed(float time) const noexcept;

	float get_elapsed_millis() const noexcept;
	float get_elapsed_micros() const noexcept;

	static float get_current_millis() noexcept;
	static float get_current_micros() noexcept;
};
