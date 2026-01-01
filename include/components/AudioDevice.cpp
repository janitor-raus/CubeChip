/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <algorithm>

#include "AssignCast.hpp"
#include "GlobalAudioBase.hpp"
#include "AudioDevice.hpp"
#include "LifetimeWrapperSDL.hpp"
#include "BasicLogger.hpp"

#include <SDL3/SDL_audio.h>

/*==================================================================*/

static float calculate_gain(float stream_gain) noexcept {
	return stream_gain * (GlobalAudioBase::is_muted()
		? 0.0f : GlobalAudioBase::get_global_gain());
}

/*==================================================================*/
	#pragma region AudioDevice Class

bool AudioDevice::add_audio_stream(
	unsigned streamID, unsigned frequency,
	unsigned channels, unsigned device)
{
	SDL_AudioSpec spec{ SDL_AUDIO_F32, signed(channels), signed(frequency) };

	auto* ptr = SDL_OpenAudioDeviceStream(
		device ? device : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);

	if (!ptr) {
		blog.newEntry<BLOG::WRN>("Failed to open audio stream: {}", SDL_GetError());
		return false;
	}

	if (auto slot = at(streamID)) {
		*slot = Stream(ptr, spec.format, spec.freq, spec.channels);
		return true;
	} else {
		return (m_audio_streams.try_emplace(streamID, ptr,
			spec.format, spec.freq, spec.channels)).second;
	}
}

/*==================================================================*/

unsigned AudioDevice::get_stream_count() const noexcept {
	return unsigned(m_audio_streams.size());
}

void AudioDevice::pause_streams() noexcept {
	for (auto& stream : m_audio_streams) {
		SDL_PauseAudioStreamDevice(stream.second);
	}
}

void AudioDevice::resume_streams() noexcept {
	for (auto& stream : m_audio_streams) {
		SDL_ResumeAudioStreamDevice(stream.second);
	}
}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

AudioDevice::Stream::Stream(
	SDL_AudioStream* ptr,
	unsigned format, unsigned freq, unsigned channels
) noexcept
	: m_ptr     (ptr     )
	, m_format  (format  )
	, m_freq    (freq    )
	, m_channels(channels)
{}

auto AudioDevice::Stream::get_spec() const noexcept -> SDL_AudioSpec {
	return { SDL_AudioFormat(m_format), signed(m_freq), signed(m_channels) };
}

bool AudioDevice::Stream::is_paused() const noexcept {
	const auto deviceID = SDL_GetAudioStreamDevice(m_ptr);
	return deviceID ? SDL_AudioDevicePaused(deviceID) : true;
}

bool AudioDevice::Stream::is_playback() const noexcept {
	return SDL_IsAudioDevicePlayback(SDL_GetAudioStreamDevice(m_ptr));
}

float AudioDevice::Stream::get_raw_sample_rate(float framerate) const noexcept {
	if (framerate < 1.0) { return 0.0; }
	return m_freq / framerate * m_channels;
}

unsigned AudioDevice::Stream::get_next_buffer_size(float framerate) noexcept {
	if (framerate < 1.0f) { return 0u; }

	static constexpr auto scale_factor = 1ull << 24;
	::assign_cast_add(m_accumulator, m_freq / framerate * scale_factor);
	const auto sample_amount = m_accumulator >> 24;
	::assign_cast_and(m_accumulator, scale_factor - 1);

	return unsigned(sample_amount * m_channels);
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
	std::size_t buffer_size, std::size_t sample_size) const
{
	if (is_paused() || buffer_size == 0u) { return; }

	SDL_SetAudioDeviceGain(SDL_GetAudioStreamDevice(m_ptr), ::calculate_gain(get_gain()));
	SDL_PutAudioStreamData(m_ptr, sample_data, signed(buffer_size * sample_size));
}
