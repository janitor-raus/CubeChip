/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <numbers>
#include "AudioFilters.hpp"

/*==================================================================*/

LowPassFilter::LowPassFilter(float sampleRate, float cutoffFreq) noexcept
	{ setCoefficient(sampleRate, cutoffFreq); }

void LowPassFilter::setCoefficient(float sampleRate, float cutoffFreq) noexcept {
	if (sampleRate <= 1.0f) {
		mCoefficient = 0.0f;
	} else {
		const auto dt = 1.0f / sampleRate;
		const auto rc = 1.0f / (2.0f * float(std::numbers::pi) \
			* (cutoffFreq ? cutoffFreq : sampleRate * 0.01f));
		mCoefficient = dt / (rc + dt);
	}
}

float LowPassFilter::filterSample(float sample) noexcept {
	if (mCoefficient <= 0.0f) { return sample; }
	else [[likely]] {
		mLastSampleI = mCoefficient * sample \
			+ (1.0f - mCoefficient) * mLastSampleI;
		return mLastSampleI;
	}
}

/*==================================================================*/

HighPassFilter::HighPassFilter(float sampleRate, float cutoffFreq) noexcept
	{ setCoefficient(sampleRate, cutoffFreq); }

void HighPassFilter::setCoefficient(float sampleRate, float cutoffFreq) noexcept {
	if (sampleRate <= 1.0f)
		{ mCoefficient = 0.0f; }
	else {
		const auto dt = 1.0f / sampleRate;
		const auto rc = 1.0f / (2.0f * float(std::numbers::pi) \
			* (cutoffFreq ? cutoffFreq : sampleRate * 0.01f));
		mCoefficient = rc / (rc + dt);
	}
}

float HighPassFilter::filterSample(float sample) noexcept {
	if (mCoefficient <= 0.0f) { return sample; }
	else [[likely]] {
		const auto output = mCoefficient * (mLastSampleO + sample - mLastSampleI);
		mLastSampleI = sample;
		mLastSampleO = output;
		return output;
	}
}
