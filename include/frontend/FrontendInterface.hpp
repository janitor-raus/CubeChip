/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "ImLabel.hpp"

#include <utility>
#include <memory>
#include <functional>

/*==================================================================*/

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

template <typename Fn>
concept VoidInvocable = std::is_nothrow_invocable_r_v<void, Fn>;

/*==================================================================*/

class FrontendInterface {
public:
	using Func = std::function<void()>;
	using Hook = std::shared_ptr<Func>;

	using LabelKey = ImLabel;
	using OrderKey = std::pair<std::size_t, std::string>;

public:
	/**
	 * @brief Registers a nothrow callable to be invoked during the window rendering phase.
	 * Returns a Hook (shared_func) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the callable is unregistered automatically.
	 * Nesting is allowed, but lifetime of the nested hook is not extended automatically.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static Hook register_window(Fn&& fn) noexcept {
		Hook shared_func = std::make_shared<Func>(std::forward<Fn>(fn));
		register_window_impl(shared_func);
		return shared_func;
	}

private:
	static void register_window_impl(const Hook& shared_func) noexcept;

public:
	/**
	 * @brief Registers a nothrow callable to be invoked during the main menu rendering phase.
	 * Returns a Hook (shared_ptr) that is used to manage lifetime of the registration.
	 * When the Hook is destroyed, the callable is unregistered automatically.
	 * Nesting is allowed, but lifetime of the nested hook is not extended automatically.
	 */
	template <VoidInvocable Fn> [[nodiscard]]
	static Hook register_menu(LabelKey window_tag, OrderKey menu_title, Fn&& fn) noexcept {
		Hook shared_func = std::make_shared<Func>(std::forward<Fn>(fn));
		register_menu_impl(std::move(window_tag), std::move(menu_title), shared_func);
		return shared_func;
	}

private:
	static void register_menu_impl(LabelKey window_tag, OrderKey menu_title, const Hook& shared_func) noexcept;

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

public:
	static bool was_menu_clicked() noexcept;

	static void call_menubar(const char* window_name, bool* can_render = nullptr) noexcept;
	static void call_autohide_menubar(const char* window_name, bool& hidden) noexcept;
};
