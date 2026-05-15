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

class AudioDevice {
	using self = AudioDevice;

public:
	class Stream {
		SDL_Unique<SDL_AudioStream> m_ptr;
		signed m_freq{}, m_channels{};
		float m_last_buffer_framerate{};
		float m_last_freq_ratio{};
		unsigned long long m_accumulator{};

	public:
		Stream(SDL_AudioStream* ptr) noexcept
			: m_ptr(ptr) { update_cached_spec(); }

	private:
		void update_cached_spec() noexcept;

	public:
		// Sets the stream's audio spec. May reset the internal accumulator.
		bool set_spec(signed frequency = 0, signed channels = 0) noexcept;

		auto get_freq()      const noexcept { return m_freq; }
		bool set_freq(signed freq) noexcept { return set_spec(freq, m_channels); }

		auto get_freq_ratio() const noexcept { return m_last_freq_ratio; }
		bool set_freq_ratio(float ratio) noexcept;

		auto get_channels()          const noexcept { return m_channels; }
		bool set_channels(signed channels) noexcept { return set_spec(m_freq, channels); }

		bool is_paused()   const noexcept;
		bool is_playback() const noexcept;

		float get_samples_per_frame(float framerate) const noexcept;

		/**
		 * @brief Calculates the number of audio samples needed for the next frame, based on
		 *        the stream's frequency and the given framerate. Internally uses an
		 *        accumulator to handle fractional sample counts across frames, to handle
		 *        cases where the framerate doesn't perfectly divide the frequency.
		 * @note The internal accumulator progress will be reset if the framerate given
		 *       has changed since the last call of this function to avoid drift.
		 * @param framerate :: The current (expected) framerate of a system.
		 * @return The number of audio samples needed for your next frame, multiplied by the
		 *         number of channels. If the given framerate is less than 1.0, returns 0.
		 */
		[[nodiscard]]
		auto next_frame_sample_count(float framerate) noexcept -> std::size_t;

		void pause() noexcept;
		void resume() noexcept;

		float get_gain() const noexcept;
		void  set_gain(float gain) noexcept;
		void  add_gain(float gain) noexcept;

		operator SDL_AudioStream*() const noexcept { return m_ptr.get(); }

		void push_raw_audio_data(void* sample_data, std::size_t buffer_size, std::size_t sample_size) const;

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
	std::unordered_map<signed, Stream>
		m_audio_streams{};

public:
	AudioDevice() noexcept = default;

	AudioDevice(const self&)  = delete;
	self& operator=(const self&) = delete;

private:
	auto insert_audio_stream(signed stream_id, SDL_AudioStream* stream_ptr) noexcept -> Stream*;

public:
	void add_playback_stream(signed stream_id, signed freq = 0, signed channels = 0);
	void add_recording_stream(signed stream_id, signed freq = 0, signed channels = 0);

	auto get_stream_count() const noexcept { return m_audio_streams.size(); }

	void pause_all_streams()  noexcept;
	void resume_all_streams() noexcept;

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
