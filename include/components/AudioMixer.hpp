/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "AudioDevice.hpp"
#include "ShortKey.hpp"
#include "Voice.hpp"
#include "Mailbox.hpp"

#include <algorithm>
#include <string>
#include <atomic>
#include <vector>
#include <array>
#include <span>
#include <bit>

/*==================================================================*/

using SampleBuffer = std::span<float>;

template <typename T>
concept IsSampleGenerator = std::is_nothrow_invocable_r_v
	<void, T, SampleBuffer, Voice&>;

enum WaveType {
	None,    // no waveform, absence of a voice track
	Unknown, // General-purpose sampled waveform
	Sine,
	Triangle,
	Pulse,
	Sawtooth,
};

template <typename Fn> requires (IsSampleGenerator<Fn>)
struct VoiceTrack {
	using Decay_Fn = std::decay_t<Fn>;

	const ShortKey descriptor;
	const WaveType wave_type;
	const unsigned track_id;
	const Decay_Fn generate;

	VoiceTrack() = delete;
	VoiceTrack(
		unsigned track_id, Fn&& fn,
		WaveType wave_type = WaveType::Unknown,
		ShortKey descriptor = {}
	) noexcept
		: descriptor(std::move(descriptor))
		, track_id(track_id)
		, wave_type(wave_type)
		, generate(std::forward<Fn>(fn))
	{}
};

struct Timing {
	float base_framerate  = 0.0f;
	float rate_multiplier = 0.0f;

	constexpr operator bool() const noexcept {
		return base_framerate > 0.0f && rate_multiplier > 0.0f;
	}
};

template <typename T>  inline constexpr bool is_voice_track_v = false;
template <typename Fn> inline constexpr bool is_voice_track_v<VoiceTrack<Fn>> = true;

/*==================================================================*/

template <std::size_t Voices = 1>
class AudioMixer {
	static_assert(Voices <= 64, "AudioMixer supports a maximum of 64 voices");
	static_assert(Voices >=  1, "AudioMixer expects a minimum of 1 voice");
	using VoicesGroup = std::array<Voice, Voices>;
	using SamplesVec  = std::vector<float>;

	static constexpr auto c_untouched = -0.0f;
	static constexpr bool is_untouched(float value) noexcept {
		return std::bit_cast<u32>(value)
			== std::bit_cast<u32>(c_untouched);
	}

	class Snapshot {
		friend class AudioMixer;
		 // One per track, plus the final mix result
		using VoiceWaveTypes   = std::array<WaveType, Voices>;
		using VoiceDescriptors = std::array<ShortKey, Voices>;
		static constexpr auto c_rows = Voices + 1;

		SamplesVec  m_track_samples;
		VoicesGroup m_cached_voices;

		VoiceWaveTypes   m_cached_wave_types;
		VoiceDescriptors m_cached_descriptors;

		static auto row_span(auto& samples, std::size_t row) noexcept {
			const auto len = samples.size() / c_rows;
			return std::span(samples.data() + row * len, len);
		}

		auto& reset(std::size_t length, const VoicesGroup& voices) {
			m_cached_voices = voices;
			m_track_samples.assign(length * c_rows, c_untouched);
			m_cached_wave_types.fill(WaveType::None);
			return *this;
		}

		auto get_descriptor(std::size_t i) noexcept {
			assert(i < Voices);
			return m_cached_descriptors[i];
		}

		auto get_track(std::size_t i) /***/ noexcept {
			assert(i < Voices);
			return row_span(m_track_samples, i);
		}

		auto get_mixed() /***/ noexcept {
			return row_span(m_track_samples, Voices);
		}

	public:
		const auto& get_voice(std::size_t i) const noexcept {
			assert(i < Voices);
			return m_cached_voices[i];
		}

		const auto& get_wave_type(std::size_t i) const noexcept {
			assert(i < Voices);
			return m_cached_wave_types[i];
		}

		auto get_track(std::size_t i) const noexcept {
			assert(i < Voices);
			return row_span(m_track_samples, i);
		}

		auto get_mixed() const noexcept {
			return row_span(m_track_samples, Voices);
		}
	};

	Mailbox<Snapshot> m_track_data;
	SamplesVec        m_scratch;
	std::atomic<bool> m_capture = false;

public:
	AudioDevice device;
	VoicesGroup voices;

	const auto& track_data() const noexcept { return m_track_data; }
	void capture_tracks(bool state) noexcept {
		m_capture.store(state, std::memory_order::relaxed);
	}

private:
	void generate_track(SampleBuffer dst, const auto& track) noexcept {
		track.generate(dst, voices[track.track_id]);
	};

	void mix_track(SampleBuffer dst, SampleBuffer src) noexcept {
		for (std::size_t i = 0; i < src.size(); ++i) { dst[i] += src[i]; }
	}

public:
	template <typename... Track>
		requires (is_voice_track_v<std::remove_cvref_t<Track>> && ...)
	void mix_tracks(bool silent, Timing t, Track&&... tracks) noexcept {
		if (!device || !t || device.is_paused()) { return; }
		device.set_freq_ratio(t.rate_multiplier);

		m_scratch.assign(device.next_frame_sample_count(
			t.base_framerate * t.rate_multiplier), 0.0f);

		if (silent) { goto push_audio; }

		if (m_capture.load(std::memory_order::relaxed) == true) {
			m_track_data.acquire([&](Snapshot& snapshot) noexcept {
				auto scratch_span = std::span(m_scratch);
				snapshot.reset(m_scratch.size(), voices);

				(generate_track(snapshot.get_track(tracks.track_id), tracks), ...);

				for (std::size_t i = 0; i < voices.size(); ++i) {
					auto track = snapshot.get_track(i);
					if (track.empty() || is_untouched(track[0])) { continue; }
					mix_track(scratch_span, std::move(track));
				}

				((snapshot.m_cached_wave_types[tracks.track_id] = tracks.wave_type), ...);

				for (auto& sample : m_scratch) { sample = ez::fast_tanh(sample); }
				// Copy-add the scratch buffer to the (empty) mixed track span
				mix_track(snapshot.get_mixed(), scratch_span);
			});
		} else {
			(generate_track(std::span(m_scratch), tracks), ...);
			for (auto& sample : m_scratch) { sample = ez::fast_tanh(sample); }
		}

		push_audio:
		device.push_audio_data(m_scratch);
	}
};
