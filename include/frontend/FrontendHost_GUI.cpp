/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_misc.h>

#include "FrontendHost.hpp"
#include "FrontendInterface.hpp"
#include "HomeDirManager.hpp"
#include "BasicVideoSpec.hpp"
#include "GlobalAudioBase.hpp"
#include "DefaultConfig.hpp"
#include "BasicLogger.hpp"
#include "Millis.hpp"
#include "ColorOps.hpp"
#include "Thread.hpp"

#include <imgui.h>
#include <ranges>

/*==================================================================*/

void FrontendHost::setup_gui_callables() noexcept {
	static auto s_menu_file__open_file = FrontendInterface::register_menu("",
	{ 0, "File" }, [&]() noexcept {
		if (ImGui::MenuItem("Open File...")) {
			SDL_ShowOpenFileDialog([](void*, const char* const* file_list, int) noexcept {
				if (file_list && file_list[0]) { set_open_file_dialog_result(file_list[0]); }
			}, nullptr, BVS->get_main_window(), nullptr, 0, nullptr, false);
		}
	});

	static auto s_menu_file__data_folder = FrontendInterface::register_menu("",
	{ 0, "File" }, [&]() noexcept {
		static std::atomic<bool> s_opening_url{};
		static auto s_home_url = "file:///" + HomeDirManager::get_home_path();

		ImGui::BeginDisabled(s_opening_url.load(mo::acquire));
		if (ImGui::MenuItem("Open Data Folder...", nullptr, nullptr, !s_opening_url.load(mo::acquire))) {
			s_opening_url.store(true, mo::release);

			Thread([]() noexcept {
				if (!SDL_OpenURL(s_home_url.c_str())) {
					blog.newEntry<BLOG::ERR>("Failed to open Data folder! [{}]", SDL_GetError());
				}
				s_opening_url.store(false, mo::release);
			}).detach();
		}
		ImGui::EndDisabled();
	});

	static auto s_menu_file__recent_files = FrontendInterface::register_menu("",
	{ 0, "File" }, [&]() noexcept {
		if (!s_file_mru.size()) { return; }
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::TextUnformatted("Recently opened:");
		ImGui::Spacing(); ImGui::Spacing();

		if (FrontendInterface::was_menu_clicked()) {
			for (auto& e : s_file_mru.span()) { e.exists(); }
		}

		bool clicked = FrontendInterface::was_menu_clicked();

		for (auto& entry : s_file_mru.span()) {
			if (ImGui::MenuItem(entry->filename().string().c_str(),
				nullptr, nullptr, clicked ? entry.exists() : entry))
			{
				load_file_from_disk(entry->string());
			}
		}
	});

	static bool s_show_window_demo{};
	static auto s_menu_debug__imgui_demo = FrontendInterface::register_menu("",
	{ 10, "Debug" }, [&]() noexcept {
		if (ImGui::MenuItem("ImGUI Demo...", nullptr, s_show_window_demo)) {
			s_show_window_demo = !s_show_window_demo;
		}
	});

	static bool s_show_window_logger{};
	static auto s_menu_debug__show_logs = FrontendInterface::register_menu("",
	{ 10, "Debug" }, [&]() noexcept {
		if (ImGui::MenuItem("Show Logs...", nullptr, s_show_window_logger)) {
			s_show_window_logger = !s_show_window_logger;
		}
	});

	static auto s_menu_debug__about_app = FrontendInterface::register_menu("",
	{ 10, "Debug" }, [&]() noexcept {
		if (ImGui::BeginMenu("About...")) {
			ImGui::PushFont(nullptr, 21.0f);
			ImGui::TextUnformatted(AppName);
		#if !defined(NDEBUG) || defined(DEBUG)
			ImGui::SameLine();
			ImGui::TextUnformatted(" [DEBUG]");
		#endif
			ImGui::PopFont();

			ImGui::Spacing(); ImGui::Spacing();

			ImGui::TextUnformatted("Version: ");
			ImGui::SameLine();
			ImGui::TextUnformatted(AppVer.with_hash);

			ImGui::Spacing(); ImGui::Spacing();

			ImGui::TextLinkOpenURL("License",
				"https://github.com/janitor-raus/CubeChip/blob/master/LICENSE.txt");
			ImGui::SameLine();
			ImGui::TextUnformatted("|");
			ImGui::SameLine();
			ImGui::TextLinkOpenURL("GitHub",
				"https://github.com/janitor-raus/CubeChip");

			ImGui::EndMenu();
		}
	});

	static auto s_menu_settings__zoom_scale = FrontendInterface::register_menu("",
	{ 20, "Settings" }, [&]() noexcept {
		static int  s_scale_factor{};
		static bool s_click_active{};

		if (!s_click_active) { s_scale_factor = int(FrontendInterface::get_ui_zoom_scaling() * 100); }
		ImGui::SliderInt("UI Zoom Scale", &s_scale_factor, 100, 200, "%d%%");

		s_click_active = ImGui::IsItemActive();
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			FrontendInterface::set_ui_zoom_scaling(s_scale_factor * 0.01f);
		}
	});

	static auto s_menu_settings__text_scale = FrontendInterface::register_menu("",
	{ 20, "Settings" }, [&]() noexcept {
		static int  s_scale_factor{};
		static bool s_click_active{};

		if (!s_click_active) { s_scale_factor = int(FrontendInterface::get_ui_text_scaling() * 100); }
		ImGui::SliderInt("UI Text Scale", &s_scale_factor, 50, 150, "%d%%");

		s_click_active = ImGui::IsItemActive();
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			FrontendInterface::set_ui_text_scaling(s_scale_factor * 0.01f);
		}
	});

	static auto s_menu_settings__master_vol = FrontendInterface::register_menu("",
	{ 20, "Settings" }, [&]() noexcept {
		auto global_gain = int(GAB->get_global_gain() * 100);
		if (ImGui::SliderInt("Master Volume", &global_gain, 0, 100, "%d%%"))
			{ GAB->set_glogal_gain(global_gain * 0.01f); }
	});

	static auto s_window_none__imgui_demo = FrontendInterface::register_window(
	[&]() noexcept {
		if (!s_show_window_demo) { return; }
		ImGui::ShowDemoWindow(&s_show_window_demo);
	});

	static auto s_window_none__log_viewer = FrontendInterface::register_window(
	[&]() noexcept {
		if (!s_show_window_logger) { return; }

		static bool s_auto_scroll = true;
		static bool s_goto_bottom = false;
		static bool s_is_bottomed = true;

		static std::atomic<bool> s_opening_log{};

		if (ImGui::Begin("Log Viewer##log_viewer", &s_show_window_logger,
			ImGuiWindowFlags_NoCollapse
		)) {
			ImGui::BeginDisabled(s_is_bottomed);
			if (ImGui::Button("Scroll to bottom")) { s_goto_bottom = true; }
			ImGui::EndDisabled();

			ImGui::SameLine();
			ImGui::Checkbox("Auto-scroll", &s_auto_scroll);
			ImGui::SameLine();

			constexpr static const char* s_open_log_file_label = "Open log file...";
			const float right_width = ImGui::CalcTextSize(s_open_log_file_label).x
				+ ImGui::GetStyle().FramePadding.x * 2;

			ImGui::AddCursorPosX(ImGui::GetContentRegionAvail().x - right_width);

			auto current_log_path = blog.get_log_path();
			ImGui::BeginDisabled(s_opening_log.load(mo::acquire) || current_log_path.empty());
			if (ImGui::Button(s_open_log_file_label)) {
				s_opening_log.store(true, mo::release);

				Thread([log_path_copy = std::move(current_log_path)]() noexcept {
					if (!SDL_OpenURL(("file:///" + log_path_copy).c_str())) {
						blog.newEntry<BLOG::ERR>("Failed to open Log file! [{}]", SDL_GetError());
					}
					s_opening_log.store(false, mo::release);
				}).detach();
			}
			ImGui::EndDisabled();

			ImGui::Spacing();

			if (ImGui::BeginTable("LogTable", 4, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg
				| ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY
			)) {
				ImGui::TableSetupScrollFreeze(1, 1);
				ImGui::TableSetupColumn("#",        ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortAscending);
				ImGui::TableSetupColumn("Time",     ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort);
				ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort);
				ImGui::TableSetupColumn("Message",  ImGuiTableColumnFlags_NoSort);
				ImGui::TableHeadersRow();

				const auto snapshot = blog->snapshot(0).fast();
				static bool s_sort_descending{};

				if (auto* sort = ImGui::TableGetSortSpecs()) {
					if (sort->SpecsCount > 0 && sort->SpecsDirty) {
						s_sort_descending = sort->Specs[0].SortDirection
							== ImGuiSortDirection_Descending;
					}
				}

				static auto renderTable = [](auto& entry) {
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%u", entry.index);

					ImGui::TableSetColumnIndex(1);
					ImGui::TextUnformatted(NanoTime(entry.time)
						.format_as_timer().c_str());

					ImGui::TableSetColumnIndex(2);
					ImGui::TextUnformatted(BLOG(entry.level).as_string(),
						RGBA(BLOG(entry.level).as_color()).XBGR());

					ImGui::TableSetColumnIndex(3);
					ImGui::TextUnformatted(entry.message.c_str());
				};

				if (s_sort_descending) {
					namespace rv = std::ranges::views;
					for (auto& entry : rv::reverse(snapshot)) { renderTable(entry); }
				} else {
					for (auto& entry : snapshot) { renderTable(entry); }
				}

				s_is_bottomed = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

				if (s_goto_bottom || s_auto_scroll && s_is_bottomed) {
					s_is_bottomed = true; s_goto_bottom = false;
					ImGui::SetScrollHereY(1.0f);
				}

				ImGui::EndTable();
			}
		}
		ImGui::End();
	});
}
