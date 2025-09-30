/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <vector>
#include <algorithm>
#include <cassert>

#include "AssignCast.hpp"
#include "GlobalAudioBase.hpp"
#include "AudioDevice.hpp"
#include "LifetimeWrapperSDL.hpp"
#include "BasicLogger.hpp"

#include <SDL3/SDL_audio.h>

/*==================================================================*/

static float calculateGain(float streamGain) noexcept {
	return streamGain * (GlobalAudioBase::isMuted()
		? 0.0f : GlobalAudioBase::getGlobalGain());
}

/*==================================================================*/
	#pragma region AudioDevice Class

bool AudioDevice::addAudioStream(
	unsigned streamID, unsigned frequency,
	unsigned channels, unsigned device
) {
	SDL_AudioSpec spec{ SDL_AUDIO_F32, signed(channels), signed(frequency) };

	auto* ptr{ SDL_OpenAudioDeviceStream(
		device ? device : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK
		, &spec, nullptr, nullptr) };

	if (!ptr) {
		blog.newEntry<BLOG::WARN>("Failed to open audio stream: {}", SDL_GetError());
		return false;
	}

	if (auto slot{ at(streamID) }) {
		*slot = Stream(ptr, spec.format, spec.freq, spec.channels);
		return true;
	} else {
		auto result{ audioStreams.try_emplace(streamID,
			ptr, spec.format, spec.freq, spec.channels) };

		return result.second;
	}
}

/*==================================================================*/

unsigned AudioDevice::getStreamCount() const noexcept
	{ return unsigned(audioStreams.size()); }

void AudioDevice::pauseStreams() noexcept {
	for (auto& stream : audioStreams)
		{ SDL_PauseAudioStreamDevice(stream.second); }
}

void AudioDevice::resumeStreams() noexcept {
	for (auto& stream : audioStreams)
		{ SDL_ResumeAudioStreamDevice(stream.second); }
}

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

AudioDevice::Stream::Stream(
	SDL_AudioStream* ptr,
	unsigned format, unsigned freq, unsigned channels
) noexcept
	: ptr     { ptr      }
	, format  { format   }
	, freq    { freq     }
	, channels{ channels }
{}

auto AudioDevice::Stream::getSpec() const noexcept -> SDL_AudioSpec {
	return { SDL_AudioFormat(format), signed(freq), signed(channels) };
}

bool AudioDevice::Stream::isPaused() const noexcept {
	const auto deviceID{ SDL_GetAudioStreamDevice(ptr) };
	return deviceID ? SDL_AudioDevicePaused(deviceID) : true;
}

bool AudioDevice::Stream::isPlayback() const noexcept {
	return SDL_IsAudioDevicePlayback(SDL_GetAudioStreamDevice(ptr));
}

float AudioDevice::Stream::getRawSampleRate(float framerate) const noexcept {
	if (framerate < 1.0) { return 0.0; }
	return freq / framerate * channels;
}

unsigned AudioDevice::Stream::getNextBufferSize(float framerate) noexcept {
	if (framerate < 1.0f) { return 0u; }

	static constexpr auto scale_factor{ 1ull << 24 };
	::assign_cast_add(accumulator, freq / framerate * scale_factor);
	const auto sample_amount{ accumulator >> 24 };
	::assign_cast_and(accumulator, scale_factor - 1);

	return unsigned(sample_amount * channels);
}

void AudioDevice::Stream::pause() noexcept
	{ SDL_PauseAudioStreamDevice(ptr); }

void AudioDevice::Stream::resume() noexcept
	{ SDL_ResumeAudioStreamDevice(ptr); }

float AudioDevice::Stream::getGain() const noexcept
	{ return SDL_GetAudioStreamGain(ptr); }

void AudioDevice::Stream::setGain(float new_gain) noexcept
	{ SDL_SetAudioStreamGain(ptr, new_gain); }

void AudioDevice::Stream::addGain(float add_gain) noexcept
	{ setGain(getGain() + add_gain); }

void AudioDevice::Stream::pushRawAudio(void* sampleData,
	std::size_t bufferSize, std::size_t sampleSize
) const {
	if (isPaused() || bufferSize == 0u) { return; }

	SDL_SetAudioDeviceGain(SDL_GetAudioStreamDevice(ptr), calculateGain(getGain()));
	SDL_PutAudioStreamData(ptr, sampleData, signed(bufferSize * sampleSize));
}
