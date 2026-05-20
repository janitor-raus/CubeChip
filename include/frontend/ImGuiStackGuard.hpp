/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

struct ImVec2;
struct ImVec4;
struct ImFont;

using ImGuiCol = int;
using ImGuiStyleVar = int;

/*==================================================================*/

class ImGuiStackGuard {
	int m_style_colors = 0;
	int m_style_vars = 0;
	int m_fonts = 0;

public:
	ImGuiStackGuard() noexcept = default;
	~ImGuiStackGuard() noexcept;

	ImGuiStackGuard(const ImGuiStackGuard&) = delete;
	ImGuiStackGuard& operator=(const ImGuiStackGuard&) = delete;

/*==================================================================*/

	class Pusher {
		ImGuiStackGuard& m_parent;

	public:
		explicit Pusher(ImGuiStackGuard& parent) noexcept
			: m_parent(parent) {}

		void push_style_color(ImGuiCol idx, unsigned int col) noexcept {
			m_parent.push_style_color(idx, col);
		}
		void push_style_color(ImGuiCol idx, const ImVec4& col) noexcept {
			m_parent.push_style_color(idx, col);
		}

		void push_style_var(ImGuiStyleVar idx, float val) noexcept {
			m_parent.push_style_var(idx, val);
		}
		void push_style_var(ImGuiStyleVar idx, const ImVec2& val) noexcept {
			m_parent.push_style_var(idx, val);
		}

		void push_font(ImFont* font) noexcept {
			m_parent.push_font(font);
		}
		void push_font(ImFont* font, float font_size_base_unscaled) noexcept {
			m_parent.push_font(font, font_size_base_unscaled);
		}
	};

	auto make_pusher() noexcept { return Pusher(*this); }

/*==================================================================*/

	void push_style_color(ImGuiCol idx, unsigned int col) noexcept;
	void push_style_color(ImGuiCol idx, const ImVec4& col) noexcept;

	void push_style_var(ImGuiStyleVar idx, float val) noexcept;
	void push_style_var(ImGuiStyleVar idx, const ImVec2& val) noexcept;

	void push_font(ImFont* font) noexcept;
	void push_font(ImFont* font, float font_size_base_unscaled) noexcept;

/*==================================================================*/

	void unwind_style_colors(int count = 0) noexcept;
	void unwind_style_vars(int count = 0) noexcept;
	void unwind_fonts(int count = 0) noexcept;

	void unwind_all() noexcept;

/*==================================================================*/

	int style_color_count() const noexcept { return m_style_colors; }
	int style_var_count()   const noexcept { return m_style_vars; }
	int font_count()        const noexcept { return m_fonts; }
};
