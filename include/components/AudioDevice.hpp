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
		SDL_Unique<SDL_AudioStream> m_ptr;
		unsigned m_format{}; unsigned m_freq{};
		unsigned m_channels{};
		unsigned long long m_accumulator{};

	public:
		Stream(
			SDL_AudioStream* ptr,
			unsigned format, unsigned freq, unsigned channels
		) noexcept;

		auto get_spec()     const noexcept -> SDL_AudioSpec;
		auto get_format()   const noexcept { return m_format; }
		auto get_freq()     const noexcept { return m_freq; }
		auto get_channels() const noexcept { return m_channels; }

		bool is_paused()   const noexcept;
		bool is_playback() const noexcept;

		float get_raw_sample_rate(float framerate) const noexcept;

		[[nodiscard]]
		unsigned get_next_buffer_size(float framerate) noexcept;

		void pause() noexcept;
		void resume() noexcept;

		float get_gain() const noexcept;
		void  set_gain(float gain) noexcept;
		void  add_gain(float gain) noexcept;

		operator SDL_AudioStream*() const noexcept { return m_ptr.get(); }

		void push_raw_audio_data(void* sampleData, std::size_t bufferSize, std::size_t sampleSize) const;

		/**
		 * @brief Pushes buffer of audio samples to SDL device/stream.
		 * @param[in] index :: the device/stream to push audio to.
		 * @param[in] sampleData :: pointer to audio samples buffer.
		 * @param[in] bufferSize :: size of buffer in bytes.
		 */
		template <IsPlainOldData T>
		void push_audio_data(T* sampleData, std::size_t bufferSize) const {
			push_raw_audio_data(sampleData, bufferSize, sizeof(T));
		}

		/**
		 * @brief Pushes buffer of audio samples to SDL device/stream.
		 * @param[in] index :: the device/stream to push audio to.
		 * @param[in] samplesBuffer :: audio samples buffer (C style).
		 */
		template <IsPlainOldData T, std::size_t N>
		void push_audio_data(T(&samplesBuffer)[N]) const {
			push_raw_audio_data(samplesBuffer, N, sizeof(T));
		}

		/**
		 * @brief Pushes buffer of audio samples to SDL device/stream.
		 * @param[in] index :: the device/stream to push audio to.
		 * @param[in] samplesBuffer :: audio samples buffer (C++ style).
		 */
		template <IsContiguousContainer T> requires(IsPlainOldData<ValueType<T>>)
		void push_audio_data(T& samplesBuffer) const {
			push_raw_audio_data(std::data(samplesBuffer), std::size(samplesBuffer), sizeof(ValueType<T>));
		}
	};

private:
	std::unordered_map<unsigned, Stream>
		m_audio_streams{};

public:
	AudioDevice() noexcept = default;

	AudioDevice(const self&)  = delete;
	self& operator=(const self&) = delete;

	bool add_audio_stream(
		unsigned streamID,     unsigned frequency,
		unsigned channels = 1, unsigned device = 0
	);

	unsigned get_stream_count() const noexcept;
	void pause_streams()  noexcept;
	void resume_streams() noexcept;

	[[nodiscard]]
	Stream& operator[](signed key) noexcept {
		return m_audio_streams.at(key);
	}

	[[nodiscard]]
	Stream* at(signed key) noexcept {
		auto it = m_audio_streams.find(key);
		return it != m_audio_streams.end() ? &it->second : nullptr;
	}
};
