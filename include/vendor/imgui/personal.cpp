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
		DummyY(mult * 0.5f);
		Separator();
		DummyY(mult * 0.5f);
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
