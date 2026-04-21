/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "SettingWrapper.hpp"
#include "EzMaths.hpp"

/*==================================================================*/

struct SDL_Window;
struct SDL_Renderer;

class BasicVideoSpec final {

public:
	struct Settings {
		static constexpr ez::Rect
			defaults = { 0, 0, 640, 480 };

		ez::Rect window = defaults;
		bool first_run = true;

		SettingsMap map() noexcept;
	};

	[[nodiscard]]
	auto export_settings() const noexcept -> Settings;

private:
	BasicVideoSpec(const Settings& settings, bool& success) noexcept;
	~BasicVideoSpec() noexcept;
	BasicVideoSpec(const BasicVideoSpec&) = delete;
	BasicVideoSpec& operator=(const BasicVideoSpec&) = delete;

/*==================================================================*/

public:
	static auto* initialize(const Settings& settings) noexcept {
		static bool s_init_success = true;
		static BasicVideoSpec self(settings, s_init_success);
		return s_init_success ? &self : nullptr;
	}

/*==================================================================*/

private:
	void normalize_rect_to_display(ez::Rect& rect, ez::Rect& deco, bool first_run) noexcept;

/*==================================================================*/

public:
	SDL_Window*   get_main_window()   const noexcept;
	SDL_Renderer* get_main_renderer() const noexcept;

/*==================================================================*/

public:
	static SDL_Texture* create_stream_texture(SDL_Renderer* renderer,
		int w, int h, bool linear_scaling = false) noexcept;

	static SDL_Texture* create_target_texture(SDL_Renderer* renderer,
		int w, int h, bool linear_scaling = false) noexcept;

	// Writes pixel data from src_buffer into a given Stream texture.
	static void write_stream_texture(SDL_Renderer* renderer,
		SDL_Texture* texture, const std::byte* src_buffer) noexcept;

	// Renders Stream src_texture onto a given Target dst_texture.
	static void write_stream_texture(SDL_Renderer* renderer,
		SDL_Texture* dst_texture, SDL_Texture* src_texture) noexcept;

public:
	bool  set_window_title(const std::string& title, SDL_Window* window = nullptr) noexcept;
	bool  is_main_window_id(unsigned id) const noexcept;
	bool  raise_window(SDL_Window* window = nullptr) noexcept;

	template <typename Fn> requires(std::is_invocable_r_v<void, Fn>)
	bool render_present(Fn&& render_callable) noexcept(
		std::is_nothrow_invocable_r_v<void, Fn>
	) {
		set_automatic_render_scale_for_renderer();
		std::forward<Fn>(render_callable)();
		return try_render_present();
	}

private:
	void set_automatic_render_scale_for_renderer() noexcept;
	bool try_render_present() noexcept;
};
