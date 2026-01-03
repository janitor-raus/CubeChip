/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <numbers>
#include "AudioFilters.hpp"

/*==================================================================*/

LowPassFilter::LowPassFilter(float sampleRate, float cutoffFreq) noexcept {
	set_coefficient(sampleRate, cutoffFreq);
}

void LowPassFilter::set_coefficient(float sampleRate, float cutoffFreq) noexcept {
	if (sampleRate <= 1.0f) {
		m_coefficient = 0.0f;
	} else {
		const auto dt = 1.0f / sampleRate;
		const auto rc = 1.0f / (2.0f * float(std::numbers::pi) \
			* (cutoffFreq ? cutoffFreq : sampleRate * 0.01f));
		m_coefficient = dt / (rc + dt);
	}
}

float LowPassFilter::filter_sample(float sample) noexcept {
	if (m_coefficient <= 0.0f) { return sample; }
	else [[likely]] {
		m_last_sample_in = m_coefficient * sample \
			+ (1.0f - m_coefficient) * m_last_sample_in;
		return m_last_sample_in;
	}
}

/*==================================================================*/

HighPassFilter::HighPassFilter(float sampleRate, float cutoffFreq) noexcept {
	set_coefficient(sampleRate, cutoffFreq);
}

void HighPassFilter::set_coefficient(float sampleRate, float cutoffFreq) noexcept {
	if (sampleRate <= 1.0f) {
		m_coefficient = 0.0f;
	} else {
		const auto dt = 1.0f / sampleRate;
		const auto rc = 1.0f / (2.0f * float(std::numbers::pi) \
			* (cutoffFreq ? cutoffFreq : sampleRate * 0.01f));
		m_coefficient = rc / (rc + dt);
	}
}

float HighPassFilter::filter_sample(float sample) noexcept {
	if (m_coefficient <= 0.0f) { return sample; }
	else [[likely]] {
		const auto output = m_coefficient * (m_last_sample_out + sample - m_last_sample_in);
		m_last_sample_in  = sample;
		m_last_sample_out = output;
		return output;
	}
}
