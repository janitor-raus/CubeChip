/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <string>
#include <optional>

#include <imgui.h>

#include "ISystemEmu.hpp"

/*==================================================================*/

void ISystemEmu::prepare_user_interface() noexcept {
	using namespace ImGui;

	m_workspace_host.set_window_visible_output(&m_is_viewport_visible);
	m_workspace_host.set_window_focused_output(&m_is_viewport_focused);

	m_workspace_host.edit_callbacks().window_init =
	[](auto& flags, auto& window_tidy) noexcept {
		flags |= ImGuiWindowFlags_NoCollapse  | ImGuiWindowFlags_NoScrollWithMouse
			  |  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings
			  |  ImGuiWindowFlags_MenuBar;

		DockNextWindowTo(UserInterface::get_main_dockspace_id(), true);

		PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
		window_tidy = []() noexcept { PopStyleVar(1); };
	};

	m_workspace_host.edit_callbacks().window_dock =
	[docker_class = ImGuiWindowClass()](bool window_open, auto window_id) mutable noexcept {
		if (window_open && IsWindowAppearing()) {
			SelectWindowInDockNodeTab();
		}

		docker_class.ClassId = window_id;
		docker_class.DockingAllowUnclassed = false;

		static constexpr auto docker_flags =
			ImGuiDockNodeFlags_AutoHideTabBar;

		DockSpace(window_id, ImVec2(), docker_flags, &docker_class);
	};

	m_frontend_hooks.emplace_back(UserInterface::register_menu(
		m_workspace_host.get_window_label(), { 90, "Debug" },
		[&]() noexcept {
			MenuItem("Memory Editor", nullptr,
				&m_memory_editor.settings.window_visible_out);
		}
	));

	m_frontend_hooks.emplace_back(UserInterface::register_menu(
		m_workspace_host.get_window_label(), { 50, "System" },
		[&]() noexcept {
			Dummy(ImVec2(GetTextLineHeight() * 12, 0.0f));
			if (MenuItem(can_system_work() ? "Pause" : "Resume",
				"F9", false, can_system_pause()))
			{
				xor_system_state(EmuState::PAUSED);
			}
		}
	));

	m_memview_window.set_window_visible_output(&m_memory_editor.settings.window_visible_out);
	m_memview_window.set_window_focused_output(&m_is_viewport_focused);

	m_memview_window.edit_callbacks().window_init = [
		window_id = m_workspace_host.get_window_id(),
		window_class = ImGuiWindowClass()
	](auto& window_flags, auto&) mutable noexcept {
		window_class.ClassId = window_id;
		window_class.DockingAllowUnclassed = false;

		SetNextWindowClass(&window_class);
		DockNextWindowTo(window_class.ClassId, true);

		window_flags |= ImGuiWindowFlags_NoSavedSettings;
	};

	m_memview_window.edit_callbacks().window_body =
	[&](bool window_open, bool) noexcept {
		if (window_open) {
			m_memory_editor.settings.toggle_const_view = can_system_work();
			m_memory_editor.render_memory_editor();
		}
	};
}

/*==================================================================*/

namespace widgets {
	static void simple_memory_viewer(
		const void* memory, std::size_t size,
		std::optional<std::size_t> active = std::nullopt
	) {
		if (!memory || size == 0)
			return;

		const auto* data = static_cast<const char*>(memory);

		constexpr auto bytes_per_row = 8;

		if (ImGui::BeginTable("##hex_view", 2 + bytes_per_row,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
			ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit
		)) {
			ImGui::TableSetupColumn("#offset", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			for (int i = 0; i < bytes_per_row; ++i) {
				ImGui::TableSetupColumn("##col", ImGuiTableColumnFlags_WidthFixed, 30.0f);
			}
			ImGui::TableSetupColumn("#ascii", ImGuiTableColumnFlags_WidthFixed, 80.0f);

			const auto active_index = active.value_or(0);
			const auto row_count = (size + bytes_per_row - 1) / bytes_per_row;

			for (std::size_t row = 0; row < row_count; ++row) {
				ImGui::TableNextRow();

				const auto base_index = row * bytes_per_row;

				// Auto-scroll to active row
				if (active && (active_index / bytes_per_row) == row) {
					ImGui::SetScrollHereY(0.5f);
				}

				// Offset
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%08zX", base_index);

				// Hex bytes
				for (auto col = 0; col < bytes_per_row; ++col) {
					ImGui::TableSetColumnIndex(1 + col);

					const auto final_index = base_index + col;
					if (final_index >= size) { break; }

					if (active && active_index == final_index) {
						ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 100, 100, 255));
						ImGui::Text("%02X", data[final_index]);
						ImGui::PopStyleColor();
					} else {
						ImGui::Text("%02X", data[final_index]);
					}
				}

				// ASCII column
				ImGui::TableSetColumnIndex(1 + bytes_per_row);

				char ascii[bytes_per_row]{};
				for (auto col = 0; col < bytes_per_row; ++col) {
					const auto byte_index = base_index + col;
					if (byte_index >= size) {
						ascii[col] = ' ';
						continue;
					}

					const auto c = char(data[byte_index]);
					ascii[col] = (c >= 32 && c <= 126) ? c : '.';
				}

				ImGui::TextUnformatted(ascii, ascii + bytes_per_row);
			}

			ImGui::EndTable();
		}
	}
}

/*==================================================================*/

namespace osd {
	using namespace ImGui;

	void simple_text_overlay(const std::string& overlay_data) noexcept {
		const auto text_zone = CalcTextSize(overlay_data.c_str())
			+ GetStyle().WindowPadding * 2.0f;

		SetCursorPos(GetCursorPos()
			+ (GetContentRegionAvail() - text_zone) * ImVec2(0.0f, 1.0f));

		if (BeginChild("##text_overlay", text_zone, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav
		)) {
			TextUnformatted(overlay_data.c_str());
		}
		EndChild();
	}

	void key_press_indicator(float phase) noexcept {
		const auto text_height = GetTextLineHeight() * 0.8f;
		const auto size = ImVec2(text_height * 0.8f, text_height);
		const auto widget_zone = size + GetStyle().WindowPadding * 2.0f;

		SetCursorPos(GetCursorPos()
			+ (GetContentRegionAvail() - widget_zone) * ImVec2(0.0f, 0.0f));

		if (BeginChild("##key_indicator", widget_zone, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav
		)) {
			const auto origin = GetCursorScreenPos();
			const auto& style = GetStyle();

			{
				const auto pos1 = origin + ImVec2(0.0f, size.y * 0.75f);
				const auto pos2 = pos1 + ImVec2(size.x, size.y * 0.25f);

				GetWindowDrawList()->AddRectFilled(pos1, pos2,
					GetColorU32(ImGuiCol_Text), 0.0f, ImDrawFlags_None);

				if (style.FrameBorderSize >= 1.0f) {
					GetWindowDrawList()->AddRect(pos1, pos2,
						GetColorU32(ImGuiCol_Border), 0.0f,
						ImDrawFlags_None, style.FrameBorderSize);
				}
			}

			{
				const auto pos1 = origin + ImVec2(0.0f, size.y * 0.20f * phase);
				const auto pos2 = pos1 + ImVec2(size.x, 0.0f);
				const auto pos3 = pos2 + ImVec2(size.x * -0.5f, size.y * 0.45f);

				GetWindowDrawList()->AddTriangleFilled(pos1, pos2, pos3,
					GetColorU32(ImGuiCol_Text));

				if (style.FrameBorderSize >= 1.0f) {
					GetWindowDrawList()->AddTriangle(pos1, pos2, pos3,
						GetColorU32(ImGuiCol_Border), style.FrameBorderSize);
				}
			}

			Dummy(widget_zone);
		}
		EndChild();
	}
}
