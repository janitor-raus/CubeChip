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

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <ranges>

/*==================================================================*/

static constexpr auto RGBA_to_ImVec4(RGBA color) noexcept {
	return ImVec4(
		color.R / 255.0f, color.G / 255.0f,
		color.B / 255.0f, color.A / 255.0f
	);
}

/*==================================================================*/

void FrontendHost::initializeInterface() noexcept {
	static auto sMenu_File_Open = FrontendInterface::registerMenu("", "File",
	[&]() noexcept {
		if (ImGui::MenuItem("Open File...")) {
			SDL_ShowOpenFileDialog(HomeDirManager::probableFileCallback,
				nullptr, BVS->getMainWindow(), nullptr, 0, nullptr, false);
		}
	});

	static auto sMenu_File_Data = FrontendInterface::registerMenu("", "File",
	[&]() noexcept {
		static std::atomic<bool> sOpeningURL{};
		if (ImGui::MenuItem("Open Data Folder...", nullptr, nullptr, !sOpeningURL.load(mo::acquire))) {
			sOpeningURL.store(true, mo::release);
			Thread([url = HomeDirManager::getHomeDirectoryURL()]() noexcept {
				if (!SDL_OpenURL(url.c_str())) {
					blog.newEntry<BLOG::ERR>("Failed to open Data folder! [{}]", SDL_GetError());
				}
				sOpeningURL.store(false, mo::release);
			}).detach();
		}
	});

	static bool sShowDemoWindow{};
	static auto sMenu_Debug_Demo = FrontendInterface::registerMenu("", "Debug",
	[&]() noexcept {
		if (ImGui::MenuItem("ImGUI Demo...", nullptr, sShowDemoWindow)) {
			sShowDemoWindow = !sShowDemoWindow;
		}
	});

	static bool sShowLogWindow{};
	static auto sMenu_Debug_Log = FrontendInterface::registerMenu("", "Debug",
	[&]() noexcept {
		if (ImGui::MenuItem("Show Logs...", nullptr, sShowLogWindow)) {
			sShowLogWindow = !sShowLogWindow;
		}
	});

	static auto sMenu_AboutApp = FrontendInterface::registerMenu("", "Debug",
	[&]() noexcept {
		if (ImGui::BeginMenu("About...")) {
			ImGui::PushFont(nullptr, 21.0f);
			ImGui::TextUnformatted(AppName);
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

	static auto sMenu_Settings_ScaleUI = FrontendInterface::registerMenu("", "Settings",
	[&]() noexcept {
		static int  scale_factor{};
		static bool click_active{};

		if (!click_active) { scale_factor = int(FrontendInterface::GetScaleFactor() * 100); }
		ImGui::SliderInt(" UI Scale", &scale_factor, 100, 300, "%d%%");

		click_active = ImGui::IsItemActive();
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			FrontendInterface::SetScaleFactor(scale_factor * 0.01f);
		}
	});

	static auto sMenu_Settings_MasterVol = FrontendInterface::registerMenu("", "Settings",
	[&]() noexcept {
		auto global_gain = int(GAB->getGlobalGain() * 100);
		if (ImGui::SliderInt(" Master Volume", &global_gain, 0, 100, "%d%%"))
			{ GAB->setGlobalGain(global_gain * 0.01f); }
	});

	static auto sWindow_Demo = FrontendInterface::registerWindow(
	[&]() noexcept {
		if (!sShowDemoWindow) { return; }
		ImGui::ShowDemoWindow(&sShowDemoWindow);
	});

	static auto sWindow_Log = FrontendInterface::registerWindow(
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
					ImGui::TextUnformatted(
						NanoTime(entry.time).format().c_str());

					ImGui::TableSetColumnIndex(2);
					ImGui::TextColored(RGBA_to_ImVec4(
						BLOG(entry.level).as_color()), "%s",
						BLOG(entry.level).as_string());

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