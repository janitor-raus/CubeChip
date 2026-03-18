/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "WindowHost.hpp"
#include "AtomSharedPtr.hpp"
#include "FrontendInterface.hpp"
#include "BasicLogger.hpp"

#include <imgui.h>

/*==================================================================*/

struct WindowHost::HostContext {
	using LiveHook   = FrontendInterface::LiveHook;
	using LiveHookID = FrontendInterface::LiveHookID;

	AtomSharedPtr<ImLabel> m_window_label;

	LiveHook   m_window_hook;
	LiveHookID m_docker_hook;

	bool* m_window_visible_out = nullptr;
	bool* m_window_focused_out = nullptr;

	AtomSharedPtr<Callbacks> m_callbacks;

	const unsigned c_dockspace_id;

	bool m_fullscreen_mode = false;
	bool m_docker_executed = false;
	bool m_display_menubar = false;

private:
	auto make_sanitized_label(ImLabel&& name) const noexcept {
		return std::make_shared<ImLabel>(!name->empty() ? std::move(name)
			: ImLabel("Unnamed Window", std::to_string(ImGui::GetID(this))));
	}

public:
	HostContext(ImLabel name) noexcept
		: m_window_label(make_sanitized_label(std::move(name)))
		, m_window_hook(FrontendInterface::register_window(
			[this]() noexcept { render_window(); }))
		, c_dockspace_id(ImGui::GetID(this))
		, m_callbacks(std::make_shared<Callbacks>())
	{}

	auto get_window_label() const noexcept {
		return m_window_label.load(std::memory_order::relaxed);
	}

	void set_window_label(ImLabel&& name) noexcept {
		m_window_label.store(make_sanitized_label(std::move(name)),
			std::memory_order::relaxed);
	}

private:
	void render_window() noexcept {
		const auto local = m_callbacks.load(std::memory_order::acquire);

		const auto window_label = get_window_label();

		int window_flags = m_display_menubar
			? ImGuiWindowFlags_MenuBar : 0;

		std::function<void()> window_tidy = nullptr;
		if (local->window_init) { local->window_init(window_flags, window_tidy); }

		const bool window_open = ImGui::Begin(*window_label,
			m_window_visible_out, ImGuiWindowFlags(window_flags));

		if (window_tidy) { window_tidy(); }

		if (window_open) {
			if (m_window_focused_out) {
				*m_window_focused_out |= ImGui::IsWindowFocused(
					ImGuiFocusedFlags_RootAndChildWindows);
			}
			FrontendInterface::call_menubar(*window_label, &m_display_menubar);
		}

		int docker_flags = ImGuiDockNodeFlags_AutoHideTabBar;
		if (local->window_prep) { local->window_prep(window_open, docker_flags); }

		ImGui::DockSpace(c_dockspace_id, ImVec2(), ImGuiDockNodeFlags(docker_flags));
		FrontendInterface::call_docker(c_dockspace_id, &m_docker_executed);

		if (local->window_body) { local->window_body(window_open); }

		ImGui::End();

		if (local->window_post) { local->window_post(window_open); }
	}
};

/*==================================================================*/

WindowHost::WindowHost(ImLabel name) noexcept
	: m_context(std::make_unique<HostContext>(std::move(name)))
{}

WindowHost::~WindowHost() noexcept = default;

WindowHost::WindowHost(WindowHost&&) noexcept = default;
WindowHost& WindowHost::operator=(WindowHost&&) noexcept = default;

/*==================================================================*/

auto WindowHost::get_dockspace_id() const noexcept -> unsigned {
	return m_context->c_dockspace_id;
}

bool WindowHost::is_fullscreened() const noexcept {
	return m_context->m_fullscreen_mode;
}

auto WindowHost::get_window_label() const noexcept -> ImLabel {
	return *m_context->get_window_label();
}

void WindowHost::set_window_label(std::string_view name) noexcept {
	m_context->set_window_label(name);
}

void WindowHost::set_window_visible_output(bool* out) noexcept {
	m_context->m_window_visible_out = out;
}

void WindowHost::set_window_focused_output(bool* out) noexcept {
	m_context->m_window_focused_out = out;
}

void WindowHost::set_layout_callable(DockerFn callable) noexcept {
	if (!callable) {
		m_context->m_docker_hook.reset();
		m_context->m_docker_executed = true; // prevents lookup
		return;
	}

	m_context->m_docker_hook = FrontendInterface::register_docker(m_context->c_dockspace_id,
		[docker_callable = std::move(callable)](auto id) noexcept { docker_callable(id); });
	m_context->m_docker_executed = false; // allow lookup on next frame
}

auto WindowHost::get_callbacks() noexcept -> std::shared_ptr<Callbacks> {
	return m_context->m_callbacks.load(std::memory_order::acquire);
}

void WindowHost::set_callbacks(std::shared_ptr<Callbacks> new_callbacks) noexcept {
	m_context->m_callbacks.store(std::move(new_callbacks), std::memory_order::release);
}
