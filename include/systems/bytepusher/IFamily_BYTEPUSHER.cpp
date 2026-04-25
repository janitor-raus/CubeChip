/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "IFamily_BYTEPUSHER.hpp"

#ifdef ENABLE_BYTEPUSHER_SYSTEM

#include "BasicLogger.hpp"
#include "BasicInput.hpp"
#include "SimpleFileIO.hpp"

/*==================================================================*/

IFamily_BYTEPUSHER::IFamily_BYTEPUSHER(std::size_t W, std::size_t H) noexcept
	: ISystemEmu(family_pretty_name)
	, m_display_window({ "Display", make_system_id(instance_id, "display") })
	, m_display_device(W, H, UserInterface::get_current_renderer())
{
	prepare_user_interface();
	load_preset_binds();

	if (calc_file_image_sha1()) {
		if (auto* path = add_system_path("savestate", family_name)) {
			s_savestate_path = (fs::Path(*path) / m_file_sha1_hash).string();
		}
		else {
			blog.error("Unable to create savestate directory for system '{}', "
				"savestates will be unavailable!", family_pretty_name);
		}
	}
}

/*==================================================================*/

void IFamily_BYTEPUSHER::main_system_loop() {
	if (has_system_state(EmuState::IS_PAUSED)) {
		push_audio_data();
		return;
	}

	instruction_loop();
	push_audio_data();
	push_video_data();
	create_statistics_data();
}

void IFamily_BYTEPUSHER::load_preset_binds() noexcept {
	static constexpr auto _ = SDL_SCANCODE_UNKNOWN;
	static constexpr SimpleKeyMapping default_key_mappings[]{
		{0x1, KEY(1), _}, {0x2, KEY(2), _}, {0x3, KEY(3), _}, {0xC, KEY(4), _},
		{0x4, KEY(Q), _}, {0x5, KEY(W), _}, {0x6, KEY(E), _}, {0xD, KEY(R), _},
		{0x7, KEY(A), _}, {0x8, KEY(S), _}, {0x9, KEY(D), _}, {0xE, KEY(F), _},
		{0xA, KEY(Z), _}, {0x0, KEY(X), _}, {0xB, KEY(C), _}, {0xF, KEY(V), _},
	};

	load_custom_binds(std::span(default_key_mappings));
}

u32  IFamily_BYTEPUSHER::get_key_states() noexcept {
	auto key_states = 0u;

	m_input.advance_state();

	for (const auto& mapping : m_custom_binds) {
		if (m_input.is_held(mapping.key) || m_input.is_held(mapping.alt)) {
			key_states |= 1 << mapping.idx;
		}
	}

	return key_states;
}

#endif
