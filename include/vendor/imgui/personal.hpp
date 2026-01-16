/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

/*==================================================================*/

struct ImVec2;
struct ImVec4;

struct Vec2 {
	float x{}, y{};
	constexpr Vec2() noexcept = default;
	constexpr Vec2(float x, float y) noexcept
		: x(x), y(y) {}

	explicit Vec2(const ImVec2& vec2) noexcept;
	explicit Vec2(ImVec2&& vec2) noexcept;
	operator ImVec2() const noexcept;
};
struct Vec4 {
	float x{}, y{}, z{}, w{};
	constexpr Vec4() noexcept = default;
	constexpr Vec4(float x, float y, float z, float w) noexcept
		: x(x), y(y), z(z), w(w) {}

	explicit Vec4(const ImVec4& vec4) noexcept;
	operator ImVec4() const noexcept;
};

namespace ImGui {
	ImVec2 clamp(const ImVec2& value, const ImVec2& min, const ImVec2& max) noexcept;
	ImVec2 floor(const ImVec2& value) noexcept;
	ImVec2 ceil (const ImVec2& value) noexcept;
	ImVec2 abs  (const ImVec2& value) noexcept;
	ImVec2 min  (const ImVec2& value, const ImVec2& min) noexcept;
	ImVec2 max  (const ImVec2& value, const ImVec2& max) noexcept;

	void TextUnformatted(const char* text, unsigned color, const char* text_end = 0) noexcept;

	void AddCursorPos(const ImVec2& delta) noexcept;
	void AddCursorPosX(float delta) noexcept;
	void AddCursorPosY(float delta) noexcept;

	ImVec2 GetWindowDecoSize() noexcept;

	void SetNextWindowMinClientSize(const ImVec2& min) noexcept;

	void DockNextWindowTo(unsigned id, bool first_use = false) noexcept;

	void writeText(
		const char* textString,
		unsigned textColor   = 0xFFFFFFFF,
		Vec2 textAlign   = Vec2{ 0.5f, 0.5f },
		Vec2 textPadding = Vec2{ 6.0f, 6.0f }
	) noexcept;

	void writeShadowedText(
		const char* textString,
		unsigned textColor   = 0xFFFFFFFF,
		Vec2 textAlign   = Vec2{ 0.5f, 0.5f },
		Vec2 textPadding = Vec2{ 6.0f, 6.0f },
		Vec2 shadowDist  = Vec2{ 2.0f, 2.0f }
	) noexcept;

	void DrawRotatedImage(
		void* texture, const ImVec2& image_dims, unsigned rotation,
		const ImVec2& uv0, // top-left of region
		const ImVec2& uv1, // bottom-right of region
		unsigned tint = 0xFFFFFFFF // modulo texture channels
	) noexcept;
	void DrawRect(
		const ImVec2& dims, float width,
		float round, unsigned color = 0xFFFFFFFF
	) noexcept;
	void DrawRectFilled(
		const ImVec2& dims, float round,
		unsigned color = 0xFFFFFFFF
	) noexcept;
}
