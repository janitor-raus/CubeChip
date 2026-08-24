/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

module;

#include "BasicLogger.hpp"

#include <cstdio>
#include <utility>
#include <SDL3/SDL_render.h>

module PlatformWindow;


/*==================================================================*/

const PlatformWindow::Registry& PlatformWindow::registry() noexcept {
	return internal::s_platform_window_registry;
}

void PlatformWindow::clear_registry() noexcept {
	internal::s_platform_window_registry.clear();
}

void PlatformWindow::internal::retarget_main(Handle* cur_handle, Handle* new_handle) noexcept {
	if (cur_handle != PlatformWindow::get_main()) { return; }
	s_main_platform_window_ptr = new_handle == nullptr ||
		!new_handle->get_window() ? nullptr : new_handle;
}

void PlatformWindow::internal::retarget_sync(Handle* cur_handle, Handle* new_handle) noexcept {
	if (cur_handle != PlatformWindow::get_sync()) { return; }
	s_sync_platform_window_ptr = new_handle == nullptr ||
		!new_handle->is_ready() ? nullptr : new_handle;
}

void PlatformWindow::internal::retarget_live(Handle* cur_handle, Handle* new_handle) noexcept {
	if (cur_handle != PlatformWindow::get_live()) { return; }
	s_live_platform_window_ptr = new_handle == nullptr ||
		!new_handle->is_ready() ? nullptr : new_handle;
}

/*==================================================================*/

PlatformWindow::Handle& PlatformWindow::create(
	const char* key, const char* title, int w, int h,
	GVB_WindowFlags window_flags, const char* rendering_driver_name
) noexcept {
	ShortKey new_key = [key]() noexcept {
		if (key && key[0] != '\0') { return ShortKey(key); }

		static unsigned int s_auto_counter = 0;

		ShortKey temp;
		std::snprintf(temp.data, temp.size,
			"~auto:%u", ++s_auto_counter);
		return temp;
	}();

	// check for existing entry, erase, produce warning just in case
	if (find(new_key)) {
		blog.warn("Replacing existing PlatformWindow with key '{}'. If intentional,"
			" destroy the existing PlatformWindow explicitly first!", new_key.data);
		internal::s_platform_window_registry.erase(new_key);
	}

	// check if we exceeded the limit, return an inert object if so.
	if (registry().size() >= PlatformWindow::maximum_allowed) {
		static PlatformWindow::Handle s_inert_handle(0, ShortKey(), nullptr, 0, 0, 0, nullptr);

		blog.error("The maximum allowed number of PlatformWindow objects was exceeded! "
			"The returned PlatformWindow with key '{}' is inert, thus unregistered and "
			"ineligible for participating in any rendering/events whatsoever!", new_key);

		return s_inert_handle;
	}

	static unsigned int s_handle_count = 0;

	return internal::s_platform_window_registry.try_emplace(
		new_key, ++s_handle_count, new_key, title, w, h,
		window_flags, rendering_driver_name).first->second;
}

PlatformWindow::Handle* PlatformWindow::find(const char* key) noexcept {
	if (!key || key[0] == '\0') { return get_main(); }

	auto it = registry().find(ShortKey(key));
	return it == registry().end() ? nullptr : &*it->second;
}

PlatformWindow::Handle* PlatformWindow::exists(Handle* handle) noexcept {
	if (!handle) { return nullptr; }
	for (auto& handle_entry : registry()) {
		if (handle == &handle_entry.second) { return handle; }
	}
	return nullptr;
}

/*==================================================================*/

void PlatformWindow::destroy(Handle& handle) noexcept {
	internal::s_platform_window_registry.erase(handle.handle_key);
}

void PlatformWindow::destroy(const char* key) noexcept {
	internal::s_platform_window_registry.erase(ShortKey(key));
}

/*==================================================================*/

bool PlatformWindow::set_main(const Handle& handle) noexcept {
	if (handle.is_inert() || !handle.get_window()) { return false; }
	internal::s_main_platform_window_ptr = &*handle;

	if (!get_sync()) { set_sync(*handle); }
	if (!get_live()) { set_live(*handle); }
	return true;
}

PlatformWindow::Handle* PlatformWindow::get_main() noexcept {
	return internal::s_main_platform_window_ptr;
}

bool PlatformWindow::set_sync(const Handle& handle) noexcept {
	if (handle.is_inert() || !handle.get_renderer()) { return false; }
	internal::s_sync_platform_window_ptr = &*handle;
	return true;
}

PlatformWindow::Handle* PlatformWindow::get_sync() noexcept {
	return internal::s_sync_platform_window_ptr;
}

bool PlatformWindow::set_live(const Handle& handle) noexcept {
	if (handle.is_inert() || !handle.is_ready()) { return false; }
	internal::s_live_platform_window_ptr = &*handle;
	return true;
}

PlatformWindow::Handle* PlatformWindow::get_live() noexcept {
	return internal::s_live_platform_window_ptr;
}

/*==================================================================*/

bool PlatformWindow::render_present() noexcept {
	auto* main_window = get_main();
	if (!main_window) { return false; }

	for (const auto& [key, window] : PlatformWindow::registry()) {
		if (&window == main_window) { continue; }
		(*window)->on_present();
	}

	// Explicitly present the main window last in order to
	// enforce vsync at the very end of the frame's workload
	(*main_window)->on_present();

	return true;
}

/*==================================================================*/
/*==================================================================*/

static_assert(GVB_SYSTEM_THEME_UNKNOWN == SDL_SYSTEM_THEME_UNKNOWN);
static_assert(GVB_SYSTEM_THEME_LIGHT   == SDL_SYSTEM_THEME_LIGHT  );
static_assert(GVB_SYSTEM_THEME_DARK    == SDL_SYSTEM_THEME_DARK   );

static_assert(GVB_ORIENTATION_UNKNOWN           == SDL_ORIENTATION_UNKNOWN          );
static_assert(GVB_ORIENTATION_LANDSCAPE         == SDL_ORIENTATION_LANDSCAPE        );
static_assert(GVB_ORIENTATION_LANDSCAPE_FLIPPED == SDL_ORIENTATION_LANDSCAPE_FLIPPED);
static_assert(GVB_ORIENTATION_PORTRAIT          == SDL_ORIENTATION_PORTRAIT         );
static_assert(GVB_ORIENTATION_PORTRAIT_FLIPPED  == SDL_ORIENTATION_PORTRAIT_FLIPPED );

static_assert(GVB_WINDOW_FULLSCREEN          == SDL_WINDOW_FULLSCREEN         );
static_assert(GVB_WINDOW_OPENGL              == SDL_WINDOW_OPENGL             );
static_assert(GVB_WINDOW_OCCLUDED            == SDL_WINDOW_OCCLUDED           );
static_assert(GVB_WINDOW_HIDDEN              == SDL_WINDOW_HIDDEN             );
static_assert(GVB_WINDOW_BORDERLESS          == SDL_WINDOW_BORDERLESS         );
static_assert(GVB_WINDOW_RESIZABLE           == SDL_WINDOW_RESIZABLE          );
static_assert(GVB_WINDOW_MINIMIZED           == SDL_WINDOW_MINIMIZED          );
static_assert(GVB_WINDOW_MAXIMIZED           == SDL_WINDOW_MAXIMIZED          );
static_assert(GVB_WINDOW_MOUSE_GRABBED       == SDL_WINDOW_MOUSE_GRABBED      );
static_assert(GVB_WINDOW_INPUT_FOCUS         == SDL_WINDOW_INPUT_FOCUS        );
static_assert(GVB_WINDOW_MOUSE_FOCUS         == SDL_WINDOW_MOUSE_FOCUS        );
static_assert(GVB_WINDOW_EXTERNAL            == SDL_WINDOW_EXTERNAL           );
static_assert(GVB_WINDOW_MODAL               == SDL_WINDOW_MODAL              );
static_assert(GVB_WINDOW_HIGH_PIXEL_DENSITY  == SDL_WINDOW_HIGH_PIXEL_DENSITY );
static_assert(GVB_WINDOW_MOUSE_CAPTURE       == SDL_WINDOW_MOUSE_CAPTURE      );
static_assert(GVB_WINDOW_MOUSE_RELATIVE_MODE == SDL_WINDOW_MOUSE_RELATIVE_MODE);
static_assert(GVB_WINDOW_ALWAYS_ON_TOP       == SDL_WINDOW_ALWAYS_ON_TOP      );
static_assert(GVB_WINDOW_UTILITY             == SDL_WINDOW_UTILITY            );
static_assert(GVB_WINDOW_TOOLTIP             == SDL_WINDOW_TOOLTIP            );
static_assert(GVB_WINDOW_POPUP_MENU          == SDL_WINDOW_POPUP_MENU         );
static_assert(GVB_WINDOW_KEYBOARD_GRABBED    == SDL_WINDOW_KEYBOARD_GRABBED   );
static_assert(GVB_WINDOW_FILL_DOCUMENT       == SDL_WINDOW_FILL_DOCUMENT      );
static_assert(GVB_WINDOW_VULKAN              == SDL_WINDOW_VULKAN             );
static_assert(GVB_WINDOW_METAL               == SDL_WINDOW_METAL              );
static_assert(GVB_WINDOW_TRANSPARENT         == SDL_WINDOW_TRANSPARENT        );
static_assert(GVB_WINDOW_NOT_FOCUSABLE       == SDL_WINDOW_NOT_FOCUSABLE      );

static_assert(GVB_FLASH_CANCEL        == SDL_FLASH_CANCEL       );
static_assert(GVB_FLASH_BRIEFLY       == SDL_FLASH_BRIEFLY      );
static_assert(GVB_FLASH_UNTIL_FOCUSED == SDL_FLASH_UNTIL_FOCUSED);

static_assert(GVB_PROGRESS_STATE_INVALID       == SDL_PROGRESS_STATE_INVALID      );
static_assert(GVB_PROGRESS_STATE_NONE          == SDL_PROGRESS_STATE_NONE         );
static_assert(GVB_PROGRESS_STATE_INDETERMINATE == SDL_PROGRESS_STATE_INDETERMINATE);
static_assert(GVB_PROGRESS_STATE_NORMAL        == SDL_PROGRESS_STATE_NORMAL       );
static_assert(GVB_PROGRESS_STATE_PAUSED        == SDL_PROGRESS_STATE_PAUSED       );
static_assert(GVB_PROGRESS_STATE_ERROR         == SDL_PROGRESS_STATE_ERROR        );

static_assert(GVB_SCALEMODE_INVALID  == SDL_SCALEMODE_INVALID );
static_assert(GVB_SCALEMODE_NEAREST  == SDL_SCALEMODE_NEAREST );
static_assert(GVB_SCALEMODE_LINEAR   == SDL_SCALEMODE_LINEAR  );
static_assert(GVB_SCALEMODE_PIXELART == SDL_SCALEMODE_PIXELART);
