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
		::make_setting_link("Audio.Global.Volume", &volume),
		::make_setting_link("Audio.Global.Muted",  &muted),
	};
}

auto GlobalAudioBase::export_settings() const noexcept -> Settings {
	Settings out;

	out.volume = s_global_gain.load(std::memory_order::relaxed);
	out.muted = s_is_muted.load(std::memory_order::relaxed);

	return out;
}

/*==================================================================*/

GlobalAudioBase::GlobalAudioBase(const Settings& settings) noexcept {
	s_has_audio_output = SDL_InitSubSystem(SDL_INIT_AUDIO);

	set_glogal_gain(settings.volume);
	is_muted(settings.muted);
}

GlobalAudioBase::~GlobalAudioBase() noexcept {
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

/*==================================================================*/

bool GlobalAudioBase::is_muted()           noexcept {
	return s_is_muted.load(std::memory_order::relaxed);
}

void GlobalAudioBase::is_muted(bool state) noexcept {
	s_is_muted.store(state, std::memory_order::relaxed);
}

void GlobalAudioBase::toggle_mute()       noexcept {
	s_is_muted.store(is_muted(), std::memory_order::relaxed);
}

/*==================================================================*/

float GlobalAudioBase::get_global_gain() noexcept {
	return s_global_gain.load(std::memory_order::relaxed);
}

void GlobalAudioBase::set_glogal_gain(float gain) noexcept {
	s_global_gain.store(std::clamp(gain, 0.0f, 1.0f), std::memory_order::relaxed);
}

void GlobalAudioBase::add_global_gain(float gain) noexcept {
	set_glogal_gain(get_global_gain() + gain);
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
