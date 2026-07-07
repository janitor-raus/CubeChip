/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <algorithm>

#include "Waveforms.hpp"

/*==================================================================*/

inline constexpr auto c_envelope_default_step = 0.01f;

inline constexpr float transient_rise(unsigned iter, float step = c_envelope_default_step) noexcept {
	return std::min(0.0f + step * (iter + 1), 1.0f);
}

inline constexpr float transient_fall(unsigned iter, float step = c_envelope_default_step) noexcept {
	return std::max(1.0f - step * (iter + 1), 0.0f);
}

/**
* @brief The FadeEnvelope struct is used to adjust sample strength for a fade-in/out period.
* Usually constructed from an AudioTimer instance, but can be operated independently too.
*/
struct FadeEnvelope {
	bool intro    : 1 = false;
	bool outro    : 1 = false;
	bool fallback : 1 = true;

	static constexpr auto default_step = c_envelope_default_step;

	constexpr FadeEnvelope() noexcept = default;
	constexpr FadeEnvelope(bool intro, bool outro, bool fallback) noexcept
		: intro(intro), outro(outro), fallback(fallback)
	{}

	constexpr auto calculate(unsigned sample_idx, float step = default_step) const noexcept {
		return  intro ? ::transient_rise(sample_idx, step) :
				outro ? ::transient_fall(sample_idx, step) : float(fallback);
	}
};

inline constexpr float calc_fade_step(std::size_t sample_count, float fade_divisor = 2.0f) noexcept {
	return sample_count ? std::max(c_envelope_default_step, fade_divisor / float(sample_count)) : 0.0f;
}

/*==================================================================*/

/**
* @brief The AudioTimer represents how many frames a voice can be active.
* The internal state change, when the AudioTimer is updated, allows for transient
* gain calculations, though they're not useful for data-driven voices.
*/
class AudioTimer {
	unsigned m_timer_old{};
	unsigned m_timer_new{};

public:
	constexpr unsigned get()        const noexcept { return m_timer_new; }
	constexpr void     set(unsigned time) noexcept { m_timer_old = std::exchange(m_timer_new, time); }
	constexpr void     dec()              noexcept { set(m_timer_new ? m_timer_new - 1 : 0); }

	// Reset the timer to zero, clearing both old and new values
	constexpr void reset() noexcept { m_timer_old = m_timer_new = 0; }

	// Check if the timer is currently rising (intro)
	constexpr bool intro() const noexcept { return m_timer_new && !m_timer_old; }

	// Check if the timer is currently falling (outro)
	constexpr bool outro() const noexcept { return !m_timer_new && m_timer_old; }

	constexpr operator unsigned() const noexcept { return m_timer_new; }

	constexpr operator FadeEnvelope () const noexcept {
		return FadeEnvelope{ intro(), outro(), !!m_timer_new };
	}
};

/*==================================================================*/

/**
 * @brief The Voice class represents a single audio voice with phase, step, volume control
 * and a built-in AudioTimer. It provides methods to manage phase progression and volume levels, and is suitable for
 * voice synthesis and audio processing. Implicitly converts to the current timer value to
 * allow easy gauging of the voice's active state and duration remaining.
 */
class Voice {
	using self = Voice;

protected:
	double m_phase{}; // [0..1) range.
	double m_step{};  // [0..1) range.
	float  m_volume_gain{}; // System-facing volume control, to be controlled by a system.
	float  m_master_gain{}; // Mastering volume control to manually balance against other voices.

public:
	AudioTimer timer{};

	Voice(float master_gain = 0.2f) noexcept : m_volume_gain(1.0f) {
		set_master_gain(master_gain);
	}

	// When accessed directly, implicitly forward along to the timer member.
	constexpr operator unsigned() const noexcept { return timer.get(); }

	// Get the volume of the voice, in range of: [0..1]
	constexpr float get_volume()     const noexcept { return m_volume_gain; }
	// Set the volume of the voice, clamped to: [0..1]
	constexpr self& set_volume(float gain) noexcept { m_volume_gain = std::clamp(gain, 0.0f, 1.0f); return *this; }

	// Get the mastering volume of the voice, in range of: [0..1]
	constexpr float get_master_gain()     const noexcept { return m_master_gain; }
	// Set the mastering volume of the voice, clamped to: [0..1]
	constexpr self& set_master_gain(float gain) noexcept { m_master_gain = std::clamp(gain, 0.0f, 1.0f); return *this; }

	constexpr Phase get_step()     const noexcept { return m_step; }
	constexpr self& set_step(Phase step) noexcept { m_step = step; return *this; }

	constexpr Phase get_phase()      const noexcept { return m_phase; }
	constexpr self& set_phase(Phase phase) noexcept { m_phase = phase; return *this; }

	// Peek the next raw phase without wrapping it, default is 1 step ahead.
	constexpr double peek_raw_phase(double steps = 1.0) const noexcept {
		return m_phase + m_step * steps;
	}

	// Peek the next phase without modifying it, default is 1 step ahead.
	constexpr Phase peek_phase(double steps = 1.0) const noexcept {
		return peek_raw_phase(steps);
	}

	// Advance the phase by a number of steps, default is 1 step ahead.
	constexpr self& step_phase(double steps = 1.0) noexcept {
		m_phase = peek_phase(steps); return *this;
	}

	// Get the current level of the voice sample, optionally with fade envelope calculation.
	constexpr float get_level(unsigned sample_idx, FadeEnvelope envelope = {}) const noexcept {
		return envelope.calculate(sample_idx) * get_volume() * get_master_gain();
	}

	// Get the current level of the voice sample with fade envelope calculation and optional custom step.
	constexpr float get_level(unsigned sample_idx, FadeEnvelope envelope, float fade_step) const noexcept {
		return envelope.calculate(sample_idx, fade_step) * get_volume() * get_master_gain();
	}
};

/*==================================================================*/

#include <span>
using SampleBuffer = std::span<float>;

template <typename T>
concept IsSampleGenerator = std::is_nothrow_invocable_r_v<void, T, SampleBuffer>;
