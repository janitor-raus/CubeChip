/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "personal.hpp"

#include "imgui.h"
#include "imgui_internal.h"

#include <cmath>
#include <algorithm>

/*==================================================================*/

namespace ImGui {
	ImVec2 clamp(const ImVec2& value, const ImVec2& min, const ImVec2& max) noexcept {
		return { std::clamp(value.x, min.x, max.x), std::clamp(value.y, min.y, max.y) };
	}

	ImVec2 floor(const ImVec2& value) noexcept {
		return { std::floor(value.x), std::floor(value.y) };
	}

	ImVec2 ceil(const ImVec2& value) noexcept {
		return { std::ceil(value.x), std::ceil(value.y) };
	}

	ImVec2 abs(const ImVec2& value) noexcept {
		return { std::abs(value.x), std::abs(value.y) };
	}
	ImVec2 min(const ImVec2& value, const ImVec2& min) noexcept {
		return { std::min(value.x, min.x), std::min(value.y, min.y) };
	}
	ImVec2 max(const ImVec2& value, const ImVec2& max) noexcept {
		return { std::max(value.x, max.x), std::max(value.y, max.y) };
	}
}


Vec2::Vec2(const ImVec2& vec2) noexcept
	: x(vec2.x), y(vec2.y)
{}

Vec2::Vec2(ImVec2&& vec2) noexcept
	: x(vec2.x), y(vec2.y)
{}

Vec2::operator ImVec2() const noexcept {
	return ImVec2(x, y);
}

Vec4::Vec4(const ImVec4& vec4) noexcept
	: x(vec4.x), y(vec4.y), z(vec4.z), w(vec4.w)
{}

Vec4::operator ImVec4() const noexcept {
	return ImVec4(x, y, z, w);
}

namespace ImGui {
	void TextUnformatted(const char* text, unsigned color, const char* text_end) {
		PushStyleColor(ImGuiCol_Text, color);
		TextUnformatted(text, text_end);
		PopStyleColor();
	}

	void AddCursorPos(const ImVec2& delta) {
		SetCursorPos(GetCursorPos() + delta);
	}

	void AddCursorPosX(float delta) {
		SetCursorPosX(GetCursorPosX() + delta);
	}

	void AddCursorPosY(float delta) {
		SetCursorPosY(GetCursorPosY() + delta);
	}

	ImVec2 GetWindowDecoSize() {
		const auto& style = GetStyle();

		return ImVec2(style.WindowPadding.x * 2.0f, style.WindowPadding.y * 2.0f
			+ GetFontSize() + style.FramePadding.y * 2.0f);
	}

	void Dummy(float mult_w, float mult_h) {
		const auto mult_vec2 = ImVec2(mult_w, mult_h);
		Dummy(GetStyle().FramePadding * mult_vec2);
	}

	void DummyX(float mult) {
		Dummy(ImVec2(mult * GetStyle().FramePadding.x, 0.0f));
	}

	void DummyY(float mult) {
		Dummy(ImVec2(0.0f, mult * GetStyle().FramePadding.y));
	}

	void Separator(float mult) {
		mult *= 0.5f; // halve and add to both sides
		if (GetCurrentWindow()->DC.LayoutType == ImGuiLayoutType_Horizontal) {
			SameLine(0, mult); Separator(); SameLine(0, mult);
		} else {
			DummyY(mult); Separator(); DummyY(mult);
		}
	}

	void SetNextWindowMinClientSize(const ImVec2& min) {
		SetNextWindowSizeConstraints(min + GetWindowDecoSize(), ImVec2(FLT_MAX, FLT_MAX));
	}

	void DockNextWindowTo(unsigned dock_id, bool first_use) {
		SetNextWindowDockID(dock_id, first_use
			? ImGuiCond_FirstUseEver : ImGuiCond_Always);
	}

	void SelectWindowInDockNodeTab(void* window_ptr) {
		auto window = reinterpret_cast<ImGuiWindow*>
			(window_ptr ? window_ptr : GetCurrentWindow());

		if (!window || !window->DockIsActive) { return; }

		auto* node = window->DockNode;
		if (!node || !node->TabBar) { return; }

		auto* tab_bar = node->TabBar;
		tab_bar->SelectedTabId = window->TabId;
		tab_bar->NextSelectedTabId = window->TabId;
	}

	bool IsDockspaceFocused(unsigned dockspace_id) {
		const auto* context = ImGui::GetCurrentContext();
		if (!context) { return false; }

		const auto* nav_window = context->NavWindow;
		if (!nav_window) { return false; }

		const auto* dock_node = nav_window->DockNode;
		if (!dock_node) { return false; }

		while (dock_node->ParentNode) {
			dock_node = dock_node->ParentNode;
		}

		return dock_node->ID == dockspace_id;
	}

	void WriteText(
		const char* textString, unsigned textColor,
		Vec2 textAlign, Vec2 textPadding
	) {
		using namespace ImGui;
		const auto textPos = GetCursorPos() + textPadding + textAlign * (
			GetContentRegionAvail() - CalcTextSize(textString) - textPadding * 2);

		SetCursorPos(textPos);
		TextUnformatted(textString, textColor);
	}

	void WriteShadowedText(
		const char* textString, unsigned textColor,
		Vec2 textAlign, Vec2 textPadding, Vec2 shadowDist
	) {
		using namespace ImGui;
		const auto textPos = GetCursorPos() + textPadding + textAlign * (
			GetContentRegionAvail() - CalcTextSize(textString) - textPadding * 2);
		const auto shadowOffset = shadowDist * 0.5f;

		SetCursorPos(textPos + shadowOffset);
		TextUnformatted(textString, IM_COL32_BLACK);

		SetCursorPos(textPos - shadowOffset);
		TextUnformatted(textString, textColor);
	}

	void DrawRotatedImage(
		void* texture, const ImVec2& dims, unsigned rotation,
		const ImVec2& uv0, const ImVec2& uv1, unsigned tint
	) {
		if (!texture) { return; }

		const ImVec2 TL = GetCursorScreenPos();
		const ImVec2 TR = { TL.x + dims.x, TL.y          };
		const ImVec2 BR = { TL.x + dims.x, TL.y + dims.y };
		const ImVec2 BL = { TL.x,          TL.y + dims.y };

		static constexpr int rotLUT[4][4] = {
			{ 0, 1, 2, 3 }, //   0 deg : TL TR BR BL
			{ 3, 0, 1, 2 }, //  90 deg
			{ 2, 3, 0, 1 }, // 180 deg
			{ 1, 2, 3, 0 }, // 270 deg
		};

		const ImVec2 uvs[] = {
			{ uv0.x, uv0.y }, // TL
			{ uv1.x, uv0.y }, // TR
			{ uv1.x, uv1.y }, // BR
			{ uv0.x, uv1.y }, // BL
		};
		const auto& m = rotLUT[rotation & 3];

		GetWindowDrawList()->AddImageQuad(
			reinterpret_cast<ImTextureID>(texture), TL, TR, BR, BL,
			uvs[m[0]], uvs[m[1]], uvs[m[2]], uvs[m[3]], tint
		);
	}

	void DrawRect(const ImVec2& dims, float width, float round, unsigned color) {
		const auto origin = GetCursorScreenPos();

		GetWindowDrawList()->AddRect(origin, origin + dims,
			color, round, ImDrawFlags_RoundCornersAll, width);
	}

	void DrawRectFilled(const ImVec2& dims, float round, unsigned color) {
		const auto origin = GetCursorScreenPos();

		GetWindowDrawList()->AddRectFilled(origin, origin + dims,
			color, round, ImDrawFlags_RoundCornersAll);
	}

	void DrawPartialRect(
		const ImVec2& p_min, const ImVec2& p_max,
		unsigned col, float rounding, unsigned flags,
		PartialRectFlags sides, float thickness
	) {
		if ((col & IM_COL32_A_MASK) == 0) { return; }
		if (sides == 0 || thickness < 0.5f) { return; }

		if ((flags & ImDrawFlags_RoundCornersMask_) == 0) {
			flags |= ImDrawFlags_RoundCornersAll;
		}

		ImDrawList* dl = ImGui::GetWindowDrawList();

		const ImVec2 a = p_min + ImVec2(0.50f, 0.50f); // center of pixel for better anti-aliasing
		const ImVec2 b = p_max - ImVec2(0.49f, 0.49f); // almost center, we'd lose a pixel otherwise

		const bool has_adjacent_edges =
			((sides & PartialRectFlags::CORNER_TL) == PartialRectFlags::CORNER_TL) ||
			((sides & PartialRectFlags::CORNER_TR) == PartialRectFlags::CORNER_TR) ||
			((sides & PartialRectFlags::CORNER_BR) == PartialRectFlags::CORNER_BR) ||
			((sides & PartialRectFlags::CORNER_BL) == PartialRectFlags::CORNER_BL);

		if (has_adjacent_edges && rounding >= 0.5f) {
			rounding = ImMin(rounding, ImAbs(b.x - a.x) * (((flags & ImDrawFlags_RoundCornersTop) == ImDrawFlags_RoundCornersTop) ||
				((flags & ImDrawFlags_RoundCornersBottom) == ImDrawFlags_RoundCornersBottom) ? 0.5f : 1.0f) - 1.0f);
			rounding = ImMin(rounding, ImAbs(b.y - a.y) * (((flags & ImDrawFlags_RoundCornersLeft) == ImDrawFlags_RoundCornersLeft) ||
				((flags & ImDrawFlags_RoundCornersRight) == ImDrawFlags_RoundCornersRight) ? 0.5f : 1.0f) - 1.0f);

			const float radius_tl = rounding * !!(flags & ImDrawFlags_RoundCornersTopLeft);
			const float radius_tr = rounding * !!(flags & ImDrawFlags_RoundCornersTopRight);
			const float radius_br = rounding * !!(flags & ImDrawFlags_RoundCornersBottomRight);
			const float radius_bl = rounding * !!(flags & ImDrawFlags_RoundCornersBottomLeft);

			struct CornerData {
				ImVec2 point;  // the actual corner coordinate
				float  radius; // arc radius (0.0f if not rounded)
				ImVec2 center; // center point of the arc
				int    angle;  // arc start angle (in 12 segments, e.g. 3 = 90 degrees)
			};

			const CornerData corners[4] = {
				{ ImVec2(a.x, a.y), radius_tl, ImVec2(a.x + radius_tl, a.y + radius_tl), 6 },
				{ ImVec2(b.x, a.y), radius_tr, ImVec2(b.x - radius_tr, a.y + radius_tr), 9 },
				{ ImVec2(b.x, b.y), radius_br, ImVec2(b.x - radius_br, b.y - radius_br), 0 },
				{ ImVec2(a.x, b.y), radius_bl, ImVec2(a.x + radius_bl, b.y - radius_bl), 3 },
			};

			auto stroke_arc = [&](auto... edge_i) {
				auto emit_corner = [&](const CornerData& c) {
					if (c.radius >= thickness) {
						dl->PathArcToFast(c.center, c.radius, c.angle, c.angle + 3);
					} else {
						dl->PathLineTo(c.point);
					}
				};

				dl->PathClear(); const bool closed = (sizeof...(edge_i) == 4);
				int last = -1; ((emit_corner(corners[edge_i]), last = edge_i), ...);
				if (!closed) { emit_corner(corners[(last + 1) % 4]); }
				dl->PathStroke(col, closed ? ImDrawFlags_Closed : 0, thickness);
			};

			switch (sides) {
				case PartialRectFlags::ALL:         stroke_arc(0, 1, 2, 3); break;
				case PartialRectFlags::PIPE_END_UP: stroke_arc(3, 0, 1); break;
				case PartialRectFlags::PIPE_END_RT: stroke_arc(0, 1, 2); break;
				case PartialRectFlags::PIPE_END_DN: stroke_arc(1, 2, 3); break;
				case PartialRectFlags::PIPE_END_LT: stroke_arc(2, 3, 0); break;
				case PartialRectFlags::CORNER_TL:   stroke_arc(3, 0); break;
				case PartialRectFlags::CORNER_TR:   stroke_arc(0, 1); break;
				case PartialRectFlags::CORNER_BR:   stroke_arc(1, 2); break;
				case PartialRectFlags::CORNER_BL:   stroke_arc(2, 3); break;
				default: IM_ASSERT("We're not meant to be here...");
			}
		}
		else {
			auto stroke = [&](bool closed, auto... points) {
				dl->PathClear(); (dl->PathLineTo(points), ...);
				dl->PathStroke(col, closed ? ImDrawFlags_Closed : 0, thickness);
			};

			const ImVec2 tl = ImVec2(a.x, a.y);
			const ImVec2 tr = ImVec2(b.x, a.y);
			const ImVec2 br = ImVec2(b.x, b.y);
			const ImVec2 bl = ImVec2(a.x, b.y);

			switch (sides) {
				case PartialRectFlags::ALL:         stroke(true,  tl, tr, br, bl); break;
				case PartialRectFlags::PIPE_END_UP: stroke(false, bl, tl, tr, br); break;
				case PartialRectFlags::PIPE_END_RT: stroke(false, tl, tr, br, bl); break;
				case PartialRectFlags::PIPE_END_DN: stroke(false, tr, br, bl, tl); break;
				case PartialRectFlags::PIPE_END_LT: stroke(false, br, bl, tl, tr); break;
				case PartialRectFlags::CORNER_TL:   stroke(false, tr, tl, bl); break;
				case PartialRectFlags::CORNER_TR:   stroke(false, tl, tr, br); break;
				case PartialRectFlags::CORNER_BR:   stroke(false, tr, br, bl); break;
				case PartialRectFlags::CORNER_BL:   stroke(false, br, bl, tl); break;
				case PartialRectFlags::PIPE_V:      stroke(false, tl, bl); stroke(false, tr, br); break;
				case PartialRectFlags::PIPE_H:      stroke(false, tl, tr); stroke(false, bl, br); break;
				case PartialRectFlags::UP:          stroke(false, tl, tr); break;
				case PartialRectFlags::RT:          stroke(false, tr, br); break;
				case PartialRectFlags::DN:          stroke(false, bl, br); break;
				case PartialRectFlags::LT:          stroke(false, tl, bl); break;
				default: IM_ASSERT("We're not meant to be here...");
			}
		}
	}

	bool ButtonContainer(
		const char* id, const ImVec2& size,
		const std::function<void()>& foreground_children,
		const std::function<void()>& background_children,
		bool selected
	) {
		const auto& style = GetStyle();

		const bool pressed = InvisibleButton(id, size);
		const bool hovered = IsItemHovered();
		const bool active  = IsItemActive();
		const auto item_id = GetItemID();

		const auto button_TL = GetItemRectMin();
		const auto button_BR = GetItemRectMax();

		RenderFrame(button_TL, button_BR, GetColorU32(
			active   ? ImGuiCol_ButtonActive  :
			hovered  ? ImGuiCol_ButtonHovered :
			selected ? ImGuiCol_Header        : ImGuiCol_Button
		), style.FrameBorderSize >= 1.0f, style.FrameRounding);

		RenderNavCursor(ImRect(button_TL, button_BR), item_id);

		const auto backup_cursor = GetCursorScreenPos();

		SetCursorScreenPos(button_TL);

		PushClipRect(button_TL, button_BR, true);
		BeginGroup();
		PushItemFlag(ImGuiItemFlags_Disabled, true);
		background_children();
		PopItemFlag();
		EndGroup();
		PopClipRect();

		const auto content_TL = button_TL + style.FramePadding;
		const auto content_BR = button_BR - style.FramePadding;

		SetCursorScreenPos(content_TL);

		PushClipRect(content_TL, content_BR, true);
		BeginGroup();
		foreground_children();
		EndGroup();
		PopClipRect();

		SetCursorScreenPos(backup_cursor);

		// reassure imgui our layout is correct
		// since we restored the cursor position
		ItemSize(ImVec2());

		return pressed;
	}
}
