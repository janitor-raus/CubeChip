/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <atomic>

#include "SettingWrapper.hpp"

/*==================================================================*/

class GlobalAudioBase final {
	static inline std::atomic<float> s_master_volume = 1.0f;
	static inline std::atomic<bool>  s_all_audio_muted = false;

	static inline float s_passive_background_volume = 0.25f;
	static inline float s_active_background_volume = 1.0f;

	static inline bool  s_has_audio_output = false;

public:
	static bool has_audio_output() noexcept { return s_has_audio_output; }

public:
	struct Settings {
		float master_volume     = 0.7f;
		float background_volume = 1.0f;
		bool  all_audio_muted   = false;

		SettingsMap map() noexcept;
	};

	[[nodiscard]]
	auto export_settings() const noexcept -> Settings;

private:
	GlobalAudioBase(const Settings& settings) noexcept;
	~GlobalAudioBase() noexcept;

	GlobalAudioBase(const GlobalAudioBase&) = delete;
	GlobalAudioBase& operator=(const GlobalAudioBase&) = delete;

public:
	static auto* initialize(const Settings& settings) noexcept {
		static GlobalAudioBase self(settings);
		return &self;
	}

	static void toggle_background_volume(bool enable) noexcept {
		s_active_background_volume = enable
			? s_passive_background_volume : 1.0f;
	}

	static void set_background_volume(float volume) noexcept {
		s_passive_background_volume = volume;
	}
	static float get_background_volume() noexcept {
		return s_passive_background_volume;
	}

	static float get_final_volume() noexcept;

	static bool is_muted()           noexcept;
	static void is_muted(bool state) noexcept;
	static void toggle_mute()        noexcept;

	static float get_master_volume()             noexcept;
	static void  set_master_volume(float volume) noexcept;
	static void  add_master_volume(float volume) noexcept;

	static int get_playback_device_count() noexcept;
	static int get_recording_device_count() noexcept;
};
