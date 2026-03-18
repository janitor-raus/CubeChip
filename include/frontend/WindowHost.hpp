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
	using DockerFn = std::function<void(unsigned id)>;

	struct HostContext;
	friend struct HostContext;

	std::unique_ptr<HostContext> m_context;

public:
	struct Callbacks {
		// Pre-Begin(), for setting window flags, pushing styles/colors, and other init needs.
		// Set the post-Begin() `&window_tidy` callable if needed, such as to pop styles.
		std::function<void(int& window_flags, std::function<void()>& window_tidy)>
			window_init{};
		// Mid-Begin(), before dockspace init and after menubar/focus is handled.
		std::function<void(bool open, int& docker_flags)>
			window_prep{};
		// Mid-Begin(), after dockspace is applied, intendedfor actual window contents.
		std::function<void(bool open)>
			window_body{};
		// Post-End(), for finalizing logic or for additional rendering if needed.
		std::function<void(bool open)>
			window_post{};
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

	void set_layout_callable(DockerFn callable) noexcept;

private:
	auto get_callbacks() noexcept -> std::shared_ptr<Callbacks>;
	void set_callbacks(std::shared_ptr<Callbacks> new_callbacks) noexcept;

public:
	template <typename Fn>
		requires(std::is_nothrow_invocable_r_v<void, Fn, Callbacks&>)
	void edit_callbacks(Fn&& fn) noexcept {
		auto next = std::make_shared<Callbacks>(*get_callbacks());
		std::forward<Fn>(fn)(*next);
		set_callbacks(std::move(next));
	}
};
