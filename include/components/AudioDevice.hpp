/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <unordered_map>

#include "Concepts.hpp"
#include "LifetimeWrapperSDL.hpp"


/*==================================================================*/

struct SDL_AudioSpec;

/*==================================================================*/

class AudioDevice {
	using self = AudioDevice;

public:
	class Stream {
		SDL_Unique<SDL_AudioStream> ptr;
		unsigned format{}; unsigned freq{};
		unsigned channels{};
		unsigned long long accumulator{};

	public:
		Stream(
			SDL_AudioStream* ptr,
			unsigned format, unsigned freq, unsigned channels
		) noexcept;

		auto getSpec()     const noexcept -> SDL_AudioSpec;
		auto getFormat()   const noexcept { return format; }
		auto getFreq()     const noexcept { return freq; }
		auto getChannels() const noexcept { return channels; }

		bool isPaused()   const noexcept;
		bool isPlayback() const noexcept;

		float getRawSampleRate(float framerate) const noexcept;

		[[nodiscard]]
		unsigned getNextBufferSize(float framerate) noexcept;

		void pause() noexcept;
		void resume() noexcept;

		float getGain() const noexcept;
		void  setGain(float gain) noexcept;
		void  addGain(float gain) noexcept;

		operator SDL_AudioStream*() const noexcept
			{ return ptr.get(); }

		void pushRawAudio(void* sampleData, std::size_t bufferSize, std::size_t sampleSize) const;

		/**
		 * @brief Pushes buffer of audio samples to SDL device/stream.
		 * @param[in] index :: the device/stream to push audio to.
		 * @param[in] sampleData :: pointer to audio samples buffer.
		 * @param[in] bufferSize :: size of buffer in bytes.
		 */
		template <IsPlainOldData T>
		void pushAudioData(T* sampleData, std::size_t bufferSize) const
			{ pushRawAudio(sampleData, bufferSize, sizeof(T)); }

		/**
		 * @brief Pushes buffer of audio samples to SDL device/stream.
		 * @param[in] index :: the device/stream to push audio to.
		 * @param[in] samplesBuffer :: audio samples buffer (C style).
		 */
		template <IsPlainOldData T, std::size_t N>
		void pushAudioData(T(&samplesBuffer)[N]) const
			{ pushRawAudio(samplesBuffer, N, sizeof(T)); }

		/**
		 * @brief Pushes buffer of audio samples to SDL device/stream.
		 * @param[in] index :: the device/stream to push audio to.
		 * @param[in] samplesBuffer :: audio samples buffer (C++ style).
		 */
		template <IsContiguousContainer T> requires(IsPlainOldData<ValueType<T>>)
		void pushAudioData(T& samplesBuffer) const
			{ pushRawAudio(std::data(samplesBuffer), std::size(samplesBuffer), sizeof(ValueType<T>)); }
	};

private:
	std::unordered_map<unsigned, Stream>
		audioStreams{};

public:
	AudioDevice() noexcept = default;

	AudioDevice(const self&)  = delete;
	self& operator=(const self&) = delete;

	bool addAudioStream(
		unsigned streamID,     unsigned frequency,
		unsigned channels = 1, unsigned device = 0
	);

	unsigned getStreamCount() const noexcept;
	void pauseStreams()  noexcept;
	void resumeStreams() noexcept;

	[[nodiscard]]
	Stream& operator[](signed key) noexcept
		{ return audioStreams.at(key); }

	[[nodiscard]]
	Stream* at(signed key) noexcept {
		auto it = audioStreams.find(key);
		return it != audioStreams.end() ? &it->second : nullptr;
	}
};
