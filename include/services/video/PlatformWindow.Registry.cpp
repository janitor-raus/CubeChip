/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <cstdio>
#include <utility>
#include <SDL3/SDL_render.h>

#include "BasicLogger.hpp"
#include "PlatformWindow.hpp"

/*==================================================================*/

void PlatformWindow::render_present() noexcept {
	static std::vector<ShortKey> s_presented_keys(maximum_allowed);
	static const auto& mru = PWi::s_auto_sync_window_mru;
	if (mru.empty()) { return; }

	s_presented_keys.clear();
	std::size_t cached_mru_gen = 0;
	std::size_t restarts_count = 0;

	for (auto it = mru.rbegin(); it != mru.rend();) {
		auto* window = *it;

		auto it_find = std::find(s_presented_keys.begin(),
			s_presented_keys.end(), window->handle_key);

		if (it_find != s_presented_keys.end()) { ++it; continue; }
		cached_mru_gen = PWi::s_sync_window_mru_generation;

		s_presented_keys.push_back(window->handle_key);
		bool vsync = std::next(it) == mru.rend();

		// The final (vsync) window terminates this presentation pass.
		// Windows added after it will be presented on the next frame.
		(*window)->on_present();
		if (vsync) { break; }

		if (cached_mru_gen < PWi::s_sync_window_mru_generation) {
			if (++restarts_count >= 16) {
				blog.error("MRU permutations exceeded 16 iterations "
					"in a single render_present() call! This may be "
					"an infinite loop, and is thus aborted!");
				break;
			} else {
				// mru changed, restart the loop
				it = mru.rbegin();
			}
		} else { ++it; }
	}
}

const PlatformWindow::Registry& PlatformWindow::registry() noexcept {
	return internal::s_platform_window_registry;
}

void PlatformWindow::clear_registry() noexcept {
	internal::s_platform_window_registry.clear();
}

void PlatformWindow::internal::retarget_main(Handle* handle) noexcept {
	if (handle != PlatformWindow::get_main_handle()) { return; }
	s_main_window_ptr = nullptr;
}

void PlatformWindow::internal::retarget_sync(Handle* handle) noexcept {
	if (handle != PlatformWindow::get_sync_handle()) { return; }
	s_explicit_sync_mru_node = s_auto_sync_window_mru.end();
}

void PlatformWindow::internal::retarget_live(Handle* handle) noexcept {
	if (handle != PlatformWindow::get_live_handle()) { return; }
	s_live_window_ptr = nullptr;
}

PlatformWindow::SyncNode& PlatformWindow::internal::get_handle_node(Handle* handle) noexcept {
	return handle->m_sync_node;
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
	if (find_by_key(new_key)) {
		blog.warn("Replacing existing PlatformWindow with key '{}'. If intentional,"
			" destroy the existing PlatformWindow explicitly first!", new_key.data);
		internal::s_platform_window_registry.erase(new_key);
	}

	// check if we exceeded the limit, return an inert object if so.
	if (registry().size() >= PlatformWindow::maximum_allowed) {
		static auto s_inert_handle = Handle(Handle::CreatorKey(),
			ShortKey(), nullptr, 0, 0, 0, nullptr);

		blog.error("The maximum allowed number of PlatformWindow objects was exceeded! "
			"The returned PlatformWindow with key '{}' is inert, thus unregistered and "
			"ineligible for participating in any rendering/events whatsoever!", new_key.data);

		return s_inert_handle;
	}

	auto& new_handle = internal::s_platform_window_registry
		.try_emplace(new_key, Handle::CreatorKey(), new_key,
			title, w, h, window_flags, rendering_driver_name
		).first->second;

	set_live_handle(new_handle);
	return new_handle;
}

void PlatformWindow::destroy(Handle& handle) noexcept {
	internal::s_platform_window_registry.erase(handle.handle_key);
}

void PlatformWindow::destroy(const char* key) noexcept {
	internal::s_platform_window_registry.erase(ShortKey(key));
}

/*==================================================================*/

PlatformWindow::Handle* PlatformWindow::find_by_key(const ShortKey& key) noexcept {
	auto it = internal::s_platform_window_registry.find(key);
	return it == internal::s_platform_window_registry.end() ? nullptr : &it->second;
}

PlatformWindow::Handle* PlatformWindow::find_by_key(const char* key) noexcept {
	if (!key || key[0] == '\0') { return nullptr; }
	return find_by_key(ShortKey(key));
}

PlatformWindow::Handle* PlatformWindow::find_by_id(unsigned id) noexcept {
	auto it = internal::s_platform_window_id_map.find(id);
	return it == internal::s_platform_window_id_map.end()
		? nullptr : find_by_key(it->second);
}

PlatformWindow::Handle* PlatformWindow::exists(Handle* handle) noexcept {
	if (!handle) { return nullptr; }
	for (auto& handle_entry : registry()) {
		if (handle == &handle_entry.second) { return handle; }
	}
	return nullptr;
}

PlatformWindow::Handle* PlatformWindow::has_exec_context() noexcept {
	return internal::s_exec_context_ptr;
}

/*==================================================================*/

bool PlatformWindow::set_main_handle(const Handle& handle) noexcept {
	if (handle.is_inert() || !handle.get_window()) { return false; }
	internal::s_main_window_ptr = &*handle;
	return true;
}

void PlatformWindow::clear_main_handle() noexcept {
	internal::s_main_window_ptr = nullptr;
}

PlatformWindow::Handle* PlatformWindow::get_main_handle() noexcept {
	return internal::s_main_window_ptr;
}

bool PlatformWindow::set_sync_handle(const Handle& handle) noexcept {
	if (handle.is_inert() || !handle.get_renderer()) { return false; }
	internal::s_explicit_sync_mru_node = internal::get_handle_node(&*handle);
	internal::insert_sync_to_mru(&*handle);
	return true;
}

void PlatformWindow::clear_sync_handle() noexcept {
	internal::s_explicit_sync_mru_node = internal::s_auto_sync_window_mru.end();
}

PlatformWindow::Handle* PlatformWindow::get_sync_handle() noexcept {
	return internal::s_auto_sync_window_mru.size() > 0
		? internal::s_auto_sync_window_mru.front() : nullptr;
}

bool PlatformWindow::set_live_handle(const Handle& handle) noexcept {
	if (handle.is_inert() || !handle.is_ready()) { return false; }
	internal::s_live_window_ptr = &*handle;
	return true;
}

void PlatformWindow::clear_live_handle() noexcept {
	internal::s_live_window_ptr = nullptr;
}

PlatformWindow::Handle* PlatformWindow::get_live_handle() noexcept {
	return internal::s_live_window_ptr;
}

/*==================================================================*/

void PlatformWindow::internal::insert_sync_to_mru(Handle* handle) noexcept {
	auto& window_sync_node = get_handle_node(handle);

	if (window_sync_node == s_auto_sync_window_mru.end()) {
		s_auto_sync_window_mru.push_front(handle);
		window_sync_node = s_auto_sync_window_mru.begin();
		++s_sync_window_mru_generation;
	} else if (window_sync_node != s_auto_sync_window_mru.begin()) {
		s_auto_sync_window_mru.splice(s_auto_sync_window_mru.begin(),
			s_auto_sync_window_mru, window_sync_node);
		++s_sync_window_mru_generation;
	}

	apply_explicit_sync_node();
}

void PlatformWindow::internal::erase_sync_from_mru(Handle* handle) noexcept {
	auto& window_sync_node = get_handle_node(handle);

	if (window_sync_node == s_auto_sync_window_mru.end()) {
		apply_explicit_sync_node(); return;
	} else {
		if (s_explicit_sync_mru_node == window_sync_node) {
			s_explicit_sync_mru_node = s_auto_sync_window_mru.end();
		}
		s_auto_sync_window_mru.erase(window_sync_node);
		window_sync_node = s_auto_sync_window_mru.end();

		++s_sync_window_mru_generation;
		apply_explicit_sync_node();
	}
}

void PlatformWindow::internal::apply_explicit_sync_node() noexcept {
	if (s_explicit_sync_mru_node == s_auto_sync_window_mru.begin()) { return; }
	if (s_explicit_sync_mru_node != s_auto_sync_window_mru.end()) {
		// if there's an explicit sync ptr, force to the front
		s_auto_sync_window_mru.splice(s_auto_sync_window_mru.begin(),
			s_auto_sync_window_mru, s_explicit_sync_mru_node);
		++s_sync_window_mru_generation;
	}
}

void PlatformWindow::internal::insert_id_to_map(Handle* handle) noexcept {
	s_platform_window_id_map[handle->get_id()] = handle->handle_key;
}

void PlatformWindow::internal::erase_id_from_map(Handle* handle) noexcept {
	s_platform_window_id_map.erase(handle->get_id());
}

/*==================================================================*/

static_assert(+GVB_SYSTEM_THEME_UNKNOWN == SDL_SYSTEM_THEME_UNKNOWN);
static_assert(+GVB_SYSTEM_THEME_LIGHT   == SDL_SYSTEM_THEME_LIGHT  );
static_assert(+GVB_SYSTEM_THEME_DARK    == SDL_SYSTEM_THEME_DARK   );

static_assert(+GVB_ORIENTATION_UNKNOWN           == SDL_ORIENTATION_UNKNOWN          );
static_assert(+GVB_ORIENTATION_LANDSCAPE         == SDL_ORIENTATION_LANDSCAPE        );
static_assert(+GVB_ORIENTATION_LANDSCAPE_FLIPPED == SDL_ORIENTATION_LANDSCAPE_FLIPPED);
static_assert(+GVB_ORIENTATION_PORTRAIT          == SDL_ORIENTATION_PORTRAIT         );
static_assert(+GVB_ORIENTATION_PORTRAIT_FLIPPED  == SDL_ORIENTATION_PORTRAIT_FLIPPED );

static_assert(+GVB_WINDOW_FULLSCREEN          == SDL_WINDOW_FULLSCREEN         );
static_assert(+GVB_WINDOW_OPENGL              == SDL_WINDOW_OPENGL             );
static_assert(+GVB_WINDOW_OCCLUDED            == SDL_WINDOW_OCCLUDED           );
static_assert(+GVB_WINDOW_HIDDEN              == SDL_WINDOW_HIDDEN             );
static_assert(+GVB_WINDOW_BORDERLESS          == SDL_WINDOW_BORDERLESS         );
static_assert(+GVB_WINDOW_RESIZABLE           == SDL_WINDOW_RESIZABLE          );
static_assert(+GVB_WINDOW_MINIMIZED           == SDL_WINDOW_MINIMIZED          );
static_assert(+GVB_WINDOW_MAXIMIZED           == SDL_WINDOW_MAXIMIZED          );
static_assert(+GVB_WINDOW_MOUSE_GRABBED       == SDL_WINDOW_MOUSE_GRABBED      );
static_assert(+GVB_WINDOW_INPUT_FOCUS         == SDL_WINDOW_INPUT_FOCUS        );
static_assert(+GVB_WINDOW_MOUSE_FOCUS         == SDL_WINDOW_MOUSE_FOCUS        );
static_assert(+GVB_WINDOW_EXTERNAL            == SDL_WINDOW_EXTERNAL           );
static_assert(+GVB_WINDOW_MODAL               == SDL_WINDOW_MODAL              );
static_assert(+GVB_WINDOW_HIGH_PIXEL_DENSITY  == SDL_WINDOW_HIGH_PIXEL_DENSITY );
static_assert(+GVB_WINDOW_MOUSE_CAPTURE       == SDL_WINDOW_MOUSE_CAPTURE      );
static_assert(+GVB_WINDOW_MOUSE_RELATIVE_MODE == SDL_WINDOW_MOUSE_RELATIVE_MODE);
static_assert(+GVB_WINDOW_ALWAYS_ON_TOP       == SDL_WINDOW_ALWAYS_ON_TOP      );
static_assert(+GVB_WINDOW_UTILITY             == SDL_WINDOW_UTILITY            );
static_assert(+GVB_WINDOW_TOOLTIP             == SDL_WINDOW_TOOLTIP            );
static_assert(+GVB_WINDOW_POPUP_MENU          == SDL_WINDOW_POPUP_MENU         );
static_assert(+GVB_WINDOW_KEYBOARD_GRABBED    == SDL_WINDOW_KEYBOARD_GRABBED   );
static_assert(+GVB_WINDOW_FILL_DOCUMENT       == SDL_WINDOW_FILL_DOCUMENT      );
static_assert(+GVB_WINDOW_VULKAN              == SDL_WINDOW_VULKAN             );
static_assert(+GVB_WINDOW_METAL               == SDL_WINDOW_METAL              );
static_assert(+GVB_WINDOW_TRANSPARENT         == SDL_WINDOW_TRANSPARENT        );
static_assert(+GVB_WINDOW_NOT_FOCUSABLE       == SDL_WINDOW_NOT_FOCUSABLE      );

static_assert(+GVB_FLASH_CANCEL        == SDL_FLASH_CANCEL       );
static_assert(+GVB_FLASH_BRIEFLY       == SDL_FLASH_BRIEFLY      );
static_assert(+GVB_FLASH_UNTIL_FOCUSED == SDL_FLASH_UNTIL_FOCUSED);

static_assert(+GVB_PROGRESS_STATE_INVALID       == SDL_PROGRESS_STATE_INVALID      );
static_assert(+GVB_PROGRESS_STATE_NONE          == SDL_PROGRESS_STATE_NONE         );
static_assert(+GVB_PROGRESS_STATE_INDETERMINATE == SDL_PROGRESS_STATE_INDETERMINATE);
static_assert(+GVB_PROGRESS_STATE_NORMAL        == SDL_PROGRESS_STATE_NORMAL       );
static_assert(+GVB_PROGRESS_STATE_PAUSED        == SDL_PROGRESS_STATE_PAUSED       );
static_assert(+GVB_PROGRESS_STATE_ERROR         == SDL_PROGRESS_STATE_ERROR        );

static_assert(+GVB_SCALEMODE_INVALID  == SDL_SCALEMODE_INVALID );
static_assert(+GVB_SCALEMODE_NEAREST  == SDL_SCALEMODE_NEAREST );
static_assert(+GVB_SCALEMODE_LINEAR   == SDL_SCALEMODE_LINEAR  );
static_assert(+GVB_SCALEMODE_PIXELART == SDL_SCALEMODE_PIXELART);
