/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <memory>
#include <functional>

#include "ImLabel.hpp"
#include "ImGuiStackGuard.hpp"

/*==================================================================*/

class WindowHost {
	struct HostContext;

	std::unique_ptr<HostContext>
		m_context;

public:
	struct Callbacks {
		using Pusher = ImGuiStackGuard::Pusher;
		using ImGuiWindowFlags = int;

		/**
		 * @brief Callback for window initialization, executed before ImGui::Begin().
		 *        Provides references to window_flags for modification, a 'pusher' object
		 *        to allow for easy color/style/font pushes with automatic popping, and a
		 *        'fullscreen' state to indicate whether the callback is currently invoked
		 *        in a fullscreen-window context.
		 */
		std::function<void(ImGuiWindowFlags& window_flags, Pusher& pusher, bool fullscreen)>
			window_init{};

		/**
		 * @brief Callback for window dockspace setup, executed after ImGui::Begin()
		 *        and before the window_body() callback. Provides the `window_open`
		 *        state and window's instance ID for docking decisions.
		 */
		std::function<void(bool window_open, unsigned int window_id)>
			window_dock{};

		/**
		 * @brief Callback for window body rendering, executed after ImGui::Begin()
		 *        and the window_dock() callback. Provides the `window_open` state and
		 *        the `fullscreen` state, which indicates whether the callback is
		 *        currently invoked in a fullscreen-window context.
		 */
		std::function<void(bool window_open, bool fullscreen)>
			window_body{};

		/**
		 * @brief Callback for toggling fullscreen mode, only runs when enabled via
		 *        constructor flag. Provides a reference to the `fullscreen` state
		 *        for modification as the user sees fit. If no callback is provided,
		 *        a default implementation is used that operates on double-click.
		 */
		std::function<void(bool& out_state)>
			toggle_fullscreen{};
	};

public:
	WindowHost(ImLabel name = {}) noexcept;
	~WindowHost() noexcept;

	WindowHost(WindowHost&&) noexcept;
	WindowHost& operator=(WindowHost&&) noexcept;

public:
	// Wire output pointer for window state. Only call on main thread!
	void set_window_visible_output(bool* out) noexcept;
	// Wire output pointer for window state. Only call on main thread!
	void set_window_focused_output(bool* out) noexcept;

	// Toggle fullscreen capability for this window. Only call on main thread!
	void allow_fullscreen(bool enable) noexcept;

	// Get current window label. Thread-safe.
	auto get_window_label() const noexcept -> ImLabel;
	// Set Current window label. Thread-safe.
	void set_window_label(std::string_view name) noexcept;

	auto get_parent() const noexcept -> const WindowHost*;
	void set_parent(const WindowHost* parent) noexcept;

	// Get unique window ID number. Only call on main thread!
	auto get_window_id() const noexcept -> unsigned int;
	// Get fullscreen state status. Only call on main thread!
	bool is_fullscreen() const noexcept;

	// Get reference to Callbacks struct for editing. Only call on main thread!
	auto edit_callbacks() noexcept -> Callbacks&;
};
