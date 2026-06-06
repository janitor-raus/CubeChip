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

void AudioDevice::init_stream(signed freq, signed channels, bool recording_device) noexcept {
	if (auto* device_ptr = SDL_OpenAudioDeviceStream(recording_device
		? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
		: SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		nullptr, nullptr, nullptr
	)) {
		const bool new_freq = freq > 0;
		const bool new_channels = channels >= 1 && channels <= 8;

		m_stream = device_ptr;
		set_spec(new_freq ? freq : 0, new_channels ? channels : 0);
	} else {
		blog.error("Failed to open audio stream: {}", SDL_GetError());
	}
}

/*==================================================================*/

void AudioDevice::update_cached_spec() noexcept {
	SDL_AudioSpec actual;

	if (is_playback()) {
		SDL_GetAudioStreamFormat(m_stream, &actual, nullptr);
	} else {
		SDL_GetAudioStreamFormat(m_stream, nullptr, &actual);
	}

	if (actual.format == SDL_AUDIO_UNKNOWN) {
		// If the stream fails to provide a valid spec, all fields are zeroed
		// and this allows us to know implicitly that something went wrong.
		blog.warn("Failed to fetch audio stream spec (the device might "
			"have been lost, consider re-creating it): {}", SDL_GetError());
		m_stream = nullptr; actual.freq = 0; actual.channels = 0;
	}

	m_freq = actual.freq; m_channels = actual.channels;
}

bool AudioDevice::set_spec(signed freq, signed channels) noexcept {
	const bool needs_default_freq = freq <= 0;
	const bool needs_default_channels = channels < 1 || channels > 8;

	signed new_freq = freq, new_channels = channels;

	if (needs_default_freq || needs_default_channels) {
		update_cached_spec();
		if (needs_default_freq)     { new_freq = m_freq; }
		if (needs_default_channels) { new_channels = m_channels; }
	}

	// return early if spec is unchanged, avoid needless calls and accumulator reset
	if (new_freq == m_freq && new_channels == m_channels) { return true; }
	const SDL_AudioSpec spec{ SDL_AUDIO_F32, new_channels, new_freq };

	if (SDL_SetAudioStreamFormat(m_stream, &spec, &spec)) {
		update_cached_spec(); m_accumulator = 0;
		return true;
	} else {
		blog.warn("Failed to update audio stream spec (the device might "
			"have been lost, consider re-creating it): {}", SDL_GetError());
		m_stream = nullptr; m_freq = 0; m_channels = 0;
		return false;
	}
}

bool AudioDevice::set_freq_ratio(float ratio) noexcept {
	ratio = std::clamp(ratio, 0.01f, 100.0f);
	if (SDL_SetAudioStreamFrequencyRatio(m_stream, ratio)) {
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
	const auto device_id = SDL_GetAudioStreamDevice(m_stream);
	return device_id ? SDL_AudioDevicePaused(device_id) : true;
}

bool AudioDevice::is_playback() const noexcept {
	return SDL_IsAudioDevicePlayback(SDL_GetAudioStreamDevice(m_stream));
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
	SDL_PauseAudioStreamDevice(m_stream);
}

void AudioDevice::resume() noexcept {
	SDL_ResumeAudioStreamDevice(m_stream);
}

float AudioDevice::get_gain() const noexcept {
	return SDL_GetAudioStreamGain(m_stream);
}

void AudioDevice::set_gain(float new_gain) noexcept {
	SDL_SetAudioStreamGain(m_stream, std::clamp(new_gain, 0.0f, 2.0f));
}

void AudioDevice::add_gain(float add_gain) noexcept {
	set_gain(get_gain() + add_gain);
}

void AudioDevice::push_raw_audio_data(const float* sample_data, std::size_t sample_count) noexcept {
	if (is_paused() || sample_count == 0) { return; }

	SDL_SetAudioDeviceGain(SDL_GetAudioStreamDevice(m_stream),
		GlobalAudioBase::get_final_volume());
	SDL_PutAudioStreamData(m_stream, sample_data,
		signed(sample_count * sizeof(float)));
}
