/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "BytePusher_CoreInterface.hpp"

#ifdef ENABLE_BYTEPUSHER_SYSTEM

#include "BasicInput.hpp"
#include "SimpleFileIO.hpp"
#include <imgui_internal.h>

/*==================================================================*/

BytePusher_CoreInterface::BytePusher_CoreInterface(std::size_t W, std::size_t H) noexcept
	: SystemInterface(family_pretty_name)
	, m_display_device(W, H, false, { "Display", make_system_id(instance_id, family_name) })
{
	m_window_host.set_layout_callable([&](auto id) noexcept {
		//ImGui::DockBuilderRemoveNode(id);
		//auto new_node = ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_HiddenTabBar);
		//ImGui::DockBuilderDockWindow(m_display_device.get_window_label(), new_node);
		//ImGui::DockBuilderFinish(id);
		ImGui::DockNextWindowTo(id, true);
		blog.warn("Docking system {} to node {}", instance_id, id);
	});

	if (calc_file_image_sha1()) {
		if (auto* path = add_system_path("savestate", family_name)) {
			s_savestate_path = (fs::Path(*path) / m_file_sha1_hash).string();
		} else {
			blog.error("Unable to create savestate directory for system '{}', "
				"savestates will be unavailable!", family_pretty_name);
		}
	}

	m_display_device.set_window_focus_output(&m_is_window_focused);
	m_display_device.set_osd_callable([&]() {
		if (!has_system_state(EmuState::STATS)) { return; }
		osd::simple_text_overlay(copy_statistics_string());
	});

	load_preset_binds();
}

/*==================================================================*/

void BytePusher_CoreInterface::main_system_loop() {
	instruction_loop();
	push_audio_data();
	push_video_data();
	create_statistics_data();
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

#endif
