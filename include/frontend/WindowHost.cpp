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

	AtomSharedPtr<ImLabel> m_window_label;

	LiveHook   m_window_hook;

	bool* m_window_visible_out = nullptr;
	bool* m_window_focused_out = nullptr;

	AtomSharedPtr<Callbacks> m_callbacks;

	const unsigned c_window_id;

	bool m_have_fullscreen = false;
	bool m_fullscreen_mode = false;
	bool m_disable_menubar = false;

private:
	auto make_sanitized_label(ImLabel&& name) const noexcept {
		return std::make_shared<ImLabel>(!name->empty() ? std::move(name)
			: ImLabel("Unnamed Window", std::to_string(ImGui::GetID(this))));
	}

public:
	HostContext(ImLabel name, bool allow_fullscreen) noexcept
		: m_window_label(make_sanitized_label(std::move(name)))
		, m_window_hook(FrontendInterface::register_window(
			[this]() noexcept { render_window(); }))
		, m_callbacks(std::make_shared<Callbacks>())
		, c_window_id(ImGui::GetID(this))
		, m_have_fullscreen(allow_fullscreen)
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
		using FI = FrontendInterface;

		// XXX - this won't work currently with docked windows, and also it should be flexible,
		//       meaning we should allow the user to define the behavior and possibly trigger point
		static constexpr auto toggle_fullscreen_mode = [](bool allowed, bool& state) noexcept {
			if (!allowed) { return; }
			const auto hover_flags = ImGuiHoveredFlags_NoPopupHierarchy
								   | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem;

			if (ImGui::IsWindowHovered(hover_flags) && !ImGui::IsAnyItemHovered() &&
				ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
			) { state = !state; }
		};

		const auto callbacks    = m_callbacks.load(std::memory_order::acquire);
		const auto window_label = get_window_label();

		int window_flags = (m_fullscreen_mode && !m_disable_menubar)
			? ImGuiWindowFlags_MenuBar : ImGuiWindowFlags_None;

		std::function<void()> window_tidy = nullptr;
		if (callbacks->window_init) { callbacks->window_init(window_flags, window_tidy); }

		const bool window_open = ImGui::Begin(*window_label,
			m_window_visible_out, ImGuiWindowFlags(window_flags));

		if (m_window_focused_out) {
			*m_window_focused_out |= ImGui::IsWindowFocused
				(ImGuiFocusedFlags_RootAndChildWindows);
		}

		if (window_tidy) { window_tidy(); }
		if (window_open && !m_fullscreen_mode) { FI::call_menubar(*window_label); }

		if (callbacks->window_dock) { callbacks->window_dock(window_open, c_window_id); }
		if (callbacks->window_body) { callbacks->window_body(window_open, false); }
		if (window_open) { toggle_fullscreen_mode(m_have_fullscreen, m_fullscreen_mode); }
		else { m_fullscreen_mode = false; }

		ImGui::End();

		if (m_have_fullscreen && m_fullscreen_mode) {
			const auto* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);

			static constexpr auto full_flags = ImGuiWindowFlags_NoScrollWithMouse
				| ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove;

			const auto temp_label = ::join(window_label->get_id(), "_fullscreen");
			if (ImGui::Begin(temp_label.c_str(), nullptr, window_flags | full_flags)) {
				if (m_window_focused_out) { *m_window_focused_out |= true; }
				FI::call_autohide_menubar(*window_label, m_disable_menubar);
				if (callbacks->window_body) { callbacks->window_body(true, true); }
				toggle_fullscreen_mode(m_have_fullscreen, m_fullscreen_mode);
			}
			ImGui::End();
		}

		if (callbacks->window_post) { callbacks->window_post(window_open); }
	}
};

/*==================================================================*/

WindowHost::WindowHost(ImLabel name, bool allow_fullscreen) noexcept
	: m_context(std::make_unique<HostContext>(std::move(name), allow_fullscreen))
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

auto WindowHost::get_window_label() const noexcept -> ImLabel {
	return *m_context->get_window_label();
}

void WindowHost::set_window_label(std::string_view name) noexcept {
	m_context->set_window_label(name);
}

auto WindowHost::get_window_id() const noexcept -> unsigned {
	return m_context->c_window_id;
}

bool WindowHost::is_fullscreen() const noexcept {
	return m_context->m_fullscreen_mode;
}

/*==================================================================*/

auto WindowHost::get_callbacks() noexcept -> std::shared_ptr<Callbacks> {
	return m_context->m_callbacks.load(std::memory_order::acquire);
}

void WindowHost::set_callbacks(std::shared_ptr<Callbacks> new_callbacks) noexcept {
	m_context->m_callbacks.store(std::move(new_callbacks), std::memory_order::release);
}
