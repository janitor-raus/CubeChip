/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "FrontendInterface.hpp"
#include "BasicVideoSpec.hpp"
#include "BasicLogger.hpp"

#include <vector>
#include <exception>
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>

#ifdef _WIN32
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
	blog.newEntry<BLOG::FTL>("L{} : {}(): {}",
		line, function, SDL_GetError());

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
		"Fatal SDL Error", SDL_GetError(), nullptr);

	throw FatalError("Fatal SDL Error");
}

/*==================================================================*/

BasicVideoSpec::BasicVideoSpec(const Settings& settings, bool& success) noexcept {
	try {
		if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) { \
			throw_fatal_error(__LINE__, __func__);
		}

		m_main_window = SDL_CreateWindow(nullptr, 0, 0, \
			SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY); \
		if (!m_main_window) { throw_fatal_error(__LINE__, __func__); }

		#if defined(_WIN32) && defined(WINDOWS_NO_ROUNDED_CORNERS)
			#ifdef OLD_WINDOWS_SDK
				static constexpr auto NTDDI_MAJOR{ ((NTDDI_VERSION >> 24) & 0x00FF) };
				static constexpr auto NTDDI_MINOR{ ((NTDDI_VERSION >> 16) & 0x00FF) };
				static constexpr auto NTDDI_BUILD{ ( NTDDI_VERSION        & 0xFFFF) };
				blog.newEntry<BLOG::DEBUG>("Unable to adjust Main Window corner style, "
					"Windows SDK is too old: {}.{}.{}", NTDDI_MAJOR, NTDDI_MINOR, NTDDI_BUILD);
			#else
				else {
					const auto windowHandle = SDL_GetPointerProperty(
						SDL_GetWindowProperties(m_main_window),
						SDL_PROP_WINDOW_WIN32_HWND_POINTER,
						nullptr
					);

					if (windowHandle) {
						static constexpr auto cornerMode = DWMWCP_DONOTROUND;
						DwmSetWindowAttribute(
							static_cast<HWND>(windowHandle),
							DWMWA_WINDOW_CORNER_PREFERENCE,
							&cornerMode, sizeof(cornerMode)
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
			SDL_RenderPresent(m_main_renderer);
			SDL_GetWindowBordersSize(dummy, &deco.x, &deco.y, &deco.w, &deco.h);
		}

		auto window = settings.window;
		normalize_rect_to_display(window, deco, settings.first_run);

		SDL_SetWindowPosition(m_main_window, window.x, window.y);
		SDL_SetWindowSize(m_main_window, window.w, window.h);
		SDL_SetWindowMinimumSize(m_main_window, 800, 600);

		m_main_renderer = SDL_CreateRenderer(m_main_window, nullptr); \
		if (!m_main_renderer) { throw_fatal_error(__LINE__, __func__); }

		FrontendInterface::init_video(m_main_window, m_main_renderer);
		FrontendInterface::set_display_scaling(get_display_pixel_density());

		SDL_ShowWindow(m_main_window);
	}
	catch (const FatalError&) {
		success = false; return;
	}
	catch (const std::exception& e) {
		blog.newEntry<BLOG::FTL>("{}(): Unknown exception caught: {}",
			__func__, e.what());
		success = false; return;
	}
}

BasicVideoSpec::~BasicVideoSpec() noexcept {
	FrontendInterface::quit_video();
	FrontendInterface::quit_context();

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

	if (SDL_GetWindowFlags(m_main_window) & SDL_WINDOW_MAXIMIZED) {
		SDL_RestoreWindow(m_main_window);
		SDL_SyncWindow(m_main_window);
	}

	SDL_GetWindowPosition(m_main_window, &out.window.x, &out.window.y);
	SDL_GetWindowSize(m_main_window, &out.window.w, &out.window.h);
	out.first_run = false;

	return out;
}

/*==================================================================*/

float BasicVideoSpec::get_display_refresh_rate(SDL_DisplayID display) noexcept {
	if (const auto* mode = SDL_GetCurrentDisplayMode(display)) {
		if (mode->refresh_rate_denominator > 0) {
			return float(mode->refresh_rate_numerator) /
				float(mode->refresh_rate_denominator);
		} else {
			return (mode->refresh_rate > 0.0f)
				? mode->refresh_rate : 60.0f;
		}
	}
	return 60.0f;
}

void BasicVideoSpec::normalize_rect_to_display(ez::Rect& rect, ez::Rect& deco, bool first_run) noexcept {
	auto numDisplays =  0; // count of displays SDL found
	auto bestDisplay = -1; // index of display our window will use
	bool rectIntersectsDisplay{};

	// 1: fetch all eligible display IDs
	auto displays = sdl::make_unique(SDL_GetDisplays(&numDisplays));
	if (!displays || numDisplays <= 0) [[unlikely]]
		{ rect = Settings::defaults; return; }

	// 2: fill vector with usable display bounds rects
	std::vector<ez::Rect> displayBounds;
	displayBounds.reserve(numDisplays);

	for (auto i = 0; i < numDisplays; ++i) {
		if (displays[i] == SDL_GetPrimaryDisplay()) { bestDisplay = i; }
		SDL_Rect display;
		if (SDL_GetDisplayUsableBounds(displays[i], &display)) {
			ez::Rect bounds{ display.x, display.y, display.w, display.h };
			displayBounds.push_back(std::move(bounds));
		}
	}
	if (displayBounds.empty()) [[unlikely]]
		{ rect = Settings::defaults; return; }

	// 3: validate rect w/h, use fallbacks if needed
	rect.w = std::max(rect.w, Settings::defaults.w);
	rect.h = std::max(rect.h, Settings::defaults.h);

	if (!first_run) {
		// 4: find largest window/display overlap, if any
		auto bestOverlap = 0ull;
		for (auto i = 0u; i < displayBounds.size(); ++i) {
			const auto overlapArea{ ez::intersect(rect, displayBounds[i]).area() };
			if (overlapArea > bestOverlap) { bestOverlap = overlapArea; bestDisplay = i; }
		}

		// 5: fall back to searching for closest display
		if ((rectIntersectsDisplay = !!bestOverlap) == false) {
			auto bestDistance = ~0ull;
			const auto currentCenter{ rect.center() };

			for (auto i = 0u; i < displayBounds.size(); ++i) {
				const auto displayCenter{ displayBounds[i].center() };
				const auto distance{ ez::distance(currentCenter, displayCenter) };
				if (distance < bestDistance) { bestDistance = distance; bestDisplay = i; }
			}
		}
	}

	// 6: shrink window to best fit chosen display
	const auto& target = displayBounds[bestDisplay];

	const auto up = deco.x, lt = deco.y;
	const auto dn = deco.w, rt = deco.h;

	rect.w = std::min(rect.w, target.w - lt - rt);
	rect.h = std::min(rect.h, target.h - up - dn);

	if (!rectIntersectsDisplay) {
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

SDL_Texture* BasicVideoSpec::create_stream_texture(SDL_Renderer* renderer, int w, int h) noexcept {
	if (!renderer) { return nullptr; }
	try {
		auto* texture = SDL_CreateTexture(renderer, \
			SDL_PIXELFORMAT_RGBX8888, SDL_TEXTUREACCESS_STREAMING, w, h); \
		if (!texture) { throw_fatal_error(__LINE__, __func__); }
		return texture;
	}
	catch (const FatalError&) {
		return nullptr;
	}
	catch (const std::exception& e) {
		blog.newEntry<BLOG::FTL>("{}(): Unknown exception caught: {}",
			__func__, e.what());
		return nullptr;
	}
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

/*==================================================================*/

float BasicVideoSpec::get_display_pixel_density(SDL_DisplayID display_id) noexcept {
	return SDL_GetDisplayContentScale(display_id ? display_id : SDL_GetPrimaryDisplay());
}

float BasicVideoSpec::get_window_pixel_density(SDL_Window* window) noexcept {
	return SDL_GetWindowPixelDensity(window ? window : m_main_window.get());
}

bool BasicVideoSpec::set_window_title(const std::string& title, SDL_Window* window) noexcept {
	return SDL_SetWindowTitle(window ? window : m_main_window.get(), title.c_str());
}

bool BasicVideoSpec::is_main_window_id(unsigned id) const noexcept {
	return id && id == SDL_GetWindowID(m_main_window);
}

bool BasicVideoSpec::raise_window(SDL_Window* window) noexcept {
	return SDL_RaiseWindow(window ? window : m_main_window.get());
}

bool BasicVideoSpec::update_renderer_logical_presentation(SDL_Renderer* renderer) noexcept {
	auto current_renderer = renderer ? renderer : m_main_renderer.get();
	int  window_w, window_h;

	SDL_GetWindowSize(SDL_GetRenderWindow(current_renderer),
		&window_w, &window_h);

	return SDL_SetRenderLogicalPresentation(current_renderer,
		window_w, window_h, SDL_LOGICAL_PRESENTATION_STRETCH);
}

/*==================================================================*/

bool BasicVideoSpec::render_present() noexcept {
	try {
		FrontendInterface::begin_new_frame();
		FrontendInterface::render_frame(m_main_renderer);

		if (!SDL_RenderPresent(m_main_renderer)) { \
			throw_fatal_error(__LINE__, __func__);
		}

		return true;
	}
	catch (const FatalError&) {
		return false;
	}
	catch (const std::exception& e) {
		blog.newEntry<BLOG::FTL>("{}(): Unknown exception caught: {}",
			__func__, e.what());
		return false;
	}
}
