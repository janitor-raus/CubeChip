/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "BytePusher_CoreInterface.hpp"

#ifdef ENABLE_BYTEPUSHER_SYSTEM

#include "BasicLogger.hpp"
#include "BasicInput.hpp"
#include "SimpleFileIO.hpp"
#include "FrontendInterface.hpp"
#include <imgui.h>

/*==================================================================*/

BytePusher_CoreInterface::BytePusher_CoreInterface(std::size_t W, std::size_t H) noexcept
	: SystemInterface(family_pretty_name)
	, m_display_window({ "Display", make_system_id(instance_id, "display") }, true)
	, m_display_device(W, H, m_display_window.get_window_label())
{
	m_display_window.set_window_focused_output(&m_is_window_focused);
	m_display_window.edit_callbacks([&](auto& callbacks) noexcept {
		callbacks.window_init = [
			window_id = m_window_host.get_window_id(),
			window_class = ImGuiWindowClass()
		](auto& flags, auto&) mutable noexcept {
			window_class.ClassId = window_id;
			window_class.DockingAllowUnclassed = false;

			ImGui::SetNextWindowClass(&window_class);
			ImGui::DockNextWindowTo(window_class.ClassId, true);
			ImGui::SetNextWindowMinClientSize(ImVec2(480.0f, 360.0f)
				* FrontendInterface::get_ui_total_scaling());

			flags |= ImGuiWindowFlags_NoCollapse  | ImGuiWindowFlags_NoScrollWithMouse
				  |  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings
				  |  ImGuiWindowFlags_MenuBar;
		};

		callbacks.window_body = [&](bool window_open, bool) noexcept {
			if (window_open) { m_display_device.render_display(); }
		};
	});

	m_display_device.set_osd_callable([&]() noexcept {
		if (!has_system_state(EmuState::STATS)) { return; }
		osd::simple_text_overlay(copy_statistics_string());
	});

	if (calc_file_image_sha1()) {
		if (auto* path = add_system_path("savestate", family_name)) {
			s_savestate_path = (fs::Path(*path) / m_file_sha1_hash).string();
		} else {
			blog.error("Unable to create savestate directory for system '{}', "
				"savestates will be unavailable!", family_pretty_name);
		}
	}

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

	m_input.advance_state();

	for (const auto& mapping : m_custom_binds) {
		if (m_input.is_held(mapping.key) || m_input.is_held(mapping.alt)) {
			key_states |= 1 << mapping.idx;
		}
	}

	return key_states;
}

#endif
