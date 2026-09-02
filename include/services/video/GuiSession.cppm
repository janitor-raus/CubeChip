/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

module;

#include "LifetimeWrapperSDL.hpp"
#include "WindowHost.hpp"
#include "ImLabel.hpp"

#include <map>
#include <utility>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>

export module GuiSession;
export import PlatformWindow;

#ifdef __INTELLISENSE__
# include "PlatformWindow.cppm"
#endif

/*==================================================================*/

struct ImGuiContext;
struct ImFontAtlas;

template <typename Fn>
concept VoidInvocable = std::is_nothrow_invocable_r_v<void, Fn>;

export struct UserInterface {
	using Func = std::function<void()>;
	using Hook = std::shared_ptr<Func>;

protected:
	using LabelKey = ImLabel;
	using OrderKey = std::pair<std::size_t, std::string>;

	template <typename Signature>
	struct HookRegistry {
		using HookBuffer = std::vector<std::weak_ptr<Signature>>;

		HookBuffer buffer{};
		unsigned   offset{}; // used when merging, don't touch

		bool has_focus{}; // indicates focus status, don't touch
		bool first_hit{}; // set for a single frame only on focus

		// make Clang happy I guess.
		HookRegistry() noexcept = default;
	};

	using HookRegistryMenuMap = std::unordered_map
		<LabelKey, std::map<OrderKey, HookRegistry<Func>>>;

	template <typename T>
	struct RegistryBox {
		T registry, overflow;

		std::mutex registry_lock;
		std::mutex overflow_lock;
	};

	struct RegistryAggregate {
		RegistryBox<HookRegistry<Func>>  windows{};
		RegistryBox<HookRegistryMenuMap> menus{};
	};

	using GuiWindowUserHooks = std::unordered_map<WindowNode::ShortKey,
		RegistryAggregate, WindowNode::ShortKey::Hash>;

	static inline GuiWindowUserHooks  s_gui_session_user_hooks;
	static inline HookRegistry<Func>* s_active_menu = nullptr;

public:
	/**
	 * @brief Registers a nothrow callable to be invoked during the window rendering phase.
	 * Returns a Hook (shared_func) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the callable is unregistered automatically.
	 * Nesting is allowed, but lifetime of the nested hook is not extended automatically.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static Hook register_window(Fn&& fn) noexcept;

	/**
	 * @brief Registers a nothrow callable to be invoked during the main menu rendering phase.
	 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the callable is unregistered automatically.
	 * Nesting is allowed, but lifetime of the nested hook is not extended automatically.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static Hook register_menu(LabelKey window_tag, OrderKey menu_title, Fn&& fn) noexcept;

	/**
	 * @brief Registers a nothrow callable to be invoked during the main menu rendering phase.
	 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the callable is unregistered automatically.
	 * Nesting is allowed, but lifetime of the nested hook is not extended automatically.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static Hook register_menu(const WindowHost& window, OrderKey menu_title, Fn&& fn) noexcept {
		return register_menu(window.get_window_label(),
			std::move(menu_title), std::forward<Fn>(fn));
	}

	static void  set_ui_zoom_scaling(float scale) noexcept;
	static float get_ui_zoom_scaling() noexcept;
	static void  set_ui_text_scaling(float scale) noexcept;
	static float get_ui_text_scaling() noexcept;
	static float get_ui_total_scaling() noexcept;

	static void toggle_borderless_view_mode() noexcept;
	static void set_borderless_view_mode(bool enabled) noexcept;
	static bool get_borderless_view_mode() noexcept;
	static const bool& get_borderless_view_mode_hook() noexcept;

	static unsigned get_live_dockspace_id() noexcept;

	static void dock_next_window_to(unsigned id, bool first_time = false) noexcept;

	static bool was_menu_clicked() noexcept;

	static void call_menubar(const char* window_name, bool* can_render = nullptr) noexcept;
	static void call_autohide_menubar(const char* window_name, bool& hidden) noexcept;
};

export namespace GuiSession {
	// IMPORTANT! Call before creating any GuiSession handles if you
	// want to save/load imgui settings. Does not work retroactively!
	// If you explicitly do not want storage, call and pass 'nullptr'!
	void set_file_path(const char* home_path) noexcept;

	class Handle : public WindowNode, private UserInterface {
		friend struct UserInterface;
		PlatformWindow::Handle& m_window_handle;
		ImGuiContext*           m_ctx = nullptr;
		RegistryAggregate&      m_session_hooks;

		unsigned    m_main_dock_id = 0;
		unsigned    m_style_generation = 0;
		bool        m_live_renderer = false;
		bool        m_main_menubar = true;
		std::string m_ini_file{};

		struct CreatorKey {};

		friend Handle& attach(PlatformWindow::Handle&) noexcept;

	public:
		explicit Handle(
			CreatorKey&&,
			PlatformWindow::Handle& window_handle,
			RegistryAggregate& hooks_aggregate
		) noexcept;
		~Handle() noexcept;

		Handle(const Handle&) = delete;
		Handle& operator=(const Handle&) = delete;
		Handle(Handle&&) = delete;
		Handle& operator=(Handle&&) = delete;

	private:
		void update_style() noexcept;

	protected:
		void drop_linked(LinkAction action) noexcept override;
		void notify_link(LinkNotify action) noexcept override;

		void on_present() noexcept override;
		auto on_event(const SDL_Event& event, EventCallback = nullptr)
			noexcept -> EventResult override;

	public:
		// Direct mutable access to linked PlatformWindow.
		/***/ PlatformWindow::Handle& handle() /***/ noexcept { return m_window_handle; }
		// Direct const access to linked PlatformWindow.
		const PlatformWindow::Handle& handle() const noexcept { return m_window_handle; }

	public:
		operator SDL_Window*()   const noexcept { return handle(); }
		operator SDL_Renderer*() const noexcept { return handle(); }
		operator ImGuiContext*() const noexcept { return m_ctx; }

		// 'GuiSession::registry()' is publicly exposed as const, preventing callers from
		// modifying the registry directly. Its contained objects are still mutable;
		// this const_cast is therefore intentional and safe for registry-owned objects.
		Handle& operator*() const noexcept { return const_cast<Handle&>(*this); }

		operator const PlatformWindow::Handle* () const noexcept { return &handle(); }
		operator /***/ PlatformWindow::Handle* () /***/ noexcept { return &handle(); }

		operator const PlatformWindow::Handle& () const noexcept { return handle(); }
		operator /***/ PlatformWindow::Handle& () /***/ noexcept { return handle(); }

		bool operator==(SDL_Window* window)   const noexcept { return handle() == window; }
		bool operator==(SDL_Renderer* renderer) const noexcept { return handle() == renderer; }

		friend bool operator==(SDL_Window* window, const Handle& app) noexcept { return app == window; }
		friend bool operator==(SDL_Renderer* renderer, const Handle& app) noexcept { return app == renderer; }

		bool is_ready() const noexcept { return m_ctx && m_live_renderer; }
		operator bool() const noexcept { return is_ready(); }

		auto get_dockspace_id() const noexcept { return m_main_dock_id; }
		void allow_main_menubar(bool enabled) noexcept { m_main_menubar = enabled; }

		// Direct access to the ImGui context this GuiSession owns. Caller is
		// responsible for calling ImGui::SetCurrentContext(...) before any
		// ImGui:: calls if working across multiple GuiSession instances.
		ImGuiContext* get_context() const noexcept { return m_ctx; }

	private:
		void init_context() noexcept;

		void register_window_impl(const Hook& shared_func) noexcept;
		void register_menu_impl(LabelKey window_tag, OrderKey menu_title, const Hook& shared_func) noexcept;

		bool merge_overflowing_windows() noexcept;
		bool invoke_registered_windows() noexcept;

		bool merge_overflowing_menus(const LabelKey& tag) noexcept;
		bool invoke_registered_menus(const LabelKey& tag) noexcept;

	public:
		/**
		 * @brief Registers a nothrow callable to be invoked during the window rendering phase.
		 * Returns a Hook (shared_func) that is used to manage lifetime of the registration.
		 * When the Hook is destroyed, the callable is unregistered automatically.
		 * Nesting is allowed, but lifetime of the nested hook is not extended automatically.
		 */
		template <VoidInvocable Fn> [[nodiscard]]
		Hook register_window(Fn&& fn) noexcept {
			Hook shared_func = std::make_shared<Func>(std::forward<Fn>(fn));
			register_window_impl(shared_func);
			return shared_func;
		}

		/**
		 * @brief Registers a nothrow callable to be invoked during the main menu rendering phase.
		 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
		 * When the Hook is destroyed, the callable is unregistered automatically.
		 * Nesting is allowed, but lifetime of the nested hook is not extended automatically.
		 */
		template <VoidInvocable Fn> [[nodiscard]]
		Hook register_menu(LabelKey window_tag, OrderKey menu_title, Fn&& fn) noexcept {
			Hook shared_func = std::make_shared<Func>(std::forward<Fn>(fn));
			register_menu_impl(std::move(window_tag), std::move(menu_title), shared_func);
			return shared_func;
		}
		/**
		 * @brief Registers a nothrow callable to be invoked during the main menu rendering phase.
		 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
		 * When the Hook is destroyed, the callable is unregistered automatically.
		 * Nesting is allowed, but lifetime of the nested hook is not extended automatically.
		 */
		template <VoidInvocable Fn> [[nodiscard]]
		Hook register_menu(const WindowHost& window, OrderKey menu_title, Fn&& fn) noexcept {
			return register_menu(window.get_window_label(), std::move(menu_title), std::forward<Fn>(fn));
		}
	};
}

/*==================================================================*/

namespace GuiSession {
	using ShortKey = WindowNode::ShortKey;
	using Registry = std::unordered_map<ShortKey,
		GuiSession::Handle, ShortKey::Hash>;

	namespace internal {
		// Registry of all GuiSession handles, keyed by their unique user-provided string.
		// Handles in this registry are not guaranteed to be "live" -- they may be
		// lacking a valid window/renderer pair or ImGui initialization.
		GuiSession::Registry s_gui_session_registry;
	}
}

export namespace GuiSession {
	// Returns a const view of the GuiSession registry.
	// Useful for iterating through and accessing state.
	// Use the * operator if you wish to modify items.
	const Registry& registry() noexcept;

	// Clear out the GuiSession registry, destroying all handles in the process.
	// This will not touch any AppWindows that do not have a GuiSession linked.
	void clear_registry() noexcept;

	// Create (and register) a new GuiSession from an existing PlatformWindow.
	// If the PlatformWindow already has a GuiSession owner, the existing GuiSession
	// is returned from the registry.
	[[nodiscard]] Handle& attach(PlatformWindow::Handle& window_handle) noexcept;

	// Search the registry for a GuiSession with a given key. If a match is
	// found, its pointer is returned. If the key is null/empty, the method will
	// return the GuiSession that was declared as "main", if one is available.
	Handle* find(const char* key = nullptr) noexcept;

	// Search the registry using a Guisession handle pointer. If it exists, the
	// same pointer will be returned, otherwise a 'nullptr' will be returned.
	Handle* exists(Handle* handle) noexcept;

	// De-register and destroy a given GuiSession.
	// Delegates to 'PlatformWindow::destroy()' for consistency.
	void destroy(Handle& handle) noexcept { PlatformWindow::destroy(handle); }

	// De-register and destroy a given GuiSession via key.
	// Delegates to 'PlatformWindow::destroy()' for consistency.
	void destroy(const char* key) noexcept { PlatformWindow::destroy(key); }

	Handle* get_main_handle() noexcept;
	Handle* get_sync_handle() noexcept;
	Handle* get_live_handle() noexcept;
}

/*==================================================================*/

export class ScopedGuiContext {
	ImGuiContext* m_prev;

public:
	explicit ScopedGuiContext(ImGuiContext* ctx) noexcept;
	~ScopedGuiContext() noexcept;

	ScopedGuiContext(const ScopedGuiContext&) = delete;
	ScopedGuiContext& operator=(const ScopedGuiContext&) = delete;
};

/*==================================================================*/

template<VoidInvocable Fn>
UserInterface::Hook UserInterface::register_window(Fn&& fn) noexcept {
	auto* live_session = GuiSession::get_live_handle();
	if (!live_session) { return Hook(); }
	return live_session->register_window(std::forward<Fn>(fn));
}

template<VoidInvocable Fn>
UserInterface::Hook UserInterface::register_menu(LabelKey window_tag, OrderKey menu_title, Fn&& fn) noexcept {
	auto* live_session = GuiSession::get_live_handle();
	if (!live_session) { return Hook(); }
	return live_session->register_menu(std::move(window_tag),
		std::move(menu_title), std::forward<Fn>(fn));
}
