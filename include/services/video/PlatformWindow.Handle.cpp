/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

module;

#include <vector>
#include <memory>
#include "LifetimeWrapperSDL.hpp"
#include "SettingWrapper.hpp"
#include "EzMaths.hpp"
#include "BasicLogger.hpp"
#include "StringJoin.hpp"
#include <SDL3/SDL_render.h>

/*==================================================================*/

#ifdef _WIN32
  #ifdef WINDOWS_NO_ROUNDED_CORNERS
    #include <sdkddkver.h>

    #if (NTDDI_VERSION < NTDDI_WIN10_CO)
      #define OLD_WINDOWS_SDK
    #else
      #ifndef NOMINMAX
        #define NOMINMAX
      #endif
    #endif
  #endif

  #pragma warning(push)
  #pragma warning(disable : 5039)
    #include <dwmapi.h>
    #pragma comment(lib, "Dwmapi")
  #pragma warning(pop)
#endif

/*==================================================================*/

module PlatformWindow;
#ifdef __INTELLISENSE__
# include "PlatformWindow.cppm"
#endif

/*==================================================================*/

static void log_warn(unsigned line, const char* function) noexcept {
	ScopedLogSource source("platform_window");
	blog.warn("L{} : {}(): {}", line, function, SDL_GetError());
}

static void log_error(unsigned line, const char* function) noexcept {
	ScopedLogSource source("platform_window");
	blog.error("L{} : {}(): {}", line, function, SDL_GetError());
}

static auto create_texture(
	SDL_Renderer* renderer, int w, int h, SDL_PixelFormat format,
	SDL_TextureAccess access, SDL_ScaleMode scale_mode
) noexcept {
	auto* texture = SDL_CreateTexture(renderer, format, access, w, h);
	if (!texture) { log_error(__LINE__ - 1, __func__); }
	else {
		if (!SDL_SetTextureScaleMode(texture, scale_mode)) {
			log_warn(__LINE__ - 1, __func__);
		}
		if (!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND)) {
			log_warn(__LINE__ - 1, __func__);
		}
	}

	return sdl::make_shared(texture);
}

static void normalize_to_display(PlatformWindow::Handle& handle) noexcept {
	auto num_displays = 0; // count of displays SDL found
	auto best_display = 0; // index of display our window will use
	bool rect_intersects_display = false;

	// 1: fetch all eligible display IDs
	const auto displays = sdl::make_unique(SDL_GetDisplays(&num_displays));
	if (!displays || num_displays <= 0) { return; }

	// 2: fill vector with usable display bounds rects
	std::vector<ez::Rect> display_bounds;
	display_bounds.reserve(num_displays);

	for (auto i = 0; i < num_displays; ++i) {
		if (displays.get()[i] == SDL_GetPrimaryDisplay()) { best_display = i; }
		SDL_Rect display;
		if (!SDL_GetDisplayUsableBounds(displays.get()[i], &display)) {
			log_warn(__LINE__ - 1, __func__);
		} else {
			display_bounds.push_back(ez::Rect{
				display.x, display.y, display.w, display.h
			});
		}
	}
	if (display_bounds.empty()) { return; }

	ez::Rect rect;
	handle.get_position(&rect.x, &rect.y);
	handle.get_size(&rect.w, &rect.h);

	// 3: find largest window/display overlap, if any
	auto best_overlap = 0ull;
	for (auto i = 0u; i < display_bounds.size(); ++i) {
		const auto overlap_area = ez::intersection(rect, display_bounds[i]).area();
		if (overlap_area > best_overlap) { best_overlap = overlap_area; best_display = i; }
	}

	// 4: fall back to searching for closest display
	if ((rect_intersects_display = !!best_overlap) == false) {
		auto best_distance = ~0ull;
		const auto current_center = rect.center();

		for (auto i = 0u; i < display_bounds.size(); ++i) {
			const auto display_center = display_bounds[i].center();
			const auto distance = ez::distance(current_center, display_center);
			if (distance < best_distance) { best_distance = distance; best_display = i; }
		}
	}

	// 5: shrink window to best fit chosen display
	const auto& target = display_bounds[best_display];

	rect.w = std::min(rect.w, target.w);
	rect.h = std::min(rect.h, target.h);

	if (!rect_intersects_display) {
		// 6a: if we didn't overlap before, center to display
		rect.x = target.x + (target.w - rect.w) / 2;
		rect.y = target.y + (target.h - rect.h) / 2;
	} else {
		// 6b: otherwise, clamp origin to lie within display bounds
		rect.x = std::clamp(rect.x, target.x, target.x + target.w - rect.w);
		rect.y = std::clamp(rect.y, target.y, target.y + target.h - rect.h);
	}

	handle.set_size(rect.w, rect.h);
	handle.set_position(rect.x, rect.y);
}

static void adjust_window_corners(PlatformWindow::Handle& handle) noexcept {
#if defined(_WIN32) && defined(WINDOWS_NO_ROUNDED_CORNERS)
  #ifdef OLD_WINDOWS_SDK
	static constexpr auto NTDDI_MAJOR = ((NTDDI_VERSION >> 24) & 0x00FF);
	static constexpr auto NTDDI_MINOR = ((NTDDI_VERSION >> 16) & 0x00FF);
	static constexpr auto NTDDI_BUILD = ( NTDDI_VERSION        & 0xFFFF);
	blog.debug("Unable to adjust PlatformWindow with key '{}' corner style, "
		"Windows SDK is too old: {}.{}.{}", handle.handle_key.data, NTDDI_MAJOR, NTDDI_MINOR, NTDDI_BUILD);
  #else
	if (const auto window_handle = SDL_GetPointerProperty(
		SDL_GetWindowProperties(handle),
		SDL_PROP_WINDOW_WIN32_HWND_POINTER,
		nullptr
	)) {
		static constexpr auto corner_mode = DWMWCP_DONOTROUND;
		DwmSetWindowAttribute(
			static_cast<HWND>(window_handle),
			DWMWA_WINDOW_CORNER_PREFERENCE,
			&corner_mode, sizeof(corner_mode)
		);
	}
  #endif
#endif
	(void)handle;
}

/*==================================================================*/

PlatformWindow::Handle::Handle(
	CreatorKey&&, ShortKey key, const char* title, int w, int h,
	GVB_WindowFlags window_flags, const char* rendering_driver_name
) noexcept
	: handle_key(key)
{
	m_sync_node = PWi::s_auto_sync_window_mru.end();
	create_window(title, w, h, window_flags);
	create_renderer(rendering_driver_name);
}

PlatformWindow::Handle::~Handle() noexcept {
	blog.debug("Platform Window '{}' with ID {} destroyed.",
		handle_key.data, get_id());

	PWi::erase_id_from_map(this);
	PWi::erase_sync_from_mru(this);
	PWi::retarget_main(this);
	PWi::retarget_sync(this);
	PWi::retarget_live(this);

	drop_linked(DESTROY_LINKED);
	m_texture_list.clear();
	m_renderer_ptr.reset();
	m_window_ptr.reset();
}

/*==================================================================*/

void PlatformWindow::Handle::drop_linked(LinkAction action) noexcept {
	auto* link_ptr = std::exchange(m_link_ptr, nullptr);
	if (action == DESTROY_LINKED && link_ptr) {
		link_ptr->drop_linked(DESTROY_LINKED);
	}
}

void PlatformWindow::Handle::notify_link(LinkNotify action) noexcept {
	if (!m_link_ptr) { return; }
	m_link_ptr->notify_link(action);
}

void PlatformWindow::Handle::on_present() noexcept {
	if (is_inert() || !is_ready()) { return; }

	if (m_link_ptr) { m_link_ptr->on_present(); }

	int vsync = this == PlatformWindow::get_sync_handle();
	if (!SDL_SetRenderVSync(*this, vsync)) { log_warn (__LINE__, __func__); }
	if (!SDL_RenderPresent (*this       )) { log_error(__LINE__, __func__); }
}

auto PlatformWindow::Handle::on_event(const SDL_Event& event, EventCallback callback) noexcept -> EventResult {
	if (is_inert() || !get_window() || event.window.windowID != get_id()) { return EVENT_CONTINUE; }
	if (callback) {
		auto result = callback(event);
		if (result != EVENT_CONTINUE) { return result; }
	}

	if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
		PlatformWindow::destroy(handle_key);
		return EVENT_SUCCESS;
	}

	if (get_renderer()) {
		switch (event.type) {
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				PWi::insert_sync_to_mru(this);
				break;

			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				if (auto pixel_scale = get_pixel_density()) {
					set_render_scale(pixel_scale, pixel_scale);
				}
				break;

			case SDL_EVENT_RENDER_DEVICE_RESET:
			case SDL_EVENT_RENDER_DEVICE_LOST:
				create_renderer(get_renderer_name());
				break;

			default: break;
		}
	}

	return !m_link_ptr ? EVENT_CONTINUE
		: m_link_ptr->on_event(event);
}

/*==================================================================*/

bool PlatformWindow::Handle::create_window(const char* title, int w, int h, GVB_WindowFlags window_flags) noexcept {
	if (is_inert()) { return false; }

	if (get_window()) {
		if (get_renderer()) {
			notify_link(TEARDOWN_PHASE);
			PWi::erase_sync_from_mru(this);
			m_texture_list.clear();
			m_renderer_ptr.reset();
		}

		(void)export_settings();
		PWi::erase_id_from_map(this);
		m_window_ptr.reset();
	}

	if (auto* window = SDL_CreateWindow(title, w, h, window_flags)) {
		m_window_ptr.reset(window);
		PWi::insert_id_to_map(this);
		import_settings();
		::adjust_window_corners(*this);
		::normalize_to_display(*this);
		blog.debug("Platform Window '{}' with ID {} created.",
			handle_key.data, get_id());
	} else {
		log_error(__LINE__ - 7, __func__);
		PWi::retarget_main(this);
	}

	// no renderer, invalidate
	PWi::retarget_sync(this);
	PWi::retarget_live(this);

	return get_window();
}

bool PlatformWindow::Handle::create_renderer(const char* driver) noexcept {
	if (is_inert() || !get_window()) { return false; }

	if (get_renderer()) {
		notify_link(TEARDOWN_PHASE);
		PWi::erase_sync_from_mru(this);
		m_texture_list.clear();
		m_renderer_ptr.reset();
	}

	if (auto* renderer = SDL_CreateRenderer(*this, driver)) {
		m_renderer_ptr.reset(renderer);
		PWi::insert_sync_to_mru(this);
		notify_link(REBUILD_PHASE);
	} else {
		log_error(__LINE__ - 5, __func__);
		PWi::retarget_sync(this);
		PWi::retarget_live(this);
	}

	return get_renderer();
}

/*==================================================================*/

bool PlatformWindow::Handle::set_title(const char* title) noexcept {
	bool success = SDL_SetWindowTitle(*this, title);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

const char* PlatformWindow::Handle::get_title() const noexcept {
	return SDL_GetWindowTitle(*this);
}

bool PlatformWindow::Handle::set_position(int x, int y) noexcept {
	bool success = SDL_SetWindowPosition(*this, x, y);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_position(int* x, int* y) const noexcept {
	bool success = SDL_GetWindowPosition(*this, x, y);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::set_size(int w, int h) noexcept {
	bool success = SDL_SetWindowSize(*this, w, h);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_size(int* w, int* h) const noexcept {
	bool success = SDL_GetWindowSize(*this, w, h);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::set_min_size(int w, int h) noexcept {
	bool success = SDL_SetWindowMinimumSize(*this, w, h);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_min_size(int* w, int* h) const noexcept {
	bool success = SDL_GetWindowMinimumSize(*this, w, h);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::set_max_size(int w, int h) noexcept {
	bool success = SDL_SetWindowMaximumSize(*this, w, h);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_max_size(int* w, int* h) const noexcept {
	bool success = SDL_GetWindowMaximumSize(*this, w, h);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::set_aspect_ratio(float w, float h) noexcept {
	bool success = SDL_SetWindowAspectRatio(*this, w, h);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_aspect_ratio(float* w, float* h) const noexcept {
	bool success = SDL_GetWindowAspectRatio(*this, w, h);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::set_render_scale(float w, float h) noexcept {
	bool success = SDL_SetRenderScale(*this, w, h);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_render_scale(float* w, float* h) const noexcept {
	bool success = SDL_GetRenderScale(*this, w, h);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

/*==================================================================*/

bool PlatformWindow::Handle::set_bmp_icon(const char* icon_path) noexcept {
	if (!icon_path || icon_path[0] == '\0') { return false; }
	auto icon_surface = sdl::make_unique(SDL_LoadBMP(icon_path));
	bool success = SDL_SetWindowIcon(*this, icon_surface.get());
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::set_parent(Handle* parent) noexcept {
	if (parent) { return SDL_SetWindowParent(*this, *parent); }
	else { return SDL_SetWindowParent(*this, nullptr); }
}

/*==================================================================*/

bool PlatformWindow::Handle::set_bordered(bool enabled) noexcept {
	bool success = SDL_SetWindowBordered(*this, enabled);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_bordered() const noexcept {
	return get_window() && (SDL_GetWindowFlags(*this) & SDL_WINDOW_BORDERLESS) == 0;
}

bool PlatformWindow::Handle::toggle_bordered() noexcept {
	return set_bordered(!get_bordered());
}

bool PlatformWindow::Handle::set_resizable(bool enabled) noexcept {
	bool success = SDL_SetWindowResizable(*this, enabled);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_resizable() const noexcept {
	return get_window() && (SDL_GetWindowFlags(*this) & SDL_WINDOW_RESIZABLE) != 0;
}

bool PlatformWindow::Handle::toggle_resizable() noexcept {
	return set_resizable(!get_resizable());
}

bool PlatformWindow::Handle::set_always_on_top(bool enabled) noexcept {
	bool success = SDL_SetWindowAlwaysOnTop(*this, enabled);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_always_on_top() const noexcept {
	return get_window() && (SDL_GetWindowFlags(*this) & SDL_WINDOW_ALWAYS_ON_TOP) != 0;
}

bool PlatformWindow::Handle::toggle_always_on_top() noexcept {
	return set_always_on_top(!get_always_on_top());
}

bool PlatformWindow::Handle::set_fullscreen(bool enabled) noexcept {
	bool success = SDL_SetWindowFullscreen(*this, enabled);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_fullscreen() const noexcept {
	return get_window() && (SDL_GetWindowFlags(*this) & SDL_WINDOW_FULLSCREEN) != 0;
}

bool PlatformWindow::Handle::toggle_fullscreen() noexcept {
	return set_fullscreen(!get_fullscreen());
}

bool PlatformWindow::Handle::set_modal(bool enabled) noexcept {
	bool success = SDL_SetWindowModal(*this, enabled);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_modal() const noexcept {
	return get_window() && (SDL_GetWindowFlags(*this) & SDL_WINDOW_MODAL) != 0;
}

bool PlatformWindow::Handle::toggle_modal() noexcept {
	return set_modal(!get_modal());
}

bool PlatformWindow::Handle::set_focusable(bool enabled) noexcept {
	bool success = SDL_SetWindowFocusable(*this, enabled);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_focusable() const noexcept {
	return get_window() && (SDL_GetWindowFlags(*this) & SDL_WINDOW_NOT_FOCUSABLE) == 0;
}

bool PlatformWindow::Handle::toggle_focusable() noexcept {
	return set_focusable(!get_focusable());
}

bool PlatformWindow::Handle::set_fill_document(bool enabled) noexcept {
	bool success = SDL_SetWindowFillDocument(*this, enabled);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_fill_document() const noexcept {
	return get_window() && (SDL_GetWindowFlags(*this) & SDL_WINDOW_FILL_DOCUMENT) != 0;
}

bool PlatformWindow::Handle::toggle_fill_document() noexcept {
	return set_fill_document(!get_fill_document());
}

bool PlatformWindow::Handle::set_keyboard_grab(bool enabled) noexcept {
	bool success = SDL_SetWindowKeyboardGrab(*this, enabled);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_keyboard_grab() const noexcept {
	return get_window() && (SDL_GetWindowFlags(*this) & SDL_WINDOW_KEYBOARD_GRABBED) != 0;
}

bool PlatformWindow::Handle::toggle_keyboard_grab() noexcept {
	return set_keyboard_grab(!get_keyboard_grab());
}

bool PlatformWindow::Handle::set_mouse_grab(bool enabled) noexcept {
	bool success = SDL_SetWindowMouseGrab(*this, enabled);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_mouse_grab() const noexcept {
	return get_window() && (SDL_GetWindowFlags(*this) & SDL_WINDOW_MOUSE_GRABBED) != 0;
}

bool PlatformWindow::Handle::toggle_mouse_grab() noexcept {
	return set_mouse_grab(!get_mouse_grab());
}

bool PlatformWindow::Handle::set_relative_mouse_mode(bool enabled) noexcept {
	bool success = SDL_SetWindowRelativeMouseMode(*this, enabled);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::get_relative_mouse_mode() const noexcept {
	return get_window() && (SDL_GetWindowFlags(*this) & SDL_WINDOW_MOUSE_RELATIVE_MODE) != 0;
}

bool PlatformWindow::Handle::toggle_relative_mouse_mode() noexcept {
	return set_relative_mouse_mode(!get_relative_mouse_mode());
}

/*==================================================================*/

bool PlatformWindow::Handle::set_progress_state(GVB_ProgressState state) noexcept {
	bool success = SDL_SetWindowProgressState(*this, SDL_ProgressState(state));
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

GVB_ProgressState PlatformWindow::Handle::get_progress_state() const noexcept {
	return GVB_ProgressState(SDL_GetWindowProgressState(*this));
}

bool PlatformWindow::Handle::set_progress_value(float value) noexcept {
	bool success = SDL_SetWindowProgressValue(*this, value);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

float PlatformWindow::Handle::get_progress_value() const noexcept {
	return SDL_GetWindowProgressValue(*this);
}

/*==================================================================*/

bool PlatformWindow::Handle::minimize() noexcept {
	bool success = SDL_MinimizeWindow(*this);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::maximize() noexcept {
	bool success = SDL_MaximizeWindow(*this);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::is_minimized() const noexcept {
	return get_window() && (SDL_GetWindowFlags(*this) & SDL_WINDOW_MINIMIZED) != 0;
}

bool PlatformWindow::Handle::is_maximized() const noexcept {
	return get_window() && (SDL_GetWindowFlags(*this) & SDL_WINDOW_MAXIMIZED) != 0;
}

/*==================================================================*/

const char* PlatformWindow::Handle::get_renderer_name() const noexcept {
	auto renderer_name = SDL_GetRendererName(*this);
	if (!renderer_name) { log_warn(__LINE__ - 1, __func__); }
	return renderer_name;
}

unsigned PlatformWindow::Handle::get_display() const noexcept {
	auto display_id = SDL_GetDisplayForWindow(*this);
	if (!display_id) { log_warn(__LINE__ - 1, __func__); }
	return display_id;
}

float PlatformWindow::Handle::get_pixel_density() const noexcept {
	auto density = SDL_GetWindowPixelDensity(*this);
	if (!density) { log_warn(__LINE__ - 1, __func__); }
	return density;
}

/*==================================================================*/

bool PlatformWindow::Handle::flash(GVB_FlashOperation operation) noexcept {
	bool success = SDL_FlashWindow(*this, SDL_FlashOperation(operation));
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::restore() noexcept {
	bool success = SDL_RestoreWindow(*this);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::raise() noexcept {
	bool success = SDL_RaiseWindow(*this);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::sync() noexcept {
	bool success = SDL_SyncWindow(*this);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::hide() noexcept {
	bool success = SDL_HideWindow(*this);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

bool PlatformWindow::Handle::show() noexcept {
	bool success = SDL_ShowWindow(*this);
	if (!success) { log_warn(__LINE__ - 1, __func__); }
	return success;
}

unsigned PlatformWindow::Handle::get_id() const noexcept {
	auto window_id = SDL_GetWindowID(*this);
	if (!window_id) { log_warn(__LINE__ - 1, __func__); }
	return window_id;
}

bool PlatformWindow::Handle::has_persistent_geometry() const noexcept {
	return get_window() && (SDL_GetWindowFlags(*this) & (
		SDL_WINDOW_MODAL | SDL_WINDOW_UTILITY | SDL_WINDOW_TOOLTIP
	)) == 0;
}

/*==================================================================*/

SDL_Weak<SDL_Texture> PlatformWindow::Handle::create_stream_texture(
	int w, int h,
	bool transparency,
	bool linear_scaling
) noexcept {
	return m_texture_list.emplace_front(::create_texture(
		*this, w, h, transparency
			? SDL_PIXELFORMAT_RGBA8888
			: SDL_PIXELFORMAT_RGBX8888,
		SDL_TEXTUREACCESS_STREAMING,
		SDL_ScaleMode(linear_scaling)
	));
}

SDL_Weak<SDL_Texture> PlatformWindow::Handle::create_target_texture(
	int w, int h,
	bool transparency,
	bool linear_scaling
) noexcept {
	return m_texture_list.emplace_front(::create_texture(
		*this, w, h, transparency
			? SDL_PIXELFORMAT_RGBA8888
			: SDL_PIXELFORMAT_RGBX8888,
		SDL_TEXTUREACCESS_TARGET,
		SDL_ScaleMode(linear_scaling)
	));
}

void PlatformWindow::Handle::upload_stream_texture(
	const std::byte* src_buffer, SDL_Texture* texture, GVB_ScaleMode scale_mode
) noexcept {
	if (!get_renderer() || !texture) { return; }

	void* pixels_ptr; int pitch;

	if (!SDL_LockTexture(texture, nullptr, &pixels_ptr, &pitch)) {
		log_error(__LINE__ - 1, __func__); return;
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

	if (!SDL_SetTextureScaleMode(texture, SDL_ScaleMode(scale_mode))) {
		log_warn(__LINE__ - 1, __func__);
	}
	if (!SDL_RenderTexture(*this, texture, nullptr, &dest_frect)) {
		log_error(__LINE__ - 1, __func__);
	}
}

void PlatformWindow::Handle::render_whole_to_target(
	SDL_Texture* src_texture, SDL_Texture* dst_texture
) noexcept {
	if (!get_renderer() || !src_texture) { return; }

	const SDL_FRect dest_frect = { 0.0f, 0.0f,
		float(dst_texture ? dst_texture->w : src_texture->w),
		float(dst_texture ? dst_texture->h : src_texture->h),
	};

	auto* current_target = SDL_GetRenderTarget(*this);

	if (!SDL_SetRenderTarget(*this, dst_texture)) {
		log_error(__LINE__ - 1, __func__); return;
	}
	if (!SDL_RenderTexture(*this, src_texture, nullptr, &dest_frect)) {
		log_error(__LINE__ - 1, __func__);
	}
	if (!SDL_SetRenderTarget(*this, current_target)) {
		log_error(__LINE__ - 1, __func__);
	}
}

/*==================================================================*/

SettingsMap PlatformWindow::Handle::Settings::map(const Handle& handle) noexcept {
	return {
		::make_setting_link(::join("Window.", handle.handle_key.data, ".Pos.X"), &window.x),
		::make_setting_link(::join("Window.", handle.handle_key.data, ".Pos.Y"), &window.y),
		::make_setting_link(::join("Window.", handle.handle_key.data, ".Size.W"), &window.w),
		::make_setting_link(::join("Window.", handle.handle_key.data, ".Size.H"), &window.h),
	};
}

// XXX -- per-window, but we need to fix the toml modeller
auto PlatformWindow::Handle::export_settings() const noexcept -> Settings {
	Settings out;
	if (!has_persistent_geometry() || !get_window()) { return out; }

	if (is_maximized()) {
		// const-cast here
		(**this).restore();
		(**this).sync();
	}
	get_position(&out.window.x, &out.window.y);
	get_size(&out.window.w, &out.window.h);

	return out;
}

// XXX - not actually importing lol -- fixxxxxxx
void PlatformWindow::Handle::import_settings() noexcept {
	if (!has_persistent_geometry()) { return; }

}
