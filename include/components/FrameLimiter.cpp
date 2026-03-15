/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <cmath>
#include <thread>
#include <algorithm>

#include "FrameLimiter.hpp"
#include "RelaxCPU.hpp"

/*==================================================================*/

static constexpr auto c_millis = std::chrono::milliseconds(1);

// 99.5% of 1ms sleeps measure below this value
static constexpr auto c_spin_threshold = 2.3f;

static auto current_time() noexcept { return std::chrono::steady_clock::now(); }

/*==================================================================*/

void FrameLimiter::set_limiter_props(float framerate) noexcept {
	m_target_time_period = 1000.0f / std::clamp(framerate, 0.5f, 10000.0f);
}

void FrameLimiter::set_limiter_props(float framerate, bool force_initial_pass, bool force_skip_periods) noexcept {
	set_limiter_props(framerate);
	m_force_initial_pass = force_initial_pass;
	m_force_skip_periods = force_skip_periods;
	m_missed_last_period = false;
}

/*==================================================================*/

bool FrameLimiter::is_frame_ready(bool lazy) noexcept {
	if (has_target_period_elapsed()) { return true; }

	if ((lazy && m_target_time_period >= c_spin_threshold) \
		|| (get_period_remaining() >= c_spin_threshold))
	{
		std::this_thread::sleep_for(c_millis);
		return false;
	} else {
		for (auto i = 0; ++i <= 128;) { ::cpu_relax(); }
		std::this_thread::yield();
		return false;
	}
}

bool FrameLimiter::is_frame_ready_no_block() noexcept {
	return has_target_period_elapsed();
}

auto FrameLimiter::get_elapsed_millis_since() const noexcept {
	return std::chrono::duration_cast<std::chrono::microseconds>
		(::current_time() - m_last_period_origin).count() / 1e3f;
}

auto FrameLimiter::get_elapsed_micros_since() const noexcept {
	return std::chrono::duration_cast<std::chrono::nanoseconds>
		(::current_time() - m_last_period_origin).count() / 1e3f;
}

/*==================================================================*/

bool FrameLimiter::has_target_period_elapsed() noexcept {
	const auto current_time_point = ::current_time();

	if (!m_new_init_completed) [[unlikely]] {
		m_last_period_origin = current_time_point;
		m_new_init_completed = true;
	}

	if (m_force_initial_pass) [[unlikely]] {
		m_force_initial_pass = false;
		++m_valid_period_count;
		return true;
	}

	m_time_elapsed_since = m_time_yield_accrued + std::chrono::duration
		<float, std::milli>(current_time_point - m_last_period_origin).count();

	if (m_time_elapsed_since < m_target_time_period)
		[[likely]] { return false; }

	if (m_force_skip_periods) {
		m_missed_last_period = m_time_elapsed_since >= m_target_time_period + 0.050f;
		m_time_yield_accrued = std::fmod(m_time_elapsed_since, m_target_time_period);
	} else {
		// without frameskip, we carry over frame debt until caught up
		m_time_yield_accrued = m_time_elapsed_since - m_target_time_period;
	}

	m_last_period_origin = current_time_point;
	++m_valid_period_count;
	return true;
}
