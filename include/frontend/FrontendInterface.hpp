/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <unordered_map>
#include <mutex>
#include <vector>
#include <utility>
#include <string>
#include <functional>

#include "ColorOps.hpp"

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

	void writeText(
		const std::string& textString,
		RGBA textColor   = 0xFFFFFFFF,
		Vec2 textAlign   = Vec2{ 0.5f, 0.5f },
		Vec2 textPadding = Vec2{ 6.0f, 6.0f }
	) noexcept;

	void writeShadowedText(
		const std::string& textString,
		RGBA textColor   = 0xFFFFFFFF,
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

/*==================================================================*/

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

template <typename Fn>
concept VoidInvocable = std::is_invocable_r_v<void, Fn>;

/*==================================================================*/

class FrontendInterface {
public:
	using Func = std::function<void()>;
	using Hook = std::shared_ptr<Func>;
	using Key  = std::string;

private:
	struct HookRegistry {
		using HookBuffer = std::vector<std::weak_ptr<Func>>;

		HookBuffer buffer{};
		unsigned   offset{}; // used when merging, don't touch

		bool has_focus{}; // indicates focus status, don't touch
		bool first_hit{}; // set for a single frame only on focus

		// make Clang happy I guess.
		HookRegistry() = default;
	};

	using HookRegistryMapped = std::unordered_map
		<Key, std::unordered_map<Key, HookRegistry>>;

	template <typename T>
	struct RegistryBox {
		T registry, overflow;

		std::mutex registry_lock;
		std::mutex overflow_lock;
	};

	struct RegistryAggregate {
		RegistryBox<HookRegistry>     windows{};
		RegistryBox<HookRegistryMapped> menus{};
	};

	static inline std::unique_ptr
		<RegistryAggregate> s_hooks = nullptr;

public:
	/**
	 * Registers a function to be called during the window rendering phase.
	 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the function is unregistered automatically.
	 * Nesting is allowed, but unless lifetime persists, the hooks will be erased
	 * during the next loop of invocation during the same render frame.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static Hook register_window(Fn&& fn) noexcept {
		auto shared_ptr = std::make_shared<Func>(std::forward<Fn>(fn));

		if (s_hooks->windows.registry_lock.try_lock()) { // may fail spuriously (fine)
			s_hooks->windows.registry.buffer.push_back(shared_ptr);
			s_hooks->windows.registry_lock.unlock();
		} else {
			std::unique_lock lock(s_hooks->windows.overflow_lock); // must wait to acquire
			s_hooks->windows.overflow.buffer.push_back(shared_ptr);
		}

		return shared_ptr;
	}

	/**
	 * Registers a function to be called during the main menu rendering phase.
	 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the function is unregistered automatically.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static Hook register_menu(const Key& window_tag, const Key& menu_title, Fn&& fn) noexcept {
		auto shared_ptr = std::make_shared<Func>(std::forward<Fn>(fn));

		if (s_hooks->menus.registry_lock.try_lock()) { // may fail spuriously (fine)
			s_hooks->menus.registry[window_tag][menu_title].buffer.push_back(shared_ptr);
			s_hooks->menus.registry_lock.unlock();
		} else {
			std::unique_lock lock(s_hooks->menus.overflow_lock); // must wait to acquire
			s_hooks->menus.overflow[window_tag][menu_title].buffer.push_back(shared_ptr);
		}

		return shared_ptr;
	}

private:
	static inline HookRegistry* s_active_menu{};
public:
	static bool was_menu_clicked() noexcept {
		return s_active_menu && s_active_menu->first_hit;
	}

private:
	static bool merge_overflowing_windows() noexcept;
	static void invoke_registered_windows() noexcept;

	static bool merge_overflowing_menus(const Key& tag) noexcept;
	static void invoke_registered_menus(const Key& tag) noexcept;

public:
	static void init_context(const char*);
	static void quit_context();

	static void init_video(SDL_Window*, SDL_Renderer*);
	static void quit_video();

private:
	static inline SDL_Renderer* s_current_renderer{};
public:
	static auto* get_current_renderer() noexcept { return s_current_renderer; }

public:
	static void  set_ui_scale_factor(float scale) noexcept;
	static float get_ui_scale_factor() noexcept;

public:
	static void process_event(void* event);
	static void begin_new_frame();
	static void render_frame(SDL_Renderer*);

	static void dock_next_window_to(unsigned id, bool first_time) noexcept;
};
