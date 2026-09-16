/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <atomic>

/*==================================================================*/

class GlobalAudioBase final {
	inline static std::atomic<float> s_master_volume   = 1.0f;
	inline static std::atomic<bool>  s_all_audio_muted = false;

	inline static float s_passive_background_volume = 0.25f;
	inline static float s_active_background_volume  = 1.00f;

public:
	static void export_settings() noexcept;
	static void import_settings() noexcept;

private:
	GlobalAudioBase() noexcept = default;

	GlobalAudioBase(const GlobalAudioBase&) = delete;
	GlobalAudioBase& operator=(const GlobalAudioBase&) = delete;

public:
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
