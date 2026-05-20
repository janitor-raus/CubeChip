/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "IFamily_BYTEPUSHER.hpp"

#ifdef ENABLE_BYTEPUSHER_SYSTEM

#include "BasicVideoSpec.hpp"
#include <imgui.h>

/*==================================================================*/

void IFamily_BYTEPUSHER::prepare_user_interface() noexcept {
	using namespace ImGui;

	m_display_window.set_window_focused_output(&m_is_viewport_focused);
	m_display_window.set_parent(&m_workspace_host);
	m_display_window.allow_fullscreen(true);

	m_display_device.set_borderless_view_input(
		&UserInterface::get_borderless_view_mode_hook());

	m_display_window.edit_callbacks().window_init = [
		window_id = m_workspace_host.get_window_id(),
		window_class = ImGuiWindowClass()
	](auto& window_flags, auto& pusher, bool fullscreen) mutable noexcept {
		if (!fullscreen) {
			window_flags |= ImGuiWindowFlags_NoCollapse  | ImGuiWindowFlags_NoScrollWithMouse
						 |  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

			window_class.ClassId = window_id;
			window_class.DockingAllowUnclassed = false;

			SetNextWindowClass(&window_class);
			DockNextWindowTo(window_class.ClassId, true);
			SetNextWindowMinClientSize(ImVec2(480.0f, 360.0f)
				* UserInterface::get_ui_total_scaling());
		}

		const bool borderless = UserInterface::get_borderless_view_mode();

		if (fullscreen) { pusher.push_style_color(ImGuiCol_WindowBg, IM_COL32_BLACK); }
		if (borderless) { pusher.push_style_var(ImGuiStyleVar_WindowPadding, ImVec2()); }
	};

	m_display_window.edit_callbacks().window_body =
	[&](bool window_open, bool) noexcept {
		if (window_open) { m_display_device.render_display(); }
	};

	m_display_device.set_osd_callable([&]() noexcept {
		if (!has_cached_system_state(EmuState::STATS)) { return; }
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
			m_memory_editor.follow_address(get_program_counter(), 3);
		}
	};

	m_frontend_hooks.emplace_back(UserInterface::register_menu(
	m_workspace_host.get_window_label(), { 60, "System" }, [&]() noexcept {
		if (BeginMenu("Emulation")) {
			const auto widget_width = CalcTextSize("F").x * 28.0f;
			const bool is_benching = has_cached_system_state(EmuState::BENCH);

			BeginDisabled(has_system_state(EmuState::ANY_STOP));

			SeparatorText("Framerate");

			SetNextItemWidth(widget_width);
			DragFloat("##framerate_multiplier", &*m_framerate_multiplier, 0.001f,
				m_framerate_multiplier.min, m_framerate_multiplier.max,
				"Multiplier: %5.2fx", ImGuiSliderFlags_AlwaysClamp);

			SeparatorText("CPU Control");

			if (MenuItem("Enable Delimiter", "F10", is_benching, false)) {
				xor_system_state(EmuState::BENCH);
			}

			EndDisabled();
			EndMenu();
		}
	}));
}

#endif
