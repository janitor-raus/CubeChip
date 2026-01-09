/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "personal.hpp"

#include "imgui.h"

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
	void TextUnformatted(const char* text, unsigned color, const char* text_end) noexcept {
		PushStyleColor(ImGuiCol_Text, color);
		TextUnformatted(text, text_end);
		PopStyleColor();
	}

	void AddCursorPos(const ImVec2& delta) noexcept {
		ImGui::SetCursorPos(ImGui::GetCursorPos() + delta);
	}

	void AddCursorPosX(float delta) noexcept {
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + delta);
	}

	void AddCursorPosY(float delta) noexcept {
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + delta);
	}

	void writeText(
		const char* textString, unsigned textColor,
		Vec2 textAlign, Vec2 textPadding
	) noexcept {
		using namespace ImGui;
		const auto textPos = GetCursorPos() + textPadding + textAlign * (
			GetContentRegionAvail() - CalcTextSize(textString) - textPadding * 2);

		SetCursorPos(textPos);
		TextUnformatted(textString, textColor);
	}

	void writeShadowedText(
		const char* textString, unsigned textColor,
		Vec2 textAlign, Vec2 textPadding, Vec2 shadowDist
	) noexcept {
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
	) noexcept {
		if (!texture) { return; }

		const ImVec2 TL = ImGui::GetCursorScreenPos();
		const ImVec2 TR = { TL.x + dims.x, TL.y          };
		const ImVec2 BR = { TL.x + dims.x, TL.y + dims.y };
		const ImVec2 BL = { TL.x,          TL.y + dims.y };

		static constexpr int rotLUT[4][4] = {
			{ 0, 1, 3, 2 }, //   0 deg : TL TR BR BL
			{ 3, 0, 1, 2 }, //  90 deg
			{ 2, 3, 0, 1 }, // 180 deg
			{ 1, 2, 3, 0 }, // 270 deg
		};

		const ImVec2 uvs[] = {
			{ uv0.x, uv0.y }, // TL
			{ uv1.x, uv0.y }, // TR
			{ uv0.x, uv1.y }, // BL
			{ uv1.x, uv1.y }, // BR
		};
		const auto& m = rotLUT[rotation & 3];

		ImGui::GetWindowDrawList()->AddImageQuad(
			reinterpret_cast<ImTextureID>(texture), TL, TR, BR, BL,
			uvs[m[0]], uvs[m[1]], uvs[m[2]], uvs[m[3]], tint
		);
	}

	void DrawRect(const ImVec2& dims, float width, float round, unsigned color) noexcept {
		const auto origin = ImGui::GetCursorScreenPos();

		ImGui::GetWindowDrawList()->AddRect(origin, origin + dims,
			color, round, ImDrawFlags_RoundCornersAll, width);
	}

	void DrawRectFilled(const ImVec2& dims, float round, unsigned color) noexcept {
		const auto origin = ImGui::GetCursorScreenPos();

		ImGui::GetWindowDrawList()->AddRectFilled(origin, origin + dims,
			color, round, ImDrawFlags_RoundCornersAll);
	}
}
