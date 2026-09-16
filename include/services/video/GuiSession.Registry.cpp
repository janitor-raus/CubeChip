/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <utility>
#include <imgui.h>

#include <SDL3/SDL_filesystem.h>

#include "fonts/RobotoMono.hpp"
#include "BasicLogger.hpp"
#include "StringJoin.hpp"
#include "GuiSession.hpp"

/*==================================================================*/

struct SharedContextProperties {
	ImGuiStyle       default_style{};
	ImGuiConfigFlags default_config_flags{};

	unsigned style_generation  = 0;
	bool  borderless_view_mode = false;
	float zoom_scaling = 1.0f;
	float text_scaling = 1.0f;

	const char* home_path = "";

private:
	void setup_default_theme() noexcept {
		default_style.AntiAliasedLinesUseTex = false;

		default_style.WindowPadding     = ImVec2(8.0f, 8.0f);
		default_style.FramePadding      = ImVec2(8.0f, 4.0f);
		default_style.ItemSpacing       = ImVec2(8.0f, 7.0f);
		default_style.ItemInnerSpacing  = ImVec2(8.0f, 2.0f);
		default_style.TouchExtraPadding = ImVec2(0.0f, 0.0f);

		default_style.IndentSpacing = 38.0f;
		default_style.GrabMinSize   = 8.0f;

		default_style.WindowBorderSize = 2.0f;
		default_style.ChildBorderSize  = 2.0f;
		default_style.PopupBorderSize  = 2.0f;
		default_style.FrameBorderSize  = 0.0f;

		default_style.WindowRounding = 4.0f;
		default_style.ChildRounding  = 4.0f;
		default_style.FrameRounding  = 4.0f;
		default_style.PopupRounding  = 4.0f;
		default_style.GrabRounding   = 2.0f;

		default_style.ScrollbarSize     = 10.0f;
		default_style.ScrollbarRounding = 2.0f;
		default_style.ScrollbarPadding  = 0.0f;

		default_style.TabBorderSize      = 0.0f;
		default_style.TabBarBorderSize   = 2.0f;
		default_style.TabBarOverlineSize = 3.0f;
		default_style.TabMinWidthBase    = 64.0f;
		default_style.TabMinWidthShrink  = 64.0f;
		default_style.TabCloseButtonMinWidthSelected   = -1.0f;
		default_style.TabCloseButtonMinWidthUnselected = -1.0f;
		default_style.TabRounding = 8.0f;

		default_style.WindowTitleAlign         = ImVec2(0.0f, 0.5f);
		default_style.WindowBorderHoverPadding = 4.0f;
		default_style.WindowMenuButtonPosition = ImGuiDir_Right;

		default_style.CellPadding                 = ImVec2(6.0f, 2.0f);
		default_style.TableAngledHeadersAngle     = 35.0f;
		default_style.TableAngledHeadersTextAlign = ImVec2(0.5f, 0.0f);

		default_style.TreeLinesFlags    = ImGuiTreeNodeFlags_DrawLinesToNodes;
		default_style.TreeLinesRounding = 16.0f;
		default_style.TreeLinesSize     = 2.0f;

		default_style.ColorMarkerSize     = 4.0f;
		default_style.ColorButtonPosition = ImGuiDir_Left;

		default_style.SeparatorTextBorderSize = 3.0f;
		default_style.SeparatorTextPadding    = ImVec2(24.0f, 4.0f);

		default_style.DockingNodeHasCloseButton = false;
		default_style.DockingSeparatorSize      = 3.0f;

		default_style.DisplaySafeAreaPadding = ImVec2(0.0f, 0.0f);

		auto* colors = default_style.Colors;
		colors[ImGuiCol_Text]                      = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
		colors[ImGuiCol_TextDisabled]              = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		colors[ImGuiCol_WindowBg]                  = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
		colors[ImGuiCol_ChildBg]                   = ImVec4(0.08f, 0.08f, 0.09f, 0.50f);
		colors[ImGuiCol_PopupBg]                   = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
		colors[ImGuiCol_Border]                    = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
		colors[ImGuiCol_BorderShadow]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg]                   = ImVec4(0.17f, 0.17f, 0.21f, 1.00f);
		colors[ImGuiCol_FrameBgHovered]            = ImVec4(0.77f, 0.50f, 1.00f, 0.25f);
		colors[ImGuiCol_FrameBgActive]             = ImVec4(0.77f, 0.50f, 1.00f, 0.36f);
		colors[ImGuiCol_TitleBg]                   = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
		colors[ImGuiCol_TitleBgActive]             = ImVec4(0.23f, 0.18f, 0.29f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed]          = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
		colors[ImGuiCol_MenuBarBg]                 = ImVec4(0.77f, 0.50f, 1.00f, 0.06f);
		colors[ImGuiCol_ScrollbarBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_ScrollbarGrab]             = ImVec4(0.77f, 0.50f, 1.00f, 0.38f);
		colors[ImGuiCol_ScrollbarGrabHovered]      = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
		colors[ImGuiCol_ScrollbarGrabActive]       = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
		colors[ImGuiCol_CheckMark]                 = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
		colors[ImGuiCol_CheckboxSelectedBg]        = colors[ImGuiCol_FrameBg];
		colors[ImGuiCol_SliderGrab]                = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
		colors[ImGuiCol_SliderGrabActive]          = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
		colors[ImGuiCol_Button]                    = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
		colors[ImGuiCol_ButtonHovered]             = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
		colors[ImGuiCol_ButtonActive]              = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
		colors[ImGuiCol_Header]                    = ImVec4(0.77f, 0.50f, 1.00f, 0.38f);
		colors[ImGuiCol_HeaderHovered]             = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
		colors[ImGuiCol_HeaderActive]              = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
		colors[ImGuiCol_Separator]                 = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
		colors[ImGuiCol_SeparatorHovered]          = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
		colors[ImGuiCol_SeparatorActive]           = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
		colors[ImGuiCol_ResizeGrip]                = ImVec4(0.77f, 0.50f, 1.00f, 0.00f);
		colors[ImGuiCol_ResizeGripHovered]         = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
		colors[ImGuiCol_ResizeGripActive]          = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
		colors[ImGuiCol_InputTextCursor]           = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_TabHovered]                = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
		colors[ImGuiCol_Tab]                       = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
		colors[ImGuiCol_TabSelected]               = ImVec4(0.77f, 0.50f, 1.00f, 0.50f);
		colors[ImGuiCol_TabSelectedOverline]       = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
		colors[ImGuiCol_TabDimmed]                 = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
		colors[ImGuiCol_TabDimmedSelected]         = ImVec4(0.77f, 0.50f, 1.00f, 0.25f);
		colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.77f, 0.50f, 1.00f, 0.38f);
		colors[ImGuiCol_DockingPreview]            = ImVec4(0.77f, 0.50f, 1.00f, 0.38f);
		colors[ImGuiCol_DockingEmptyBg]            = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
		colors[ImGuiCol_PlotLines]                 = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
		colors[ImGuiCol_PlotLinesHovered]          = ImVec4(1.00f, 0.43f, 0.35f, 0.75f);
		colors[ImGuiCol_PlotHistogram]             = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
		colors[ImGuiCol_PlotHistogramHovered]      = ImVec4(1.00f, 0.60f, 0.00f, 0.75f);
		colors[ImGuiCol_TableHeaderBg]             = ImVec4(0.77f, 0.50f, 1.00f, 0.19f);
		colors[ImGuiCol_TableBorderStrong]         = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
		colors[ImGuiCol_TableBorderLight]          = ImVec4(0.21f, 0.21f, 0.25f, 1.00f);
		colors[ImGuiCol_TableRowBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt]             = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
		colors[ImGuiCol_TextLink]                  = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_TextSelectedBg]            = ImVec4(0.77f, 0.50f, 1.00f, 0.19f);
		colors[ImGuiCol_TreeLines]                 = ImVec4(0.31f, 0.31f, 0.38f, 1.00f);
		colors[ImGuiCol_DragDropTarget]            = ImVec4(0.77f, 0.50f, 1.00f, 1.00f);
		colors[ImGuiCol_DragDropTargetBg]          = ImVec4(0.77f, 0.50f, 1.00f, 0.13f);
		colors[ImGuiCol_UnsavedMarker]             = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_NavCursor]                 = ImVec4(0.77f, 0.50f, 1.00f, 0.75f);
		colors[ImGuiCol_NavWindowingHighlight]     = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg]         = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.65f);
	}

public:
	SharedContextProperties() noexcept {
		IMGUI_CHECKVERSION();

		default_config_flags = ImGuiConfigFlags_DockingEnable
			| ImGuiConfigFlags_NavEnableGamepad
			| ImGuiConfigFlags_NavEnableKeyboard;

		// XXX we should perform font merging here if we want to support icons in the
		//     future, but for now we just have a single main font which is saaaaaad.

		setup_default_theme();
	}
};

static SharedContextProperties s_ctx_props;

/*==================================================================*/

const GuiSession::Registry& GuiSession::registry() noexcept {
	return internal::s_gui_session_registry;
}

void GuiSession::clear_registry() noexcept {
	internal::s_gui_session_registry.clear();
}

GuiSession::Handle& GuiSession::attach(PlatformWindow::Handle& window_handle) noexcept {
	// return existing GuiSession::Handle if given PlatformWindow::Handle is owned
	if (auto* owner_ptr = window_handle->get_link()) {
		return *static_cast<Handle*>(owner_ptr);
	}

	// erase existing GuiSession::Handle if present, to replace it
	auto cur_key = window_handle.handle_key;
	internal::s_gui_session_registry.erase(cur_key);

	// produce warning if given handle belongs to an inert object
	if (window_handle.is_inert()) {
		blog.warn("The PlatformWindow chosen for attachment with key '{}' is "
			"inert and thus unregistered and ineligible for participating in "
			"any rendering/events whatsoever!", cur_key.data);
	}

	// produce storage path warning if file path is not explicitly set
	if (s_ctx_props.home_path && s_ctx_props.home_path[0] == '\0') {
		blog.warn("A GuiSession was attached to PlatformWindow with key '{}' "
			"without first calling 'GuiSession::set_file_path()'! Call with a "
			"'nullptr' parameter to disable storage, or provide a valid path!", cur_key.data);
	}

	// emplace a new RegistryAggregate if missing, otherwise reuse existing
	auto& aggregate = UserInterface::s_gui_session_user_hooks
		.try_emplace(cur_key).first->second;

	return internal::s_gui_session_registry.try_emplace(cur_key,
		Handle::CreatorKey(), window_handle, aggregate).first->second;
}

GuiSession::Handle* GuiSession::find(const char* key) noexcept {
	if (!key || key[0] == '\0') { return get_main_handle(); }

	auto it = registry().find(ShortKey(key));
	return it == registry().end() ? nullptr : &*it->second;
}

GuiSession::Handle* GuiSession::exists(Handle* handle) noexcept {
	if (!handle) { return nullptr; }
	for (auto& handle_entry : registry()) {
		if (handle == &handle_entry.second) { return handle; }
	}
	return nullptr;
}

GuiSession::Handle* GuiSession::get_main_handle() noexcept {
	auto* main_handle = PlatformWindow::get_main_handle();
	if (!main_handle) { return nullptr; }

	auto* owner_ptr = main_handle->get_link();
	if (!owner_ptr) { return nullptr; }

	return static_cast<Handle*>(owner_ptr);
}

GuiSession::Handle* GuiSession::get_sync_handle() noexcept {
	auto* live_handle = PlatformWindow::get_sync_handle();
	if (!live_handle) { return nullptr; }

	auto* owner_ptr = live_handle->get_link();
	if (!owner_ptr) { return nullptr; }

	return static_cast<Handle*>(owner_ptr);
}

GuiSession::Handle* GuiSession::get_live_handle() noexcept {
	auto* live_handle = PlatformWindow::get_live_handle();
	if (!live_handle) { return nullptr; }

	auto* owner_ptr = live_handle->get_link();
	if (!owner_ptr) { return nullptr; }

	return static_cast<Handle*>(owner_ptr);
}

/*==================================================================*/

ScopedGuiContext::ScopedGuiContext(ImGuiContext* ctx) noexcept
	: m_prev(ImGui::GetCurrentContext())
{
	ImGui::SetCurrentContext(ctx);
}

ScopedGuiContext::~ScopedGuiContext() noexcept {
	ImGui::SetCurrentContext(m_prev);
}

/*==================================================================*/

void GuiSession::Handle::init_context() noexcept {
	ScopedGuiContext guard(*this);
	auto& io = ImGui::GetIO();

	io.ConfigDpiScaleFonts     = true;
	io.ConfigDpiScaleViewports = true;
	io.ConfigFlags = s_ctx_props.default_config_flags;

	io.FontDefault = io.Fonts->AddFontFromMemoryCompressedTTF(
		FontData::Roboto_Mono, std::size(FontData::Roboto_Mono), 18.0f);

	if (!s_ctx_props.home_path || s_ctx_props.home_path[0] == '\0') {
		io.LogFilename = nullptr;
		io.IniFilename = nullptr;
	} else {
		const auto folder_path = ::join_path(
			s_ctx_props.home_path, "imgui");
		SDL_CreateDirectory(folder_path.c_str());

		const auto key = handle().handle_key.c_str();
		m_log_file = folder_path / ::join(key, ".log");
		m_ini_file = folder_path / ::join(key, ".ini");

		io.LogFilename = m_log_file.c_str();
		io.IniFilename = m_ini_file.c_str();
	}
}

void GuiSession::Handle::update_style() noexcept {
	if (m_style_generation < s_ctx_props.style_generation) {
		auto& style = ImGui::GetStyle();
		style = s_ctx_props.default_style;

		style.ScaleAllSizes(get_ui_zoom_scaling());
		style.FontScaleMain = get_ui_total_scaling();

		m_style_generation = s_ctx_props.style_generation;
	}
}

void GuiSession::set_file_path(const char* home_path) noexcept {
	s_ctx_props.home_path = home_path;
}

/*==================================================================*/

void UserInterface::set_ui_zoom_scaling(float scale) noexcept {
	const auto new_scale = std::clamp(scale, 1.0f, 4.0f);
	if (get_ui_zoom_scaling() != new_scale) {
		s_ctx_props.zoom_scaling = new_scale;
		++s_ctx_props.style_generation;
	}
}

float UserInterface::get_ui_zoom_scaling() noexcept {
	return s_ctx_props.zoom_scaling;
}

void UserInterface::set_ui_text_scaling(float scale) noexcept {
	const auto new_scale = std::clamp(scale, 0.5f, 2.0f);
	if (get_ui_text_scaling() != new_scale) {
		s_ctx_props.text_scaling = new_scale;
		++s_ctx_props.style_generation;
	}
}

float UserInterface::get_ui_text_scaling() noexcept {
	return s_ctx_props.text_scaling;
}

float UserInterface::get_ui_total_scaling() noexcept {
	return get_ui_zoom_scaling() * get_ui_text_scaling();
}

void UserInterface::toggle_borderless_view_mode() noexcept {
	s_ctx_props.borderless_view_mode = !s_ctx_props.borderless_view_mode;
}

void UserInterface::set_borderless_view_mode(bool enabled) noexcept {
	s_ctx_props.borderless_view_mode = enabled;
}

bool UserInterface::get_borderless_view_mode() noexcept {
	return s_ctx_props.borderless_view_mode;
}

const bool& UserInterface::get_borderless_view_mode_hook() noexcept {
	return s_ctx_props.borderless_view_mode;
}

unsigned UserInterface::get_live_dockspace_id() noexcept {
	auto* live_window = GuiSession::get_live_handle();
	return live_window ? live_window->get_dockspace_id() : 0;
}

/*==================================================================*/

void UserInterface::dock_next_window_to(unsigned id, bool first_time) noexcept {
	ImGui::SetNextWindowDockID(id ? id : get_live_dockspace_id(),
		first_time ? ImGuiCond_FirstUseEver : ImGuiCond_Always);
}

bool UserInterface::was_menu_clicked() noexcept {
	return s_active_menu && s_active_menu->first_hit;
}

void UserInterface::call_menubar(const char* window_name, bool* can_render) noexcept {
	auto* live_window = GuiSession::get_live_handle();
	if (live_window && ImGui::BeginMenuBar()) {
		bool invoked = live_window->invoke_registered_menus(window_name);
		if (can_render) { *can_render = invoked; }
		ImGui::EndMenuBar();
	}
}

void UserInterface::call_autohide_menubar(const char* window_name, bool& hidden) noexcept {
	const bool is_gui_window_focused = ImGui::IsWindowFocused(
		ImGuiFocusedFlags_RootAndChildWindows);

	if (is_gui_window_focused) {
		if (ImGui::IsKeyReleased(ImGuiKey_LeftAlt) ||
			ImGui::IsKeyReleased(ImGuiKey_RightAlt)
		) { hidden = !hidden; }
	} else {
		hidden = true;
	}

	auto* live_window = GuiSession::get_live_handle();
	if (live_window && !hidden && ImGui::BeginMenuBar()) {
		live_window->invoke_registered_menus(window_name);
		ImGui::EndMenuBar();
	}
}
