/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <memory>
#include <functional>

#include "ImLabel.hpp"

/*==================================================================*/

class WindowHost {
	struct HostContext;
	friend struct HostContext;

	std::unique_ptr<HostContext> m_context;

public:
	struct Callbacks {
		// Pre-Begin(), for setting window flags, pushing styles/colors, and other init needs.
		// Set the post-Begin() `&window_tidy` callable if needed, such as to pop styles.
		std::function<void(int& window_flags, std::function<void()>& window_tidy)>
			window_init{};

		// Mid-Begin(), after menubar/focus is handled where you can prepare a Dockspace.
		std::function<void(bool open, unsigned window_id)>
			window_dock{};

		// Mid-Begin(), after the Dockspace, intended for actual window contents.
		// The 'fullscreen' parameter signals whether the callback executes in a fullscreen context.
		std::function<void(bool open, bool fullscreen)>
			window_body{};

		// Post-End(), for finalizing logic or for additional rendering if needed.
		std::function<void(bool open)>
			window_post{};
	};

public:
	WindowHost(ImLabel name = {}, bool allow_fullscreen = false) noexcept;
	~WindowHost() noexcept;

	WindowHost(WindowHost&&) noexcept;
	WindowHost& operator=(WindowHost&&) noexcept;

public:
	// Wire output pointers for window state. Only call on main thread!
	void set_window_visible_output(bool* out) noexcept;
	// Wire output pointers for window state. Only call on main thread!
	void set_window_focused_output(bool* out) noexcept;

	auto get_window_label() const noexcept -> ImLabel;
	void set_window_label(std::string_view name) noexcept;

	auto get_window_id() const noexcept -> unsigned;
	bool is_fullscreen() const noexcept;

private:
	auto get_callbacks() noexcept -> std::shared_ptr<Callbacks>;
	void set_callbacks(std::shared_ptr<Callbacks> new_callbacks) noexcept;

public:
	/**
	 * @brief Edit the window's callbacks. A copy of the current callbacks is made and passed to the provided
	 *        function as a reference, allowing for partial callback updates to take place atomically.
	 *        Users are encouraged to take advantage of lambda captures (and be careful of expiring ref captures)
	 *        when editing the callbacks to benefit from function storage captures for additional state control.
	 * @tparam Fn A void-returning invocable type that takes a reference to a Callbacks object. Must not throw exceptions.
	 * @param fn The function to invoke with the copied callbacks for editing. The modified callbacks will be set atomically after the function returns.
	 */
	template <typename Fn>
		requires(std::is_nothrow_invocable_r_v<void, Fn, Callbacks&>)
	void edit_callbacks(Fn&& fn) noexcept {
		auto next = std::make_shared<Callbacks>(*get_callbacks());
		std::forward<Fn>(fn)(*next);
		set_callbacks(std::move(next));
	}
};
