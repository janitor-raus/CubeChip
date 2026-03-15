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

	bool   m_new_init_completed{}; // Ensures we use the current time in `last_period_origin` for the first check only. Initializtion measure.
	bool   m_force_initial_pass{}; // Force the first limiter check to pass unconditionally, without waiting for a whole frame period since initialization.
	bool   m_force_skip_periods{}; // Allows the limiter to skip period frames if the target time period is overshot, rather than accruing time yield to try and catch up later.
	bool   m_missed_last_period{}; // Indicates whether we skipped a frame in the last valid limiter check (does not track how many).

	float  m_target_time_period{}; // Stores the target frame period (1s / framerate) in floating-point milliseconds.
	float  m_time_yield_accrued{}; // Stores the amount of time accrued to yield for the next valid period, to prevent period drift from delayed limiter checks.
	float  m_time_elapsed_since{}; // Stores the time elapsed since the last valid limiter check.

	chrono m_last_period_origin{}; // Stores the actual timestamp of the last valid limiter check.
	sint64 m_valid_period_count{}; // Counts the total amount of valid limiter checks. Does not include skipped frames, if any.

private:
	bool has_target_period_elapsed() noexcept;

/*==================================================================*/

public:
	FrameLimiter(
		float framerate = 60.0f,
		bool  force_initial_pass = true,
		bool  force_skip_periods = true
	) noexcept {
		set_limiter_props(framerate, force_initial_pass, force_skip_periods);
	}

	FrameLimiter(const FrameLimiter&)            = delete;
	FrameLimiter& operator=(const FrameLimiter&) = delete;
	FrameLimiter& operator=(FrameLimiter&&)      = delete;

	void set_limiter_props(float framerate) noexcept;
	void set_limiter_props(float framerate, bool force_initial_pass, bool force_skip_periods) noexcept;

/*==================================================================*/

public:
	// Check if a new frame is ready, yield/sleep as needed otherwise.
	// If 'lazy' is true, does not yield at all, only sleeps.
	bool is_frame_ready(bool lazy = false) noexcept;
	// Check if a new frame is ready, without blocking the thread.
	bool is_frame_ready_no_block() noexcept;

	auto get_elapsed_millis_since() const noexcept;
	auto get_elapsed_micros_since() const noexcept;

/*==================================================================*/

public:
	bool has_missed_last_period() const noexcept { return m_missed_last_period; }
	auto get_valid_period_count() const noexcept { return m_valid_period_count; }
	auto get_time_elapsed_since() const noexcept { return m_time_elapsed_since; }
	auto get_target_time_period() const noexcept { return m_target_time_period; }
	auto get_time_yield_accrued() const noexcept { return m_time_yield_accrued; }

	auto get_period_remaining() const noexcept { return m_target_time_period - m_time_elapsed_since; }
	auto get_period_fraction()  const noexcept { return m_time_elapsed_since / m_target_time_period; }
};
