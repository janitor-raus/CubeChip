/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <memory>
#include <mutex>
#include <functional>

#include "ImLabel.hpp"

/*==================================================================*/

class WindowHost {
	using CallableID = std::function<void(unsigned)>;

	struct HostContext;
	friend struct HostContext;

	std::unique_ptr<HostContext> m_context;

public:
	struct Callbacks {
		// Pre-Begin(), for setting window flags/style/etc, must return number of style pushes, if any.
		std::function<int(int& window_flags)> window_init{};
		// Mid-Begin(), before dockspace init and after menubar/focus is handled.
		std::function<void(bool open, int& docker_flags)> window_prep{};
		// Mid-Begin(), after dockspace is applied, intendedfor actual window contents.
		std::function<void(bool open)> window_body{};
		// Post-End(), for cleanup purposes or for additional rendering if needed.
		std::function<void(bool open)> window_quit{};
	};

public:
	WindowHost(ImLabel name = {}) noexcept;
	~WindowHost() noexcept;

	WindowHost(WindowHost&&) noexcept;
	WindowHost& operator=(WindowHost&&) noexcept;

public:
	auto get_dockspace_id() const noexcept -> unsigned;
	bool is_fullscreened() const noexcept;

	auto get_window_label() const noexcept -> ImLabel;
	void set_window_label(std::string_view name) noexcept;

	void set_window_visible_output(bool* out) noexcept;
	void set_window_focused_output(bool* out) noexcept;

	void set_layout_callable(CallableID callable) noexcept;

private:
	class LockedCallbacks {
		std::scoped_lock<std::mutex> lock;
		Callbacks& callbacks_reference;

	public:
		LockedCallbacks(std::mutex& mutex, Callbacks& callbacks) noexcept
			: lock(mutex), callbacks_reference(callbacks)
		{}

		constexpr Callbacks& operator*() noexcept { return callbacks_reference; }
	};

	auto get_callbacks() noexcept -> LockedCallbacks;

public:
	template <typename Fn>
		requires(std::is_nothrow_invocable_r_v<void, Fn, Callbacks&>)
	void edit_callbacks(Fn&& fn) noexcept {
		auto callbacks = get_callbacks();
		std::forward<Fn>(fn)(*callbacks);
	}
};
