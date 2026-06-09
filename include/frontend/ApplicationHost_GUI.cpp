/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_misc.h>

#include "ApplicationHost.hpp"
#include "UserInterface.hpp"
#include "HomeDirManager.hpp"
#include "BasicVideoSpec.hpp"
#include "GlobalAudioBase.hpp"
#include "SystemDescriptor.hpp"
#include "SystemStaging.hpp"
#include "CoreRegistry.hpp"
#include "BasicLogger.hpp"
#include "Millis.hpp"
#include "ColorOps.hpp"
#include "Thread.hpp"
#include "SHA1_Helpers.hpp"

#include <imgui.h>
#include <filesystem>

/*==================================================================*/

// Tearing Demo: shows a full-screen canvas with horizontally-scrolling
// vertical bars. This is useful for testing tearing and refresh rates.
// An interactive divider bar appears when you click and drag, so you
// can check for input lag in windowed/exclusive fullscreen modes.

namespace {
	static float s_scroll_x = 0.f;
	static float s_scroll_spd = 120.f;
	static bool  s_paused = false;
	static float s_bar_y = -1.f;
	static bool  s_dragging = false;
}

static void ShowTearingTest(bool* p_open = nullptr) {
	if (p_open && !*p_open) return;

	ImGuiIO& io = ImGui::GetIO();
	const float dt = s_paused ? 0.f : io.DeltaTime;
	const float scr_w = io.DisplaySize.x;
	const float scr_h = io.DisplaySize.y;
	const float bar_w = 60.f;

	if (s_bar_y < 0.f) s_bar_y = scr_h * 0.5f;

	const bool  dragging = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	const float mouse_y = io.MousePos.y;

	if (dragging) {
		// Clamp to screen
		s_bar_y = mouse_y < 0.f ? 0.f : mouse_y > scr_h ? scr_h : mouse_y;
	}
	else {
		s_scroll_x = std::fmod(s_scroll_x + s_scroll_spd * dt, bar_w * 2.f);
	}

	if (s_bar_y < 0.f) s_bar_y = scr_h * 0.5f;

	// -----------------------------------------------------------------------
	// Full-screen canvas
	// -----------------------------------------------------------------------
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize(io.DisplaySize);
	ImGui::SetNextWindowBgAlpha(1.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
	ImGui::Begin("##tear_bg", nullptr,
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoSavedSettings
	);

	ImDrawList* dl = ImGui::GetWindowDrawList();

	if (!dragging) {
	// --- Scrolling bars ---
		int   n_bars = (int)(scr_w / bar_w) + 3;
		float x0 = -bar_w + std::fmod(s_scroll_x, bar_w * 2.f) - bar_w;

		for (int i = 0; i < n_bars; ++i) {
			float bx = x0 + i * bar_w;
			float cx = (bx + bar_w * 0.5f) / scr_w;
			cx = cx < 0.f ? 0.f : cx > 1.f ? 1.f : cx;

			ImU32 col;
			if (i & 1) {
				col = IM_COL32(
					(int)(220 - cx * 160),
					(int)(80 + cx * 80),
					(int)(30 + cx * 170), 255);
			}
			else {
				col = IM_COL32(
					(int)(30 + cx * 20),
					(int)(20 + cx * 40),
					(int)(50 + cx * 60), 255);
			}
			dl->AddRectFilled({ bx, 0.f }, { bx + bar_w, scr_h }, col);
		}
	}
	else {
	 // --- Split view ---
	 // Top half: warm
		dl->AddRectFilled({ 0.f, 0.f }, { scr_w, s_bar_y }, IM_COL32(210, 80, 40, 255));
		// Bottom half: cool
		dl->AddRectFilled({ 0.f, s_bar_y }, { scr_w, scr_h }, IM_COL32(40, 90, 190, 255));

		// Divider bar — 3px bright line so you can see exactly where it is
		dl->AddRectFilled(
			{ 0.f,   s_bar_y - 1.5f },
			{ scr_w, s_bar_y + 1.5f },
			IM_COL32(255, 255, 255, 220)
		);
	}

	ImGui::End();
	ImGui::PopStyleVar();

	// -----------------------------------------------------------------------
	// HUD
	// -----------------------------------------------------------------------
	ImGui::SetNextWindowSize({ 340, 0 });
	ImGui::SetNextWindowBgAlpha(0.70f);
	ImGui::Begin("Tear Test", p_open,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking
	);

	char buf[64];
	std::snprintf(buf, sizeof(buf), "%.3f FPS  (%.3f ms)",
		io.Framerate, 1000.f / io.Framerate);
	ImGui::TextUnformatted(buf);
	ImGui::Spacing();
	ImGui::SetNextItemWidth(-1);
	ImGui::SliderFloat("##spd", &s_scroll_spd, 20.f, 800.f, "speed %.0f px/s");
	if (ImGui::Button(s_paused ? "Resume" : "Pause", { -1, 0 }))
		s_paused = !s_paused;

	ImGui::End();
}

/*==================================================================*/

namespace CandidateList {
	struct System {
		using Hook = CoreRegistry::LiveHook;
		Hook        system_hook{};
		const char* error_message{};

		System(const Hook& entry, std::span<const char> file) noexcept
			: system_hook(entry)
			, error_message(entry->descriptor->validate_program(file))
		{}

		auto& get_descriptor() const noexcept { return system_hook->descriptor; }
		bool  eligible()        const noexcept { return error_message == nullptr; }
	};

	struct Family {
		std::string_view    family_name{};
		std::size_t         total_eligible{};
		std::vector<System> systems{};

		Family(std::string_view name) noexcept
			: family_name(name) {}
	};

	struct ListState {
		std::vector<Family>   families{};
		std::size_t           extension_match_count{};
		const System*         best_match{};
		std::filesystem::path file_image{};
		static constexpr auto c_highlight_color = 0xF5A30CFF_bgr;

		void refresh_candidates() {
			families.clear();
			extension_match_count = 0;
			best_match = nullptr;
			file_image = SystemStaging::file_image.path();

			for (Family* current_group = nullptr;
				const auto& hook : CoreRegistry::get_candidate_core_span()
			) {
				if (!current_group || current_group->family_name
					!= hook->descriptor->family_pretty_name
				) {
					families.emplace_back(hook->descriptor->family_pretty_name);
					current_group = &families.back();
				}
				current_group->systems.emplace_back(hook, SystemStaging::file_image.span());
			}

			const System* best_guess = nullptr;

			for (auto& group : families) {
				std::stable_partition(group.systems.begin(), group.systems.end(),
					[](const auto& system) { return system.eligible(); });

				for (const auto& candidate : group.systems) {
					if (!candidate.eligible()) { continue; }
					++group.total_eligible;

					const auto& exts = candidate.get_descriptor()->known_extensions;
					if (std::any_of(exts.begin(), exts.end(), [&](const auto& ext) {
						return ext == file_image.extension();
					})) {
						best_guess = &candidate;
						++extension_match_count;
					}
				}
			}

			if (best_guess && extension_match_count == 1) {
				best_match = best_guess;
			}
		}
	};
}

void ApplicationHost::setup_gui_callables() noexcept {
	using namespace ImGui;

	static auto s_menu_file__open_file = UserInterface::register_menu("",
	{ 0, "File" }, [&]() noexcept {
		if (MenuItem("Open File...")) {
			SDL_ShowOpenFileDialog([](void*, const char* const* file_list, int) noexcept {
				if (file_list && file_list[0]) { set_open_file_dialog_result(file_list[0]); }
			}, nullptr, BVS->get_main_window(), nullptr, 0, nullptr, false);
		}
	});

	static auto s_menu_file__data_folder = UserInterface::register_menu("",
	{ 0, "File" }, [&]() noexcept {
		static std::atomic<bool> s_opening_url{};
		static auto s_home_url = "file:///" + HDM->get_home_path();

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

	static auto s_menu_file__recent_files = UserInterface::register_menu("",
	{ 5, "File" }, [&]() noexcept {
		if (!s_file_mru.size()) { return; }
		Separator();
		TextUnformatted("Recently opened:");
		DummyY(2.0f);

		if (UserInterface::was_menu_clicked()) {
			for (auto& e : s_file_mru.span()) { e.exists(); }
		}

		bool clicked = UserInterface::was_menu_clicked();

		for (auto& entry : s_file_mru.span()) {
			if (MenuItem(("• " + entry->filename().string()).c_str(),
				nullptr, nullptr, clicked ? entry.exists() : entry))
			{
				load_file_from_disk(entry->string());
			}
		}
	});

/*==================================================================*/

	static bool s_show_window_demo{};
	static auto s_menu_debug__imgui_demo = UserInterface::register_menu("",
	{ 10, "Debug" }, [&]() noexcept {
		if (MenuItem("ImGUI Demo...", nullptr, s_show_window_demo)) {
			s_show_window_demo = !s_show_window_demo;
		}
	});

	static bool s_show_tearing_demo{};
	static auto s_menu_debug__tearing_demo = UserInterface::register_menu("",
	{ 10, "Debug" }, [&]() noexcept {
		if (MenuItem("Tearing Demo...", nullptr, s_show_tearing_demo)) {
			s_show_tearing_demo = !s_show_tearing_demo;
		}
	});

	static bool s_show_window_logger{};
	static auto s_menu_debug__show_logs = UserInterface::register_menu("",
	{ 10, "Debug" }, [&]() noexcept {
		if (MenuItem("Show Logs...", nullptr, s_show_window_logger)) {
			s_show_window_logger = !s_show_window_logger;
		}
	});

	static auto s_menu_debug__about_app = UserInterface::register_menu("",
	{ 10, "Debug" }, [&]() noexcept {
		if (BeginMenu("About...")) {
			PushFont(nullptr, 21.0f);
			TextUnformatted(c_app_name);
		#if !defined(NDEBUG) || defined(DEBUG)
			SameLine();
			TextUnformatted(" [DEBUG]");
		#endif
			PopFont();

			Separator();

			TextUnformatted("Version: ");
			SameLine();
			TextUnformatted(c_app_ver.with_hash);

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

/*==================================================================*/

	static auto s_menu_settings__zoom_scale = UserInterface::register_menu("",
	{ 20, "Settings" }, [&]() noexcept {
		static int  s_scale_factor{};
		static bool s_click_active{};

		if (!s_click_active) { s_scale_factor = int(UserInterface::get_ui_zoom_scaling() * 100); }
		SliderInt("UI Zoom Scale", &s_scale_factor, 100, 200, "%d%%");

		s_click_active = IsItemActive();
		if (IsItemDeactivatedAfterEdit()) {
			UserInterface::set_ui_zoom_scaling(s_scale_factor * 0.01f);
		}
	});

	static auto s_menu_settings__text_scale = UserInterface::register_menu("",
	{ 20, "Settings" }, [&]() noexcept {
		static int  s_scale_factor{};
		static bool s_click_active{};

		if (!s_click_active) { s_scale_factor = int(UserInterface::get_ui_text_scaling() * 100); }
		SliderInt("UI Text Scale", &s_scale_factor, 100, 200, "%d%%");

		s_click_active = IsItemActive();
		if (IsItemDeactivatedAfterEdit()) {
			UserInterface::set_ui_text_scaling(s_scale_factor * 0.01f);
		}
	});

	static auto s_menu_settings__master_vol = UserInterface::register_menu("",
	{ 25, "Settings" }, [&]() noexcept {
		Separator();
		auto master_volume = int(GAB->get_master_volume() * 100);
		if (SliderInt("Master Volume", &master_volume, 0, 100, "%d%%"))
			{ GAB->set_master_volume(master_volume * 0.01f); }
	});

	static auto s_menu_settings__focus_vol = UserInterface::register_menu("",
	{ 25, "Settings" }, [&]() noexcept {
		auto focus_volume = int(GAB->get_background_volume() * 100);
		if (SliderInt("Background Volume", &focus_volume, 0, 100, "%d%%"))
			{ GAB->set_background_volume(focus_volume * 0.01f); }
	});

	static auto s_menu_settings__borderless_view = UserInterface::register_menu("",
	{ 30, "Settings" }, [&]() noexcept {
		Separator();
		Checkbox("Borderless View Mode", &UserInterface::borderless_view_mode);
		if (IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
			SetTooltip("Removes all decorations and margins from a System's "
				"display window(s) for a flush fit.");
		}
	});

/*==================================================================*/

	static auto s_window_none__imgui_demo = UserInterface::register_window(
	[&]() noexcept {
		if (!s_show_window_demo) { return; }
		ShowDemoWindow(&s_show_window_demo);
	});

	static auto s_window_none__tearing_demo = UserInterface::register_window(
	[&]() noexcept {
		if (!s_show_tearing_demo) { return; }
		ShowTearingTest(&s_show_tearing_demo);
	});

	static auto s_window_none__log_viewer = UserInterface::register_window(
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

				const auto head  = blog->head();
				const auto total = head ? std::min(head + 1, blog->size()) : 0;

				static bool s_sort_descending{};

				if (auto* sort = TableGetSortSpecs()) {
					if (sort->SpecsCount > 0 && sort->SpecsDirty) {
						s_sort_descending = sort->Specs[0].SortDirection
							== ImGuiSortDirection_Descending;
						sort->SpecsDirty = false;
					}
				}

				static auto render_log_table = [](const auto& entry) {
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

				ImGuiListClipper clipper;
				clipper.Begin(int(total));
				while (clipper.Step()) {
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
						render_log_table(blog->at(s_sort_descending ? i : total - 1 - i));
					}
				}
				clipper.End();

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

	static auto s_window_none__load_image = UserInterface::register_window(
	[&]() noexcept {
		if (!SystemStaging::file_image.valid()) { return; }

		static bool s_render_modal = false;
		const auto  scaling = UserInterface::get_ui_total_scaling();

		static CandidateList::ListState     s_list_state;
		static const CandidateList::System* s_chosen_system  = nullptr;
		static const CandidateList::System* s_pending_system = nullptr;

		static SHA1_ThreadedWidget s_sha1_widget;
		static ScrollingTextState s_scroll_state;

		static auto s_close_modal = []() noexcept {
			s_sha1_widget.reset();
			s_render_modal = false;
			SystemStaging::clear();
			CloseCurrentPopup();
		};

		if (!s_render_modal) {
			s_render_modal = true;
			s_list_state.refresh_candidates();

			s_sha1_widget.setup(SystemStaging::file_image.size());
			s_scroll_state.set_speed(128.0f * scaling);

			s_pending_system = s_list_state.best_match;
			OpenPopup("FileImageModal");
		}

		const auto viewport_mid = GetMainViewport()->GetCenter();
		SetNextWindowPos(viewport_mid, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		SetNextWindowMinClientSize(ImVec2(540.0f, 420.0f) * scaling);

		if (BeginPopupModal("FileImageModal", nullptr,
			ImGuiWindowFlags_NoMove    | ImGuiWindowFlags_NoTitleBar  |
			ImGuiWindowFlags_NoResize  | ImGuiWindowFlags_NoCollapse  |
			ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_AlwaysAutoResize
		)) {
			const auto padding = GetStyle().WindowPadding;
			const auto spacing = GetStyle().ItemSpacing;
			const auto scroll_area = ImVec2(GetContentRegionAvail().x, 420.0f * scaling);

			// filename title
			PushFont(nullptr, 26.0f);
			const auto file_name = s_list_state.file_image.filename().string();
			const auto filename_text_width = CalcTextSize(file_name.c_str()).x;

			PushStyleColor(ImGuiCol_Text, s_list_state.c_highlight_color);
			if (filename_text_width > scroll_area.x) {
				ScrollingText(file_name.c_str(), scroll_area.x, &s_scroll_state);
			} else {
				AddCursorPosX((scroll_area.x - filename_text_width) * 0.5f);
				TextUnformatted(file_name.c_str());
			}

			PopStyleColor();
			PopFont();
			Separator();

			if (SystemStaging::sha1_hash.empty()) {
				if (s_sha1_widget.running()) {
					const auto progress = s_sha1_widget.progress();
					const auto percentage = fmt::format("{:.1f}%###pc", progress * 100.0f);

					constexpr static auto button_label = "Cancel";

					const auto cancel_button_size = ImVec2(GetFrameWidth(button_label), 0.0f);
					if (Button(button_label, cancel_button_size)) { s_sha1_widget.reset(); }
					SameLine();
					const auto progress_bar_size = ImVec2(GetContentRegionAvail().x, GetFrameHeight());
					ProgressBar(progress, progress_bar_size, percentage.c_str());
				}
				else {
					if (Button("Calculate SHA1", ImVec2(scroll_area.x, 0.0f))) {
						s_sha1_widget.start(SystemStaging::file_image.data(), [](auto&& hash) noexcept {
							SystemStaging::sha1_hash = std::move(hash);
							blog.info("SHA1: {}", SystemStaging::sha1_hash);
						});
					}
				}
			} else {
				AlignTextToFramePadding();
				AddCursorPosX((scroll_area.x - CalcTextSize(
					SystemStaging::sha1_hash.c_str()).x) * 0.5f);
				Text("‹%s›", SystemStaging::sha1_hash.c_str());
			}

			// Candidate scroll region
			if (BeginChild("##candidate_scroll_region", scroll_area, true)) {
				static constexpr auto system_name_font_size = 20.0f;
				static constexpr auto half_button_font_size = 18.0f;

				const auto sha1_thread_active = s_sha1_widget.running();
				const auto scroll_avail_width = GetContentRegionAvail().x;
				const auto system_name_height = CalcFontHeight(system_name_font_size);

				const auto base_button_size = ImVec2(scroll_avail_width,
					system_name_height + padding.y * 2.0f);

				const auto half_button_size = ImVec2(
					(base_button_size.x - spacing.x) * 0.5f - padding.x,
					CalcFontHeight(half_button_font_size) + padding.y * 2.0f
				);

				if (s_pending_system) {
					s_chosen_system  = s_pending_system;
					s_pending_system = nullptr;
				}

				for (auto& group : s_list_state.families) {
					PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.0f);
					// Family name
					PushFont(nullptr, 24.0f);
					PushStyleVarX(ImGuiStyleVar_SeparatorTextAlign, 0.5f);
					SeparatorText(group.family_name.data());
					PopStyleVar();
					PopFont();

					// Eligibility count
					PushFont(nullptr, 16.0f);
					const auto counter = fmt::format("({}/{})",
						group.total_eligible, group.systems.size());
					const auto counter_text_width = CalcTextSize(counter.c_str()).x;
					AddCursorPosX((scroll_avail_width - counter_text_width) * 0.5f);
					TextUnformatted(counter.c_str(), GetColorU32(ImGuiCol_TextDisabled));
					PopFont();

					DummyY(padding.y * 2.0f);
					PopStyleVar();

					for (auto& candidate : group.systems) {
						const bool is_expanded = s_chosen_system == &candidate;
						const bool is_best_match = s_list_state.best_match == &candidate;

						const auto full_button_size = ImVec2(base_button_size.x,
							base_button_size.y + is_expanded * (half_button_size.y + padding.y));

						PushID(candidate.system_hook.get());
						if (ButtonContainer("##btn", full_button_size, [&]() noexcept {
							PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2());

							// System name, centered
							const auto system_name_string = std::string(
								candidate.get_descriptor()->system_pretty_name);

							PushFont(nullptr, system_name_font_size);

							const auto indicator_text_size = is_best_match
								* 2.0f * CalcTextSize("‹‹››").x;

							AddCursorPos(ImVec2((base_button_size.x - indicator_text_size
								- CalcTextSize(system_name_string.c_str()).x) * 0.5f, padding.y));

							if (is_best_match) {
								TextUnformatted("››› ", s_list_state.c_highlight_color);
								SameLine();
							}

							BeginDisabled(!candidate.eligible());
							TextUnformatted(system_name_string.c_str());
							EndDisabled();

							if (is_best_match) {
								SameLine();
								TextUnformatted(" ‹‹‹", s_list_state.c_highlight_color);
							}

							PopFont();

							PopStyleVar();
							AddCursorPosY(padding.y);

							const auto action_button = [&](
								const char* label, const ImVec2& button_size, bool replace
							) noexcept {
								BeginDisabled(sha1_thread_active);
								if (ButtonContainer(label, button_size, [&]() noexcept {
									PushFont(nullptr, half_button_font_size);
									AddCursorPos((button_size - CalcTextSize(label)) * 0.5f);
									TextUnformatted(label);
									PopFont();
								}, false, true)) {
									s_file_mru.insert(SystemStaging::file_image.path());
									if (auto* system = candidate.system_hook->construct_core()) {
										if (replace) { unload_system_instance(); }
										insert_system_instance(system);
										s_close_modal();
									} else {
										blog.error("Failed to construct instance for '{}'",
											candidate.get_descriptor()->system_pretty_name);
									}
								}
								EndDisabled();
							};

							if (is_expanded) {
								AddCursorPosX(padding.x);
								action_button("New Instance", half_button_size, false);
								SameLine();
								BeginDisabled(m_systems.empty());
								action_button("Replace Instance", half_button_size, true);
								EndDisabled();
							}
						}, is_expanded, !is_expanded)) {
							s_pending_system = &candidate;
							SetScrollFromPosY(GetItemRectMin().y
								- GetWindowPos().y, 0.5f);
						}
						PopID();
					}
				}
			}

			EndChild();

			if (IsKeyPressed(ImGuiKey_Escape)) {
				s_close_modal();
			} else {
				const auto pos  = GetWindowPos();
				const auto size = GetWindowSize();
				if (IsMouseClicked(ImGuiMouseButton_Left) &&
					!IsMouseHoveringRect(pos, pos + size))
				{
					s_close_modal();
				}
			}

			EndPopup();
		}
	});
}
