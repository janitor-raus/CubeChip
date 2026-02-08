/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "BasicInput.hpp"
#include "HomeDirManager.hpp"

#include "BytePusher_CoreInterface.hpp"

#ifdef ENABLE_BYTEPUSHER_SYSTEM

/*==================================================================*/

BytePusher_CoreInterface::BytePusher_CoreInterface(DisplayDevice display_device) noexcept
	: m_display_device(std::move(display_device))
{
	if (auto* path = HDM->add_user_directory("savestate", "BYTEPUSHER")) {
		s_savestate_path = (*path / HDM->get_loaded_file_sha1()).string();
	}

	m_display_device.set_osd_callable([&]() {
		if (!has_system_state(EmuState::STATS)) { return; }
		osd::simple_text_overlay(copy_statistics_string());
	});
	m_display_device.set_shutdown_signal(&m_is_system_alive);

	load_preset_binds();
}

/*==================================================================*/

void BytePusher_CoreInterface::main_system_loop() {
	instruction_loop();
	push_audio_data();
	create_statistics_data();
	push_video_data();
}

void BytePusher_CoreInterface::load_preset_binds() noexcept {
	static constexpr auto _ = SDL_SCANCODE_UNKNOWN;
	static constexpr SimpleKeyMapping default_key_mappings[]{
		{0x1, KEY(1), _}, {0x2, KEY(2), _}, {0x3, KEY(3), _}, {0xC, KEY(4), _},
		{0x4, KEY(Q), _}, {0x5, KEY(W), _}, {0x6, KEY(E), _}, {0xD, KEY(R), _},
		{0x7, KEY(A), _}, {0x8, KEY(S), _}, {0x9, KEY(D), _}, {0xE, KEY(F), _},
		{0xA, KEY(Z), _}, {0x0, KEY(X), _}, {0xB, KEY(C), _}, {0xF, KEY(V), _},
	};

	load_custom_binds(std::span(default_key_mappings));
}

u32  BytePusher_CoreInterface::get_key_states() noexcept {
	auto key_states = 0u;

	m_input->update_states();

	for (const auto& mapping : m_custom_binds) {
		if (m_input->are_any_held(mapping.key, mapping.alt))
			{ key_states |= 1u << mapping.idx; }
	}

	return key_states;
}

void BytePusher_CoreInterface::copy_game_to_memory(u8* dest) noexcept {
	std::copy_n(EXEC_POLICY(unseq)
		HDM->get_loaded_file_data(), HDM->get_loaded_file_size(), dest);
}

#endif
