/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "SimpleTimer.hpp"

/*==================================================================*/

auto SimpleTimer::start() noexcept -> self& {
	mPausedTime = duration::zero();
	mTimerStart = clock::now();
	mTimerStop  = {};
	mIsPaused = false;
	mIsActive = true;
	return *this;
}

auto SimpleTimer::resume() noexcept -> self& {
	if (is_paused()) {
		mPausedTime += clock::now() - mTimerStop;
		mIsPaused    = false;
	}
	return *this;
}

auto SimpleTimer::pause() noexcept -> self& {
	if (!is_paused()) {
		mTimerStop = clock::now();
		mIsPaused  = true;
	}
	return *this;
}

auto SimpleTimer::reset() noexcept -> self& {
	mPausedTime = duration::zero();
	mTimerStart = mTimerStop = {};
	mIsActive   = mIsPaused  = false;
	return *this;
}

/*==================================================================*/

auto SimpleTimer::mark_lap() noexcept {
	const auto current_time{ clock::now() };

	duration diff{ duration::zero() };
	if (is_active()) {
		if (is_paused()) {
			diff = mTimerStop - mTimerStart - mPausedTime;
		} else {
			diff = current_time - mTimerStart - mPausedTime;
		}
	}

	mTimerStart = current_time;
	mPausedTime = duration::zero();
	mTimerStop  = {};
	mIsPaused   = false;
	mIsActive   = true;

	return diff;
}

float SimpleTimer::lap_millis() noexcept
	{ return std::chrono::duration<float, std::milli>(mark_lap()).count(); }

float SimpleTimer::lap_micros() noexcept
	{ return std::chrono::duration<float, std::micro>(mark_lap()).count(); }

/*==================================================================*/

auto SimpleTimer::get_elapsed() const noexcept {
	if (!is_active()) [[unlikely]]
		{ return duration::zero(); }

	if (!is_paused()) {
		return clock::now() - mTimerStart - mPausedTime;
	} else {
		return mTimerStop - mTimerStart - mPausedTime;
	}
}

bool SimpleTimer::has_millis_elapsed(float time) const noexcept
	{ return get_elapsed_millis() >= time; }

bool SimpleTimer::has_micros_elapsed(float time) const noexcept
	{ return get_elapsed_micros() >= time; }

float SimpleTimer::get_elapsed_millis() const noexcept
	{ return std::chrono::duration<float, std::milli>(get_elapsed()).count(); }

float SimpleTimer::get_elapsed_micros() const noexcept
	{ return std::chrono::duration<float, std::micro>(get_elapsed()).count(); }

float SimpleTimer::get_current_millis() noexcept
	{ return std::chrono::duration<float, std::milli>(clock::now().time_since_epoch()).count(); }

float SimpleTimer::get_current_micros() noexcept
	{ return std::chrono::duration<float, std::micro>(clock::now().time_since_epoch()).count(); }


