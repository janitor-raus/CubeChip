/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <concepts>
#include <memory>

/*==================================================================*/

class AudioStreamingFilter {
	virtual float filterSample(float sample) noexcept = 0;

public:
	virtual ~AudioStreamingFilter() = default;

	virtual void setCoefficient(float sampleRate, float cutoffFreq) noexcept = 0;

	template <typename T>
		requires (std::is_integral_v<T> || std::is_floating_point_v<T>)
	T process(T sample) noexcept
		{ return static_cast<T>(filterSample(static_cast<float>(sample))); }
};

/*==================================================================*/

using UniqueFilter = std::unique_ptr<AudioStreamingFilter>;

template <typename Filter, typename... Args>
	requires (std::is_base_of_v<AudioStreamingFilter, Filter>)
UniqueFilter make_stream_filter(Args&&... args)
	{ return std::make_unique<Filter>(std::forward<Args>(args)...); }

/*==================================================================*/

class LowPassFilter final : public AudioStreamingFilter {
	float mLastSampleI{};
	float mCoefficient{};

public:
	LowPassFilter(float sampleRate, float cutoffFreq = 0.0f) noexcept;

	void setCoefficient(float sampleRate, float cutoffFreq = 0.0f) noexcept override;

	float filterSample(float sample) noexcept override;
};

/*==================================================================*/

class HighPassFilter final : public AudioStreamingFilter {
	float mLastSampleI{};
	float mLastSampleO{};
	float mCoefficient{};

public:
	HighPassFilter(float sampleRate, float cutoffFreq = 0.0f) noexcept;

	void setCoefficient(float sampleRate, float cutoffFreq = 0.0f) noexcept override;

	float filterSample(float sample) noexcept override;
};
