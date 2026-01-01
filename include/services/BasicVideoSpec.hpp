/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "LifetimeWrapperSDL.hpp"
#include "SettingWrapper.hpp"
#include "EzMaths.hpp"

/*==================================================================*/
	#pragma region BasicVideoSpec Singleton Class

class BasicVideoSpec final {

	SDL_Unique<SDL_Window>   m_main_window{};
	SDL_Unique<SDL_Renderer> m_main_renderer{};

/*==================================================================*/

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
		static bool sInitSuccess = true;
		static BasicVideoSpec self(settings, sInitSuccess);
		return sInitSuccess ? &self : nullptr;
	}

/*==================================================================*/

public:
	float get_display_refresh_rate(SDL_DisplayID display) noexcept;
	void  normalize_rect_to_display(ez::Rect& rect, ez::Rect& deco, bool first_run) noexcept;

/*==================================================================*/

	SDL_Window*   get_main_window()   const noexcept { return m_main_window; }
	SDL_Renderer* get_main_renderer() const noexcept { return m_main_renderer; }

/*==================================================================*/

public:
	static SDL_Texture* create_stream_texture(SDL_Renderer* renderer, int w, int h) noexcept;
	static void write_stream_texture(SDL_Renderer* renderer,
		SDL_Texture* texture, const std::byte* src_buffer) noexcept;

public:
	void set_window_title(const std::string& title, SDL_Window* window = nullptr) noexcept;
	bool is_main_window_id(unsigned id) const noexcept;
	void raise_window(SDL_Window* window = nullptr) noexcept;

	bool render_present() noexcept;
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
