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
#include "DefaultConfig.hpp"
#include "BasicLogger.hpp"
#include "Millis.hpp"

#include <imgui.h>

/*==================================================================*/

void FrontendHost::initializeInterface() noexcept {
	static bool sShowLogWindow{};

	static auto sMenu_File_Open = FrontendInterface::registerMenu("File",
	[&]() noexcept {
		if (ImGui::MenuItem("Open File...")) {
			SDL_ShowOpenFileDialog(HomeDirManager::probableFileCallback,
				nullptr, BVS->getMainWindow(), nullptr, 0, nullptr, false);
		}
	});

	static auto sMenu_File_Data = FrontendInterface::registerMenu("File",
	[&]() noexcept {
		if (ImGui::MenuItem("Open Data Folder...")) {
			if (!SDL_OpenURL(HomeDirManager::getHomeDirectoryURL().c_str())) {
				blog.newEntry<BLOG::ERROR>("Failed to open Data folder! [{}]", SDL_GetError());
			}
		}
	});

	static auto sMenu_Debug_Log = FrontendInterface::registerMenu("Debug",
	[&]() noexcept {
		if (ImGui::MenuItem("Show Logs...", nullptr, sShowLogWindow)) {
			sShowLogWindow = !sShowLogWindow;
		}
	});

	static auto sWindow_Log = FrontendInterface::registerWindow(
	[&]() noexcept {
		static bool autoScroll = true;
		static bool scrollToBottom = false;

		if (!sShowLogWindow) { return; }
		const auto snapshot{ blog->snapshot(0).fast() };

		ImGui::Begin("AppLog##Logger", &sShowLogWindow, ImGuiWindowFlags_NoCollapse);
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

			for (const auto& entry : snapshot) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%u", entry.index);
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(NanoTime(entry.time).format().c_str());
				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(entry.level.to_string().data());
				ImGui::TableSetColumnIndex(3);
				ImGui::TextUnformatted(entry.message.c_str());
			}

			if (scrollToBottom || (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
				ImGui::SetScrollHereY(1.0f);
				scrollToBottom = false;
			}

			ImGui::EndTable();
		}

		ImGui::EndChild();
		ImGui::End();
	});
}