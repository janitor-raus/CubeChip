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

auto AudioDevice::insert_audio_stream(StreamID stream_id, SDL_AudioStream* device_ptr) noexcept -> Stream* {
	if (auto slot = at(stream_id)) {
		blog.warn("Audio stream with ID '{}' already exists!", stream_id);
		*slot = Stream(device_ptr);
		return slot;
	} else {
		auto [it, inserted] = m_audio_streams.try_emplace(stream_id, device_ptr);
		if (inserted) {
			return &it->second;
		} else {
			blog.error("Failed to insert audio stream with ID '{}'!", stream_id);
			(void) sdl::make_unique<SDL_AudioStream>(device_ptr); // auto cleanup!
			return nullptr;
		}
	}
}

void AudioDevice::add_playback_stream(StreamID stream_id, signed freq, signed channels) noexcept {
	if (auto* device_ptr = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		nullptr, nullptr, nullptr
	)) {
		const bool new_freq = freq > 0;
		const bool new_channels = channels >= 1 && channels <= 8;

		if (auto stream_ptr = insert_audio_stream(stream_id, device_ptr)) {
			stream_ptr->set_spec(new_freq ? freq : 0, new_channels ? channels : 0);
		}
	} else {
		blog.error("Failed to open audio playback stream: {}", SDL_GetError());
	}
}

void AudioDevice::add_recording_stream(StreamID stream_id, signed freq, signed channels) noexcept {
	if (auto* device_ptr = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_RECORDING,
		nullptr, nullptr, nullptr
	)) {
		const bool new_freq = freq > 0;
		const bool new_channels = channels >= 1 && channels <= 8;

		if (auto stream_ptr = insert_audio_stream(stream_id, device_ptr)) {
			stream_ptr->set_spec(new_freq ? freq : 0, new_channels ? channels : 0);
		}
	} else {
		blog.error("Failed to open audio recording stream: {}", SDL_GetError());
	}
}

/*==================================================================*/

void AudioDevice::pause_all_streams() noexcept {
	for (auto& stream : m_audio_streams) {
		SDL_PauseAudioStreamDevice(stream.second);
	}
}

void AudioDevice::resume_all_streams() noexcept {
	for (auto& stream : m_audio_streams) {
		SDL_ResumeAudioStreamDevice(stream.second);
	}
}

/*==================================================================*/

void AudioDevice::Stream::update_cached_spec() noexcept {
	SDL_AudioSpec actual;

	if (is_playback()) {
		SDL_GetAudioStreamFormat(m_ptr, &actual, nullptr);
	} else {
		SDL_GetAudioStreamFormat(m_ptr, nullptr, &actual);
	}

	m_freq = actual.freq; m_channels = actual.channels;
}

bool AudioDevice::Stream::set_spec(signed freq, signed channels) noexcept {
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

	if (SDL_SetAudioStreamFormat(m_ptr, &spec, &spec)) {
		update_cached_spec(); m_accumulator = 0;
		return true;
	} else {
		blog.warn("Failed to update audio stream spec: {}", SDL_GetError());
		return false;
	}
}

bool AudioDevice::Stream::set_freq_ratio(float ratio) noexcept {
	ratio = std::clamp(ratio, 0.01f, 100.0f);
	if (SDL_SetAudioStreamFrequencyRatio(m_ptr, ratio)) {
		m_last_freq_ratio = ratio;
		return true;
	} else {
		blog.warn("Failed to set audio stream frequency ratio: {}", SDL_GetError());
		return false;
	}
}

bool AudioDevice::Stream::is_paused() const noexcept {
	// We're gating with the device's ID first, as it allows us to
	// check for an orphaned stream (one whose device was closed)
	// and thus implicitly report "paused" to gate other operations.
	const auto device_id = SDL_GetAudioStreamDevice(m_ptr);
	return device_id ? SDL_AudioDevicePaused(device_id) : true;
}

bool AudioDevice::Stream::is_playback() const noexcept {
	return SDL_IsAudioDevicePlayback(SDL_GetAudioStreamDevice(m_ptr));
}

float AudioDevice::Stream::get_samples_per_frame(float target_framerate) const noexcept {
	if (target_framerate < 0.1f) { return 0.0f; }
	return m_freq * m_last_freq_ratio / target_framerate * m_channels;
}

auto AudioDevice::Stream::next_frame_sample_count(float target_framerate) noexcept -> std::size_t {
	if (target_framerate < 0.1f) { return 0; }

	if (target_framerate != m_last_target_framerate) {
		m_last_target_framerate = target_framerate;
		m_accumulator = 0;
	}

	static constexpr auto c_scale_factor = 1ull << 24;
	::assign_cast_add(m_accumulator, m_freq * m_last_freq_ratio
		/ target_framerate * c_scale_factor);
	const auto sample_amount = m_accumulator >> 24;
	::assign_cast_and(m_accumulator, c_scale_factor - 1);

	return sample_amount * m_channels;
}

void AudioDevice::Stream::pause() noexcept {
	SDL_PauseAudioStreamDevice(m_ptr);
}

void AudioDevice::Stream::resume() noexcept {
	SDL_ResumeAudioStreamDevice(m_ptr);
}

float AudioDevice::Stream::get_gain() const noexcept {
	return SDL_GetAudioStreamGain(m_ptr);
}

void AudioDevice::Stream::set_gain(float new_gain) noexcept {
	SDL_SetAudioStreamGain(m_ptr, std::clamp(new_gain, 0.0f, 2.0f));
}

void AudioDevice::Stream::add_gain(float add_gain) noexcept {
	set_gain(get_gain() + add_gain);
}

void AudioDevice::Stream::push_raw_audio_data(void* sample_data,
	std::size_t buffer_size, std::size_t sample_size
) noexcept {
	if (is_paused() || buffer_size == 0) { return; }

	SDL_SetAudioDeviceGain(SDL_GetAudioStreamDevice(m_ptr),
		GlobalAudioBase::get_final_volume());
	SDL_PutAudioStreamData(m_ptr, sample_data, signed(buffer_size * sample_size));
}
