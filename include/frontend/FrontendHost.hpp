/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "SimpleMRU.hpp"
#include "FileItem.hpp"
#include "SettingWrapper.hpp"

/*==================================================================*/

#ifndef PROJECT_VERSION_MAJOR_I
	#define PROJECT_VERSION_MAJOR_I 0
#endif
#ifndef PROJECT_VERSION_MINOR_I
	#define PROJECT_VERSION_MINOR_I 0
#endif
#ifndef PROJECT_VERSION_PATCH_I
	#define PROJECT_VERSION_PATCH_I 0
#endif
#ifndef PROJECT_VERSION_TWEAK_I
	#define PROJECT_VERSION_TWEAK_I 0
#endif
#ifndef PROJECT_VERSION_MAJOR
	#define PROJECT_VERSION_MAJOR "0"
#endif
#ifndef PROJECT_VERSION_MINOR
	#define PROJECT_VERSION_MINOR "0"
#endif
#ifndef PROJECT_VERSION_PATCH
	#define PROJECT_VERSION_PATCH "0"
#endif
#ifndef PROJECT_VERSION_TWEAK
	#define PROJECT_VERSION_TWEAK "0"
#endif
#ifndef PROJECT_VERSION_GHASH
	#define PROJECT_VERSION_GHASH "unknown"
#endif
#ifndef PROJECT_VERSION_WITH_DATE
	#define PROJECT_VERSION_WITH_DATE "0.0.0.0"
#endif
#ifndef PROJECT_VERSION_WITH_HASH
	#define PROJECT_VERSION_WITH_HASH "0.0.0.0.unknown"
#endif
#ifndef PROJECT_NAME
	#define PROJECT_NAME "CubeChip?"
#endif

/*==================================================================*/

struct ProjectVersion {
	constexpr static inline auto major_i = PROJECT_VERSION_MAJOR_I;
	constexpr static inline auto minor_i = PROJECT_VERSION_MINOR_I;
	constexpr static inline auto patch_i = PROJECT_VERSION_PATCH_I;
	constexpr static inline auto tweak_i = PROJECT_VERSION_TWEAK_I;

	constexpr static inline auto* major = PROJECT_VERSION_MAJOR;
	constexpr static inline auto* minor = PROJECT_VERSION_MINOR;
	constexpr static inline auto* patch = PROJECT_VERSION_PATCH;
	constexpr static inline auto* tweak = PROJECT_VERSION_TWEAK;

	constexpr static inline auto* ghash = PROJECT_VERSION_GHASH;

	constexpr static inline auto* with_date = PROJECT_VERSION_WITH_DATE;
	constexpr static inline auto* with_hash = PROJECT_VERSION_WITH_HASH;
};

constexpr static inline ProjectVersion AppVer{};

constexpr auto* AppName = PROJECT_NAME;
constexpr auto* OrgName = "";

/*==================================================================*/

class HomeDirManager;
class GlobalAudioBase;
class BasicVideoSpec;
class SystemInterface;

/*==================================================================*/

class FrontendHost final {
	FrontendHost() noexcept;

	FrontendHost(const FrontendHost&) = delete;
	FrontendHost& operator=(const FrontendHost&) = delete;

	static void set_open_file_dialog_result(std::string_view file) noexcept;

	static constexpr std::size_t s_mru_limit = 10;
	static inline SimpleMRU<FileItem> s_file_mru = s_mru_limit;

	static void import_mru(std::string* src) noexcept {
		for (std::size_t i = 0; i < s_mru_limit; ++i) {
			auto& entry = src[s_mru_limit - 1 - i];
			if (entry.empty()) { continue; }
			s_file_mru.insert(std::move(entry));
		}
	}

	static void export_mru(std::string* dst) noexcept {
		for (std::size_t i = 0; i < s_file_mru.size(); ++i) {
			dst[i] = s_file_mru[i]->string();
		}
	}

/*==================================================================*/

private:
	class SystemInstance final {
		struct StopSystemThread {
			void operator()(SystemInterface*) noexcept;
		};
		using SystemCore = std::unique_ptr
			<SystemInterface, StopSystemThread>;

	public:
		SystemCore core;

		bool delimiters{};
		bool statistics{};

		/***/ auto* operator->()       noexcept { return core.get(); }
		const auto* operator->() const noexcept { return core.get(); }

		operator bool() const noexcept { return bool(core); }
	};

	using SystemID = std::size_t;
	using SystemMap = std::unordered_map<SystemID, SystemInstance>;

	SystemMap m_systems{};

	SimpleMRU<SystemID> m_focus_mru;

	void prune_terminated_systems() noexcept;
	void find_last_focused_system() noexcept;

	void unload_system_instance(SystemID system_id = 0) noexcept;
	void insert_system_instance(SystemInterface* system) noexcept;

	void toggle_system_delimiters(SystemInstance& system) noexcept;
	void toggle_system_statistics(SystemInstance& system) noexcept;

	bool m_application_minimized{};
	void set_system_hidden_status(SystemInstance& system, bool state) noexcept;

/*==================================================================*/

public:
	static inline HomeDirManager*  HDM{};
	static inline GlobalAudioBase* GAB{};
	static inline BasicVideoSpec*  BVS{};

public:
	struct Settings {
		float ui_zoom_scale = 1.0f;
		float ui_text_scale = 1.0f;

		std::string file_mru_cache[s_mru_limit];

		SettingsMap map() noexcept;
	};

private:
	auto export_settings() const noexcept -> Settings;

/*==================================================================*/

private:
	void load_file_from_disk(std::string_view file_path);
	void handle_main_hotkeys();
	void setup_gui_callables() noexcept;

/*==================================================================*/

public:
	static FrontendHost*
	init_application(std::string_view home_override, std::string_view config_name,
		std::string_view game_file_path, bool force_portable) noexcept;

	void quit_application() noexcept;

	int handle_client_events(void* event) noexcept;
	int process_client_frame();
};
