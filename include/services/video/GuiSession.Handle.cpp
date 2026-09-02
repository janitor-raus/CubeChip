/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

module;

#include "BasicLogger.hpp"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

module GuiSession;
#ifdef __INTELLISENSE__
# include "GuiSession.cppm"
#endif

/*==================================================================*/

GuiSession::Handle::Handle(
	CreatorKey&&,
	PlatformWindow::Handle& window_handle,
	RegistryAggregate& hooks_aggregate
) noexcept
	: m_window_handle(window_handle)
	, m_ctx(ImGui::CreateContext())
	, m_session_hooks(hooks_aggregate)
{
	init_context();
	handle()->set_link(this, LinkToken());
	if (handle().is_ready()) {
		notify_link(REBUILD_PHASE);
	}
}

GuiSession::Handle::~Handle() noexcept {
	notify_link(TEARDOWN_PHASE);

	if (m_ctx) {
		ImGui::DestroyContext(m_ctx);
		m_ctx = nullptr;
	}

	handle()->drop_linked(SEVER_LINK_ONLY);
}

/*==================================================================*/

void GuiSession::Handle::drop_linked(LinkAction action) noexcept {
	if (action == DESTROY_LINKED) {
		GuiSession::internal::s_gui_session_registry
			.erase(handle().handle_key);
	}
}

void GuiSession::Handle::notify_link(LinkNotify action) noexcept {
	ScopedGuiContext guard(*this);

	switch (action) {
		case TEARDOWN_PHASE:
			if (is_ready()) {
				ImGui_ImplSDLRenderer3_Shutdown();
				ImGui_ImplSDL3_Shutdown();
				m_live_renderer = false;
			}
			break;

		case REBUILD_PHASE:
			if (handle().is_ready() && !is_ready()) {
				ImGui_ImplSDL3_InitForSDLRenderer(*this, *this);
				ImGui_ImplSDLRenderer3_Init(*this);
				m_live_renderer = true;
			}
			break;
	}
}

void GuiSession::Handle::on_present() noexcept {
	if (!is_ready()) { return; }
	ScopedGuiContext guard(*this);

	update_style();

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	if (m_main_menubar && ImGui::BeginMainMenuBar()) {
		invoke_registered_menus("");
		ImGui::EndMainMenuBar();
	}

	m_main_dock_id = ImGui::DockSpaceOverViewport(
		ImGui::GetID("main_dock"), ImGui::GetMainViewport());

	invoke_registered_windows();

	ImGui::Render();
	ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), *this);
}

auto GuiSession::Handle::on_event(const SDL_Event& event, EventCallback) noexcept -> EventResult {
	if (is_ready()) {
		ScopedGuiContext guard(*this);
		ImGui_ImplSDL3_ProcessEvent(&event);
	}

	return EVENT_CONTINUE;
}

/*==================================================================*/

void GuiSession::Handle::register_window_impl(const Hook& shared_func) noexcept {
	if (m_session_hooks.windows.registry_lock.try_lock()) { // may fail spuriously (fine)
		m_session_hooks.windows.registry.buffer.push_back(shared_func);
		m_session_hooks.windows.registry_lock.unlock();
	} else {
		std::scoped_lock lock(m_session_hooks.windows.overflow_lock); // must wait to acquire
		m_session_hooks.windows.overflow.buffer.push_back(shared_func);
	}
}

void GuiSession::Handle::register_menu_impl(LabelKey window_tag, OrderKey menu_title, const Hook& shared_func) noexcept {
	if (m_session_hooks.menus.registry_lock.try_lock()) { // may fail spuriously (fine)
		m_session_hooks.menus.registry[window_tag.get_id_or_label()] \
			[std::move(menu_title)].buffer.push_back(shared_func);
		m_session_hooks.menus.registry_lock.unlock();
	} else {
		std::scoped_lock lock(m_session_hooks.menus.overflow_lock); // must wait to acquire
		m_session_hooks.menus.overflow[window_tag.get_id_or_label()] \
			[std::move(menu_title)].buffer.push_back(shared_func);
	}
}

/*==================================================================*/

bool GuiSession::Handle::merge_overflowing_windows() noexcept {
	std::scoped_lock lock(m_session_hooks.windows.overflow_lock);

	auto& src_windows = m_session_hooks.windows.overflow.buffer;
	if (src_windows.empty()) { return false; }

	auto& dst_windows = m_session_hooks.windows.registry.buffer;

	blog.debug("Merging {} late general callables.", src_windows.size());

	dst_windows.insert(dst_windows.end(),
		std::make_move_iterator(src_windows.begin()),
		std::make_move_iterator(src_windows.end())
	);
	src_windows.clear();

	return true;
}

bool GuiSession::Handle::invoke_registered_windows() noexcept {
	std::scoped_lock lock(m_session_hooks.windows.registry_lock);

	auto& windows = m_session_hooks.windows.registry;

	do {
		while (windows.offset < windows.buffer.size()) {
			if (auto shared_ptr = windows.buffer[windows.offset].lock()) {
				(*shared_ptr)(); ++windows.offset;
			} else {
				windows.buffer.erase(windows.buffer.begin() + windows.offset);
			}
		}
	} while (merge_overflowing_windows());

	return !!std::exchange(windows.offset, 0);
}

bool GuiSession::Handle::merge_overflowing_menus(const LabelKey& window_key) noexcept {
	std::scoped_lock lock(m_session_hooks.menus.overflow_lock);

	auto& src_window = m_session_hooks.menus.overflow
		[window_key.get_id_or_label()];
	if (src_window.empty()) { return false; }

	unsigned migration_count = 0;
	for (auto& [menu_key, src_hooks] : src_window) {
		if (src_hooks.buffer.empty()) { continue; }
		auto& dst_hooks = m_session_hooks.menus.registry
			[window_key.get_id_or_label()][menu_key];

		blog.debug("Merging {} late '{}' menu callables.",
			src_hooks.buffer.size(), menu_key.second.c_str());

		dst_hooks.buffer.insert(dst_hooks.buffer.end(),
			std::make_move_iterator(src_hooks.buffer.begin()),
			std::make_move_iterator(src_hooks.buffer.end())
		);
		src_hooks.buffer.clear();
		++migration_count;
	}

	return !!migration_count;
}

bool GuiSession::Handle::invoke_registered_menus(const LabelKey& window_key) noexcept {
	std::scoped_lock lock(m_session_hooks.menus.registry_lock);

	merge_overflowing_menus(window_key); // unconditional first merge
	auto it = m_session_hooks.menus.registry.find(window_key.get_id_or_label());
	if (it == m_session_hooks.menus.registry.end()) { return false; }

	const auto window_padding = ImGui::GetStyle().WindowPadding * 1.5f;
	// WindowPadding - style.ItemSpacing.x * 0.5f

	do {
		// iterate over all registered menu tabs for this window
		for (auto& [menu_key, hooks] : it->second) {
			if (hooks.buffer.empty()) { continue; }

			// clean-up pass before we enter BeginMenu tabs
			hooks.buffer.erase(std::remove_if(
				hooks.buffer.begin(), hooks.buffer.end(),
				[](auto& w) noexcept { return w.expired(); }
			), hooks.buffer.end());

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, window_padding);
			const bool menu_opened = ImGui::BeginMenu(menu_key.second.c_str());
			ImGui::PopStyleVar();

			if (menu_opened) {
				hooks.first_hit = !std::exchange(hooks.has_focus, true);
				UserInterface::s_active_menu = &hooks;

				// invoke all registered hooks for this menu tabs
				while (hooks.offset < hooks.buffer.size()) {
					// if the weak_ptr is expired, erase it, otherwise invoke it
					if (auto shared_ptr = hooks.buffer[hooks.offset].lock()) {
						(*shared_ptr)(); ++hooks.offset;
					} else {
						hooks.buffer.erase(hooks.buffer.begin() + hooks.offset);
					}
				}

				ImGui::EndMenu();
			} else {
				s_active_menu = nullptr;
				hooks.has_focus = false;
				continue;
			}
		}
	} while (merge_overflowing_menus(window_key));

	auto invoke_count = 0u;

	for (auto& [_, hooks] : it->second) {
		invoke_count += std::exchange(hooks.offset, 0); // reset for next frame
	}

	return invoke_count > 0;
}
