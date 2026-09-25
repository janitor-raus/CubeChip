/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <algorithm>

#include "AssignCast.hpp"
#include "GlobalAudioBase.hpp"
#include "AudioDevice.hpp"
#include "BasicLogger.hpp"

#include <SDL3/SDL_audio.h>

/*==================================================================*/

void AudioDevice::init_stream(Channels channels, signed freq, bool recording_device) noexcept {
	if (auto* device_ptr = SDL_OpenAudioDeviceStream(recording_device
		? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
		: SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		nullptr, nullptr, nullptr
	)) {
		m_stream.reset(device_ptr);
		set_spec(channels, freq);
	} else {
		blog.error("Failed to open audio stream: {}", SDL_GetError());
	}
}

/*==================================================================*/

static bool get_current_spec(SDL_AudioStream* stream, SDL_AudioSpec& current) noexcept {
	if (SDL_IsAudioDevicePlayback(SDL_GetAudioStreamDevice(stream))) {
		SDL_GetAudioStreamFormat(stream, &current, nullptr);
	} else {
		SDL_GetAudioStreamFormat(stream, nullptr, &current);
	}

	if (current.format == SDL_AUDIO_UNKNOWN) {
		blog.warn("Failed to fetch audio stream spec (the device might "
			"have been lost, consider re-creating it): {}", SDL_GetError());
		current.freq = 0; current.channels = 0;
	}

	return current.format != SDL_AUDIO_UNKNOWN;
}

bool AudioDevice::set_spec(Channels channels, signed freq) noexcept {
	SDL_AudioSpec spec;
	if (::get_current_spec(m_stream.get(), spec)) {
		spec.format   = SDL_AUDIO_F32;
		spec.channels = channels != SAME ? channels : spec.channels;
		spec.freq     = freq > 0         ? freq     : spec.freq;
	} else {
		m_stream.reset();
		m_channels = m_freq = 0;
		return false;
	}

	if (SDL_SetAudioStreamFormat(m_stream.get(), &spec, &spec)) {
		// reset accumulator if frequency changed
		m_accumulator *= spec.freq == m_freq;
		m_channels = spec.channels;
		m_freq     = spec.freq;
		return true;
	} else {
		blog.warn("Failed to update audio stream spec (the device might "
			"have been lost, consider re-creating it): {}", SDL_GetError());
		m_stream.reset();
		m_channels = m_freq = 0;
		return false;
	}
}

bool AudioDevice::set_freq_ratio(float ratio) noexcept {
	ratio = std::clamp(ratio, 0.01f, 100.0f);
	if (SDL_SetAudioStreamFrequencyRatio(m_stream.get(), ratio)) {
		m_freq_ratio = ratio;
		return true;
	} else {
		blog.warn("Failed to set audio stream frequency ratio: {}", SDL_GetError());
		return false;
	}
}

bool AudioDevice::is_paused() const noexcept {
	// We're gating with the device's ID first, as it allows us to
	// check for an orphaned stream (one whose device was closed)
	// and thus implicitly report "paused" to gate other operations.
	const auto device_id = SDL_GetAudioStreamDevice(m_stream.get());
	return device_id ? SDL_AudioDevicePaused(device_id) : true;
}

bool AudioDevice::is_playback() const noexcept {
	return SDL_IsAudioDevicePlayback(SDL_GetAudioStreamDevice(m_stream.get()));
}

float AudioDevice::get_samples_per_frame(float target_framerate) const noexcept {
	if (target_framerate < 0.1f) { return 0.0f; }
	return m_freq * m_freq_ratio / target_framerate * m_channels;
}

auto AudioDevice::next_frame_sample_count(float target_framerate) noexcept -> std::size_t {
	if (target_framerate < 0.1f) { return 0; }

	if (target_framerate != m_target_framerate) {
		m_target_framerate = target_framerate;
		m_accumulator = 0;
	}

	static constexpr auto c_scale_factor = 1ull << 24;
	::assign_cast_add(m_accumulator, m_freq * m_freq_ratio
		/ target_framerate * c_scale_factor);
	const auto sample_amount = m_accumulator >> 24;
	::assign_cast_and(m_accumulator, c_scale_factor - 1);

	return sample_amount * m_channels;
}

void AudioDevice::pause() noexcept {
	SDL_PauseAudioStreamDevice(m_stream.get());
}

void AudioDevice::resume() noexcept {
	SDL_ResumeAudioStreamDevice(m_stream.get());
}

float AudioDevice::get_gain() const noexcept {
	return SDL_GetAudioStreamGain(m_stream.get());
}

void AudioDevice::set_gain(float new_gain) noexcept {
	SDL_SetAudioStreamGain(m_stream.get(), std::clamp(new_gain, 0.0f, 2.0f));
}

void AudioDevice::add_gain(float add_gain) noexcept {
	set_gain(get_gain() + add_gain);
}

void AudioDevice::push_raw_audio_data(const float* sample_data, std::size_t sample_count) noexcept {
	if (is_paused() || sample_count == 0) { return; }

	SDL_SetAudioDeviceGain(SDL_GetAudioStreamDevice(m_stream.get()),
		GlobalAudioBase::get_final_volume());
	SDL_PutAudioStreamData(m_stream.get(), sample_data,
		signed(sample_count * sizeof(float)));
}
