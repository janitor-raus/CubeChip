/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

module;

#include "WindowNode.hpp"
#include "LifetimeWrapperSDL.hpp"

#include <forward_list>
#include <unordered_map>

#include "SettingWrapper.hpp"
#include "EzMaths.hpp"

export module PlatformWindow;

/*==================================================================*/

export enum GVB_SystemTheme {
	GVB_SYSTEM_THEME_UNKNOWN,   /**< Unknown system theme */
	GVB_SYSTEM_THEME_LIGHT,     /**< Light colored system theme */
	GVB_SYSTEM_THEME_DARK       /**< Dark colored system theme */
};

export enum GVB_DisplayOrientation {
	GVB_ORIENTATION_UNKNOWN,            /**< The display orientation can't be determined */
	GVB_ORIENTATION_LANDSCAPE,          /**< The display is in landscape mode, with the right side up, relative to portrait mode */
	GVB_ORIENTATION_LANDSCAPE_FLIPPED,  /**< The display is in landscape mode, with the left side up, relative to portrait mode */
	GVB_ORIENTATION_PORTRAIT,           /**< The display is in portrait mode */
	GVB_ORIENTATION_PORTRAIT_FLIPPED    /**< The display is in portrait mode, upside down */
};

/*==================================================================*/

export using GVB_WindowFlags = unsigned long long;

export enum GVB_WindowFlags_ : GVB_WindowFlags {
	GVB_WINDOW_DEFAULT              = 0x0000000000000000ull, /**< no flags set */
	GVB_WINDOW_FULLSCREEN           = 0x0000000000000001ull, /**< window is in fullscreen mode */
	GVB_WINDOW_OPENGL               = 0x0000000000000002ull, /**< window usable with OpenGL context */
	GVB_WINDOW_OCCLUDED             = 0x0000000000000004ull, /**< window is occluded */
	GVB_WINDOW_HIDDEN               = 0x0000000000000008ull, /**< window is neither mapped onto the desktop nor shown in the taskbar/dock/window list; SDL_ShowWindow() is required for it to become visible */
	GVB_WINDOW_BORDERLESS           = 0x0000000000000010ull, /**< no window decoration */
	GVB_WINDOW_RESIZABLE            = 0x0000000000000020ull, /**< window can be resized */
	GVB_WINDOW_MINIMIZED            = 0x0000000000000040ull, /**< window is minimized */
	GVB_WINDOW_MAXIMIZED            = 0x0000000000000080ull, /**< window is maximized */
	GVB_WINDOW_MOUSE_GRABBED        = 0x0000000000000100ull, /**< window has grabbed mouse input */
	GVB_WINDOW_INPUT_FOCUS          = 0x0000000000000200ull, /**< window has input focus */
	GVB_WINDOW_MOUSE_FOCUS          = 0x0000000000000400ull, /**< window has mouse focus */
	GVB_WINDOW_EXTERNAL             = 0x0000000000000800ull, /**< window not created by SDL */
	GVB_WINDOW_MODAL                = 0x0000000000001000ull, /**< window is modal */
	GVB_WINDOW_HIGH_PIXEL_DENSITY   = 0x0000000000002000ull, /**< window uses high pixel density back buffer if possible */
	GVB_WINDOW_MOUSE_CAPTURE        = 0x0000000000004000ull, /**< window has mouse captured (unrelated to MOUSE_GRABBED) */
	GVB_WINDOW_MOUSE_RELATIVE_MODE  = 0x0000000000008000ull, /**< window has relative mode enabled */
	GVB_WINDOW_ALWAYS_ON_TOP        = 0x0000000000010000ull, /**< window should always be above others */
	GVB_WINDOW_UTILITY              = 0x0000000000020000ull, /**< window should be treated as a utility window, not showing in the task bar and window list */
	GVB_WINDOW_TOOLTIP              = 0x0000000000040000ull, /**< window should be treated as a tooltip and does not get mouse or keyboard focus, requires a parent window */
	GVB_WINDOW_POPUP_MENU           = 0x0000000000080000ull, /**< window should be treated as a popup menu, requires a parent window */
	GVB_WINDOW_KEYBOARD_GRABBED     = 0x0000000000100000ull, /**< window has grabbed keyboard input */
	GVB_WINDOW_FILL_DOCUMENT        = 0x0000000000200000ull, /**< window is in fill-document mode (Emscripten only), since SDL 3.4.0 */
	GVB_WINDOW_VULKAN               = 0x0000000010000000ull, /**< window usable for Vulkan surface */
	GVB_WINDOW_METAL                = 0x0000000020000000ull, /**< window usable for Metal view */
	GVB_WINDOW_TRANSPARENT          = 0x0000000040000000ull, /**< window with transparent buffer */
	GVB_WINDOW_NOT_FOCUSABLE        = 0x0000000080000000ull, /**< window should not be focusable */
};

export enum GVB_FlashOperation {
	GVB_FLASH_CANCEL,        /**< Cancel any window flash state */
	GVB_FLASH_BRIEFLY,       /**< Flash the window briefly to get attention */
	GVB_FLASH_UNTIL_FOCUSED, /**< Flash the window until it gets focus */
};

export enum GVB_ProgressState {
	GVB_PROGRESS_STATE_INVALID = -1,  /**< An invalid progress state indicating an error; check SDL_GetError() */
	GVB_PROGRESS_STATE_NONE,          /**< No progress bar is shown */
	GVB_PROGRESS_STATE_INDETERMINATE, /**< The progress bar is shown in a indeterminate state */
	GVB_PROGRESS_STATE_NORMAL,        /**< The progress bar is shown in a normal state */
	GVB_PROGRESS_STATE_PAUSED,        /**< The progress bar is shown in a paused state */
	GVB_PROGRESS_STATE_ERROR,         /**< The progress bar is shown in a state indicating the application had an error */
};

export enum GVB_ScaleMode {
	GVB_SCALEMODE_INVALID = -1,
	GVB_SCALEMODE_NEAREST,  /**< nearest pixel sampling */
	GVB_SCALEMODE_LINEAR,   /**< linear filtering */
	GVB_SCALEMODE_PIXELART, /**< nearest pixel sampling with improved scaling for pixel art, available since SDL 3.4.0 */
};

/*==================================================================*/

export namespace PlatformWindow {
	constexpr inline auto maximum_allowed = 64ull;

	class Handle : public WindowNode {
		SDL_Unique<SDL_Window>   m_window_ptr;
		SDL_Unique<SDL_Renderer> m_renderer_ptr;

		WindowNode* m_owner_ptr = nullptr;

		std::forward_list<SDL_Shared<SDL_Texture>>
			m_texture_list;

		friend Handle& create(const char*, const char*,
			int, int, GVB_WindowFlags, const char*) noexcept;

	public:
		// Immutable after construction. User-created handles can never have an empty key.
		// An empty key is reserved exclusively for creation of inert handles.
		const ShortKey handle_key;
		const unsigned int handle_id;

		bool is_inert() const noexcept { return handle_key[0] == '\0'; }

		void internal_set_owner(WindowNode* owner) noexcept { m_owner_ptr = owner; }
		auto internal_get_owner() const noexcept { return m_owner_ptr; }

	protected:
		Handle(
			unsigned int handle_id, ShortKey key, const char* title, int w, int h,
			GVB_WindowFlags window_flags, const char* rendering_driver_name
		) noexcept;

	public:
		~Handle() noexcept;

		Handle(Handle&&) = delete;
		Handle& operator=(Handle&&) = delete;
		Handle(const Handle&) = delete;
		Handle& operator=(const Handle&) = delete;

	protected:
		void drop_linked(LinkAction action) noexcept override;
		void notify_link(LinkNotify action) noexcept override;

		void on_present() noexcept override;
		auto on_event(const SDL_Event& event, EventCallback callback)
			noexcept -> EventStatus override;

	public:
		auto get_window()   const noexcept { return m_window_ptr.get(); }
		auto get_renderer() const noexcept { return m_renderer_ptr.get(); }

		// 'PlatformWindow::registry()' is publicly exposed as const, preventing callers from
		// modifying the registry directly. Its contained objects are still mutable;
		// this const_cast is therefore intentional and safe for registry-owned objects.
		Handle& operator*() const noexcept { return const_cast<Handle&>(*this); }

		operator SDL_Window*()   const noexcept { return get_window(); }
		operator SDL_Renderer*() const noexcept { return get_renderer(); }

		bool operator==(SDL_Window* window)   const noexcept { return get_window() == window; }
		bool operator==(SDL_Renderer* renderer) const noexcept { return get_renderer() == renderer; }

		friend bool operator==(SDL_Window* window, const Handle& app) noexcept { return app == window; }
		friend bool operator==(SDL_Renderer* renderer, const Handle& app) noexcept { return app == renderer; }

		bool is_ready() const noexcept { return get_window() && get_renderer(); }
		operator bool() const noexcept { return is_ready(); }

	public:
		bool create_window(
			const char* title, int w, int h,
			GVB_WindowFlags window_flags = GVB_WINDOW_DEFAULT
		) noexcept;

		bool create_renderer(const char* rendering_driver_name = nullptr) noexcept;

	public:
		const char* get_title() const noexcept;
		bool set_title(const char* title) noexcept;

		bool set_position(int x, int y) noexcept;
		bool get_position(int* x, int* y) const noexcept;

		bool set_size(int w, int h) noexcept;
		bool get_size(int* w, int* h) const noexcept;

		bool set_min_size(int w, int h) noexcept;
		bool get_min_size(int* w, int* h) const noexcept;

		bool set_max_size(int w, int h) noexcept;
		bool get_max_size(int* w, int* h) const noexcept;

		bool set_aspect_ratio(float w, float h) noexcept;
		bool get_aspect_ratio(float* w, float* h) const noexcept;

		bool set_render_scale(float w, float h) noexcept;
		bool get_render_scale(float* w, float* h) const noexcept;

		bool set_bmp_icon(const char* icon_path) noexcept;
		bool set_parent(Handle* parent) noexcept;

	public:
		bool set_bordered(bool enabled) noexcept;
		bool set_resizable(bool enabled) noexcept;
		bool set_always_on_top(bool enabled) noexcept;
		bool set_fullscreen(bool enabled) noexcept;
		bool set_modal(bool enabled) noexcept;
		bool set_focusable(bool enabled) noexcept;
		bool set_fill_document(bool enabled) noexcept;
		bool set_keyboard_grab(bool enabled) noexcept;
		bool set_mouse_grab(bool enabled) noexcept;
		bool set_relative_mouse_mode(bool enabled) noexcept;

		bool get_bordered() const noexcept;
		bool get_resizable() const noexcept;
		bool get_always_on_top() const noexcept;
		bool get_fullscreen() const noexcept;
		bool get_modal() const noexcept;
		bool get_focusable() const noexcept;
		bool get_fill_document() const noexcept;
		bool get_keyboard_grab() const noexcept;
		bool get_mouse_grab() const noexcept;
		bool get_relative_mouse_mode() const noexcept;

		bool toggle_bordered() noexcept;
		bool toggle_resizable() noexcept;
		bool toggle_always_on_top() noexcept;
		bool toggle_fullscreen() noexcept;
		bool toggle_modal() noexcept;
		bool toggle_focusable() noexcept;
		bool toggle_fill_document() noexcept;
		bool toggle_keyboard_grab() noexcept;
		bool toggle_mouse_grab() noexcept;
		bool toggle_relative_mouse_mode() noexcept;

	public:
		bool set_progress_state(GVB_ProgressState state) noexcept;
		GVB_ProgressState get_progress_state() const noexcept;

		bool set_progress_value(float value) noexcept;
		float get_progress_value() const noexcept;

	public:
		bool minimize() noexcept;
		bool maximize() noexcept;

		bool is_minimized() const noexcept;
		bool is_maximized() const noexcept;

	public:
		const char* get_renderer_name() const noexcept;
		unsigned int get_display() const noexcept;
		float get_pixel_density() const noexcept;

	public:
		bool flash(GVB_FlashOperation operation) noexcept;
		bool restore() noexcept;
		bool raise() noexcept;
		bool sync() noexcept;
		bool hide() noexcept;
		bool show() noexcept;

		unsigned int get_id() const noexcept;
		bool has_persistent_geometry() const noexcept;

	public:
		template <typename... T>
			requires ((sizeof...(T) > 0) && (std::same_as<T, SDL_Texture*> && ...))
		void destroy_textures(T... textures) noexcept {
			m_texture_list.remove_if([&](const auto& entry) noexcept {
				return ((entry.get() == textures) || ...);
			});
		}

		[[nodiscard("The resulting texture handle will be lost if not stored!")]]
		SDL_Weak<SDL_Texture> create_stream_texture(
			int w, int h,
			bool transparency = false,
			bool linear_scaling = false
		) noexcept;

		[[nodiscard("The resulting texture handle will be lost if not stored!")]]
		SDL_Weak<SDL_Texture> create_target_texture(
			int w, int h,
			bool transparency = false,
			bool linear_scaling = false
		) noexcept;

		// Writes pixel data from src_buffer into a given Stream texture.
		void upload_stream_texture(
			const std::byte* src_buffer, SDL_Texture* texture, GVB_ScaleMode scale_mode = GVB_SCALEMODE_NEAREST
		) noexcept;

		// Renders Stream src_texture onto a given Target dst_texture.
		void render_whole_to_target(
			SDL_Texture* src_texture, SDL_Texture* dst_texture = nullptr
		) noexcept;

	public:
		struct Settings {
			static constexpr ez::Rect
				defaults = { 0, 0, 640, 480 };
			ez::Rect window = defaults;

			SettingsMap map(const Handle& handle) noexcept;
		};

	private:
		[[nodiscard]]
		auto export_settings() const noexcept -> Settings;
		void import_settings() noexcept;
	};
}

/*==================================================================*/

namespace PlatformWindow {
	using ShortKey = WindowNode::ShortKey;
	using Registry = std::unordered_map<ShortKey,
		PlatformWindow::Handle, ShortKey::Hash>;

	namespace internal {
		// Registry of all PlatformWindow handles, keyed by their unique user-provided string.
		// Handles in this registry are not guaranteed to be "live" -- they may be
		// lacking a valid window/renderer pair.
		inline PlatformWindow::Registry s_platform_window_registry;

		// Main platform window: If null, the applicaton is to be presumed to be
		// waiting for shutdown, unless the user explicitly set a different platform
		// window to be the "main" one, or the application runs in "headless" mode.
		inline PlatformWindow::Handle* s_main_platform_window_ptr = nullptr;

		// Sync platform window: This reflects the platform window that is used to
		// drive renderer vsync. Should the underlying window be destroyed, defaults
		// to the main platform window.
		inline PlatformWindow::Handle* s_sync_platform_window_ptr = nullptr;

		// Live platform window: This reflects the platform window that is targeted
		// by default for various windowing operations, unless another handle is
		// explicitly used for targeting. If null, automatic targeting will fail.
		inline PlatformWindow::Handle* s_live_platform_window_ptr = nullptr;

		void retarget_main(Handle* cur_handle, Handle* new_handle) noexcept;
		void retarget_sync(Handle* cur_handle, Handle* new_handle) noexcept;
		void retarget_live(Handle* cur_handle, Handle* new_handle) noexcept;
	}
}

/*==================================================================*/

export namespace PlatformWindow {
	// Returns a const view of the PlatformWindow registry.
	// Useful for iterating through and accessing state.
	// Use the * operator if you wish to modify items.
	const Registry& registry() noexcept;

	// Clear out the PlatformWindow registry, destroying all handles in the process.
	// Use when the application must exit, before SDL begins its teardown.
	void clear_registry() noexcept;

	// Create (and register) a new PlatformWindow handle from scratch.
	[[nodiscard]] Handle& create(
		const char* key, const char* title, int w, int h,
		GVB_WindowFlags window_flags = GVB_WINDOW_DEFAULT,
		const char* rendering_driver_name = nullptr
	) noexcept;

	// Search the registry for a PlatformWindow handle with a given key. If a match
	// is found, its pointer is returned. If the key is null/empty, the method will
	// return the PlatformWindow that was declared as "main", if one is available.
	Handle* find(const char* key = nullptr) noexcept;

	// Search the registry using a PlatformWindow handle pointer. If it exists, the
	// same pointer will be returned, otherwise a 'nullptr' will be returned.
	Handle* exists(Handle* handle) noexcept;

	// De-register and destroy a given PlatformWindow handle. If the handle is owned by
	// another object, it will be unlinked and signaled to be destroyed as well.
	void destroy(Handle& handle) noexcept;

	// De-register and destroy an PlatformWindow handle via key. If the handle is owned by
	// another object, it will be unlinked and signaled to be destroyed as well.
	void destroy(const char* key) noexcept;

	// Declare a given PlatformWindow handle as "main". If destroyed, the application
	// is expected to prepare for and proceed with termination.
	bool set_main(const Handle& handle) noexcept;
	Handle* get_main() noexcept;

	// Declare a given PlatformWindow handle as "sync". This is used to track
	// which platform window will be targeted to drive renderer vsync.
	bool set_sync(const Handle& handle) noexcept;
	Handle* get_sync() noexcept;

	// Declare a given PlatformWindow handle as "live". This is used to track
	// which platform window will be targeted by operations that don't explicitly
	// take a handle argument for targeting.
	bool set_live(const Handle& handle) noexcept;
	Handle* get_live() noexcept;

	bool render_present() noexcept;
}
