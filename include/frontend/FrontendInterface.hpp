/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <map>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <utility>
#include <memory>
#include <functional>

#include "ImLabel.hpp"

/*==================================================================*/

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

template <typename Fn>
concept VoidInvocable = std::is_nothrow_invocable_r_v<void, Fn>;

template <typename Fn>
concept VoidInvocableID = std::is_nothrow_invocable_r_v<void, Fn, unsigned>;

/*==================================================================*/

class FrontendInterface {
public:
	using Func   = std::function<void()>;

	using LiveHook   = std::shared_ptr<Func>;

	using LabelKey = ImLabel;
	using OrderKey = std::pair<std::size_t, std::string>;

private:
	template <typename Signature>
	struct HookRegistry {
		using HookBuffer = std::vector<std::weak_ptr<Signature>>;

		HookBuffer buffer{};
		unsigned   offset{}; // used when merging, don't touch

		bool has_focus{}; // indicates focus status, don't touch
		bool first_hit{}; // set for a single frame only on focus

		// make Clang happy I guess.
		HookRegistry() = default;
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

	static inline std::unique_ptr
		<RegistryAggregate> s_gui_hooks = nullptr;

public:
	/**
	 * @brief Registers a nothrow callable to be invoked during the window rendering phase.
	 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the callable is unregistered automatically.
	 * Nesting is allowed, but lifetime of the nested hook is not extended automatically.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static LiveHook register_window(Fn&& fn) noexcept {
		auto shared_ptr = std::make_shared<Func>(std::forward<Fn>(fn));

		if (s_gui_hooks->windows.registry_lock.try_lock()) { // may fail spuriously (fine)
			s_gui_hooks->windows.registry.buffer.push_back(shared_ptr);
			s_gui_hooks->windows.registry_lock.unlock();
		} else {
			std::scoped_lock lock(s_gui_hooks->windows.overflow_lock); // must wait to acquire
			s_gui_hooks->windows.overflow.buffer.push_back(shared_ptr);
		}

		return shared_ptr;
	}

	/**
	 * @brief Registers a nothrow callable to be invoked during the main menu rendering phase.
	 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the callable is unregistered automatically.
	 * Nesting is allowed, but lifetime of the nested hook is not extended automatically.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static LiveHook register_menu(const LabelKey& window_tag, const OrderKey& menu_title, Fn&& fn) noexcept {
		auto shared_ptr = std::make_shared<Func>(std::forward<Fn>(fn));

		if (s_gui_hooks->menus.registry_lock.try_lock()) { // may fail spuriously (fine)
			s_gui_hooks->menus.registry[window_tag.get_id_or_label()] \
				[menu_title].buffer.push_back(shared_ptr);
			s_gui_hooks->menus.registry_lock.unlock();
		} else {
			std::scoped_lock lock(s_gui_hooks->menus.overflow_lock); // must wait to acquire
			s_gui_hooks->menus.overflow[window_tag.get_id_or_label()] \
				[menu_title].buffer.push_back(shared_ptr);
		}

		return shared_ptr;
	}

private:
	static inline HookRegistry<Func>* s_active_menu{};
public:
	static bool was_menu_clicked() noexcept {
		return s_active_menu && s_active_menu->first_hit;
	}

private:
	static bool merge_overflowing_windows() noexcept;
	static bool invoke_registered_windows() noexcept;

	static bool merge_overflowing_menus(const LabelKey& tag) noexcept;
	static bool invoke_registered_menus(const LabelKey& tag) noexcept;

public:
	static void init_context(std::string_view home_dir);
	static void quit_context();

	static void init_video(SDL_Window*, SDL_Renderer*);
	static void quit_video();

public:
	static SDL_Renderer* get_current_renderer()  noexcept;
	static unsigned      get_main_dockspace_id() noexcept;

public:
	static void  set_ui_zoom_scaling(float scale) noexcept;
	static float get_ui_zoom_scaling() noexcept;
	static void  set_ui_text_scaling(float scale) noexcept;
	static float get_ui_text_scaling() noexcept;

	static float get_ui_total_scaling() noexcept;

public:
	static void process_event(void* event);
	static void begin_new_frame();
	static void render_frame(SDL_Renderer* = nullptr);

	static void dock_next_window_to(unsigned id, bool first_time = false) noexcept;

	static void call_menubar(const char* window_name, bool* can_render = nullptr) noexcept;
	static void call_autohide_menubar(const char* window_name, bool& hidden) noexcept;
};
