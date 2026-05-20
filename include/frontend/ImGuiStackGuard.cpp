/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "ImGuiStackGuard.hpp"

#include <imgui.h>

/*==================================================================*/

ImGuiStackGuard::~ImGuiStackGuard () noexcept {
	IM_ASSERT(m_style_colors == 0 && "ImGuiStackGuard : unpopped StyleColors!");
	IM_ASSERT(m_style_vars   == 0 && "ImGuiStackGuard : unpopped StyleVars!");
	IM_ASSERT(m_fonts        == 0 && "ImGuiStackGuard : unpopped Fonts!");
	unwind_all();
}

/*==================================================================*/

void ImGuiStackGuard::push_style_color(ImGuiCol idx, unsigned int col) noexcept {
	ImGui::PushStyleColor(idx, col);
	++m_style_colors;
}

void ImGuiStackGuard::push_style_color(ImGuiCol idx, const ImVec4& col) noexcept {
	ImGui::PushStyleColor(idx, col);
	++m_style_colors;
}

void ImGuiStackGuard::push_style_var(ImGuiStyleVar idx, float val) noexcept {
	ImGui::PushStyleVar(idx, val);
	++m_style_vars;
}

void ImGuiStackGuard::push_style_var(ImGuiStyleVar idx, const ImVec2& val) noexcept {
	ImGui::PushStyleVar(idx, val);
	++m_style_vars;
}

void ImGuiStackGuard::push_font(ImFont* font) noexcept {
	ImGui::PushFont(font);
	++m_fonts;
}

void ImGuiStackGuard::push_font(ImFont* font, float font_size_base_unscaled) noexcept {
	ImGui::PushFont(font, font_size_base_unscaled);
	++m_fonts;
}

/*==================================================================*/

void ImGuiStackGuard::unwind_style_colors(int n) noexcept {
	if (n <= 0 || n > m_style_colors) { n = m_style_colors; }

	if (n > 0) {
		ImGui::PopStyleColor(n);
		m_style_colors -= n;
	}
}

void ImGuiStackGuard::unwind_style_vars(int n) noexcept {
	if (n <= 0 || n > m_style_vars) { n = m_style_vars; }

	if (n > 0) {
		ImGui::PopStyleVar(n);
		m_style_vars -= n;
	}
}

void ImGuiStackGuard::unwind_fonts(int n) noexcept {
	if (n <= 0 || n > m_fonts) { n = m_fonts; }

	if (n > 0) {
		for (int i = 0; i < n; ++i)
			{ ImGui::PopFont(); }
		m_fonts -= n;
	}
}

/*==================================================================*/

void ImGuiStackGuard::unwind_all() noexcept {
	unwind_style_colors();
	unwind_style_vars();
	unwind_fonts();
}
