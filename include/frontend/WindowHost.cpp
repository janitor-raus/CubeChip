/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "WindowHost.hpp"
#include "AtomSharedPtr.hpp"
#include "UserInterface.hpp"

#include <imgui.h>

/*==================================================================*/

static auto default_toggle_fullscreen_callback = [](bool& out_state) {
	const auto hover_flags = ImGuiHoveredFlags_NoPopupHierarchy
		| ImGuiHoveredFlags_RootAndChildWindows
		| ImGuiHoveredFlags_AllowWhenBlockedByActiveItem;

	if (ImGui::IsWindowHovered(hover_flags) && !ImGui::IsAnyItemHovered() &&
		ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
	) {
		out_state = !out_state;
	}
};

/*==================================================================*/

struct WindowHost::HostContext {
	Callbacks m_callbacks;

	bool* m_window_visible_out = nullptr;
	bool* m_window_focused_out = nullptr;

	const WindowHost* m_parent = nullptr;

	const unsigned int c_window_id;

	bool m_have_fullscreen = false;
	bool m_fullscreen_mode = false;
	bool m_disable_menubar = false;

	AtomSharedPtr<ImLabel> m_window_label;
	UserInterface::Hook    m_window_hook;

private:
	auto make_sanitized_label(ImLabel&& name) const noexcept {
		return std::make_shared<ImLabel>(!name->empty() ? std::move(name)
			: ImLabel("Unnamed Window", std::to_string(c_window_id)));
	}

public:
	HostContext(ImLabel&& name) noexcept
		: c_window_id(ImGui::GetID(this))
		, m_window_label(make_sanitized_label(std::move(name)))
		, m_window_hook(UserInterface::register_window(
			[this]() noexcept { render_host_window(); }))
	{}

	auto get_window_label() const noexcept {
		return m_window_label.load(std::memory_order::relaxed);
	}

	void set_window_label(ImLabel&& name) noexcept {
		m_window_label.store(make_sanitized_label(std::move(name)),
			std::memory_order::relaxed);
	}

private:
	void render_host_window() noexcept {
		if (m_window_visible_out && !*m_window_visible_out) { return; }

		const auto window_label = get_window_label();

		int window_flags = ImGuiWindowFlags_MenuBar
			* (m_fullscreen_mode && !m_disable_menubar);

		std::function<void()> window_tidy = nullptr;
		if (m_callbacks.window_init) { m_callbacks.window_init(window_flags, window_tidy); }

		const bool window_open = ImGui::Begin(*window_label,
			m_window_visible_out, ImGuiWindowFlags(window_flags));

		if (m_window_focused_out) {
			*m_window_focused_out |= ImGui::IsWindowFocused(
				ImGuiFocusedFlags_RootAndChildWindows);
		}

		if (window_tidy) { window_tidy(); }
		if (window_open && !m_fullscreen_mode) { UserInterface::call_menubar(*window_label); }

		if (m_callbacks.window_dock) { m_callbacks.window_dock(window_open, c_window_id); }
		if (m_callbacks.window_body) { m_callbacks.window_body(window_open, false); }

		if (window_open && m_have_fullscreen) {
			if (m_callbacks.toggle_fullscreen) {
				m_callbacks.toggle_fullscreen(m_fullscreen_mode);
			} else {
				default_toggle_fullscreen_callback(m_fullscreen_mode);
			}
		} else {
			m_fullscreen_mode = false;
		}

		ImGui::End();

		if (m_have_fullscreen && m_fullscreen_mode) {
			const auto* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);

			static constexpr auto full_flags =
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove;

			ImGui::PushID(c_window_id);
			if (ImGui::Begin("##fullscreen", nullptr, window_flags | full_flags)) {
				if (m_window_focused_out) { *m_window_focused_out |= true; }
				UserInterface::call_autohide_menubar(m_parent
					? m_parent->get_window_label()
					: *window_label, m_disable_menubar);

				if (m_callbacks.window_body) { m_callbacks.window_body(true, true); }

				if (m_callbacks.toggle_fullscreen) {
					m_callbacks.toggle_fullscreen(m_fullscreen_mode);
				} else {
					default_toggle_fullscreen_callback(m_fullscreen_mode);
				}
			}
			ImGui::End();
			ImGui::PopID();
		}
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

void WindowHost::set_window_visible_output(bool* out) noexcept {
	m_context->m_window_visible_out = out;
}

void WindowHost::set_window_focused_output(bool* out) noexcept {
	m_context->m_window_focused_out = out;
}

void WindowHost::allow_fullscreen(bool enable) noexcept {
	m_context->m_have_fullscreen = enable;
}

auto WindowHost::get_window_label() const noexcept -> ImLabel {
	return *m_context->get_window_label();
}

void WindowHost::set_window_label(std::string_view name) noexcept {
	m_context->set_window_label(name);
}

auto WindowHost::get_parent() const noexcept -> const WindowHost* {
	return m_context->m_parent;
}

void WindowHost::set_parent(const WindowHost* parent) noexcept {
	m_context->m_parent = parent;
}

auto WindowHost::get_window_id() const noexcept -> unsigned int {
	return m_context->c_window_id;
}

bool WindowHost::is_fullscreen() const noexcept {
	return m_context->m_fullscreen_mode;
}

auto WindowHost::edit_callbacks() noexcept -> Callbacks& {
	return m_context->m_callbacks;
}
