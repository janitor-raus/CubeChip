/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "LifetimeWrapperSDL.hpp"
#include "BasicVideoSpec.hpp"
#include "BasicLogger.hpp"

#include <vector>
#include <exception>

#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>

#if defined(_WIN32) && defined(WINDOWS_NO_ROUNDED_CORNERS)
	#include <sdkddkver.h>

	#if (NTDDI_VERSION < NTDDI_WIN10_CO)
		#define OLD_WINDOWS_SDK
	#else
		#ifndef NOMINMAX
			#define NOMINMAX
		#endif

		#pragma warning(push)
		#pragma warning(disable : 5039)
		#include <dwmapi.h>
		#pragma comment(lib, "Dwmapi")
		#pragma warning(pop)
	#endif
#endif

/*==================================================================*/

struct FatalError : public std::runtime_error {
	using std::runtime_error::runtime_error;
};

[[noreturn]] static
void throw_fatal_error(unsigned line, const char* function) noexcept(false) {
	blog.fatal("L{} : {}(): {}", line, function, SDL_GetError());

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
		"Fatal SDL Error", SDL_GetError(), nullptr);

	throw FatalError("Fatal SDL Error");
}

/*==================================================================*/

static SDL_Unique<SDL_Window>   s_main_window{};
static SDL_Unique<SDL_Renderer> s_main_renderer{};

SDL_Window*   BasicVideoSpec::get_main_window()   const noexcept { return s_main_window; }
SDL_Renderer* BasicVideoSpec::get_main_renderer() const noexcept { return s_main_renderer; }

/*==================================================================*/

BasicVideoSpec::BasicVideoSpec(const Settings& settings, bool& success) noexcept {
	try {
		if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) { \
			throw_fatal_error(__LINE__, __func__);
		}

		s_main_window = SDL_CreateWindow(nullptr, 0, 0, \
			SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY); \
		if (!s_main_window) { throw_fatal_error(__LINE__, __func__); }

		#if defined(_WIN32) && defined(WINDOWS_NO_ROUNDED_CORNERS)
			#ifdef OLD_WINDOWS_SDK
				static constexpr auto NTDDI_MAJOR{ ((NTDDI_VERSION >> 24) & 0x00FF) };
				static constexpr auto NTDDI_MINOR{ ((NTDDI_VERSION >> 16) & 0x00FF) };
				static constexpr auto NTDDI_BUILD{ ( NTDDI_VERSION        & 0xFFFF) };
				blog.newEntry<BLOG::DEBUG>("Unable to adjust Main Window corner style, "
					"Windows SDK is too old: {}.{}.{}", NTDDI_MAJOR, NTDDI_MINOR, NTDDI_BUILD);
			#else
				else {
					const auto window_handle = SDL_GetPointerProperty(
						SDL_GetWindowProperties(m_main_window),
						SDL_PROP_WINDOW_WIN32_HWND_POINTER,
						nullptr
					);

					if (window_handle) {
						static constexpr auto corner_mode = DWMWCP_DONOTROUND;
						DwmSetWindowAttribute(
							static_cast<HWND>(window_handle),
							DWMWA_WINDOW_CORNER_PREFERENCE,
							&corner_mode, sizeof(corner_mode)
						);
					}
				}
			#endif
		#endif

		ez::Rect deco{};

		if (auto dummy = sdl::make_unique(SDL_CreateWindow(
			nullptr, 64, 64, SDL_WINDOW_UTILITY | SDL_WINDOW_HIDDEN
		))) {
			#ifndef __APPLE__
			constexpr auto away = -(1 << 15);
			SDL_SetWindowPosition(dummy, away, away);
			#endif
			SDL_ShowWindow(dummy);
			SDL_SyncWindow(dummy);
			SDL_RenderPresent(s_main_renderer);
			SDL_GetWindowBordersSize(dummy, &deco.x, &deco.y, &deco.w, &deco.h);
		}

		auto window = settings.window;
		normalize_rect_to_display(window, deco, settings.first_run);

		SDL_SetWindowPosition(s_main_window, window.x, window.y);
		SDL_SetWindowSize(s_main_window, window.w, window.h);
		SDL_SetWindowMinimumSize(s_main_window, 960, 780);

		s_main_renderer = SDL_CreateRenderer(s_main_window, nullptr); \
		if (!s_main_renderer) { throw_fatal_error(__LINE__, __func__); }

		SDL_ShowWindow(s_main_window);
	}
	catch (const FatalError&) {
		success = false; return;
	}
	catch (const std::exception& e) {
		blog.fatal("{}(): Unknown exception caught: {}", __func__, e.what());
		success = false; return;
	}
}

BasicVideoSpec::~BasicVideoSpec() noexcept {
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/*==================================================================*/

SettingsMap BasicVideoSpec::Settings::map() noexcept {
	return {
		::make_setting_link("Window.Main.Pos.X", &window.x),
		::make_setting_link("Window.Main.Pos.Y", &window.y),
		::make_setting_link("Window.Main.Size.W", &window.w),
		::make_setting_link("Window.Main.Size.H", &window.h),
		::make_setting_link("Window.Main.FirstRun", &first_run),
	};
}

auto BasicVideoSpec::export_settings() const noexcept -> Settings {
	Settings out;

	if (SDL_GetWindowFlags(s_main_window) & SDL_WINDOW_MAXIMIZED) {
		SDL_RestoreWindow(s_main_window);
		SDL_SyncWindow(s_main_window);
	}

	SDL_GetWindowPosition(s_main_window, &out.window.x, &out.window.y);
	SDL_GetWindowSize(s_main_window, &out.window.w, &out.window.h);
	out.first_run = false;

	return out;
}

/*==================================================================*/

void BasicVideoSpec::normalize_rect_to_display(ez::Rect& rect, ez::Rect& deco, bool first_run) noexcept {
	auto num_displays =  0; // count of displays SDL found
	auto best_display = -1; // index of display our window will use
	bool rect_intersects_display{};

	// 1: fetch all eligible display IDs
	const auto displays = sdl::make_unique(SDL_GetDisplays(&num_displays));
	if (!displays || num_displays <= 0) { rect = Settings::defaults; return; }

	// 2: fill vector with usable display bounds rects
	std::vector<ez::Rect> display_bounds;
	display_bounds.reserve(num_displays);

	for (auto i = 0; i < num_displays; ++i) {
		if (displays[i] == SDL_GetPrimaryDisplay()) { best_display = i; }
		SDL_Rect display;
		if (SDL_GetDisplayUsableBounds(displays[i], &display)) {
			display_bounds.push_back(ez::Rect{
				display.x, display.y,
				display.w, display.h
			});
		}
	}
	if (display_bounds.empty()) { rect = Settings::defaults; return; }

	// 3: validate rect w/h, use fallbacks if needed
	rect.w = std::max(rect.w, Settings::defaults.w);
	rect.h = std::max(rect.h, Settings::defaults.h);

	if (!first_run) {
		// 4: find largest window/display overlap, if any
		auto best_overlap = 0ull;
		for (auto i = 0u; i < display_bounds.size(); ++i) {
			const auto overlap_area = ez::intersect(rect, display_bounds[i]).area();
			if (overlap_area > best_overlap) { best_overlap = overlap_area; best_display = i; }
		}

		// 5: fall back to searching for closest display
		if ((rect_intersects_display = !!best_overlap) == false) {
			auto best_distance = ~0ull;
			const auto current_center = rect.center();

			for (auto i = 0u; i < display_bounds.size(); ++i) {
				const auto display_center = display_bounds[i].center();
				const auto distance = ez::distance(current_center, display_center);
				if (distance < best_distance) { best_distance = distance; best_display = i; }
			}
		}
	}

	// 6: shrink window to best fit chosen display
	const auto& target = display_bounds[best_display];

	const auto up = deco.x, lt = deco.y;
	const auto dn = deco.w, rt = deco.h;

	rect.w = std::min(rect.w, target.w - lt - rt);
	rect.h = std::min(rect.h, target.h - up - dn);

	if (!rect_intersects_display) {
		// 7a: if we didn't overlap before, center to display
		rect.x = target.x + (target.w - lt - rt - rect.w) / 2 + lt;
		rect.y = target.y + (target.h - up - dn - rect.h) / 2 + up;
	} else {
		// 7b: otherwise, clamp origin to lie within display bounds
		rect.x = std::clamp(rect.x, target.x + lt, target.x + target.w - rt - rect.w);
		rect.y = std::clamp(rect.y, target.y + up, target.y + target.h - dn - rect.h);
	}
}

/*==================================================================*/

static SDL_Texture* create_texture(
	SDL_Renderer* renderer, int w, int h, SDL_PixelFormat format,
	SDL_TextureAccess access, SDL_ScaleMode scale_mode
) noexcept {
	if (!renderer) { return nullptr; }
	try {
		auto* texture = SDL_CreateTexture(renderer, format, access, w, h); \
		if (!texture) { throw_fatal_error(__LINE__, __func__); }

		SDL_SetTextureScaleMode(texture, scale_mode);
		SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
		return texture;
	}
	catch (const FatalError&) {
		return nullptr;
	}
	catch (const std::exception& e) {
		blog.fatal("{}(): Unknown exception caught: {}", __func__, e.what());
		return nullptr;
	}
}

SDL_Texture* BasicVideoSpec::create_stream_texture(
	SDL_Renderer* renderer, int w, int h, bool linear_scalemode
) noexcept {
	return ::create_texture(renderer, w, h, SDL_PIXELFORMAT_RGBX8888,
		SDL_TEXTUREACCESS_STREAMING, SDL_ScaleMode(linear_scalemode));
}

SDL_Texture* BasicVideoSpec::create_target_texture(
	SDL_Renderer* renderer, int w, int h, bool linear_scalemode
) noexcept {
	return ::create_texture(renderer, w, h, SDL_PIXELFORMAT_RGBX8888,
		SDL_TEXTUREACCESS_TARGET, SDL_ScaleMode(linear_scalemode));
}

void BasicVideoSpec::write_stream_texture(
	SDL_Renderer* renderer, SDL_Texture* texture, const std::byte* src_buffer
) noexcept {
	if (!renderer || !texture) { return; }

	void* pixels_ptr; int pitch;

	if (!SDL_LockTexture(texture, nullptr, &pixels_ptr, &pitch)) { \
		throw_fatal_error(__LINE__, __func__);
	} else {
		const auto total_rows = texture->h;
		const auto row_length = texture->w * SDL_BYTESPERPIXEL(texture->format);

		for (int y = 0; y < total_rows; ++y) {
			std::memcpy(static_cast<std::byte*>(pixels_ptr) + y * pitch,
				src_buffer + y * row_length, row_length);
		}

		SDL_UnlockTexture(texture);
	}

	const SDL_FRect dest_frect = { 0.0f, 0.0f,
		float(texture->w), float(texture->h) };

	SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
	SDL_RenderTexture(renderer, texture, nullptr, &dest_frect);
}

void BasicVideoSpec::write_stream_texture(
	SDL_Renderer* renderer, SDL_Texture* dst_texture, SDL_Texture* src_texture
) noexcept {
	if (!renderer || !dst_texture || !src_texture) { return; }

	const SDL_FRect dest_frect = { 0.0f, 0.0f,
		float(dst_texture->w), float(dst_texture->h) };

	SDL_SetRenderTarget(renderer, dst_texture);
	SDL_RenderTexture(renderer, src_texture, nullptr, &dest_frect);
	SDL_SetRenderTarget(renderer, nullptr);
}

/*==================================================================*/

bool BasicVideoSpec::set_window_title(const std::string& title, SDL_Window* window) noexcept {
	return SDL_SetWindowTitle(window ? window : s_main_window.get(), title.c_str());
}

bool BasicVideoSpec::is_main_window_id(unsigned id) const noexcept {
	return id && id == SDL_GetWindowID(s_main_window);
}

bool BasicVideoSpec::raise_window(SDL_Window* window) noexcept {
	return SDL_RaiseWindow(window ? window : s_main_window.get());
}

/*==================================================================*/

void BasicVideoSpec::set_automatic_render_scale_for_renderer() noexcept {
	const auto scale = SDL_GetWindowPixelDensity(s_main_window);
	SDL_SetRenderScale(s_main_renderer, scale, scale);
}

bool BasicVideoSpec::try_render_present() noexcept {
	try {
		if (!SDL_RenderPresent(s_main_renderer)) { \
			throw_fatal_error(__LINE__, __func__);
		}

		return true;
	}
	catch (const FatalError&) {
		return false;
	}
	catch (const std::exception& e) {
		blog.fatal("{}(): Unknown exception caught: {}", __func__, e.what());
		return false;
	}
}
