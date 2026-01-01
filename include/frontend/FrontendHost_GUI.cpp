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
	static auto sMenu_File_Open = FrontendInterface::register_menu("",
	{ 0, "File" }, [&]() noexcept {
		if (ImGui::MenuItem("Open File...")) {
			SDL_ShowOpenFileDialog([](void*, const char* const* file_list, int) noexcept {
				if (file_list && file_list[0]) { set_open_file_dialog_result(file_list[0]); }
			}, nullptr, BVS->get_main_window(), nullptr, 0, nullptr, false);
		}
	});

	static auto sMenu_File_Data = FrontendInterface::register_menu("",
	{ 0, "File" }, [&]() noexcept {
		static std::atomic<bool> s_opening_url{};
		static auto s_home_url = "file:///" + HomeDirManager::get_home_path();

		if (ImGui::MenuItem("Open Data Folder...", nullptr, nullptr, !s_opening_url.load(mo::acquire))) {
			s_opening_url.store(true, mo::release);

			Thread([]() noexcept {
				if (!SDL_OpenURL(s_home_url.c_str())) {
					blog.newEntry<BLOG::ERR>("Failed to open Data folder! [{}]", SDL_GetError());
				}
				s_opening_url.store(false, mo::release);
			}).detach();
		}
	});

	static auto sMenu_Recent_Files = FrontendInterface::register_menu("",
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

	static bool sShowDemoWindow{};
	static auto sMenu_Debug_Demo = FrontendInterface::register_menu("",
	{ 10, "Debug" }, [&]() noexcept {
		if (ImGui::MenuItem("ImGUI Demo...", nullptr, sShowDemoWindow)) {
			sShowDemoWindow = !sShowDemoWindow;
		}
	});

	static bool sShowLogWindow{};
	static auto sMenu_Debug_Log = FrontendInterface::register_menu("",
	{ 10, "Debug" }, [&]() noexcept {
		if (ImGui::MenuItem("Show Logs...", nullptr, sShowLogWindow)) {
			sShowLogWindow = !sShowLogWindow;
		}
	});

	static auto sMenu_AboutApp = FrontendInterface::register_menu("",
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

	static auto sMenu_Settings_ScaleUI = FrontendInterface::register_menu("",
	{ 20, "Settings" }, [&]() noexcept {
		static int  scale_factor{};
		static bool click_active{};

		if (!click_active) { scale_factor = int(FrontendInterface::get_ui_scale_factor() * 100); }
		ImGui::SliderInt(" UI Scale", &scale_factor, 100, 300, "%d%%");

		click_active = ImGui::IsItemActive();
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			FrontendInterface::set_ui_scale_factor(scale_factor * 0.01f);
		}
	});

	static auto sMenu_Settings_MasterVol = FrontendInterface::register_menu("",
	{ 20, "Settings" }, [&]() noexcept {
		auto global_gain = int(GAB->get_global_gain() * 100);
		if (ImGui::SliderInt(" Master Volume", &global_gain, 0, 100, "%d%%"))
			{ GAB->set_glogal_gain(global_gain * 0.01f); }
	});

	static auto sWindow_Demo = FrontendInterface::register_window(
	[&]() noexcept {
		if (!sShowDemoWindow) { return; }
		ImGui::ShowDemoWindow(&sShowDemoWindow);
	});

	static auto sWindow_Log = FrontendInterface::register_window(
	[&]() noexcept {
		if (!sShowLogWindow) { return; }

		static bool autoScroll = true;
		static bool scrollToBottom{};

		if (ImGui::Begin("Application Log##Logger", &sShowLogWindow,
			ImGuiWindowFlags_NoCollapse
		)) {
			if (ImGui::Button("Scroll to Bottom")) { scrollToBottom = true; }

			ImGui::SameLine();
			ImGui::Checkbox("Auto-scroll", &autoScroll);
			ImGui::Separator();

			ImGui::BeginChild("LogTableRegion", { 0.0f, 0.0f });

			if (ImGui::BeginTable("LogTable", 4,
				ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable |
				ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg)
			) {
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn("#",        ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortAscending);
				ImGui::TableSetupColumn("Time",     ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort);
				ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort);
				ImGui::TableSetupColumn("Message",  ImGuiTableColumnFlags_NoSort);
				ImGui::TableHeadersRow();

				const auto snapshot = blog->snapshot(0).fast();
				static bool sortDescending{};

				if (auto* sort = ImGui::TableGetSortSpecs()) {
					if (sort->SpecsCount > 0 && sort->SpecsDirty) {
						sortDescending = sort->Specs[0].SortDirection
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

				if (sortDescending) {
					namespace rv = std::ranges::views;
					for (auto& entry : rv::reverse(snapshot)) { renderTable(entry); }
				} else {
					for (auto& entry : snapshot) { renderTable(entry); }
				}

				if (scrollToBottom || (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
					ImGui::SetScrollHereY(1.0f);
					scrollToBottom = false;
				}

				ImGui::EndTable();
			}

			ImGui::EndChild();
		}
		ImGui::End();
	});
}