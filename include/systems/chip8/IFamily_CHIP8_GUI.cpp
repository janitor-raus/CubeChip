/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "IFamily_CHIP8.hpp"

#ifdef ENABLE_CHIP8_SYSTEM

#include "Millis.hpp"
#include <imgui.h>
#include <imgui_internal.h>

/*==================================================================*/

void IFamily_CHIP8::prepare_user_interface() noexcept {
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
		if (m_interrupt == Interrupt::INPUT) {
			osd::key_press_indicator(WaveForms::pulse_t(
				500, u32(Millis::now())).as_unipolar());
		}
		if (!has_cached_system_state(EmuState::STATS)) { return; }
		osd::simple_text_overlay(copy_statistics_string());
	});

	m_frontend_hooks.emplace_back(UserInterface::register_menu(m_workspace_host,
	{ 9999, "Debug" }, [&]() noexcept {
		m_display_device.render_settings_menu();
	}));

	m_memory_editor.set_preview_endianness(MemoryEditor::Endian::BE);
	m_memview_window.edit_callbacks().window_dock =
	[&](bool window_open, auto) noexcept {
		if (window_open && can_system_work()) {
			m_memory_editor.follow_address(m_current_pc, 2);
		}
	};

	m_frontend_hooks.emplace_back(UserInterface::register_menu(m_workspace_host,
	{ 60, "System" }, [&]() noexcept {
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

			if (MenuItem("Enable Delimiter", "F10", is_benching)) {
				xor_system_state(EmuState::BENCH);
			}

			BeginDisabled(!is_benching);
			SetNextItemWidth(widget_width);
			DragUint("##debugger_cpf", &m_debugger_cpf, 1.0f, 0, 100'000'000,
				"Step: %u cycles", ImGuiSliderFlags_AlwaysClamp);
			EndDisabled();

			EndDisabled();
			EndMenu();
		}
	}));

	m_frontend_hooks.emplace_back(UserInterface::register_menu(m_workspace_host,
	{ 50, "System" }, [&]() noexcept {
		if (BeginMenu("Quirks", get_avail_quirks())) {
			const auto& padding = GetStyle().WindowPadding;
			const auto button_width = CalcTextSize("_").x * 38.0f;
			const auto min_height = GetFrameHeightWithSpacing() + padding.y * 2.0f;

			BeginDisabled(has_system_state(EmuState::ANY_STOP));
			for (auto i = 0; i < QuirkFlag::TOTAL_QUIRKS; ++i) {
				const auto flag = QuirkFlag(1 << i);
				if (!(get_avail_quirks() & flag)) { continue; }

				const auto button_height = min_height + CalcTextSize(get_quirk_desc(flag),
					nullptr, false, button_width - padding.x * 2.0f).y;
				const auto button_size = ImVec2(button_width, button_height);

				PushID(i);
				if (ButtonContainer("##btn", button_size, [&]() noexcept {
					AddCursorPos(padding);
					BeginInertChild("##inner", button_size - padding * 2.0f);

					int quirk_flags = m_quirk_flags;
					CheckboxFlags(get_quirk_name(flag), &quirk_flags, flag);
					BeginDisabled();
					TextWrapped("%s", get_quirk_desc(flag));
					EndDisabled();
					EndChild();
				}, has_quirk(flag))) { xor_quirk(flag); }
				PopID();
			}
			EndDisabled();

			EndMenu();
		}
	}));
}

#endif
