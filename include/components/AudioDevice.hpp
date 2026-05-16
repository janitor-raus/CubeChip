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
	using StreamID = signed int;

public:
	class Stream {
		SDL_Unique<SDL_AudioStream> m_ptr;
		signed m_freq{}, m_channels{};
		float m_last_target_framerate{};
		float m_last_freq_ratio = 1.0f;
		unsigned long long m_accumulator{};

	public:
		Stream(SDL_AudioStream* ptr) noexcept
			: m_ptr(ptr) { update_cached_spec(); }

	private:
		void update_cached_spec() noexcept;

	public:
		// Sets the stream's audio spec. Zero/invalid values fall back to the physical
		// device's default respectively. If a change in spec was applied, the internal
		// accumulator for next_frame_sample_count() will be reset to prevent drift.
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

		void push_raw_audio_data(void* sample_data, std::size_t buffer_size, std::size_t sample_size) noexcept;

		/**
		 * @brief Pushes buffer of audio samples to SDL device/stream.
		 * @param[in] index :: the device/stream to push audio to.
		 * @param[in] sample_data :: pointer to audio samples buffer.
		 * @param[in] buffer_size :: size of buffer in bytes.
		 */
		template <IsPlainOldData T>
		void push_audio_data(T* sample_data, std::size_t buffer_size) noexcept {
			push_raw_audio_data(sample_data, buffer_size, sizeof(T));
		}

		/**
		 * @brief Pushes buffer of audio samples to SDL device/stream.
		 * @param[in] index :: the device/stream to push audio to.
		 * @param[in] samples_buffer :: audio samples buffer (C style).
		 */
		template <IsPlainOldData T, std::size_t N>
		void push_audio_data(T(&samples_buffer)[N]) noexcept {
			push_raw_audio_data(samples_buffer, N, sizeof(T));
		}

		/**
		 * @brief Pushes buffer of audio samples to SDL device/stream.
		 * @param[in] index :: the device/stream to push audio to.
		 * @param[in] samples_buffer :: audio samples buffer (C++ style).
		 */
		template <IsContiguousContainer T> requires(IsPlainOldData<ValueType<T>>)
		void push_audio_data(T& samples_buffer) noexcept {
			push_raw_audio_data(std::data(samples_buffer), std::size(samples_buffer), sizeof(ValueType<T>));
		}
	};

private:
	std::unordered_map<StreamID, Stream>
		m_audio_streams{};

public:
	AudioDevice() noexcept = default;

	AudioDevice(const AudioDevice&) = delete;
	AudioDevice& operator=(const AudioDevice&) = delete;

private:
	auto insert_audio_stream(StreamID stream_id, SDL_AudioStream* stream_ptr) noexcept -> Stream*;

public:
	void add_playback_stream(StreamID stream_id, signed freq = 0, signed channels = 0) noexcept;
	void add_recording_stream(StreamID stream_id, signed freq = 0, signed channels = 0) noexcept;

	auto get_stream_count() const noexcept { return m_audio_streams.size(); }

	void pause_all_streams()  noexcept;
	void resume_all_streams() noexcept;

	[[nodiscard]]
	Stream& operator[](StreamID key) {
		return m_audio_streams.at(key);
	}

	[[nodiscard]]
	Stream* at(StreamID key) noexcept {
		auto it = m_audio_streams.find(key);
		return it != m_audio_streams.end() ? &it->second : nullptr;
	}
};
