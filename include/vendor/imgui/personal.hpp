/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include <functional>

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

	void TextUnformatted(const char* text, unsigned color, const char* text_end = nullptr);

	void AddCursorPos(const ImVec2& delta);
	void AddCursorPosX(float delta);
	void AddCursorPosY(float delta);

	ImVec2 GetWindowDecoSize();

	// Insert Dummy of proportions multiplicative of WindowPadding
	void Dummy(float mult_w, float mult_h);
	// Insert Dummy of proportions multiplicative of WindowPadding.x
	void DummyX(float mult);
	// Insert Dummy of proportions multiplicative of WindowPadding.y
	void DummyY(float mult);

	// Insert Separator with vertical padding multiplicative of WindowPadding.y
	void Separator(float mult);

	void SetNextWindowMinClientSize(const ImVec2& min);

	void DockNextWindowTo(unsigned dock_id, bool first_use = false);

	void SelectWindowInDockNodeTab(void* window_ptr = nullptr);
	bool IsDockspaceFocused(unsigned dockspace_id);

	void WriteText(
		const char* textString,
		unsigned textColor   = 0xFFFFFFFF,
		Vec2 textAlign   = Vec2{ 0.5f, 0.5f },
		Vec2 textPadding = Vec2{ 6.0f, 6.0f }
	);

	void WriteShadowedText(
		const char* textString,
		unsigned textColor   = 0xFFFFFFFF,
		Vec2 textAlign   = Vec2{ 0.5f, 0.5f },
		Vec2 textPadding = Vec2{ 6.0f, 6.0f },
		Vec2 shadowDist  = Vec2{ 2.0f, 2.0f }
	);

	void DrawRotatedImage(
		void* texture, const ImVec2& image_dims, unsigned rotation,
		const ImVec2& uv0, // top-left of region
		const ImVec2& uv1, // bottom-right of region
		unsigned tint = 0xFFFFFFFF // modulo texture channels
	);

	void DrawRect(
		const ImVec2& dims, float width,
		float round, unsigned color = 0xFFFFFFFF
	);

	void DrawRectFilled(
		const ImVec2& dims, float round,
		unsigned color = 0xFFFFFFFF
	);

	enum PartialRectFlags {
		NONE = 0,
		UP = 1 << 0,
		RT = 1 << 1,
		DN = 1 << 2,
		LT = 1 << 3,
		CORNER_TL = UP | LT,
		CORNER_TR = UP | RT,
		CORNER_BR = DN | RT,
		CORNER_BL = DN | LT,
		PIPE_V = LT | RT,
		PIPE_H = UP | DN,
		PIPE_END_UP = PIPE_V | UP,
		PIPE_END_RT = PIPE_H | RT,
		PIPE_END_DN = PIPE_V | DN,
		PIPE_END_LT = PIPE_H | LT,
		ALL = UP | RT | DN | LT
	};

	inline PartialRectFlags operator|(PartialRectFlags a, PartialRectFlags b) {
		return static_cast<PartialRectFlags>(unsigned(a) | unsigned(b));
	}

	inline PartialRectFlags& operator|=(PartialRectFlags& a, PartialRectFlags b) {
		return a = a | b;
	}

	void DrawPartialRect(
		const ImVec2& p_min,
		const ImVec2& p_max,
		unsigned col,
		float rounding = 0.0f,
		unsigned flags = 0,
		PartialRectFlags sides = PartialRectFlags::ALL,
		float thickness = 1.0f
	);

	bool ButtonContainer(
		const char* id, const ImVec2& size,
		const std::function<void()>& foreground_children,
		const std::function<void()>& background_children,
		bool selected = false
	);
}
