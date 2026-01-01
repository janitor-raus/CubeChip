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
	static inline std::atomic<float> m_global_gain{};
	static inline std::atomic<bool>  ms_is_muted{};

	static inline bool m_has_audio_output{};

public:
	static bool has_audio_output() noexcept { return m_has_audio_output; }

public:
	struct Settings {
		float volume = 0.75f;
		bool  muted  = false;

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

	static bool is_muted()           noexcept;
	static void is_muted(bool state) noexcept;
	static void toggle_mute()        noexcept;

	static float get_global_gain()           noexcept;
	static void  set_glogal_gain(float gain) noexcept;
	static void  add_global_gain(float gain) noexcept;

	static int get_playback_device_count() noexcept;
	static int get_recording_device_count() noexcept;
};
