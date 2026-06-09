/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include <algorithm>

/*==================================================================*/

struct ImVec2;
struct ImVec4;
struct ImFont;

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

/*==================================================================*/

namespace ImGui {
	ImVec2 clamp(const ImVec2& value, const ImVec2& min, const ImVec2& max) noexcept;
	ImVec2 floor(const ImVec2& value) noexcept;
	ImVec2 ceil(const ImVec2& value) noexcept;
	ImVec2 abs(const ImVec2& value) noexcept;
	ImVec2 min(const ImVec2& value, const ImVec2& min) noexcept;
	ImVec2 max(const ImVec2& value, const ImVec2& max) noexcept;

	bool DragUint(const char* label, unsigned* v, float v_speed, unsigned v_min, unsigned v_max, const char* format, int flags);
	bool DragUint2(const char* label, unsigned v[2], float v_speed, unsigned v_min, unsigned v_max, const char* format, int flags);
	bool DragUint3(const char* label, unsigned v[3], float v_speed, unsigned v_min, unsigned v_max, const char* format, int flags);
	bool DragUint4(const char* label, unsigned v[4], float v_speed, unsigned v_min, unsigned v_max, const char* format, int flags);

	void TextUnformatted(const char* text, unsigned color, const char* text_end = nullptr);

	void AddCursorPos(const ImVec2& delta);
	void AddCursorPosX(float delta);
	void AddCursorPosY(float delta);

	ImVec2 GetWindowDecoSize();

	// Dummy helper with float args instead of ImVec2
	void Dummy(float width, float height);
	// Dummy helper for horizontal span only
	void DummyX(float width);
	// Dummy helper for vertical span only
	void DummyY(float height);

	void SetNextWindowMinClientSize(const ImVec2& min);

	void DockNextWindowTo(unsigned dock_id, bool first_use = false);

	void SelectWindowInDockNodeTab(void* window_ptr = nullptr);
	bool IsDockspaceFocused(unsigned dockspace_id);

	void WriteText(
		const char* textString,
		unsigned textColor = 0xFFFFFFFF,
		Vec2 textAlign = Vec2{ 0.5f, 0.5f },
		Vec2 textPadding = Vec2{ 6.0f, 6.0f }
	);

	void WriteShadowedText(
		const char* textString,
		unsigned textColor = 0xFFFFFFFF,
		Vec2 textAlign = Vec2{ 0.5f, 0.5f },
		Vec2 textPadding = Vec2{ 6.0f, 6.0f },
		Vec2 shadowDist = Vec2{ 2.0f, 2.0f }
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

	// Overload to provide explicit rounding flags
	void RenderFrame(
		const ImVec2& p_min, const ImVec2& p_max,
		unsigned fill_col, bool borders,
		float rounding, int draw_flags
	);

	namespace detail {
		struct ButtonContainerState {
			bool pressed = false;
			float tl_x = 0.0f, tl_y = 0.0f;
		};

		void ButtonContainerBegin(
			const char* id, const ImVec2& size,
			ButtonContainerState& state, bool selected,
			bool interactive, int draw_flags
		);
		void ButtonContainerEnd(const ButtonContainerState& state, const ImVec2& size);
	}

	template <std::invocable Fn>
	bool ButtonContainer(
		const char* id, const ImVec2& size,
		Fn&& body_callable, bool selected = false,
		bool interactive = true, int draw_flags = 0
	) {
		auto state = detail::ButtonContainerState();
		detail::ButtonContainerBegin(id, size, state, selected, interactive, draw_flags);
		body_callable();
		detail::ButtonContainerEnd(state, size);
		return state.pressed;
	}

	/**
	 * @brief BeginChild() with pre-applied flags for no inputs and no background,
	 *        primarily meant for inert grouping of items (e.g. in a button)
	 */
	bool BeginInertChild(const char* id, const ImVec2& size);

	float CalcFontHeight(float size, ImFont* font = nullptr);

	float GetFrameWidth(const char* text);
	float GetFrameWidthWithSpacing(const char* text);
}

/*==================================================================*/
//  ScrollingText() and ScrollingTextState are used to implement a
//  marquee-like effect for text that exceeds a given width.
//
//  The text will scroll horizontally until the end of the text is
//  reached, then it will wait for a specified duration before
//  scrolling back to the start. The ScrollingTextState struct
//  manages the state of the scrolling effect, including the current
//  offset and timing for the waiting phase.
/*==================================================================*/

namespace ImGui {
	class ScrollingTextState {
		enum class Phase : char { WAIT_LT, SCROLL, WAIT_RT };

		float speed   = 32.0f;
		float wait_lt = 1.0f;
		float wait_rt = 2.0f;

		float offset = 0.0f;
		float timer  = 0.0f;
		Phase phase  = Phase::WAIT_LT;

	public:
		bool enabled = true;

		void set_speed(float v) noexcept   { speed = std::clamp(v, 1.0f, 1000.0f); }
		float get_speed() const noexcept   { return speed; }

		void set_wait_lt(float v) noexcept { wait_lt = std::max(0.0f, v); }
		float get_wait_lt() const noexcept { return wait_lt; }

		void set_wait_rt(float v) noexcept { wait_rt = std::max(0.0f, v); }
		float get_wait_rt() const noexcept { return wait_rt; }

		float get_offset() const noexcept  { return offset; }

		void reset() noexcept {
			offset = timer = 0.0f;
			phase  = Phase::WAIT_LT;
		}

		void update(float text_w, float width) noexcept;
	};

	void ScrollingText(const char* text, float width, ScrollingTextState* state);
}
