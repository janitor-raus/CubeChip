/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "IFamily_CHIP8.hpp"

#ifdef ENABLE_CHIP8_SYSTEM

#include "Millis.hpp"
#include <imgui.h>

/*==================================================================*/

void IFamily_CHIP8::prepare_user_interface() noexcept {
	m_display_window.set_window_focused_output(&m_is_viewport_focused);
	m_display_window.set_parent(&m_workspace_host);
	m_display_window.allow_fullscreen(true);

	m_display_window.edit_callbacks().window_init = [
		window_id = m_workspace_host.get_window_id(),
		window_class = ImGuiWindowClass()
	](auto& window_flags, auto&) mutable noexcept {
		window_flags |= ImGuiWindowFlags_NoCollapse  | ImGuiWindowFlags_NoScrollWithMouse
					 |  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

		window_class.ClassId = window_id;
		window_class.DockingAllowUnclassed = false;

		ImGui::SetNextWindowClass(&window_class);
		ImGui::DockNextWindowTo(window_class.ClassId, true);
		ImGui::SetNextWindowMinClientSize(ImVec2(480.0f, 360.0f)
			* UserInterface::get_ui_total_scaling());
	};

	m_display_window.edit_callbacks().window_body =
	[&](bool window_open, bool) noexcept {
		if (window_open) { m_display_device.render_display(); }
	};

	m_display_device.set_osd_callable([&]() noexcept {
		if (m_interrupt == Interrupt::INPUT) {
			osd::key_press_indicator(WaveForms::pulse_t(
				500, u32(Millis::now())).as_unipolar());
		}
		if (!has_system_state(EmuState::STATS)) { return; }
		osd::simple_text_overlay(copy_statistics_string());
	});

	m_frontend_hooks.emplace_back(UserInterface::register_menu(
		m_workspace_host.get_window_label(), { 9999, "Debug" },
		[&]() noexcept { m_display_device.render_settings_menu(); }
	));

	m_memory_editor.set_preview_endianness(MemoryEditor::Endian::BE);
	m_memview_window.edit_callbacks().window_dock =
	[&](bool window_open, auto) noexcept {
		if (window_open && can_system_work()) {
			m_memory_editor.follow_address(m_current_pc, 2);
		}
	};

	m_frontend_hooks.emplace_back(UserInterface::register_menu(
	m_workspace_host.get_window_label(), { 60, "System" }, [&]() noexcept {
		if (ImGui::BeginMenu("Dummy")) {
			ImGui::EndMenu();
		}
	}));
}

#endif
