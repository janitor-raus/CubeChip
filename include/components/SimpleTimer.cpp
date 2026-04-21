/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "SimpleTimer.hpp"

/*==================================================================*/

auto SimpleTimer::start() noexcept -> self& {
	m_paused_for = duration::zero();
	m_time_start = clock::now();
	m_time_stop  = time_point();
	m_is_paused  = false;
	m_is_active  = true;
	return *this;
}

auto SimpleTimer::resume() noexcept -> self& {
	if (is_paused()) {
		m_paused_for += clock::now() - m_time_stop;
		m_is_paused   = false;
	}
	return *this;
}

auto SimpleTimer::pause() noexcept -> self& {
	if (!is_paused()) {
		m_time_stop = clock::now();
		m_is_paused = true;
	}
	return *this;
}

auto SimpleTimer::reset() noexcept -> self& {
	m_paused_for = duration::zero();
	m_time_start = m_time_stop = time_point();
	m_is_active  = m_is_paused = false;
	return *this;
}

/*==================================================================*/

auto SimpleTimer::mark_lap() noexcept {
	const auto current_time = clock::now();

	duration diff = duration::zero();
	if (is_active()) {
		if (is_paused()) {
			diff = m_time_stop - m_time_start - m_paused_for;
		} else {
			diff = current_time - m_time_start - m_paused_for;
		}
	}

	m_time_start = current_time;
	m_paused_for = duration::zero();
	m_time_stop  = time_point();
	m_is_paused  = false;
	m_is_active  = true;

	return diff;
}

float SimpleTimer::lap_millis() noexcept {
	return std::chrono::duration<float, std::milli>(mark_lap()).count();
}

float SimpleTimer::lap_micros() noexcept {
	return std::chrono::duration<float, std::micro>(mark_lap()).count();
}

/*==================================================================*/

auto SimpleTimer::get_elapsed() const noexcept {
	if (!is_active()) [[unlikely]] {
		return duration::zero();
	}

	if (!is_paused()) {
		return clock::now() - m_time_start - m_paused_for;
	} else {
		return m_time_stop - m_time_start - m_paused_for;
	}
}

bool SimpleTimer::has_millis_elapsed(float time) const noexcept {
	return get_elapsed_millis() >= time;
}

bool SimpleTimer::has_micros_elapsed(float time) const noexcept {
	return get_elapsed_micros() >= time;
}

float SimpleTimer::get_elapsed_millis() const noexcept {
	return std::chrono::duration<float, std::milli>(get_elapsed()).count();
}

float SimpleTimer::get_elapsed_micros() const noexcept {
	return std::chrono::duration<float, std::micro>(get_elapsed()).count();
}

float SimpleTimer::get_current_millis() noexcept {
	return std::chrono::duration<float, std::milli>(clock::now().time_since_epoch()).count();
}

float SimpleTimer::get_current_micros() noexcept {
	return std::chrono::duration<float, std::micro>(clock::now().time_since_epoch()).count();
}
