/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <algorithm>

#include "GlobalAudioBase.hpp"
#include "LifetimeWrapperSDL.hpp"

#include <SDL3/SDL_init.h>

/*==================================================================*/

SettingsMap GlobalAudioBase::Settings::map() noexcept {
	return {
		::make_setting_link("Audio.Global.Volume", &master_volume),
		::make_setting_link("Audio.Global.BgVolume", &background_volume),
		::make_setting_link("Audio.Global.Muted",  &all_audio_muted),
	};
}

auto GlobalAudioBase::export_settings() const noexcept -> Settings {
	Settings out;

	out.master_volume = s_master_volume.load(std::memory_order::relaxed);
	out.all_audio_muted = s_all_audio_muted.load(std::memory_order::relaxed);
	out.background_volume = s_passive_background_volume;

	return out;
}

/*==================================================================*/

GlobalAudioBase::GlobalAudioBase(const Settings& settings) noexcept {
	s_has_audio_output = SDL_InitSubSystem(SDL_INIT_AUDIO);

	set_master_volume(settings.master_volume);
	set_background_volume(settings.background_volume);
	is_muted(settings.all_audio_muted);
}

GlobalAudioBase::~GlobalAudioBase() noexcept {
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

/*==================================================================*/

float GlobalAudioBase::get_final_volume() noexcept {
	return is_muted() ? 0.0f : s_active_background_volume
		* s_master_volume.load(std::memory_order::relaxed);
}

bool GlobalAudioBase::is_muted()           noexcept {
	return s_all_audio_muted.load(std::memory_order::relaxed);
}

void GlobalAudioBase::is_muted(bool state) noexcept {
	s_all_audio_muted.store(state, std::memory_order::relaxed);
}

void GlobalAudioBase::toggle_mute()        noexcept {
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
	if (has_audio_output()) {
		SDL_Unique<SDL_AudioDeviceID> devices =
			SDL_GetAudioPlaybackDevices(&device_count);
	}
	return device_count;
}

int GlobalAudioBase::get_recording_device_count() noexcept {
	auto device_count = 0;
	if (has_audio_output()) {
		SDL_Unique<SDL_AudioDeviceID> devices =
			SDL_GetAudioRecordingDevices(&device_count);
	}
	return device_count;
}
