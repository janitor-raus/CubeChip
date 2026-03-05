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
#include "SystemDescriptor.hpp"
#include "SystemStaging.hpp"
#include "CoreRegistry.hpp"
#include "BasicLogger.hpp"
#include "Millis.hpp"
#include "ColorOps.hpp"
#include "Thread.hpp"
#include "SHA1.hpp"

#include <imgui.h>
#include <ranges>
#include <filesystem>

/*==================================================================*/

void FrontendHost::setup_gui_callables() noexcept {
	using namespace ImGui;

	static auto s_menu_file__open_file = FrontendInterface::register_menu("",
	{ 0, "File" }, [&]() noexcept {
		if (MenuItem("Open File...")) {
			SDL_ShowOpenFileDialog([](void*, const char* const* file_list, int) noexcept {
				if (file_list && file_list[0]) { set_open_file_dialog_result(file_list[0]); }
			}, nullptr, BVS->get_main_window(), nullptr, 0, nullptr, false);
		}
	});

	static auto s_menu_file__data_folder = FrontendInterface::register_menu("",
	{ 0, "File" }, [&]() noexcept {
		static std::atomic<bool> s_opening_url{};
		static auto s_home_url = "file:///" + HomeDirManager::get_home_path();

		BeginDisabled(s_opening_url.load(mo::acquire));
		if (MenuItem("Open Data Folder...", nullptr, nullptr, !s_opening_url.load(mo::acquire))) {
			s_opening_url.store(true, mo::release);

			Thread([]() noexcept {
				if (!SDL_OpenURL(s_home_url.c_str())) {
					blog.error("Failed to open Data folder! [{}]", SDL_GetError());
				}
				s_opening_url.store(false, mo::release);
			}).detach();
		}
		EndDisabled();
	});

	static auto s_menu_file__recent_files = FrontendInterface::register_menu("",
	{ 0, "File" }, [&]() noexcept {
		if (!s_file_mru.size()) { return; }
		Separator(1.0f);
		TextUnformatted("Recently opened:");
		DummyY(2.0f);

		if (FrontendInterface::was_menu_clicked()) {
			for (auto& e : s_file_mru.span()) { e.exists(); }
		}

		bool clicked = FrontendInterface::was_menu_clicked();

		for (auto& entry : s_file_mru.span()) {
			if (MenuItem(("• " + entry->filename().string()).c_str(),
				nullptr, nullptr, clicked ? entry.exists() : entry))
			{
				load_file_from_disk(entry->string());
			}
		}
	});

	static bool s_show_window_demo{};
	static auto s_menu_debug__imgui_demo = FrontendInterface::register_menu("",
	{ 10, "Debug" }, [&]() noexcept {
		if (MenuItem("ImGUI Demo...", nullptr, s_show_window_demo)) {
			s_show_window_demo = !s_show_window_demo;
		}
	});

	static bool s_show_window_logger{};
	static auto s_menu_debug__show_logs = FrontendInterface::register_menu("",
	{ 10, "Debug" }, [&]() noexcept {
		if (MenuItem("Show Logs...", nullptr, s_show_window_logger)) {
			s_show_window_logger = !s_show_window_logger;
		}
	});

	static auto s_menu_debug__about_app = FrontendInterface::register_menu("",
	{ 10, "Debug" }, [&]() noexcept {
		if (BeginMenu("About...")) {
			PushFont(nullptr, 21.0f);
			TextUnformatted(AppName);
		#if !defined(NDEBUG) || defined(DEBUG)
			SameLine();
			TextUnformatted(" [DEBUG]");
		#endif
			PopFont();

			Separator(2.0f);

			TextUnformatted("Version: ");
			SameLine();
			TextUnformatted(AppVer.with_hash);

			DummyY(1.0f);

			TextLinkOpenURL("License",
				"https://github.com/janitor-raus/CubeChip/blob/master/LICENSE.txt");
			SameLine();
			TextUnformatted("|");
			SameLine();
			TextLinkOpenURL("GitHub",
				"https://github.com/janitor-raus/CubeChip");

			EndMenu();
		}
	});

	static auto s_menu_settings__zoom_scale = FrontendInterface::register_menu("",
	{ 20, "Settings" }, [&]() noexcept {
		static int  s_scale_factor{};
		static bool s_click_active{};

		if (!s_click_active) { s_scale_factor = int(FrontendInterface::get_ui_zoom_scaling() * 100); }
		SliderInt("UI Zoom Scale", &s_scale_factor, 100, 200, "%d%%");

		s_click_active = IsItemActive();
		if (IsItemDeactivatedAfterEdit()) {
			FrontendInterface::set_ui_zoom_scaling(s_scale_factor * 0.01f);
		}
	});

	static auto s_menu_settings__text_scale = FrontendInterface::register_menu("",
	{ 20, "Settings" }, [&]() noexcept {
		static int  s_scale_factor{};
		static bool s_click_active{};

		if (!s_click_active) { s_scale_factor = int(FrontendInterface::get_ui_text_scaling() * 100); }
		SliderInt("UI Text Scale", &s_scale_factor, 100, 200, "%d%%");

		s_click_active = IsItemActive();
		if (IsItemDeactivatedAfterEdit()) {
			FrontendInterface::set_ui_text_scaling(s_scale_factor * 0.01f);
		}
	});

	static auto s_menu_settings__master_vol = FrontendInterface::register_menu("",
	{ 20, "Settings" }, [&]() noexcept {
		auto global_gain = int(GAB->get_global_gain() * 100);
		if (SliderInt("Master Volume", &global_gain, 0, 100, "%d%%"))
			{ GAB->set_glogal_gain(global_gain * 0.01f); }
	});

	static auto s_window_none__imgui_demo = FrontendInterface::register_window(
	[&]() noexcept {
		if (!s_show_window_demo) { return; }
		ShowDemoWindow(&s_show_window_demo);
	});

	static auto s_window_none__log_viewer = FrontendInterface::register_window(
	[&]() noexcept {
		if (!s_show_window_logger) { return; }

		static bool s_auto_scroll = true;
		static bool s_goto_bottom = false;
		static bool s_is_bottomed = true;

		static std::atomic<bool> s_opening_log{};

		if (Begin("Log Viewer##log_viewer", &s_show_window_logger,
			ImGuiWindowFlags_NoCollapse
		)) {
			BeginDisabled(s_is_bottomed);
			if (Button("Scroll to bottom")) { s_goto_bottom = true; }
			EndDisabled();

			SameLine();
			Checkbox("Auto-scroll", &s_auto_scroll);
			SameLine();

			constexpr static const char* s_open_log_file_label = "Open log file...";
			const float right_width = CalcTextSize(s_open_log_file_label).x
				+ GetStyle().FramePadding.x * 2;

			AddCursorPosX(GetContentRegionAvail().x - right_width);

			auto current_log_path = blog.get_log_path();
			BeginDisabled(s_opening_log.load(mo::acquire) || current_log_path.empty());
			if (Button(s_open_log_file_label)) {
				s_opening_log.store(true, mo::release);

				Thread([log_path_copy = std::move(current_log_path)]() noexcept {
					if (!SDL_OpenURL(("file:///" + log_path_copy).c_str())) {
						blog.error("Failed to open Log file! [{}]", SDL_GetError());
					}
					s_opening_log.store(false, mo::release);
				}).detach();
			}
			EndDisabled();

			DummyY(1.0f);

			if (BeginTable("LogTable", 6, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg
				| ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY
			)) {
				TableSetupScrollFreeze(1, 1);
				TableSetupColumn("#",        ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortAscending);
				TableSetupColumn("tID",      ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort);
				TableSetupColumn("Time",     ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort);
				TableSetupColumn("Source",   ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort);
				TableSetupColumn("Severity", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort);
				TableSetupColumn("Message",  ImGuiTableColumnFlags_NoSort);
				TableHeadersRow();

				const auto snapshot = blog->snapshot(0).fast();
				static bool s_sort_descending{};

				if (auto* sort = TableGetSortSpecs()) {
					if (sort->SpecsCount > 0 && sort->SpecsDirty) {
						s_sort_descending = sort->Specs[0].SortDirection
							== ImGuiSortDirection_Descending;
					}
				}

				static auto renderTable = [](auto& entry) {
					TableNextRow();

					TableSetColumnIndex(0);
					Text("%u", entry.index);

					TableSetColumnIndex(1);
					Text("%u", entry.thread);

					TableSetColumnIndex(2);
					TextUnformatted(NanoTime(entry.time)
						.format_as_timer().c_str());

					TableSetColumnIndex(3);
					TextUnformatted(::get_source_name(entry.source).c_str());

					TableSetColumnIndex(4);
					TextUnformatted(BLOG(entry.level).as_string(),
						RGBA(BLOG(entry.level).as_color()).XBGR());

					TableSetColumnIndex(5);
					TextUnformatted(entry.message.c_str());
				};

				if (s_sort_descending) {
					namespace rv = std::ranges::views;
					for (auto& entry : rv::reverse(snapshot)) { renderTable(entry); }
				} else {
					for (auto& entry : snapshot) { renderTable(entry); }
				}

				s_is_bottomed = GetScrollY() >= GetScrollMaxY();

				if (s_goto_bottom || (s_auto_scroll && s_is_bottomed)) {
					s_is_bottomed = true; s_goto_bottom = false;
					SetScrollHereY(1.0f);
				}

				EndTable();
			}
		}
		End();
	});

	static auto s_window_none__load_image = FrontendInterface::register_window(
	[&]() noexcept {
		if (!SystemStaging::file_image.valid()) { return; }

		struct CandidateSystem {
			using Hook = CoreRegistry::LiveHook;
			Hook  system_hook{};
			const char* error_message{};

			CandidateSystem(const Hook& entry, std::span<const char> file) noexcept
				: system_hook(entry)
				, error_message(entry->descriptor->validate_program(file))
			{}

			auto& get_descriptor() const noexcept { return system_hook->descriptor; }
			bool  eligible() const noexcept { return error_message == nullptr; }
		};

		struct FamilyGroup {
			std::size_t total_eligible{};
			std::string_view family_name{};
			std::vector<CandidateSystem> systems;

			FamilyGroup(std::string_view name) noexcept
				: family_name(name) {}
		};

		static bool s_render_modal_window = false;
		static bool s_replace_last_system = false;

		static RGBA s_highlight_color = 0xF5A30CFF;

		static std::vector<FamilyGroup> s_families{};
		static std::size_t s_extension_match_count{};
		static const CandidateSystem* s_chosen_candidate{};
		static std::filesystem::path s_file_image{};

		const auto s_close_modal_window = [&]() noexcept {
			SystemStaging::clear();
			CloseCurrentPopup();
			s_render_modal_window = false;
		};

		if (!s_render_modal_window) {
			s_render_modal_window = true;
			OpenPopup("FileImageModal");

			s_families.clear();
			s_extension_match_count = 0u;
			s_chosen_candidate = nullptr;
			s_file_image = SystemStaging::file_image.path();

			for (FamilyGroup* current_group = nullptr;
				auto& hook : CoreRegistry::get_candidate_core_span()
			) {
				if (!current_group || current_group->family_name
					!= hook->descriptor->family_pretty_name
				) {
					s_families.emplace_back(hook->descriptor->family_pretty_name);
					current_group = &s_families.back();
				}
				current_group->systems.emplace_back(hook, SystemStaging::file_image.span());
			}

			for (auto& group : s_families) {
				std::stable_partition(group.systems.begin(), group.systems.end(),
					[](const auto& candidate) { return candidate.eligible(); });

				for (const auto& candidate : group.systems) {
					if (candidate.eligible()) {
						++group.total_eligible;

						const auto& exts = candidate.get_descriptor()->known_extensions;

						if (std::any_of(exts.begin(), exts.end(), [&](const auto& ext) noexcept {
							return ext == s_file_image.extension();
						})) {
							s_chosen_candidate = &candidate;
							++s_extension_match_count;
						}
					}
				}
			}
		}

		const auto center = GetMainViewport()->GetCenter();
		SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

		if (BeginPopupModal("FileImageModal", nullptr, ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking
			| ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize
		)) {
			// filename title segment
			{
				PushFont(nullptr, 26.0f);
				const auto file_name = s_file_image.filename().string();
				const auto text_width = CalcTextSize(file_name.c_str()).x;
				AddCursorPosX((GetContentRegionAvail().x - text_width) * 0.5f);
				TextUnformatted(file_name.c_str(), s_highlight_color.ABGR());
				PopFont();
			}

			Separator(2.0f);
			Checkbox("Replace last system instance?", &s_replace_last_system);
			SameLine();
			Dummy(ImVec2());
			DummyY(1.0f);

			{
				const auto scroll_size = ImVec2(
					GetContentRegionAvail().x, 420.0f
					* FrontendInterface::get_ui_zoom_scaling()
					* FrontendInterface::get_ui_text_scaling());
				BeginChild("##candidate_scroll_region", scroll_size, true);

				const auto button_bg_size = ImVec2(
					GetContentRegionAvail().x, 56.0f
						* FrontendInterface::get_ui_zoom_scaling()
						* FrontendInterface::get_ui_text_scaling());
				const auto button_fg_size = ImVec2(
					button_bg_size.x - GetStyle().FrameRounding,
					button_bg_size.y - GetStyle().FrameRounding
				);

				for (auto& group : s_families) {
					// Family name segment
					{
						DummyY(2.0f);
						PushFont(nullptr, 24.0f);
						const auto family_name = group.family_name.data();
						const auto text_width = CalcTextSize(family_name).x;
						AddCursorPosX((button_bg_size.x - text_width) * 0.5f);
						TextUnformatted(family_name);
						PopFont();
					}
					// Eligibility count segment
					{
						PushFont(nullptr, 16.0f);
						const auto eligibility_counter = fmt::format("({}/{})",
							group.total_eligible, group.systems.size());
						const auto text_width = CalcTextSize(eligibility_counter.c_str()).x;
						AddCursorPosX((button_bg_size.x - text_width) * 0.5f);
						AddCursorPosY(GetTextLineHeight() * -0.3f);
						BeginDisabled();
						TextUnformatted(eligibility_counter.c_str());
						EndDisabled();
						PopFont();
					}

					DummyY(1.0f);

					for (auto& candidate : group.systems) {
						PushID(candidate.system_hook.get());

						if (ButtonContainer("##btn", button_bg_size,
						[&]() noexcept {
							if (!candidate.eligible()) {
								PushStyleColor(ImGuiCol_Text,
									GetColorU32(ImGuiCol_TextDisabled));
							}

							// System name segment
							{
								const auto system_name = candidate.get_descriptor()
									->system_pretty_name.data();

								PushFont(nullptr, 22.0f);
								const auto text_width = CalcTextSize(system_name).x;
								AddCursorPosX((button_fg_size.x - text_width) * 0.5f);
								TextUnformatted(system_name);
								PopFont();
							}

							// Undernote segment
							if (candidate.error_message) {
								const auto text_width = CalcTextSize(candidate.error_message).x;
								AddCursorPosX((button_fg_size.x - text_width) * 0.5f);
								AddCursorPosY(GetTextLineHeight() * -0.2f);
								TextWrapped("%s", candidate.error_message);
							}

							if (!candidate.eligible()) {
								PopStyleColor();
							}
						},
						[&]() noexcept {
							if (s_chosen_candidate == &candidate && s_extension_match_count == 1) {
								AddCursorPos(ImVec2(button_bg_size.y * 0.1f, button_bg_size.y * 0.025f));
								PushFont(nullptr, 50.0f);
								TextUnformatted("*", s_highlight_color.ABGR());
								PopFont();
							}
						},
						false)) {
							s_file_mru.insert(SystemStaging::file_image.path());

							if (auto* system = candidate.system_hook->construct_core()) {
								if (s_replace_last_system) { unload_system_instance(); }
								insert_system_instance(system); s_close_modal_window();
							} else {
								blog.error("Failed to construct instance for candidate system '{}'",
									candidate.get_descriptor()->system_pretty_name);
							}
						}

						PopID();
					}
				}

				EndChild();
			}

			DummyY(1.0f);
			const auto button_width = (GetContentRegionAvail().x
				- GetStyle().ItemSpacing.x) / 2.0f;

			if (Button("Calc SHA1", ImVec2(button_width, 0.0f))) {
				SystemStaging::sha1_hash = SHA1::from_span(SystemStaging::file_image);
				blog.info("SHA1: {}", SystemStaging::sha1_hash);

				// XXX - later set up CoreRegistry to utilize the hash to attempt
				//       autoloading the appropriate system core matching the hash
			}
			SameLine();
			if (Button("Cancel", ImVec2(button_width, 0.0f))) {
				s_close_modal_window();
			}


			if (IsKeyPressed(ImGuiKey_Escape)) {
				s_close_modal_window();
			} else {
				const auto pos  = GetWindowPos();
				const auto size = GetWindowSize();
				if (IsMouseClicked(ImGuiMouseButton_Left) &&
					!IsMouseHoveringRect(pos, pos + size))
				{
					s_close_modal_window();
				}
			}

			EndPopup();
		}

		if (!s_render_modal_window) {
			// Clear system staging data after modal is closed
			SystemStaging::clear();
		}
	});
}
