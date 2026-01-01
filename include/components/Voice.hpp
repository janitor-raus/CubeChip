/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <algorithm>

#include "Concepts.hpp"
#include "Waveforms.hpp"
#include "AudioDevice.hpp"

/*==================================================================*/

constexpr inline float transient_gain(unsigned iter, float step = 0.01f) noexcept {
	return std::min(0.0f + step * (iter + 1), 1.0f);
}

constexpr inline float transient_fall(unsigned iter, float step = 0.01f) noexcept {
	return std::max(1.0f - step * (iter + 1), 0.0f);
}

/*==================================================================*/

/**
* @brief The TransienceGain struct is used to calculate and return gain for a fade-in/out period.
* Usually constructed from an AudioTimer instance, but can be operated independently too.
*/
struct TransienceGain {
	bool intro    : 1 = false;
	bool outro    : 1 = false;
	bool fallback : 1 = true;

	constexpr TransienceGain() noexcept = default;
	constexpr TransienceGain(bool intro, bool outro, bool fallback) noexcept
		: intro(intro), outro(outro), fallback(fallback)
	{}

	constexpr auto calculate(unsigned sample_idx, float step = 0.01f) const noexcept {
		return  intro ? ::transient_gain(sample_idx, step) :
				outro ? ::transient_fall(sample_idx, step) : fallback;
	}
};

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

	// Check if the timer is currently rising (intro)
	constexpr bool intro() const noexcept { return m_timer_new && !m_timer_old; }

	// Check if the timer is currently falling (outro)
	constexpr bool outro() const noexcept { return !m_timer_new && m_timer_old; }

	constexpr operator unsigned() const noexcept { return m_timer_new; }

	constexpr operator TransienceGain() const noexcept {
		return TransienceGain{ intro(), outro(), !!m_timer_new };
	}
};

/*==================================================================*/

/**
 * @brief The Voice class represents a single audio voice with phase, step, and volume control.
 * It provides methods to manage phase progression and volume levels, and is suitable for
 * voice synthesis and audio processing.
 */
class Voice {
	using self = Voice;

protected:
	double m_phase{}; // [0..1) range.
	double m_step{};  // [0..1) range.
	float  m_volume_gain{}; // System-facing volume control, to be controlled by a system.
	float  m_master_gain{}; // Mastering volume control to manually balance against other voices.

public:
	// Pass along additional data to a voice processor, if needed.
	void* userdata{};

	Voice(float master_gain = 0.2f) noexcept : m_volume_gain(1.0f) {
		set_master_gain(master_gain);
	}

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

	// Get the current level of the voice sample, optionally with transience gain calculation.
	constexpr float get_level(unsigned sample_idx, TransienceGain transience = {}) const noexcept {
		return transience.calculate(sample_idx) * get_volume() * get_master_gain();
	}
};

/*==================================================================*/

using Stream = AudioDevice::Stream;

using SampleGenerator = void (*)(float*, unsigned, Voice*, Stream*) noexcept;

struct GeneratorBundle {
	SampleGenerator functor;
	Voice* voice;

	GeneratorBundle(SampleGenerator p, Voice* v) noexcept
		: functor(p), voice(v)
	{}

	template <IsContiguousContainerOf<float> T>
	constexpr void run(T& buffer, Stream* stream) const noexcept {
		functor(std::data(buffer), unsigned(std::size(buffer)), voice, stream);
	}
};

using VoiceGenerators = std::initializer_list<GeneratorBundle>;
