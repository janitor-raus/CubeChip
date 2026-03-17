/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <string>

#include <imgui.h>

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
		const auto text_height = GetTextLineHeight();
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