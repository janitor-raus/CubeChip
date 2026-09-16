/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <algorithm>

#include "GlobalAudioBase.hpp"
#include "LifetimeWrapperSDL.hpp"

#include "SettingWrapper.hpp"
#include <SDL3/SDL_audio.h>

/*==================================================================*/

struct Settings {
	float master_volume = 0.7f;
	float background_volume = 1.0f;
	bool  all_audio_muted = false;
};

static Settings    s_settings;
static SettingsMap s_settings_map;

/*==================================================================*/

void GlobalAudioBase::export_settings() noexcept {
	s_settings.master_volume     = s_master_volume.load(std::memory_order::relaxed);
	s_settings.all_audio_muted   = s_all_audio_muted.load(std::memory_order::relaxed);
	s_settings.background_volume = s_passive_background_volume;

	s_settings_map.push_into(SettingsMap::main_table);
}

void GlobalAudioBase::import_settings() noexcept {
	s_settings_map
		.add_setting("Audio.Global.Volume",   &s_settings.master_volume)
		.add_setting("Audio.Global.BgVolume", &s_settings.background_volume)
		.add_setting("Audio.Global.Muted",    &s_settings.all_audio_muted);
	s_settings_map.pull_from(SettingsMap::main_table);

	set_master_volume(s_settings.master_volume);
	set_background_volume(s_settings.background_volume);
	is_muted(s_settings.all_audio_muted);
}

/*==================================================================*/

float GlobalAudioBase::get_final_volume() noexcept {
	return is_muted() ? 0.0f : s_active_background_volume
		* s_master_volume.load(std::memory_order::relaxed);
}

bool GlobalAudioBase::is_muted() noexcept {
	return s_all_audio_muted.load(std::memory_order::relaxed);
}

void GlobalAudioBase::is_muted(bool state) noexcept {
	s_all_audio_muted.store(state, std::memory_order::relaxed);
}

void GlobalAudioBase::toggle_mute() noexcept {
	s_all_audio_muted.store(!is_muted(), std::memory_order::relaxed);
}

/*==================================================================*/

float GlobalAudioBase::get_master_volume() noexcept {
	return s_master_volume.load(std::memory_order::relaxed);
}

void GlobalAudioBase::set_master_volume(float volume) noexcept {
	s_master_volume.store(std::clamp(volume, 0.0f, 1.0f), std::memory_order::relaxed);
}

void GlobalAudioBase::add_master_volume(float volume) noexcept {
	set_master_volume(get_master_volume() + volume);
}

int GlobalAudioBase::get_playback_device_count() noexcept {
	auto device_count = 0;
	auto devices = sdl::make_unique(
		SDL_GetAudioPlaybackDevices(&device_count));
	return device_count;
}

int GlobalAudioBase::get_recording_device_count() noexcept {
	auto device_count = 0;
	auto devices = sdl::make_unique(
		SDL_GetAudioRecordingDevices(&device_count));
	return device_count;
}
